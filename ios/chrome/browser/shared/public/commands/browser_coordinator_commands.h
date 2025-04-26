// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BROWSER_COORDINATOR_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BROWSER_COORDINATOR_COMMANDS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

namespace base {
class ScopedClosureRunner;
}
@protocol BadgeItem;
class GURL;

// Protocol for commands that will be handled by the BrowserCoordinator.
// TODO(crbug.com/41427057) : Rename this protocol to one that is more
// descriptive and representative of the contents.
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
// interaction with the web page until the returned value is destructed.
- (base::ScopedClosureRunner)showActivityOverlay;

#if !defined(NDEBUG)
// Inserts a new tab showing the HTML source of the current page.
- (void)viewSource;
#endif

// Animates the NTP fakebox to the focused position and focuses the real
// omnibox.
- (void)focusFakebox;

// Closes the current tab.
// TODO(crbug.com/40806293): Refactor this command away; call sites should close
// via the WebStateList.
- (void)closeCurrentTab;

// Shows the spotlight debugger.
- (void)showSpotlightDebugger;

// Preloads voice search in the current BVC.
- (void)preloadVoiceSearch;

// Dismiss the payments suggestions.
- (void)dismissPaymentSuggestions;

// Dismiss the password suggestions.
- (void)dismissPasswordSuggestions;

// Dismiss the card unmask authentication prompt.
- (void)dismissCardUnmaskAuthentication;

// Dismiss the plus address bottom sheet.
- (void)dismissPlusAddressBottomSheet;

// Dismiss the virtual card enrollment bottom sheet.
- (void)dismissVirtualCardEnrollmentBottomSheet;

// Shows the omnibox position choice screen.
- (void)showOmniboxPositionChoice;

// Dismisses the omnibox position choice screen.
- (void)dismissOmniboxPositionChoice;

// Shows and dismisses the Lens Promo.
- (void)showLensPromo;
- (void)dismissLensPromo;

// Shows and dismisses the Enhanced Safe Browsing Promo.
- (void)showEnhancedSafeBrowsingPromo;
- (void)dismissEnhancedSafeBrowsingPromo;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_BROWSER_COORDINATOR_COMMANDS_H_
