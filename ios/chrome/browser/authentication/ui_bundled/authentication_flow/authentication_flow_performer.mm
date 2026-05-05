// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer.h"

#import <memory>
#import <optional>

#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "components/policy/core/browser/signin/user_cloud_signin_restriction_policy_fetcher.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "google_apis/gaia/gaia_auth_util.h"
#import "google_apis/gaia/gaia_id.h"
#import "google_apis/gaia/gaia_urls.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/age_mismatch_signout_coordinator.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/ui/age_mismatch_prompt_mode.h"
#import "ios/chrome/browser/authentication/enterprise/managed_profile_creation/coordinator/managed_profile_creation_coordinator.h"
#import "ios/chrome/browser/authentication/enterprise/public/managed_profile_creation_constants.h"
#import "ios/chrome/browser/authentication/history_sync/model/history_sync_utils.h"
#import "ios/chrome/browser/authentication/signin/reauth/coordinator/signin_reauth_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/age_mismatch_capabilities_fetcher.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_signin_service_factory.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The results of a view informing the user of the creation of a managed
// profile.
enum class ManagedProfileCreationResult {
  // The user cancelled
  kCancelled,
  // Data must be merged
  kMerge,
  // Data must be kept separate
  kSeparate,
};

bool& FakePolicyResponsesForTesting() {
  static bool instance = false;
  return instance;
}

std::optional<policy::ProfileSeparationDataMigrationSettings>&
ForcedPolicyResponseForNextFetchRequestForTesting() {
  static std::optional<policy::ProfileSeparationDataMigrationSettings> instance;
  return instance;
}

bool ShouldUseFakePolicyResponseForTesting() {
  return FakePolicyResponsesForTesting() ||
         ForcedPolicyResponseForNextFetchRequestForTesting().has_value();
}

policy::ProfileSeparationPolicies GetFakePolicyResponseForTesting() {
  CHECK(ShouldUseFakePolicyResponseForTesting());

  policy::ProfileSeparationPolicies response;

  // If a forced response is set for the next request, use (and reset) that.
  std::optional<policy::ProfileSeparationDataMigrationSettings>&
      forced_response = ForcedPolicyResponseForNextFetchRequestForTesting();
  if (forced_response.has_value()) {
    // Note: The ProfileSeparationSettings value doesn't matter here (only the
    // ProfileSeparationDataMigrationSettings value does).
    response = policy::ProfileSeparationPolicies(
        policy::ProfileSeparationSettings::SUGGESTED, forced_response.value());
    forced_response = std::nullopt;
  }
  // Otherwise: Just return the empty default value.
  return response;
}

}  // namespace

@interface AuthenticationFlowPerformer () <
    AgeMismatchSignoutCoordinatorDelegate,
    ManagedProfileCreationCoordinatorDelegate,
    SigninReauthCoordinatorDelegate>
@end

@implementation AuthenticationFlowPerformer {
  __weak id<AuthenticationFlowPerformerDelegate> _delegate;
  // Dialog for the managed confirmation dialog.
  ManagedProfileCreationCoordinator* _managedConfirmationScreenCoordinator;
  // Dialog for the managed confirmation dialog.
  AlertCoordinator* _managedConfirmationAlertCoordinator;
  // Used to fetch the signin restriction policies for a single
  // account without needing to register it for all policies.
  // This needs to be a member of the class because it works asynchronously and
  // should have its lifecycle tied to this class and not the function that
  // calls it.
  std::unique_ptr<policy::UserCloudSigninRestrictionPolicyFetcher>
      _accountLevelSigninRestrictionPolicyFetcher;
  ActionSheetCoordinator* _leavingPrimaryAccountConfirmationDialogCoordinator;
  AgeMismatchSignoutCoordinator* _ageMismatchSignoutCoordinator;
  SigninReauthCoordinator* _reauthCoordinator;
  AgeMismatchCapabilitiesFetcher* _canSignInToChromeCapabilitiesFetcher;
}

- (instancetype)
        initWithDelegate:(id<AuthenticationFlowPerformerDelegate>)delegate
    changeProfileHandler:(id<ChangeProfileCommands>)changeProfileHandler {
  self = [super initWithDelegate:delegate
            changeProfileHandler:changeProfileHandler];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (void)interrupt {
  [self stopAgeMismatchSignoutCoordinator];
  [self stopManagedConfirmation];
  [_managedConfirmationAlertCoordinator stop];
  _managedConfirmationAlertCoordinator = nil;
  _delegate = nil;
  [self stopWatchdogTimer];
  [self stopReauthCoordinator];
  [_canSignInToChromeCapabilitiesFetcher shutdown];
  _canSignInToChromeCapabilitiesFetcher = nil;
}

- (void)reauthIdentity:(id<SystemIdentity>)identity
               browser:(Browser*)browser
        viewController:(UIViewController*)viewController
           accessPoint:(signin_metrics::AccessPoint)accessPoint {
  CHECK(!_reauthCoordinator);

  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(
          browser->GetProfile()->GetOriginalProfile());
  CoreAccountInfo accountInfo =
      identityManager->FindExtendedAccountInfoByGaiaId(identity.gaiaId);
  if (accountInfo.IsEmpty()) {
    accountInfo.gaia = identity.gaiaId;
    accountInfo.email = base::SysNSStringToUTF8(identity.userEmail);
  }

  _reauthCoordinator =
      [[SigninReauthCoordinator alloc] initWithBaseViewController:viewController
                                                          browser:browser
                                                          account:accountInfo
                                                signinAccessPoint:accessPoint];
  _reauthCoordinator.delegate = self;
  [_reauthCoordinator start];
}

- (void)fetchUnsyncedDataWithSyncService:(syncer::SyncService*)syncService {
  auto callback = base::BindOnce(
      [](__typeof(_delegate) delegate, syncer::DataTypeSet set) {
        [delegate didFetchUnsyncedDataWithUnsyncedDataTypes:set];
      },
      _delegate);
  signin::FetchUnsyncedDataForSignOutOrProfileSwitching(syncService,
                                                        std::move(callback));
}

- (void)
    showLeavingPrimaryAccountConfirmationWithBaseViewController:
        (UIViewController*)baseViewController
                                                        browser:
                                                            (Browser*)browser
                                              signedInUserState:
                                                  (SignedInUserState)
                                                      signedInUserState
                                                     anchorView:
                                                         (UIView*)anchorView
                                                     anchorRect:
                                                         (CGRect)anchorRect {
  // Sign-in related work should be done on regular browser.
  CHECK_EQ(browser->type(), Browser::Type::kRegular, base::NotFatalUntil::M145);
  __weak __typeof(self) weakSelf = self;
  _leavingPrimaryAccountConfirmationDialogCoordinator =
      GetLeavingPrimaryAccountConfirmationDialog(
          baseViewController, browser, anchorView, anchorRect,
          signedInUserState, YES, ^(BOOL continueFlow) {
            [weakSelf leavingPrimaryAccountConfirmationDone:continueFlow];
          });
  [_leavingPrimaryAccountConfirmationDialogCoordinator start];
}

- (void)fetchManagedStatus:(ProfileIOS*)profile
               forIdentity:(id<SystemIdentity>)identity {
  CHECK(identity, base::NotFatalUntil::M147);
  SystemIdentityManager* systemIdentityManager =
      GetApplicationContext()->GetSystemIdentityManager();
  if (NSString* hostedDomain =
          systemIdentityManager->GetCachedHostedDomainForIdentity(identity)) {
    [_delegate didFetchManagedStatus:hostedDomain];
    return;
  }

  [self startWatchdogTimerForManagedStatus];
  __weak __typeof(self) weakSelf = self;
  systemIdentityManager->GetHostedDomain(
      identity, base::BindOnce(^(NSString* hostedDomain, NSError* error) {
        [weakSelf handleGetHostedDomain:hostedDomain error:error];
      }));
}

- (void)fetchCanSignInToChromeCapability:(id<SystemIdentity>)identity
                                 profile:(ProfileIOS*)profile {
  CHECK(!_canSignInToChromeCapabilitiesFetcher);
  _canSignInToChromeCapabilitiesFetcher =
      [[AgeMismatchCapabilitiesFetcher alloc]
          initWithIdentityManager:IdentityManagerFactory::GetForProfile(
                                      profile)];

  __weak __typeof(self) weakSelf = self;
  CoreAccountId accountId = CoreAccountId::FromGaiaId([identity gaiaId]);
  [_canSignInToChromeCapabilitiesFetcher
      startFetchingCanSignInToChromeCapabilityWithCallback:
          base::BindOnce(
              [](__typeof(self) strong_self, signin::Tribool result) {
                [strong_self didFetchCanSignInToChromeCapability:result];
              },
              weakSelf)
                                                forAccount:accountId];
}

- (void)fetchProfileSeparationPolicies:(ProfileIOS*)profile
                           forIdentity:(id<SystemIdentity>)identity {
  CHECK(!_accountLevelSigninRestrictionPolicyFetcher);
  _accountLevelSigninRestrictionPolicyFetcher =
      std::make_unique<policy::UserCloudSigninRestrictionPolicyFetcher>(
          GetApplicationContext()->GetBrowserPolicyConnector(),
          GetApplicationContext()->GetSharedURLLoaderFactory());

  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(policy::ProfileSeparationPolicies)> callback =
      base::BindOnce(
          [](__typeof(self) strongSelf,
             policy::ProfileSeparationPolicies policies) {
            [strongSelf didFetchProfileSeparationPolicies:std::move(policies)];
          },
          weakSelf);

  if (ShouldUseFakePolicyResponseForTesting()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), GetFakePolicyResponseForTesting()));
    return;
  }

  auto* identityManager = IdentityManagerFactory::GetForProfile(profile);
  const CoreAccountId accountID = CoreAccountId::FromGaiaId(identity.gaiaId);
  _accountLevelSigninRestrictionPolicyFetcher
      ->GetManagedAccountsSigninRestriction(identityManager, accountID,
                                            std::move(callback));
}

- (void)switchToProfileWithIdentity:(id<SystemIdentity>)identity
                         sceneState:(SceneState*)sceneState
                             reason:(ChangeProfileReason)reason
                           delegate:(id<AuthenticationFlowDelegate>)delegate
                  postSignInActions:(PostSignInActionSet)postSignInActions
                        accessPoint:(signin_metrics::AccessPoint)accessPoint {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  std::optional<std::string> profileName =
      GetApplicationContext()
          ->GetAccountProfileMapper()
          ->FindProfileNameForGaiaID(identity.gaiaId);
  CHECK(profileName.has_value(), base::NotFatalUntil::M150);

  __weak __typeof(self) weakSelf = self;
  auto profileSwitchReadyCompletion = base::BindOnce(
      [](__typeof(self) strongSelf, std::string profile_name,
         SceneState* scene_state, ChangeProfileReason reason,
         PostSignInActionSet post_sign_in_actions, id<SystemIdentity> identity,
         signin_metrics::AccessPoint access_point,
         ChangeProfileContinuation continuation) {
        [strongSelf switchToProfileWithName:profile_name
                                 sceneState:scene_state
                                     reason:reason
                  changeProfileContinuation:std::move(continuation)
                          postSignInActions:post_sign_in_actions
                               withIdentity:identity
                                accessPoint:access_point];
      },
      weakSelf, *profileName, sceneState, reason, postSignInActions, identity,
      accessPoint);
  [delegate authenticationFlowWillSwitchProfileWithReadyCompletion:
                std::move(profileSwitchReadyCompletion)];
}

- (void)makePersonalProfileManagedWithIdentity:(id<SystemIdentity>)identity {
  GetApplicationContext()
      ->GetAccountProfileMapper()
      ->MakePersonalProfileManagedWithGaiaID(identity.gaiaId);
  [_delegate didMakePersonalProfileManaged];
}

- (void)showManagedConfirmationForHostedDomain:(NSString*)hostedDomain
                                      identity:(id<SystemIdentity>)identity
                                viewController:(UIViewController*)viewController
                                       browser:(Browser*)browser
                    managedProfileCreationMode:
                        (signin::ManagedAccountSigninMode)mode {
  // Sign-in related work should be done on regular browser.
  CHECK_EQ(browser->type(), Browser::Type::kRegular, base::NotFatalUntil::M145);
  [self checkNoDialog];

  base::RecordAction(
      base::UserMetricsAction("Signin_AuthenticationFlowPerformer_"
                              "ManagedConfirmationDialog_Presented"));

  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    _managedConfirmationScreenCoordinator =
        [[ManagedProfileCreationCoordinator alloc]
            initWithBaseViewController:viewController
                              identity:identity
                          hostedDomain:hostedDomain
                               browser:browser
                                  mode:mode];
    _managedConfirmationScreenCoordinator.delegate = self;
    [_managedConfirmationScreenCoordinator start];
    return;
  }
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock acceptBlock = ^{
    [weakSelf managedConfirmationAlertAccepted:YES];
  };
  ProceduralBlock cancelBlock = ^{
    [weakSelf managedConfirmationAlertAccepted:NO];
  };
  _managedConfirmationAlertCoordinator =
      ManagedConfirmationDialogContentForHostedDomain(
          hostedDomain, browser, viewController, acceptBlock, cancelBlock);
}

- (void)showAgeMismatchDialogForIdentity:(id<SystemIdentity>)identity
                          viewController:(UIViewController*)viewController
                                 browser:(Browser*)browser {
  [self checkNoDialog];
  _ageMismatchSignoutCoordinator = [[AgeMismatchSignoutCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                        identity:identity
                            mode:AgeMismatchPromptMode::kSigninFlow];
  _ageMismatchSignoutCoordinator.delegate = self;
  [_ageMismatchSignoutCoordinator start];
}

#pragma mark - SigninReauthCoordinatorDelegate

- (void)reauthFinishedWithResult:(ReauthResult)result
                          gaiaID:(const GaiaId*)gaiaID {
  [self stopReauthCoordinator];

  BOOL success = (result == ReauthResult::kSuccess);
  [_delegate didCompleteReauthWithSuccess:success];
}

#pragma mark - AuthenticationFlowPerformerBase

- (void)checkNoDialog {
  [super checkNoDialog];
  CHECK(!_managedConfirmationScreenCoordinator);
  CHECK(!_managedConfirmationAlertCoordinator);
  CHECK(!_ageMismatchSignoutCoordinator);
}

#pragma mark - Private

- (void)didFetchCanSignInToChromeCapability:(signin::Tribool)capability {
  [_delegate authenticationFlowPerformer:self
      didFetchCanSignInToChromeCapability:capability];
  [_canSignInToChromeCapabilitiesFetcher shutdown];
  _canSignInToChromeCapabilitiesFetcher = nil;
}

- (void)stopReauthCoordinator {
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

- (void)stopManagedConfirmation {
  [_managedConfirmationScreenCoordinator stop];
  _managedConfirmationScreenCoordinator.delegate = nil;
  _managedConfirmationScreenCoordinator = nil;
}

// Called when `_managedConfirmationAlertCoordinator` is finished.
// `accepted` is YES when the user confirmed or NO if the user canceled.
- (void)managedConfirmationAlertAccepted:(BOOL)accepted {
  CHECK(_managedConfirmationAlertCoordinator);
  CHECK(!_managedConfirmationScreenCoordinator);
  CHECK(!AreSeparateProfilesForManagedAccountsEnabled());
  [_managedConfirmationAlertCoordinator stop];
  _managedConfirmationAlertCoordinator = nil;
  ManagedProfileCreationResult result;
  if (accepted) {
    if (AreSeparateProfilesForManagedAccountsEnabled()) {
      result = ManagedProfileCreationResult::kSeparate;
    } else {
      result = ManagedProfileCreationResult::kMerge;
    }
  } else {
    result = ManagedProfileCreationResult::kCancelled;
  }
  [self managedConfirmationDoneWithResult:result];
}

// Called when `_leavingPrimaryAccountConfirmationDialogCoordinator` is done.
- (void)leavingPrimaryAccountConfirmationDone:(BOOL)continueFlow {
  [_leavingPrimaryAccountConfirmationDialogCoordinator stop];
  _leavingPrimaryAccountConfirmationDialogCoordinator = nil;
  [_delegate didAcceptToLeavePrimaryAccount:continueFlow];
}

// Called when the user closed the "Managed profile creation" view.
- (void)managedConfirmationDoneWithResult:(ManagedProfileCreationResult)result {
  BOOL separate;
  switch (result) {
    case ManagedProfileCreationResult::kCancelled:
      base::RecordAction(
          base::UserMetricsAction("Signin_AuthenticationFlowPerformer_"
                                  "ManagedConfirmationDialog_Canceled"));
      [_delegate didCancelManagedConfirmation];
      return;
    case ManagedProfileCreationResult::kMerge:
      separate = NO;
      break;
    case ManagedProfileCreationResult::kSeparate:
      separate = YES;
      CHECK(AreSeparateProfilesForManagedAccountsEnabled());
      break;
  }
  base::RecordAction(
      base::UserMetricsAction("Signin_AuthenticationFlowPerformer_"
                              "ManagedConfirmationDialog_Confirmed"));
  [_delegate didAcceptManagedConfirmationWithBrowsingDataSeparate:separate];
}

// Called when separation policies have been fetched, and calls the delegate.
- (void)didFetchProfileSeparationPolicies:
    (policy::ProfileSeparationPolicies)policies {
  CHECK(_accountLevelSigninRestrictionPolicyFetcher);
  _accountLevelSigninRestrictionPolicyFetcher.reset();
  auto profile_separation_data_migration_settings =
      policy::ProfileSeparationDataMigrationSettings::USER_OPT_IN;
  if (policies.profile_separation_data_migration_settings()) {
    profile_separation_data_migration_settings =
        static_cast<policy::ProfileSeparationDataMigrationSettings>(
            *policies.profile_separation_data_migration_settings());
  }
  [_delegate didFetchProfileSeparationPolicies:
                 profile_separation_data_migration_settings];
}

- (void)handleGetHostedDomain:(NSString*)hostedDomain error:(NSError*)error {
  if (![self stopWatchdogTimer]) {
    // Watchdog timer has already fired, don't notify the delegate.
    return;
  }
  if (error) {
    [_delegate didFailFetchManagedStatus:error];
    return;
  }
  [_delegate didFetchManagedStatus:hostedDomain];
}

// Starts the watchdog timer with a timeout of
// `kAuthenticationFlowTimeoutSeconds` for the fetching managed status
// operation. It will notify `_delegate` of the failure unless
// `stopWatchdogTimer` is called before it times out.
- (void)startWatchdogTimerForManagedStatus {
  __weak __typeof(self) weakSelf = self;
  [self startWatchdogTimerWithTimeoutBlock:^{
    [weakSelf onManagedStatusWatchdogTimerExpired];
  }];
}

// Handle the expiration of the watchdog timer for the method
// -startWatchdogTimerForManagedStatus.
- (void)onManagedStatusWatchdogTimerExpired {
  NSError* error = [NSError errorWithDomain:kAuthenticationErrorDomain
                                       code:TIMED_OUT_FETCH_POLICY
                                   userInfo:nil];

  [self stopWatchdogTimer];
  [_delegate didFailFetchManagedStatus:error];
}

#pragma mark - ManagedProfileCreationCoordinatorDelegate

- (void)managedProfileCreationCoordinator:
            (ManagedProfileCreationCoordinator*)coordinator
                                   result:(std::optional<
                                              signin::ManagedAccountSigninMode>)
                                              mode {
  CHECK(!_managedConfirmationAlertCoordinator);
  CHECK_EQ(_managedConfirmationScreenCoordinator, coordinator);
  [self stopManagedConfirmation];
  ManagedProfileCreationResult result;
  if (!mode) {
    result = ManagedProfileCreationResult::kCancelled;
  } else {
    switch (*mode) {
      case signin::ManagedAccountSigninMode::kMergeProfileData:
      case signin::ManagedAccountSigninMode::kAutoMergeDuringFRE:
        result = ManagedProfileCreationResult::kMerge;
        break;
      case signin::ManagedAccountSigninMode::kForceSeparateProfileDataByPolicy:
      case signin::ManagedAccountSigninMode::kMustSeparateBecauseSignedIn:
      case signin::ManagedAccountSigninMode::kSeparateProfileData:
        result = ManagedProfileCreationResult::kSeparate;
        break;
      case signin::ManagedAccountSigninMode::kInformOfForcedMigration:
        NOTREACHED();
    }
  }
  [self managedConfirmationDoneWithResult:result];
}

- (void)managedProfileCreationCoordinatorWantsToBeStopped:
    (ManagedProfileCreationCoordinator*)coordinator {
  CHECK_EQ(_managedConfirmationScreenCoordinator, coordinator);
  [_delegate managedConfirmationCouldNotProceed];
  [self stopManagedConfirmation];
}

#pragma mark - AgeMismatchSignoutCoordinatorDelegate

// TODO(crbug.com/486124651): The user wants to stay signed out.
// Update the naming.
- (void)ageMismatchSignoutCoordinatorWantsToBeStopped:
    (AgeMismatchSignoutCoordinator*)coordinator {
  CHECK_EQ(coordinator, _ageMismatchSignoutCoordinator);
  [self stopAgeMismatchSignoutCoordinator];
  [_delegate
      didDismissAgeMismatchDialogWithCancelationReason:
          signin_ui::CancelationReason::kAgeMismatchCanceledStaySignedOut];
}

- (void)ageMismatchSignoutCoordinatorWantsToSignIn:
    (AgeMismatchSignoutCoordinator*)coordinator {
  CHECK_EQ(coordinator, _ageMismatchSignoutCoordinator);
  [self stopAgeMismatchSignoutCoordinator];
  [_delegate didDismissAgeMismatchDialogWithCancelationReason:
                 signin_ui::CancelationReason::kAgeMismatchCanceled];
}

- (void)stopAgeMismatchSignoutCoordinator {
  _ageMismatchSignoutCoordinator.delegate = nil;
  [_ageMismatchSignoutCoordinator stop];
  _ageMismatchSignoutCoordinator = nil;
}

@end

@implementation AuthenticationFlowPerformer (ForTesting)

+ (void)setUseFakePolicyResponsesForTesting:(BOOL)useFakeResponses {
  FakePolicyResponsesForTesting() = useFakeResponses;
}

+ (void)forcePolicyResponseForNextRequestForTesting:
    (policy::ProfileSeparationDataMigrationSettings)
        profileSeparationDataMigrationSettings {
  auto& optionalForcedPolicy =
      ForcedPolicyResponseForNextFetchRequestForTesting();
  CHECK(!optionalForcedPolicy.has_value());
  optionalForcedPolicy = profileSeparationDataMigrationSettings;
}

@end
