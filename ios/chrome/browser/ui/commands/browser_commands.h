// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/external_search_commands.h"
#import "ios/chrome/browser/ui/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"

class GURL;
@class OpenNewTabCommand;
@class ReadingListAddCommand;

// Protocol for commands that will generally be handled by the "current tab",
// which in practice is the BrowserViewController instance displaying the tab.
@protocol BrowserCommands<NSObject,
                          ActivityServiceCommands,
                          ExternalSearchCommands,
                          PageInfoCommands,
                          PopupMenuCommands,
                          QRScannerCommands,
                          SnackbarCommands>

// Closes the current tab.
- (void)closeCurrentTab;

// Navigates backwards in the current tab's history.
- (void)goBack;

// Navigates forwards in the current tab's history.
- (void)goForward;

// Stops loading the current web page.
- (void)stopLoading;

// Reloads the current web page
- (void)reload;

// Bookmarks the current page.
- (void)bookmarkPage;

// Prints the currently active tab.
- (void)printTab;

// Adds a page to the reading list using data in |command|.
- (void)addToReadingList:(ReadingListAddCommand*)command;

// Shows the Reading List UI.
- (void)showReadingList;

// Preloads voice search on the current BVC.
- (void)preloadVoiceSearch;

// Closes all tabs.
- (void)closeAllTabs;

// Closes all incognito tabs.
- (void)closeAllIncognitoTabs;

#if !defined(NDEBUG)
// Shows the source of the current page.
- (void)viewSource;
#endif

// Shows the Find In Page bar.
- (void)showFindInPage;

// Close and disable Find In Page bar.
- (void)closeFindInPage;

// Search the current tab for the query string in the Find In Page bar.
- (void)searchFindInPage;

// Go to the next location of the Find In Page query string in the current tab.
- (void)findNextStringInPage;

// Go to the previous location of the Find In Page query string in the current
// tab.
- (void)findPreviousStringInPage;

// Shows the online help page in a tab.
- (void)showHelpPage;

// Shows the bookmarks manager.
- (void)showBookmarksManager;

// Shows recent tabs.
- (void)showRecentTabs;

// Requests the "desktop" version of the current page in the active tab.
- (void)requestDesktopSite;

// Requests the "mobile" version of the current page in the active tab.
- (void)requestMobileSite;

// Navigates to the Memex tab switcher.
// TODO(crbug.com/799601): Delete this once its not needed.
- (void)navigateToMemexTabSwitcher;

// Prepares the browser to display a popup menu.
- (void)prepareForPopupMenuPresentation:(PopupMenuCommandType)type;

// Shows the consent bump if it is required.
- (void)showConsentBumpIfNeeded;

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

// Unfocus omnibox then switch to the first tab displaying |URL|.
- (void)unfocusOmniboxAndSwitchToTabWithURL:(const GURL&)URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_
