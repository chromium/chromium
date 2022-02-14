// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COORDINATOR_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COORDINATOR_COMMANDS_H_

#import <Foundation/Foundation.h>

@protocol BadgeItem;

// Protocol for commands that will be handled by the BrowserCoordinator.
// TODO(crbug.com/906662) : Rename this protocol to one that is more descriptive
// and representative of the contents.
@protocol BrowserCoordinatorCommands

// Prints the currently active tab.
// Print preview will be presented on top of |baseViewController|.
- (void)printTabWithBaseViewController:(UIViewController*)baseViewController;

// Prints an image.
// Print preview will be presented on top of |baseViewController|.
- (void)printImage:(UIImage*)image
                 title:(NSString*)title
    baseViewController:(UIViewController*)baseViewController;

// Shows the downloads folder.
- (void)showDownloadsFolder;

// Shows the Reading List UI.
- (void)showReadingList;

// Shows recent tabs.
- (void)showRecentTabs;

// Shows the AddCreditCard UI.
- (void)showAddCreditCard;

// Displays the Badge popup menu showing |badgeItems|.
- (void)displayPopupMenuWithBadgeItems:(NSArray<id<BadgeItem>>*)badgeItems;

// Dismisses the Badge popup menu.
- (void)dismissBadgePopupMenu;

// Shows the activity indicator overlay that appears over the view to prevent
// interaction with the web page.
- (void)showActivityOverlay;

// Hides the activity indicator overlay.
- (void)hideActivityOverlay;

#if !defined(NDEBUG)
// Inserts a new tab showing the HTML source of the current page.
- (void)viewSource;
#endif

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BROWSER_COORDINATOR_COMMANDS_H_
