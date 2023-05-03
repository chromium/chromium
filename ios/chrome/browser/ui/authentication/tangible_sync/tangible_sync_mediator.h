// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_MEDIATOR_H_

#import <UIKit/UIKit.h>

@class AuthenticationFlow;
class AuthenticationService;
class ChromeAccountManagerService;
class SyncSetupService;
@protocol TangibleSyncConsumer;
@protocol TangibleSyncMediatorDelegate;

namespace consent_auditor {
class ConsentAuditor;
}  // namespace consent_auditor

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

namespace unified_consent {
class UnifiedConsentService;
}  // namespace unified_consent

// Mediator that handles the sync operations.
@interface TangibleSyncMediator : NSObject

// Consumer for this mediator.
@property(nonatomic, weak) id<TangibleSyncConsumer> consumer;

// Delegate.
@property(nonatomic, weak) id<TangibleSyncMediatorDelegate> delegate;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
      chromeAccountManagerService:
          (ChromeAccountManagerService*)chromeAccountManagerService
                   consentAuditor:
                       (consent_auditor::ConsentAuditor*)consentAuditor
                  identityManager:(signin::IdentityManager*)identityManager
                      syncService:(syncer::SyncService*)syncService
                 syncSetupService:(SyncSetupService*)syncSetupService
            unifiedConsentService:
                (unified_consent::UnifiedConsentService*)unifiedConsentService
    NS_DESIGNATED_INITIALIZER;

// Disconnect the mediator.
- (void)disconnect;

// Starts the sync engine.
// `confirmationID` The confirmation string ID of sync.
// `consentIDs` The consent string IDs of sync screen.
// `authenticationFlow` the object used to manage the authentication flow.
// `advancedSyncSettingsLinkWasTapped` whether the link to show the
// advance settings was used to start the sync.
- (void)startSyncWithConfirmationID:(const int)confirmationID
                           consentIDs:(NSArray<NSNumber*>*)consentIDs
                   authenticationFlow:(AuthenticationFlow*)authenticationFlow
    advancedSyncSettingsLinkWasTapped:(BOOL)advancedSyncSettingsLinkWasTapped;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_TANGIBLE_SYNC_TANGIBLE_SYNC_MEDIATOR_H_
