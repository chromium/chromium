// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_CONSTANTS_H_

#import <Foundation/Foundation.h>

// Accessibility IDs for the table view in various kinds of popup menus.
extern NSString* const kPopupMenuToolsMenuTableViewId;
extern NSString* const kPopupMenuNavigationTableViewId;

// Accessibility IDs for the Tools Menu items.
// Reload item accessibility Identifier.
extern NSString* const kToolsMenuReload;
// Stop item accessibility Identifier.
extern NSString* const kToolsMenuStop;
// New Tab item accessibility Identifier.
extern NSString* const kToolsMenuNewTabId;
// New incognito Tab item accessibility Identifier.
extern NSString* const kToolsMenuNewIncognitoTabId;
// Close all Tabs item accessibility Identifier.
extern NSString* const kToolsMenuCloseAllTabsId;
// Close all incognito Tabs item accessibility Identifier.
extern NSString* const kToolsMenuCloseAllIncognitoTabsId;
// Close the current tab item accessibility Identifier.
extern NSString* const kToolsMenuCloseTabId;
// Bookmarks item accessibility Identifier.
extern NSString* const kToolsMenuBookmarksId;
// Reading List item accessibility Identifier.
extern NSString* const kToolsMenuReadingListId;
// Other Devices item accessibility Identifier.
extern NSString* const kToolsMenuOtherDevicesId;
// History item accessibility Identifier.
extern NSString* const kToolsMenuHistoryId;
// Report an issue item accessibility Identifier.
extern NSString* const kToolsMenuReportAnIssueId;
// Translate item accessibility Identifier.
extern NSString* const kToolsMenuTranslateId;
// Find in Page item accessibility Identifier.
extern NSString* const kToolsMenuFindInPageId;
// Request desktop item accessibility Identifier.
extern NSString* const kToolsMenuRequestDesktopId;
// Settings item accessibility Identifier.
extern NSString* const kToolsMenuSettingsId;
// Help item accessibility Identifier.
extern NSString* const kToolsMenuHelpId;
// Request mobile item accessibility Identifier.
extern NSString* const kToolsMenuRequestMobileId;
// ReadLater item accessibility Identifier.
extern NSString* const kToolsMenuReadLater;
// AddBookmark item accessibility Identifier.
extern NSString* const kToolsMenuAddToBookmarks;
// EditBookmark item accessibility Identifier.
extern NSString* const kToolsMenuEditBookmark;
// SiteInformation item accessibility Identifier.
extern NSString* const kToolsMenuSiteInformation;
// Paste and Go item accessibility Identifier.
extern NSString* const kToolsMenuPasteAndGo;
// Voice Search item accessibility Identifier.
extern NSString* const kToolsMenuVoiceSearch;
// TODO(crbug.com/974751): Check if this is still used.
// Search item accessibility Identifier.
extern NSString* const kToolsMenuSearch;
// TODO(crbug.com/974751): Check if this is still used.
// Incognito Search item accessibility Identifier.
extern NSString* const kToolsMenuIncognitoSearch;
// QR Code Search item accessibility Identifier.
extern NSString* const kToolsMenuQRCodeSearch;
// Copied Image Search item accessibility Identifier.
extern NSString* const kToolsMenuCopiedImageSearch;

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_CONSTANTS_H_
