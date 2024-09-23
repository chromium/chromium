// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_CLEAR_BROWSING_DATA_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_CLEAR_BROWSING_DATA_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The accessibility identifier of the clear browsing data view.
extern NSString* const kClearBrowsingDataViewAccessibilityIdentifier;

// The accessibility identifiers of the cells in the clear browsing data view.
extern NSString* const kClearBrowsingHistoryCellAccessibilityIdentifier;
extern NSString* const kClearCookiesCellAccessibilityIdentifier;
extern NSString* const kClearCacheCellAccessibilityIdentifier;
extern NSString* const kClearSavedPasswordsCellAccessibilityIdentifier;
extern NSString* const kClearAutofillCellAccessibilityIdentifier;

// The accessibility identifier for the button inside a pop-up cell.
extern NSString* const kQuickDeletePopUpButtonIdentifier;

// The accessibility identifier for the browsing data button on the main page.
extern NSString* const kQuickDeleteBrowsingDataButtonIdentifier;

// The accessibility identifier for the footer string.
extern NSString* const kQuickDeleteFooterIdentifier;

// The accessibility identifier for the confirm button on the browsing data
// types selection page.
extern NSString* const kQuickDeleteBrowsingDataConfirmButtonIdentifier;

// The accessibility identifier for history row on browsing data page.
extern NSString* const kQuickDeleteBrowsingDataHistoryIdentifier;
// The accessibility identifier for tabs row on browsing data page.
extern NSString* const kQuickDeleteBrowsingDataTabsIdentifier;
// The accessibility identifier for site data row on browsing data page.
extern NSString* const kQuickDeleteBrowsingDataSiteDataIdentifier;
// The accessibility identifier for cache row on browsing data page.
extern NSString* const kQuickDeleteBrowsingDataCacheIdentifier;
// The accessibility identifier for passwords row on browsing data page.
extern NSString* const kQuickDeleteBrowsingDataPasswordsIdentifier;
// The accessibility identifier for autofill row on browsing data page.
extern NSString* const kQuickDeleteBrowsingDataAutofillIdentifier;

// The accessibility identifier for the footer string on the browsing data page.
extern NSString* const kQuickDeleteBrowsingDataFooterIdentifier;

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CELLS_CLEAR_BROWSING_DATA_CONSTANTS_H_
