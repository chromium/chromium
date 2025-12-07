// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_CONSTANTS_H_

#import <Foundation/Foundation.h>

// The accessibility identifier for the clear browsing data button.
extern NSString* const kClearBrowsingDataButtonIdentifier;

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

/* UMA histogram values for My Activity Navigiation.
 * Note: this should stay in sync with
 * ClearBrowsingDataMyActivityNavigation in enums.xml. */
enum class MyActivityNavigation {
  kTopLevel = 0,
  kSearchHistory = 1,
  // New elements go above.
  kMaxValue = kSearchHistory,
};

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CLEAR_BROWSING_DATA_QUICK_DELETE_CONSTANTS_H_
