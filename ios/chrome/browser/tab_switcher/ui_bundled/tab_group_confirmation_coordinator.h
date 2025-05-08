// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_CONFIRMATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_CONFIRMATION_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/tab_group_confirmation_commands.h"

class Browser;
enum class TabGroupActionType;

// An action block type that takes no argument and returns nothing. A tab group
// is modified inside the block.
typedef void (^TabGroupActionBlock)();

// Coordinator for displaying an action sheet to confirm the action to a tab
// group.
@interface TabGroupConfirmationCoordinator
    : ChromeCoordinator <TabGroupConfirmationCommands>

// The action that a tab group is going to take.
@property(nonatomic, strong) TabGroupActionBlock primaryAction;

// The secondary action that a tab group is going to take. Can be nil.
@property(nonatomic, strong) TabGroupActionBlock secondaryAction;

// The name of the tab group.
@property(nonatomic, copy) NSString* tabGroupName;

// The action to execute when dismissing the confirmation coordinator. Can be
// `nil`.
@property(nonatomic, strong) TabGroupActionBlock dismissAction;

// YES to display an alert instead of an action sheet. Default is NO.
@property(nonatomic, assign) BOOL showAsAlert;

// YES if the cancel button should be available. Default is YES.
@property(nonatomic, assign) BOOL canCancel;

// Designated initializer with a parameter for anchoring the popover to a
// UIView.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                actionType:(TabGroupActionType)actionType
                                sourceView:(UIView*)sourceView
    NS_DESIGNATED_INITIALIZER;

// Designated initializer with a parameter for anchoring the popover to a
// UIBarButtonItem.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                actionType:(TabGroupActionType)actionType
                          sourceButtonItem:(UIBarButtonItem*)sourceButtonItem
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_CONFIRMATION_COORDINATOR_H_
