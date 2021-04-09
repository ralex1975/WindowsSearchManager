// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

/*
This file is part of Search Manager - shows Windows Search internals.

Copyright (C) 2012-2021 Nikolay Raspopov <raspopov@cherubicsoft.com>

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
( at your option ) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see < http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"
#include "Item.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define KEY_PROTOCOLS _T("SOFTWARE\\Microsoft\\Windows Search\\ProtocolHandlers")

// CItem

CItem::CItem(group_t group)
	: Group( group )
{
}

CString CItem::GetTitle() const
{
	if ( Protocol.IsEmpty() )
	{
		return URL;
	}
	else if ( User.IsEmpty() )
	{
		return Path;
	}
	else
	{
		return _T("(") + User + _T(") ") + Path;
	}
}

void CItem::SetURL(LPCTSTR szURL)
{
	URL = szURL;

	int begin = URL.Find( _T("://") );
	if ( begin != -1 )
	{
		Protocol = URL.Left( begin );

		if ( Protocol.CompareNoCase( DEFAULT_PROTOCOL ) == 0 )
		{
			// Default root
		}
		else if ( Protocol.CompareNoCase( FILE_PROTOCOL ) == 0 )
		{
			// File
		}
		else
		{
			// Protocol handler
			TCHAR progid[ MAX_PATH ] = {};
			DWORD type, progid_size = MAX_PATH;
			if ( SHGetValue( HKEY_LOCAL_MACHINE, KEY_PROTOCOLS, Protocol, &type, progid, &progid_size ) == ERROR_SUCCESS )
			{
				CLSID clsid = {};
				if ( SUCCEEDED( CLSIDFromProgID( progid, &clsid ) ) )
				{
					LPOLESTR str_clsid = nullptr;
					if ( SUCCEEDED( StringFromCLSID( clsid, &str_clsid ) ) )
					{
						CString proc_key;
						proc_key.Format( _T("CLSID\\%s\\InprocServer32"), str_clsid );

						TCHAR proc[ MAX_PATH ] = {};
						DWORD proc_size = MAX_PATH;
						if ( SHGetValue( HKEY_CLASSES_ROOT, proc_key, _T(""), &type, proc, &proc_size ) == ERROR_SUCCESS )
						{
							;
						}
						CoTaskMemFree( str_clsid );
					}
				}
			}
		}

		begin += 3;

		int end = URL.Find( _T("/"), begin );
		if ( end != -1 )
		{
			const int len = end - begin;
			if ( len > 8 && URL.GetAt( begin ) == _T('{') && URL.GetAt( end - 1 ) == _T('}') )
			{
				User = URL.Mid( begin + 1, len - 2 );

				PSID psid = nullptr;
				if ( ConvertStringSidToSid( User, &psid ) )
				{
					TCHAR user[ MAX_PATH ];
					DWORD user_size = MAX_PATH;
					TCHAR domain[ MAX_PATH ];
					DWORD domain_size = MAX_PATH;
					SID_NAME_USE use;
					if ( LookupAccountSid( nullptr, psid, user, &user_size, domain, &domain_size, &use ) )
					{
						User.Format( _T("%s\\%s"), domain, user );
					}
					LocalFree( psid );
				}
			}
			else
			{
				//Store = URL.Mid( begin, len );
			}

			Path = URL.Mid( end + 1 );
		}
		else
		{
			Path = URL.Mid( begin );
		}
	}
}

// CRoot

CRoot::CRoot(ISearchCrawlScopeManager* pScope, ISearchRoot* pRoot)
	: CItem					( GROUP_ROOTS )
	, IncludedInCrawlScope	( FALSE )
	, IsHierarchical		( FALSE )
	, ProvidesNotifications	( FALSE )
	, UseNotificationsOnly	( FALSE )
{
	LPWSTR szURL = nullptr;
	HRESULT hr = pRoot->get_RootURL( &szURL );
	if ( SUCCEEDED( hr ) )
	{
		SetURL( szURL );
		CoTaskMemFree( szURL );

		hr = pScope->IncludedInCrawlScope( szURL, &IncludedInCrawlScope );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRoot->get_IsHierarchical( &IsHierarchical );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRoot->get_ProvidesNotifications( &ProvidesNotifications );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRoot->get_UseNotificationsOnly( &UseNotificationsOnly );
		ASSERT( SUCCEEDED( hr ) );
	}
}

void CRoot::InsertTo(CListCtrl& list)
{
	const CString title = GetTitle();

	LVITEM item = { LVIF_TEXT | LVIF_GROUPID | LVIF_PARAM };
	item.iItem = list.GetItemCount();
	item.iGroupId = GROUP_ROOTS;
	item.pszText = const_cast< LPTSTR >( static_cast< LPCTSTR >( Protocol ) );
	item.lParam = reinterpret_cast< LPARAM >( static_cast< CItem* >( this ) );
	item.iItem = list.InsertItem( &item );
	item.mask = LVIF_TEXT;

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( static_cast< LPCTSTR >( title ) );
	list.SetItem( &item );

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( IncludedInCrawlScope ? _T("In Scope") : _T("") );
	list.SetItem( &item );

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( IsHierarchical ? _T("Hierarchical") : _T("") );
	list.SetItem( &item );

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( ProvidesNotifications ? ( UseNotificationsOnly ? _T("Notify Only") : _T("Notify") ): _T("") );
	list.SetItem( &item );
}

HRESULT CRoot::DeleteFrom(ISearchCrawlScopeManager* pScope)
{
	return pScope->RemoveRoot( URL );
}

HRESULT CRoot::AddTo(ISearchCrawlScopeManager* pScope )
{
	CComPtr< ISearchRoot > root;
	HRESULT hr = root.CoCreateInstance( __uuidof( CSearchRoot ) );
	if ( SUCCEEDED( hr ) )
	{
		hr = root->put_RootURL( URL );
		if ( SUCCEEDED( hr ) )
		{
			hr = pScope->AddRoot( root );
		}
	}
	return hr;
}

// CRule

CRule::CRule(ISearchCrawlScopeManager* pScope, ISearchScopeRule* pRule)
	: CItem					( GROUP_RULES )
	, IsInclude				( FALSE )
	, IsDefault				( FALSE )
	, HasChild				( FALSE )
{
	LPWSTR szURL = nullptr;
	HRESULT hr = pRule->get_PatternOrURL( &szURL );
	if ( SUCCEEDED( hr ) )
	{
		SetURL( szURL );
		CoTaskMemFree( szURL );

		hr = pRule->get_IsIncluded( &IsInclude );
		ASSERT( SUCCEEDED( hr ) );

		hr = pRule->get_IsDefault( &IsDefault );
		ASSERT( SUCCEEDED( hr ) );

		hr = pScope->HasChildScopeRule( URL, &HasChild );
	}
}

void CRule::InsertTo(CListCtrl& list)
{
	const CString title = GetTitle();

	LVITEM item = { LVIF_TEXT | LVIF_GROUPID | LVIF_PARAM };
	item.iItem = list.GetItemCount();
	item.iGroupId = GROUP_RULES;
	item.pszText = const_cast< LPTSTR >( static_cast< LPCTSTR >( Protocol ) );
	item.lParam = reinterpret_cast< LPARAM >( static_cast< CItem* >( this ) );
	item.iItem = list.InsertItem( &item );
	item.mask = LVIF_TEXT;

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( static_cast< LPCTSTR >( title ) );
	list.SetItem( &item );

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( IsInclude ? _T("Include") : _T("Exclude") );
	list.SetItem( &item );

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( IsDefault ? _T("Default") : _T("User") );
	list.SetItem( &item );

	++ item.iSubItem;
	item.pszText = const_cast< LPTSTR >( HasChild ? _T("Has child") : _T("") );
	list.SetItem( &item );
}

HRESULT CRule::DeleteFrom(ISearchCrawlScopeManager* pScope)
{
	return IsDefault ? pScope->RemoveDefaultScopeRule( URL ) : pScope->RemoveScopeRule( URL );
}

HRESULT CRule::AddTo(ISearchCrawlScopeManager* pScope )
{
	return IsDefault ? pScope->AddDefaultScopeRule( URL, IsInclude, FF_INDEXCOMPLEXURLS ) :
		pScope->AddUserScopeRule( URL, IsInclude, FALSE, FF_INDEXCOMPLEXURLS );
}