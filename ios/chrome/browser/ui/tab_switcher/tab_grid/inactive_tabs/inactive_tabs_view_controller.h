// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class InactiveGridViewController;
@class InactiveTabsViewController;

// Protocol used to relay relevant user interactions from the
// InactiveTabsViewController.
@protocol InactiveTabsViewControllerDelegate

// Invoked when the inactive tabs view controller's back button is tapped.
- (void)inactiveTabsViewControllerDidTapBackButton:
    (InactiveTabsViewController*)inactiveTabsViewController;

// Invoked when the button to close all inactive tabs is tapped.
- (void)inactiveTabsViewController:
            (InactiveTabsViewController*)inactiveTabsViewController
    didTapCloseAllInactiveBarButtonItem:(UIBarButtonItem*)barButtonItem;

@end

// Displays the list of inactive tabs.
@interface InactiveTabsViewController : UIViewController

// The embedded grid view controller.
@property(nonatomic, readonly) InactiveGridViewController* gridViewController;

// Delegate to handle interactions.
@property(nonatomic, weak) id<InactiveTabsViewControllerDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_VIEW_CONTROLLER_H_
