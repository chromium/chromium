// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AccountPickerConfirmationScreenCoordinator;

@protocol AccountPickerConfirmationScreenCoordinatorDelegate <NSObject>

// Called when the user wants to skip the consistency promo.
- (void)accountPickerConfirmationScreenCoordinatorCancel:
    (AccountPickerConfirmationScreenCoordinator*)coordinator;

// Called when the user wants to choose a different identity.
- (void)
    accountPickerConfirmationScreenCoordinatorOpenAccountPickerSelectionScreen:
        (AccountPickerConfirmationScreenCoordinator*)coordinator;

// Called when the user wants to sign-in with the default identity.
- (void)accountPickerConfirmationScreenCoordinatorSubmit:
    (AccountPickerConfirmationScreenCoordinator*)coordinator;

// Called when the user wants to sign in without an existing account.
- (void)accountPickerConfirmationScreenCoordinatorOpenAddAccount:
    (AccountPickerConfirmationScreenCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_CONFIRMATION_ACCOUNT_PICKER_CONFIRMATION_SCREEN_COORDINATOR_DELEGATE_H_
