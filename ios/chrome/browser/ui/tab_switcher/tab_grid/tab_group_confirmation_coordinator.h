// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUP_CONFIRMATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUP_CONFIRMATION_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class Browser;
enum class TabGroupActionType;

// An action block type that takes no argument and returns nothing. A tab group
// is modified inside the block.
typedef void (^TabGroupActionBlock)();

// TODO(crbug.com/329631145): Move this class to
// ios/chrome/browser/ui/tab_switcher/ when this is used from tab strips.
// Coordinator for displaying an action sheet to confirm the action to a tab
// group.
@interface TabGroupConfirmationCoordinator : ChromeCoordinator

// The action that a tab group is going to take.
@property(nonatomic, strong) TabGroupActionBlock action;

// Designated initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                actionType:(TabGroupActionType)actionType
                                sourceView:(UIView*)sourceView
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TAB_GROUP_CONFIRMATION_COORDINATOR_H_
