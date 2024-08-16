// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol PopupMenuUIUpdating;

// Coordinator for the popup menu, handling the commands.
@interface PopupMenuCoordinator : ChromeCoordinator

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The base view controller for this coordinator
@property(weak, nonatomic, readwrite) UIViewController* baseViewController;

// UI updater.
@property(nonatomic, weak) id<PopupMenuUIUpdating> UIUpdater;

// Returns whether this coordinator is showing a popup menu.
- (BOOL)isShowingPopupMenu;

// Starts the popup menu's child help coordinator.
- (void)startPopupMenuHelpCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_COORDINATOR_H_
