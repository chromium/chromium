// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_consumer.h"

@class AccountPickerConfiguration;
@protocol AccountPickerCoordinatorDelegate;
@protocol AccountPickerLogger;
@protocol SystemIdentity;

// Presents a bottom sheet that lets the user pick or add an account on the
// device to perform some action.
@interface AccountPickerCoordinator : ChromeCoordinator <AccountPickerConsumer>

@property(nonatomic, weak) id<AccountPickerCoordinatorDelegate> delegate;

@property(nonatomic, weak) id<AccountPickerLogger> logger;

// View controller presented by the coordinator. Can be used to present a view
// on top of the account picker e.g. the AddAccountSigninCoordinator's view.
@property(nonatomic, readonly) UIViewController* viewController;

// View controller to add as a child view controller in the account confirmation
// screen above the list of accounts to choose from.
@property(nonatomic, weak)
    UIViewController* accountConfirmationChildViewController;

// The identity currently presented as selected.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;

// Inits the coordinator.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                             configuration:
                                 (AccountPickerConfiguration*)configuration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Same as -stop but can be animated.
- (void)stopAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_COORDINATOR_H_
