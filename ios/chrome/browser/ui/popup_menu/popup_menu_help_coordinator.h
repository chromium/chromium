// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_HELP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_HELP_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class OverflowMenuUIConfiguration;
@protocol PopupMenuUIUpdating;

// Coordinator for the popup menu help feature, educating users about the new
// menu
@interface PopupMenuHelpCoordinator : ChromeCoordinator

@property(nonatomic, weak) OverflowMenuUIConfiguration* uiConfiguration;

// UI updater.
@property(nonatomic, weak) id<PopupMenuUIUpdating> UIUpdater;

// An integer whose value is matching the overflow_menu::Destination,
// representing the destination on the overflow menu that should be highlighted.
// Return nil if no destination is highlighted (default scenario).
- (NSNumber*)highlightDestination;

- (void)showHistoryOnOverflowMenuIPHInViewController:(UIViewController*)menu;

@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_HELP_COORDINATOR_H_
