// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BROWSER_COORDINATOR_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BROWSER_COORDINATOR_COMMANDS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@protocol BadgeItem;
class GURL;

// Protocol for commands that will be handled by the BrowserCoordinator.
// TODO(crbug.com/906662) : Rename this protocol to one that is more descriptive
// and representative of the contents.
@protocol BrowserCoordinatorCommands

// Prints the currently active tab.
// Print preview will be presented on top of `baseViewController`.
- (void)printTabWithBaseViewController:(UIViewController*)baseViewController;

// Prints an image.
// Print preview will be presented on top of `baseViewController`.
- (void)printImage:(UIImage*)image
                 title:(NSString*)title
    baseViewController:(UIViewController*)baseViewController;

// Shows the downloads folder.
- (void)showDownloadsFolder;

// Shows the Reading List UI.
- (void)showReadingList;

// Shows an IPH pointing to where the Follow entry point is, if
// applicable.
- (void)showFollowWhileBrowsingIPH;

// Shows an IPH to explain to the user how to change the default site view, if
// applicable.
- (void)showDefaultSiteViewIPH;

// Shows bookmarks manager.
- (void)showBookmarksManager;

// Shows recent tabs.
- (void)showRecentTabs;

// Shows the translate infobar.
- (void)showTranslate;

// Shows the AddCreditCard UI.
- (void)showAddCreditCard;

// Shows the dialog for sending the page with `url` and `title` between a user's
// devices.
- (void)showSendTabToSelfUI:(const GURL&)url title:(NSString*)title;

// Hides the dialog shown by -showSendTabToSelfUI:.
- (void)hideSendTabToSelfUI;

// Shows the online help page in a tab.
- (void)showHelpPage;

// Shows the activity indicator overlay that appears over the view to prevent
// interaction with the web page.
- (void)showActivityOverlay;

// Hides the activity indicator overlay.
- (void)hideActivityOverlay;

#if !defined(NDEBUG)
// Inserts a new tab showing the HTML source of the current page.
- (void)viewSource;
#endif

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

// Closes the current tab.
// TODO(crbug.com/1272498): Refactor this command away; call sites should close
// via the WebStateList.
- (void)closeCurrentTab;

// Shows what's new.
- (void)showWhatsNew;

// Dismisses what's new.
- (void)dismissWhatsNew;

// Shows what's new IPH.
- (void)showWhatsNewIPH;

// Shows the spotlight debugger.
- (void)showSpotlightDebugger;

// Preloads voice search in the current BVC.
- (void)preloadVoiceSearch;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BROWSER_COORDINATOR_COMMANDS_H_
