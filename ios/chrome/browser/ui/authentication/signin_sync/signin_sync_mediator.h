// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

namespace consent_auditor {
class ConsentAuditor;
}

namespace signin {
class IdentityManager;
}

namespace unified_consent {
class UnifiedConsentService;
}

namespace syncer {
class SyncService;
}

@class AuthenticationFlow;
class AuthenticationService;
class ChromeAccountManagerService;
@protocol SigninSyncConsumer;
@protocol SigninSyncMediatorDelegate;
class SyncSetupService;
@protocol SystemIdentity;

@interface SigninSyncMediator : NSObject

// The designated initializer.
- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
            accountManagerService:
                (ChromeAccountManagerService*)accountManagerService
                   consentAuditor:
                       (consent_auditor::ConsentAuditor*)consentAuditor
                 syncSetupService:(SyncSetupService*)syncSetupService
            unifiedConsentService:
                (unified_consent::UnifiedConsentService*)unifiedConsentService
                      syncService:(syncer::SyncService*)syncService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer for this mediator.
@property(nonatomic, weak) id<SigninSyncConsumer> consumer;

// The identity currently selected.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;

// Whether an account has been added. Must be set externally.
@property(nonatomic, assign) BOOL addedAccount;

// Delegate.
@property(nonatomic, weak) id<SigninSyncMediatorDelegate> delegate;

// Disconnect the mediator.
- (void)disconnect;

// Reverts the sign-in and sync operation if needed.
// @param signinStateOnStart: Browser sign-in state when the coordinator starts.
// @param signinIdentityOnStart: Sign-in identity when the coordinator starts.
- (void)cancelSyncAndRestoreSigninState:(IdentitySigninState)signinStateOnStart
                  signinIdentityOnStart:
                      (id<SystemIdentity>)signinIdentityOnStart;

// Starts the sync engine.
// @param confirmationID: The confirmation string ID of sync.
// @param consentIDs: The consent string IDs of sync screen.
// @param authenticationFlow: the object used to manage the authentication flow.
// @param advancedSyncSettingsLinkWasTapped: YES if the link to show the
//   advance settings was used to start the sync.
- (void)startSyncWithConfirmationID:(const int)confirmationID
                         consentIDs:(NSArray<NSNumber*>*)consentIDs
                 authenticationFlow:(AuthenticationFlow*)authenticationFlow;

// Prepare for advanced settings before showing them.
// @param authenticationFlow: the object used to manage the authentication flow.
- (void)prepareAdvancedSettingsWithAuthenticationFlow:
    (AuthenticationFlow*)authenticationFlow;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SYNC_SIGNIN_SYNC_MEDIATOR_H_
