// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile_performer.h"

#import <memory>
#import <optional>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/browser/authentication/history_sync/model/history_sync_capabilities_fetcher.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile_performer_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base+protected.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

@implementation AuthenticationFlowInProfilePerformer {
  __weak id<AuthenticationFlowInProfilePerformerDelegate> _delegate;
  // Capabilities fetcher for the subsequent History Sync Opt-In screen.
  HistorySyncCapabilitiesFetcher* _capabilitiesFetcher;
}

- (instancetype)initWithInProfileDelegate:
                    (id<AuthenticationFlowInProfilePerformerDelegate>)delegate
                     changeProfileHandler:
                         (id<ChangeProfileCommands>)changeProfileHandler {
  self = [super initWithDelegate:delegate
            changeProfileHandler:changeProfileHandler];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (void)signInIdentity:(id<SystemIdentity>)identity
         atAccessPoint:(signin_metrics::AccessPoint)accessPoint
        currentProfile:(ProfileIOS*)currentProfile {
  AuthenticationServiceFactory::GetForProfile(currentProfile)
      ->SignIn(identity, accessPoint);
}

- (void)signOutForAccountSwitchWithProfile:(ProfileIOS*)profile {
  AuthenticationServiceFactory::GetForProfile(profile)->SignOut(
      signin_metrics::ProfileSignout::kSignoutForAccountSwitching);
}

- (void)registerUserPolicy:(ProfileIOS*)profile
               forIdentity:(id<SystemIdentity>)identity {
  std::string userEmail = base::SysNSStringToUTF8(identity.userEmail);
  const CoreAccountId accountID = CoreAccountId::FromGaiaId(identity.gaiaId);

  policy::UserPolicySigninService* userPolicyService =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile);

  [self startWatchdogTimerForUserPolicyRegistration];

  __weak __typeof(self) weakSelf = self;
  userPolicyService->RegisterForPolicyWithAccountId(
      userEmail, accountID,
      /*is_registration_for_management_consistency_check=*/false,
      base::BindOnce(^(const std::string& dmToken, const std::string& clientID,
                       const std::vector<std::string>& userAffiliationIDs) {
        [weakSelf didRegisterForUserPolicyWithDMToken:dmToken
                                             clientID:clientID
                                   userAffiliationIDs:userAffiliationIDs];
      }));
}

// Wraps -didRegisterForUserPolicyWithDMToken:clientID:userAffiliationIDs:
// method from the delegate with a check that the watchdog has not expired
// and conversion of the parameter to Objective-C types.
- (void)didRegisterForUserPolicyWithDMToken:(const std::string&)dmToken
                                   clientID:(const std::string&)clientID
                         userAffiliationIDs:(const std::vector<std::string>&)
                                                userAffiliationIDs {
  // If the watchdog timer has already fired, don't notify the delegate.
  if (![self stopWatchdogTimer]) {
    return;
  }

  NSMutableArray<NSString*>* affiliationIDs = [[NSMutableArray alloc] init];
  for (const auto& userAffiliationID : userAffiliationIDs) {
    [affiliationIDs addObject:base::SysUTF8ToNSString(userAffiliationID)];
  }

  [_delegate
      didRegisterForUserPolicyWithDMToken:base::SysUTF8ToNSString(dmToken)
                                 clientID:base::SysUTF8ToNSString(clientID)
                       userAffiliationIDs:affiliationIDs];
}

- (void)fetchAccountCapabilities:(ProfileIOS*)profile {
  // Create the capability fetcher and start fetching capabilities.
  _capabilitiesFetcher = [[HistorySyncCapabilitiesFetcher alloc]
      initWithIdentityManager:IdentityManagerFactory::GetForProfile(profile)];

  __weak __typeof(self) weakSelf = self;
  [_capabilitiesFetcher
      startFetchingRestrictionCapabilityWithCallback:base::BindOnce(^(
                                                         signin::Tribool
                                                             capability) {
        // The capability value is ignored.
        [weakSelf didFetchAccountCapabilities];
      })];
}
- (void)fetchUserPolicy:(ProfileIOS*)profile
            withDmToken:(NSString*)dmToken
               clientID:(NSString*)clientID
     userAffiliationIDs:(NSArray<NSString*>*)userAffiliationIDs
               identity:(id<SystemIdentity>)identity {
  // Need a `dmToken` and a `clientID` to fetch user policies.
  DCHECK([dmToken length] > 0);
  DCHECK([clientID length] > 0);

  policy::UserPolicySigninService* policyService =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile);
  const std::string userEmail = base::SysNSStringToUTF8(identity.userEmail);

  AccountId accountID = AccountId::FromUserEmailGaiaId(
      gaia::CanonicalizeEmail(userEmail), identity.gaiaId);

  std::vector<std::string> userAffiliationIDsVector;
  for (NSString* userAffiliationID in userAffiliationIDs) {
    userAffiliationIDsVector.push_back(
        base::SysNSStringToUTF8(userAffiliationID));
  }

  policyService->FetchPolicyForSignedInUser(
      accountID, base::SysNSStringToUTF8(dmToken),
      base::SysNSStringToUTF8(clientID), userAffiliationIDsVector,
      profile->GetSharedURLLoaderFactory(), base::BindOnce(^(bool success) {
        DLOG_IF(ERROR, !success) << "Error fetching policy for user";
      }));
}

#pragma mark - Private

- (void)didFetchAccountCapabilities {
  [_delegate didFetchAccountCapabilities];
  _capabilitiesFetcher = nil;
}

// Starts a Watchdog Timer that ends the user policy registration on time out.
- (void)startWatchdogTimerForUserPolicyRegistration {
  __weak __typeof(self) weakSelf = self;
  [self startWatchdogTimerWithTimeoutBlock:^{
    [weakSelf onUserPolicyRegistrationWatchdogTimerExpired];
  }];
}

// Handle the expiration of the watchdog time for the method
// -startWatchdogTimerForUserPolicyRegistration.
- (void)onUserPolicyRegistrationWatchdogTimerExpired {
  [self stopWatchdogTimer];
  [_delegate didRegisterForUserPolicyWithDMToken:@""
                                        clientID:@""
                              userAffiliationIDs:@[]];
}

@end
