// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class AccountPickerConfiguration;
@protocol AccountPickerConfirmationScreenCoordinatorDelegate;
@protocol AccountPickerLayoutDelegate;
@protocol SystemIdentity;

// This coordinator presents the an account picker confirmation screen which
// lets the user pick one of the accounts on the device to perform some action
// e.g. save an image to that account's Photos library. A switch can be shown to
// let the user choose whether to be presented the account picker every time
// that action is performed.
@interface AccountPickerConfirmationScreenCoordinator : ChromeCoordinator

// Inits the coordinator.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                             configuration:
                                 (AccountPickerConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, strong, readonly) UIViewController* viewController;
@property(nonatomic, weak)
    id<AccountPickerConfirmationScreenCoordinatorDelegate>
        delegate;
@property(nonatomic, weak) id<AccountPickerLayoutDelegate> layoutDelegate;
// This property can be used only after the coordinator is started.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;
// If `showAskEveryTimeSwitch` is YES, then this is the current value of the
// switch. Otherwise defaults to YES.
@property(nonatomic, assign, readonly) BOOL askEveryTime;
// View controller to add as a child view controller in the account confirmation
// screen above the list of accounts to choose from.
@property(nonatomic, weak) UIViewController* childViewController;

// Starts the spinner and disables buttons.
- (void)startValidationSpinner;
// Stops the spinner and enables buttons.
- (void)stopValidationSpinner;
// Shows/hides the identity button.
- (void)setIdentityButtonHidden:(BOOL)hidden animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_COORDINATOR_H_
