// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class AccountPickerSelectionScreenCoordinator;
@protocol AccountPickerLayoutDelegate;
@protocol AccountPickerLogger;
@protocol SystemIdentity;

// Delegate for AccountPickerSelectionScreenCoordinator.
@protocol AccountPickerSelectionScreenCoordinatorDelegate <NSObject>

// Invoked when the user selected an identity.
- (void)accountPickerSelectionScreenCoordinatorIdentitySelected:
    (AccountPickerSelectionScreenCoordinator*)coordinator;

// Invoke add account SigninCoordinator.
- (void)accountPickerSelectionScreenCoordinatorOpenAddAccount:
    (AccountPickerSelectionScreenCoordinator*)coordinator;

@end

// This coordinator presents an entry point to the Chrome sign-in flow with the
// default account available on the device.
@interface AccountPickerSelectionScreenCoordinator : ChromeCoordinator

// Identity selected by the user.
@property(nonatomic, strong, readonly) id<SystemIdentity> selectedIdentity;
@property(nonatomic, strong, readonly) UIViewController* viewController;
@property(nonatomic, weak) id<AccountPickerSelectionScreenCoordinatorDelegate>
    delegate;
@property(nonatomic, weak) id<AccountPickerLayoutDelegate> layoutDelegate;

- (void)start NS_UNAVAILABLE;
// Starts the coordinator with the selected identity.
- (void)startWithSelectedIdentity:(id<SystemIdentity>)selectedIdentity;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_COORDINATOR_H_
