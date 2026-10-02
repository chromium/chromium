// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AND_PASSWORDS_SIGNIN_PROMO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AND_PASSWORDS_SIGNIN_PROMO_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"

@protocol AutofillAndPasswordsSigninPromoConsumer;
class AuthenticationService;
class ChromeAccountManagerService;
class PrefService;
@protocol SigninPromoViewMediatorDelegate;

namespace signin {
class AccountPreviewDataService;
class IdentityManager;
}

namespace syncer {
class SyncService;
}

// Mediator class that encapsulates the SigninPromoViewMediator setup and
// auth/identity observing for the Autofill and Passwords settings page.
@interface AutofillAndPasswordsSigninPromoMediator : NSObject

// Consumer for this mediator.
@property(nonatomic, weak) id<AutofillAndPasswordsSigninPromoConsumer> consumer;

// Delegate for this mediator.
@property(nonatomic, weak) id<SigninPromoViewMediatorDelegate> delegate;

- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
                      authService:(AuthenticationService*)authService
                  identityManager:(signin::IdentityManager*)identityManager
                      prefService:(PrefService*)prefService
        accountPreviewDataService:
            (signin::AccountPreviewDataService*)accountPreviewDataService
                      syncService:(syncer::SyncService*)syncService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator and disconnects the underlying sign-in promo
// mediator.
- (void)disconnect;

// Notifies the mediator that the table view controller loaded its content.
- (void)contentDidLoad;

// Notifies the mediator that the sign-in promo progress state changed or close
// button was tapped.
- (void)updateSignInPromoVisibility;

// Notifies the mediator that the sign-in flow completed.
- (void)signinDidCompleteWithResult:(SigninCoordinatorResult)result;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_AUTOFILL_AND_PASSWORDS_COORDINATOR_AUTOFILL_AND_PASSWORDS_SIGNIN_PROMO_MEDIATOR_H_
