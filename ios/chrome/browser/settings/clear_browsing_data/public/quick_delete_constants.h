// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_CLEAR_BROWSING_DATA_PUBLIC_QUICK_DELETE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SETTINGS_CLEAR_BROWSING_DATA_PUBLIC_QUICK_DELETE_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The accessibility identifier for the clear browsing data button.
extern NSString* const kClearBrowsingDataButtonIdentifier;

// The accessibility identifier for the button inside a pop-up cell.
extern NSString* const kQuickDeletePopUpButtonIdentifier;

// The accessibility identifier for the browsing data button on the main page.
extern NSString* const kQuickDeleteBrowsingDataButtonIdentifier;

// The accessibility identifier for the done button in the quick delete browsing
// data page.
extern NSString* const kQuickDeleteBrowsingDataDoneButtonIdentifier;

// The accessibility identifier for the history cell in the quick delete
// browsing data page.
extern NSString* const kQuickDeleteBrowsingDataHistoryIdentifier;
// The accessibility identifier for the tabs cell in the quick delete browsing
// data page.
extern NSString* const kQuickDeleteBrowsingDataTabsIdentifier;
// The accessibility identifier for the site data cell in the quick delete
// browsing data page.
extern NSString* const kQuickDeleteBrowsingDataSiteDataIdentifier;
// The accessibility identifier for the cache cell in the quick delete browsing
// data page.
extern NSString* const kQuickDeleteBrowsingDataCacheIdentifier;
// The accessibility identifier for the autofill cell in the quick delete
// browsing data page.
extern NSString* const kQuickDeleteBrowsingDataAutofillIdentifier;

// The accessibility identifier for the Browsing data footer string in the quick
// delete browsing data page.
extern NSString* const kQuickDeleteBrowsingDataFooterIdentifier;

// The accessibility identifier for the "Manage other data" cell in the quick
// delete browsing data page.
extern NSString* const kQuickDeleteManageOtherDataCellIdentifier;

// The accessibility identifier for the passwords and passkeys cell in the quick
// delete other data page.
extern NSString* const kQuickDeleteOtherDataPasswordsAndPasskeysIdentifier;

// The accessibility identifier for the search history cell in the quick delete
// other data page.
extern NSString* const kQuickDeleteOtherDataSearchHistoryIdentifier;

// The accessibility identifier for the "my activity" cell in the quick delete
// other data page.
extern NSString* const kQuickDeleteOtherDataMyActivityIdentifier;

// The accessibility identifier for the footer string in the quick delete other
// data page.
extern NSString* const kQuickDeleteOtherDataFooterIdentifier;

#endif  // IOS_CHROME_BROWSER_SETTINGS_CLEAR_BROWSING_DATA_PUBLIC_QUICK_DELETE_CONSTANTS_H_
