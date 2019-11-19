// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/activity_service_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/infobar_commands.h"
#import "ios/chrome/browser/ui/commands/page_info_commands.h"
#import "ios/chrome/browser/ui/commands/popup_menu_commands.h"
#import "ios/chrome/browser/ui/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"

class GURL;
@class ReadingListAddCommand;
@class SendTabToSelfCommand;

// Protocol for commands that will generally be handled by the "current tab",
// which in practice is the BrowserViewController instance displaying the tab.
// TODO(crbug.com/906662) : Extract BrowserCoordinatorCommands from
// BrowserCommands.
@protocol BrowserCommands <NSObject,
                           ActivityServiceCommands,
                           BrowserCoordinatorCommands,
                           InfobarCommands,
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

// Adds a page to the reading list using data in |command|.
- (void)addToReadingList:(ReadingListAddCommand*)command;

// Preloads voice search on the current BVC.
- (void)preloadVoiceSearch;

// Closes all tabs.
- (void)closeAllTabs;

#if !defined(NDEBUG)
// Shows the source of the current page.
- (void)viewSource;
#endif

// Shows the translate infobar.
- (void)showTranslate;

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

// Shows the dialog for sending the current tab between a user's devices.
- (void)showSendTabToSelfUI;

// Requests the "desktop" version of the current page in the active tab.
- (void)requestDesktopSite;

// Requests the "mobile" version of the current page in the active tab.
- (void)requestMobileSite;

// Prepares the browser to display a popup menu.
- (void)prepareForPopupMenuPresentation:(PopupMenuCommandType)type;

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

// Searches for an image in the current tab.
- (void)searchByImage:(UIImage*)image;

// Sends the tab to another of the user's devices using the data in |command|.
- (void)sendTabToSelf:(SendTabToSelfCommand*)command;

// Shows/Hides the activity indicator overlay that appears over the view to
// prevent interaction with the web page.
- (void)showActivityOverlay:(BOOL)show;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COMMANDS_H_
