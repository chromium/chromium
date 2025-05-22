// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile_performer.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import <memory>
#import <optional>

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/memory/weak_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "google_apis/gaia/gaia_id.h"
#import "google_apis/gaia/gaia_urls.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile_performer_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_capabilities_fetcher.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

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
  __weak __typeof(_delegate) weakDelegate = _delegate;
  AuthenticationServiceFactory::GetForProfile(profile)->SignOut(
      signin_metrics::ProfileSignout::kSignoutForAccountSwitching, ^{
        [weakDelegate didSignOutForAccountSwitch];
      });
}

- (void)signOutImmediatelyFromProfile:(ProfileIOS*)profile {
  AuthenticationServiceFactory::GetForProfile(profile)->SignOut(
      signin_metrics::ProfileSignout::kAbortSignin, nil);
}

- (void)registerUserPolicy:(ProfileIOS*)profile
               forIdentity:(id<SystemIdentity>)identity {
  std::string userEmail = base::SysNSStringToUTF8(identity.userEmail);
  CoreAccountId accountID =
      IdentityManagerFactory::GetForProfile(profile)->PickAccountIdForAccount(
          GaiaId(identity.gaiaID), userEmail);

  policy::UserPolicySigninService* userPolicyService =
      policy::UserPolicySigninServiceFactory::GetForProfile(profile);

  [self startWatchdogTimerForUserPolicyRegistration];

  __weak __typeof(self) weakSelf = self;
  userPolicyService->RegisterForPolicyWithAccountId(
      userEmail, accountID,
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

// Wraps -didFetchUserPolicyWithSuccess: method from the delegate with a
// check that the watchdog has not expired.
- (void)didFetchUserPolicyWithSuccess:(BOOL)success {
  // If the watchdog timer has already fired, don't notify the delegate.
  if (![self stopWatchdogTimer]) {
    return;
  }

  [_delegate didFetchUserPolicyWithSuccess:success];
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
      gaia::CanonicalizeEmail(userEmail), GaiaId(identity.gaiaID));

  std::vector<std::string> userAffiliationIDsVector;
  for (NSString* userAffiliationID in userAffiliationIDs) {
    userAffiliationIDsVector.push_back(
        base::SysNSStringToUTF8(userAffiliationID));
  }

  [self startWatchdogTimerForUserPolicyFetch];

  __weak __typeof(self) weakSelf = self;
  policyService->FetchPolicyForSignedInUser(
      accountID, base::SysNSStringToUTF8(dmToken),
      base::SysNSStringToUTF8(clientID), userAffiliationIDsVector,
      profile->GetSharedURLLoaderFactory(), base::BindOnce(^(bool success) {
        [weakSelf didFetchUserPolicyWithSuccess:success];
      }));
}

#pragma mark - Private

- (void)didFetchAccountCapabilities {
  [_delegate didFetchAccountCapabilities];
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

// Starts a Watchdog Timer that ends the user policy fetch on time out.
- (void)startWatchdogTimerForUserPolicyFetch {
  __weak __typeof(self) weakSelf = self;
  [self startWatchdogTimerWithTimeoutBlock:^{
    [weakSelf onUserPolicyFetchWatchdogTimerExpired];
  }];
}

// Handle the expiration of the watchdog time for the method
// -startWatchdogTimerForUserPolicyFetch.
- (void)onUserPolicyFetchWatchdogTimerExpired {
  [self stopWatchdogTimer];
  [_delegate didFetchUserPolicyWithSuccess:NO];
}

@end
