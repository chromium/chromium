// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_HELP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_HELP_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class OverflowMenuUIConfiguration;
@protocol OverflowMenuActionProvider;
@protocol PopupMenuUIUpdating;

// Coordinator for the popup menu help feature, educating users about the new
// menu
@interface PopupMenuHelpCoordinator : ChromeCoordinator

@property(nonatomic, weak) OverflowMenuUIConfiguration* uiConfiguration;

@property(nonatomic, weak) id<OverflowMenuActionProvider> actionProvider;

// UI updater.
@property(nonatomic, weak) id<PopupMenuUIUpdating> UIUpdater;

// An integer whose value is matching the overflow_menu::Destination,
// representing the destination on the overflow menu that should be highlighted.
// Return nil if no destination is highlighted (default scenario).
- (NSNumber*)highlightDestination;

// Alerts the help coordinator that the overflow menu opened with the given
// view controller, so it can show any necessary IPH.
- (void)showIPHAfterOpenOfOverflowMenu:(UIViewController*)menu;

// Returns whether overflow menu button in the toolbar has a blue dot.
- (BOOL)hasBlueDotForOverflowMenu;

// Updates the blue dot visibility based on eligibility.
- (void)updateBlueDotVisibility;

// Notifies that IPH bubble will be presenting on tools menu button.
- (void)notifyIPHBubblePresenting;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_HELP_COORDINATOR_H_
