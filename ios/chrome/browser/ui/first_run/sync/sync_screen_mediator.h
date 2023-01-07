// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class AuthenticationFlow;
class AuthenticationService;
class ChromeAccountManagerService;
@protocol SyncScreenConsumer;
@protocol SyncScreenMediatorDelegate;
class SyncSetupService;

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

// Mediator that handles the sync operation.
@interface SyncScreenMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Inits the mediator with
// `authenticationService` provides the authentication library.
// `identityManager` gives access to information of users Google identity.
// `consentAuditor` to record the content.
// `syncSetupService` helps triggering the sync flow.
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

// Disconnect the mediator.
- (void)disconnect;

// Delegate.
@property(nonatomic, weak) id<SyncScreenMediatorDelegate> delegate;

// Consumer for this mediator.
@property(nonatomic, weak) id<SyncScreenConsumer> consumer;

// Starts the sync engine.
// @param confirmationID: The confirmation string ID of sync.
// @param consentIDs: The consent string IDs of sync screen.
// @param authenticationFlow: the object used to manage the authentication flow.
// @param advancedSyncSettingsLinkWasTapped: whether the link to show the
// advance settings was used to start the sync.
- (void)startSyncWithConfirmationID:(const int)confirmationID
                           consentIDs:(NSArray<NSNumber*>*)consentIDs
                   authenticationFlow:(AuthenticationFlow*)authenticationFlow
    advancedSyncSettingsLinkWasTapped:(BOOL)advancedSyncSettingsLinkWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_SYNC_SYNC_SCREEN_MEDIATOR_H_
