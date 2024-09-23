// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class AccountPickerCoordinator;
@protocol SystemIdentity;

@protocol AccountPickerCoordinatorDelegate <NSObject>

// Called when the account picker wants to let the user add an account to the
// device. `completion` should be called with the identity added from the
// AddAccountSigninCoordinator.
- (void)accountPickerCoordinator:
            (AccountPickerCoordinator*)accountPickerCoordinator
    openAddAccountWithCompletion:(void (^)(id<SystemIdentity>))completion;

// Called when the user confirmed their selection in the confirmation screen. If
// the "Ask every time" switch was shown, its value is passed as `askEveryTime`.
// Otherwise, `askEveryTime` will be YES.
- (void)accountPickerCoordinator:
            (AccountPickerCoordinator*)accountPickerCoordinator
               didSelectIdentity:(id<SystemIdentity>)identity
                    askEveryTime:(BOOL)askEveryTime;

// Called when the user taps "Cancel" in the confirmation screen.
- (void)accountPickerCoordinatorCancel:
    (AccountPickerCoordinator*)accountPickerCoordinator;

// Called when the form has no identities left to present (identities have been
// removed externally).
- (void)accountPickerCoordinatorAllIdentityRemoved:
    (AccountPickerCoordinator*)accountPickerCoordinator;

// Called when the account picker has been dismissed.
- (void)accountPickerCoordinatorDidStop:
    (AccountPickerCoordinator*)accountPickerCoordinator;

@end

#endif  // IOS_CHROME_BROWSER_ACCOUNT_PICKER_UI_BUNDLED_ACCOUNT_PICKER_COORDINATOR_DELEGATE_H_
