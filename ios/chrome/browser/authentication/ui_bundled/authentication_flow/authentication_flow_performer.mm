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
#import "ios/chrome/browser/authentication/history_sync/model/history_sync_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_base+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_coordinator.h"
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
    ManagedProfileCreationCoordinatorDelegate>
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
  [_managedConfirmationScreenCoordinator stop];
  _managedConfirmationScreenCoordinator = nil;
  [_managedConfirmationAlertCoordinator stop];
  _managedConfirmationAlertCoordinator = nil;
  _delegate = nil;
  [self stopWatchdogTimer];
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

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  _accountLevelSigninRestrictionPolicyFetcher
      ->GetManagedAccountsSigninRestriction(
          identity_manager,
          identity_manager->PickAccountIdForAccount(
              identity.gaiaId, base::SysNSStringToUTF8(identity.userEmail)),
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
                     skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration
                    mergeBrowsingDataByDefault:(BOOL)mergeBrowsingDataByDefault
         browsingDataMigrationDisabledByPolicy:
             (BOOL)browsingDataMigrationDisabledByPolicy {
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
                        skipBrowsingDataMigration:skipBrowsingDataMigration
                       mergeBrowsingDataByDefault:mergeBrowsingDataByDefault
            browsingDataMigrationDisabledByPolicy:
                browsingDataMigrationDisabledByPolicy
                       multiProfileForceMigration:NO];
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

#pragma mark - AuthenticationFlowPerformerBase

- (void)checkNoDialog {
  [super checkNoDialog];
  CHECK(!_managedConfirmationScreenCoordinator);
  CHECK(!_managedConfirmationAlertCoordinator);
}

#pragma mark - Private

- (void)updateUserPolicyNotificationStatusIfNeeded:(PrefService*)prefService {
  prefService->SetBoolean(policy::policy_prefs::kUserPolicyNotificationWasShown,
                          true);
}

// Called when `_managedConfirmationAlertCoordinator` is finished.
// `accepted` is YES when the user confirmed or NO if the user canceled.
- (void)managedConfirmationAlertAccepted:(BOOL)accepted {
  CHECK(_managedConfirmationAlertCoordinator);
  CHECK(!_managedConfirmationScreenCoordinator);
  Browser* browser = _managedConfirmationAlertCoordinator.browser;
  [_managedConfirmationAlertCoordinator stop];
  _managedConfirmationAlertCoordinator = nil;
  [self managedConfirmationDidAccept:accepted
                             browser:browser
                browsingDataSeparate:
                    AreSeparateProfilesForManagedAccountsEnabled()];
}

// Called when `_leavingPrimaryAccountConfirmationDialogCoordinator` is done.
- (void)leavingPrimaryAccountConfirmationDone:(BOOL)continueFlow {
  [_leavingPrimaryAccountConfirmationDialogCoordinator stop];
  _leavingPrimaryAccountConfirmationDialogCoordinator = nil;
  [_delegate didAcceptToLeavePrimaryAccount:continueFlow];
}

// Called when the user accepted to continue to sign-in with a managed account.
// `accepted` is YES when the user confirmed or NO if the user canceled.
// If `browsingDataSeparate` is `YES`, the managed account gets signed in to
// a new empty work profile. This must only be specified if
// AreSeparateProfilesForManagedAccountsEnabled() is true.
// If `browsingDataSeparate` is `NO`, the account gets signed in to the
// current profile. If AreSeparateProfilesForManagedAccountsEnabled() is true,
// this involves converting the current profile into a work profile.
- (void)managedConfirmationDidAccept:(BOOL)accepted
                             browser:(Browser*)browser
                browsingDataSeparate:(BOOL)browsingDataSeparate {
  if (!accepted) {
    base::RecordAction(
        base::UserMetricsAction("Signin_AuthenticationFlowPerformer_"
                                "ManagedConfirmationDialog_Canceled"));
    [_delegate didCancelManagedConfirmation];
    return;
  }
  CHECK(AreSeparateProfilesForManagedAccountsEnabled() ||
        !browsingDataSeparate);
  base::RecordAction(
      base::UserMetricsAction("Signin_AuthenticationFlowPerformer_"
                              "ManagedConfirmationDialog_Confirmed"));
  // TODO(crbug.com/40225944): Nullify the browser object in the
  // AlertCoordinator when the coordinator is stopped to avoid using the
  // browser object at that moment, in which case the browser object may have
  // been deleted before the callback block is called. This is to avoid
  // potential bad memory accesses.
  if (browser) {
    PrefService* prefService = browser->GetProfile()->GetPrefs();
    // TODO(crbug.com/40225352): Remove this line once we determined that the
    // notification isn't needed anymore.
    [self updateUserPolicyNotificationStatusIfNeeded:prefService];
  }
  [_delegate didAcceptManagedConfirmationWithBrowsingDataSeparate:
                 browsingDataSeparate];
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
                                didAccept:(BOOL)accepted
                     browsingDataSeparate:(BOOL)browsingDataSeparate {
  CHECK(!_managedConfirmationAlertCoordinator);
  CHECK_EQ(_managedConfirmationScreenCoordinator, coordinator);
  Browser* browser = _managedConfirmationScreenCoordinator.browser;
  [_managedConfirmationScreenCoordinator stop];
  _managedConfirmationScreenCoordinator = nil;
  [self managedConfirmationDidAccept:accepted
                             browser:browser
                browsingDataSeparate:browsingDataSeparate];
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
