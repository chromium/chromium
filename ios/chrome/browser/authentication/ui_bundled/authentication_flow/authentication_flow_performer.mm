// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer.h"

#import <MaterialComponents/MaterialSnackbar.h>

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
#import "components/policy/core/browser/signin/user_cloud_signin_restriction_policy_fetcher.h"
#import "components/policy/core/common/policy_pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"
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
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const int64_t kAuthenticationFlowTimeoutSeconds = 10;
NSString* const kAuthenticationSnackbarCategory =
    @"AuthenticationSnackbarCategory";

void AuthenticationFlowContinuation(OnProfileSwitchCompletion completion,
                                    SceneState* scene_state,
                                    base::OnceClosure closure) {
  Browser* new_browser =
      scene_state.browserProviderInterface.currentBrowserProvider.browser;

  std::move(completion).Run(/*success=*/true, new_browser);
  std::move(closure).Run();
}

// Handler for the signout action from a snackbar. Will `clear_selected_type`
// if it is not std::nullopt.
void HandleSignoutForSnackbar(
    base::WeakPtr<Browser> weak_browser,
    std::optional<syncer::UserSelectableType> clear_selected_type) {
  Browser* browser = weak_browser.get();
  if (!browser) {
    return;
  }

  base::RecordAction(
      base::UserMetricsAction("Mobile.Signin.SnackbarUndoTapped"));

  ProfileIOS* profile = browser->GetProfile();
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (!auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return;
  }

  if (clear_selected_type.has_value()) {
    SyncServiceFactory::GetForProfile(profile)
        ->GetUserSettings()
        ->SetSelectedType(clear_selected_type.value(), false);
  }

  signin::ProfileSignoutRequest(
      signin_metrics::ProfileSignout::kUserTappedUndoRightAfterSignIn)
      .Run(browser);
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
  // Dialog to display an error.
  AlertCoordinator* _errorAlertCoordinator;
  // Used to fetch the signin restriction policies for a single
  // account without needing to register it for all policies.
  // This needs to be a member of the class because it works asynchronously and
  // should have its lifecycle tied to this class and not the function that
  // calls it.
  std::unique_ptr<policy::UserCloudSigninRestrictionPolicyFetcher>
      _accountLevelSigninRestrictionPolicyFetcher;
  std::unique_ptr<base::OneShotTimer> _watchdogTimer;
  id<ChangeProfileCommands> _changeProfileHandler;
  ActionSheetCoordinator* _leavingPrimaryAccountConfirmationDialogCoordinator;
}

- (id<AuthenticationFlowPerformerDelegate>)delegate {
  return _delegate;
}

- (instancetype)
        initWithDelegate:(id<AuthenticationFlowPerformerDelegate>)delegate
    changeProfileHandler:(id<ChangeProfileCommands>)changeProfileHandler {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _changeProfileHandler = changeProfileHandler;
  }
  return self;
}

- (void)interrupt {
  [_managedConfirmationScreenCoordinator stop];
  _managedConfirmationScreenCoordinator = nil;
  [_managedConfirmationAlertCoordinator stop];
  _managedConfirmationAlertCoordinator = nil;
  [_errorAlertCoordinator stop];
  _errorAlertCoordinator = nil;
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
  SystemIdentityManager* systemIdentityManager =
      GetApplicationContext()->GetSystemIdentityManager();
  if (NSString* hostedDomain =
          systemIdentityManager->GetCachedHostedDomainForIdentity(identity)) {
    [_delegate didFetchManagedStatus:hostedDomain];
    return;
  }

  [self startWatchdogTimerForManagedStatus];
  __weak AuthenticationFlowPerformer* weakSelf = self;
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

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  __weak __typeof(self) weakSelf = self;
  base::OnceCallback<void(const policy::ProfileSeparationPolicies&)> callback =
      base::BindOnce(
          [](__typeof(self) strongSelf,
             const policy::ProfileSeparationPolicies& policies) {
            [strongSelf didFetchProfileSeparationPolicies:policies];
          },
          weakSelf);
  _accountLevelSigninRestrictionPolicyFetcher
      ->GetManagedAccountsSigninRestriction(
          identity_manager,
          identity_manager->PickAccountIdForAccount(
              GaiaId(identity.gaiaID),
              base::SysNSStringToUTF8(identity.userEmail)),
          std::move(callback));
}

- (void)signInIdentity:(id<SystemIdentity>)identity
         atAccessPoint:(signin_metrics::AccessPoint)accessPoint
        currentProfile:(ProfileIOS*)currentProfile {
  AuthenticationServiceFactory::GetForProfile(currentProfile)
      ->SignIn(identity, accessPoint);
}

- (void)switchToProfileWithIdentity:(id<SystemIdentity>)identity
                         sceneState:(SceneState*)sceneState {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());

  std::optional<std::string> profileName =
      GetApplicationContext()
          ->GetAccountProfileMapper()
          ->FindProfileNameForGaiaID(GaiaId(identity.gaiaID));
  if (!profileName.has_value()) {
    __weak __typeof(_delegate) weakDelegate = _delegate;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](__typeof(_delegate) delegate) {
                         [delegate didFailToSwitchToProfile];
                       },
                       weakDelegate));
    return;
  }

  [self switchToProfileWithName:*profileName sceneState:sceneState];
}

- (void)switchToProfileWithName:(const std::string&)profileName
                     sceneState:(SceneState*)sceneState {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());

  __weak __typeof(_delegate) weakDelegate = _delegate;
  OnProfileSwitchCompletion completion = base::BindOnce(
      [](__typeof(_delegate) delegate, bool success,
         Browser* new_profile_browser) {
        [delegate didSwitchToProfileWithNewProfileBrowser:new_profile_browser];
      },
      weakDelegate);

  [_changeProfileHandler
      changeProfile:profileName
           forScene:sceneState
       continuation:base::BindOnce(&AuthenticationFlowContinuation,
                                   std::move(completion))];
}

- (void)makePersonalProfileManagedWithIdentity:(id<SystemIdentity>)identity {
  GetApplicationContext()
      ->GetAccountProfileMapper()
      ->MakePersonalProfileManagedWithGaiaID(GaiaId(identity.gaiaID));
  [_delegate didMakePersonalProfileManaged];
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

- (void)showManagedConfirmationForHostedDomain:(NSString*)hostedDomain
                                      identity:(id<SystemIdentity>)identity
                                viewController:(UIViewController*)viewController
                                       browser:(Browser*)browser
                     skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration
                    mergeBrowsingDataByDefault:(BOOL)mergeBrowsingDataByDefault
         browsingDataMigrationDisabledByPolicy:
             (BOOL)browsingDataMigrationDisabledByPolicy {
  DCHECK(!_managedConfirmationScreenCoordinator);
  DCHECK(!_managedConfirmationAlertCoordinator);
  DCHECK(!_errorAlertCoordinator);

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
                browsingDataMigrationDisabledByPolicy];
    _managedConfirmationScreenCoordinator.delegate = self;
    [_managedConfirmationScreenCoordinator start];
    return;
  }
  __weak AuthenticationFlowPerformer* weakSelf = self;
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

- (void)completePostSignInActions:(PostSignInActionSet)postSignInActions
                     withIdentity:(id<SystemIdentity>)identity
                          browser:(Browser*)browser
                      accessPoint:(signin_metrics::AccessPoint)accessPoint {
  DCHECK(browser);
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);

  // Signing in from bookmarks and reading list enables the corresponding
  // type.
  std::optional<syncer::UserSelectableType> clearSelectableType;
  if (postSignInActions.Has(
          PostSignInAction::kEnableUserSelectableTypeBookmarks) &&
      !syncService->GetUserSettings()->GetSelectedTypes().Has(
          syncer::UserSelectableType::kBookmarks)) {
    syncService->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kBookmarks, true);
    clearSelectableType = syncer::UserSelectableType::kBookmarks;
  } else if (postSignInActions.Has(
                 PostSignInAction::kEnableUserSelectableTypeReadingList) &&
             !syncService->GetUserSettings()->GetSelectedTypes().Has(
                 syncer::UserSelectableType::kReadingList)) {
    syncService->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kReadingList, true);
    clearSelectableType = syncer::UserSelectableType::kReadingList;
  }

  if (postSignInActions.Has(
          PostSignInAction::kShowHistorySyncScreenAfterProfileSwitch)) {
    [self showHistorySyncScreenAfterProfileSwitch:browser
                                      accessPoint:accessPoint];
  }

  if (postSignInActions.Has(
          PostSignInAction::kShowIdentityConfirmationSnackbar)) {
    [self triggerAccountSwitchSnackbarWithIdentity:identity browser:browser];
    return;
  }

  if (!postSignInActions.Has(PostSignInAction::kShowSnackbar)) {
    return;
  }

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = base::CallbackToBlock(base::BindOnce(
      &HandleSignoutForSnackbar, browser->AsWeakPtr(), clearSelectableType));

  action.title = l10n_util::GetNSString(IDS_IOS_SIGNIN_SNACKBAR_UNDO);
  action.accessibilityIdentifier = kSigninSnackbarUndo;
  NSString* messageText =
      l10n_util::GetNSStringF(IDS_IOS_SIGNIN_SNACKBAR_SIGNED_IN_AS,
                              base::SysNSStringToUTF16(identity.userEmail));
  MDCSnackbarMessage* message = CreateSnackbarMessage(messageText);
  message.action = action;
  message.category = kAuthenticationSnackbarCategory;

  id<SnackbarCommands> handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);
  CHECK(handler);
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  [handler showSnackbarMessage:message];
}

- (void)showAuthenticationError:(NSError*)error
                 withCompletion:(ProceduralBlock)callback
                 viewController:(UIViewController*)viewController
                        browser:(Browser*)browser {
  DCHECK(!_managedConfirmationScreenCoordinator);
  DCHECK(!_managedConfirmationAlertCoordinator);
  DCHECK(!_errorAlertCoordinator);

  base::RecordAction(base::UserMetricsAction(
      "Signin_AuthenticationFlowPerformer_ErrorDialog_Presented"));
  _errorAlertCoordinator =
      ErrorCoordinatorNoItem(error, viewController, browser);

  __weak AuthenticationFlowPerformer* weakSelf = self;
  __weak AlertCoordinator* weakAlert = _errorAlertCoordinator;
  ProceduralBlock dismissAction = ^{
    base::RecordAction(base::UserMetricsAction(
        "Signin_AuthenticationFlowPerformer_ErrorDialog_Confirmed"));
    [weakSelf alertControllerDidDisappear:weakAlert];
    if (callback) {
      callback();
    }
  };

  NSString* okButtonLabel = l10n_util::GetNSString(IDS_OK);
  [_errorAlertCoordinator addItemWithTitle:okButtonLabel
                                    action:dismissAction
                                     style:UIAlertActionStyleDefault];

  [_errorAlertCoordinator start];
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

// Wraps -didFetchUserPolicyWithSuccess: method from the delegate with a
// check that the watchdog has not expired.
- (void)didFetchUserPolicyWithSuccess:(BOOL)success {
  // If the watchdog timer has already fired, don't notify the delegate.
  if (![self stopWatchdogTimer]) {
    return;
  }

  [_delegate didFetchUserPolicyWithSuccess:success];
}

#pragma mark - Private

// Called when `_leavingPrimaryAccountConfirmationDialogCoordinator` is done.
- (void)leavingPrimaryAccountConfirmationDone:(BOOL)continueFlow {
  [_leavingPrimaryAccountConfirmationDialogCoordinator stop];
  _leavingPrimaryAccountConfirmationDialogCoordinator = nil;
  [_delegate didAcceptToLeavePrimaryAccount:continueFlow];
}

// Called when separation policies have been fetched, and calls the delegate.
- (void)didFetchProfileSeparationPolicies:
    (const policy::ProfileSeparationPolicies&)policies {
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

- (void)updateUserPolicyNotificationStatusIfNeeded:(PrefService*)prefService {
  prefService->SetBoolean(policy::policy_prefs::kUserPolicyNotificationWasShown,
                          true);
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

// Starts a Watchdog Timer that calls `timeoutBlock` on time out.
- (void)startWatchdogTimerWithTimeoutBlock:(ProceduralBlock)timeoutBlock {
  DCHECK(!_watchdogTimer);
  _watchdogTimer.reset(new base::OneShotTimer());
  _watchdogTimer->Start(FROM_HERE,
                        base::Seconds(kAuthenticationFlowTimeoutSeconds),
                        base::BindOnce(timeoutBlock));
}

// Starts the watchdog timer with a timeout of
// `kAuthenticationFlowTimeoutSeconds` for the fetching managed status
// operation. It will notify `_delegate` of the failure unless
// `stopWatchdogTimer` is called before it times out.
- (void)startWatchdogTimerForManagedStatus {
  __weak AuthenticationFlowPerformer* weakSelf = self;
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

// Starts a Watchdog Timer that ends the user policy registration on time out.
- (void)startWatchdogTimerForUserPolicyRegistration {
  __weak AuthenticationFlowPerformer* weakSelf = self;
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
  __weak AuthenticationFlowPerformer* weakSelf = self;
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

// Stops the watchdog timer, and doesn't call the `timeoutDelegateSelector`.
// Returns whether the watchdog was actually running.
- (BOOL)stopWatchdogTimer {
  if (_watchdogTimer) {
    _watchdogTimer->Stop();
    _watchdogTimer.reset();
    return YES;
  }
  return NO;
}

// Callback for when the alert is dismissed.
- (void)alertControllerDidDisappear:(ChromeCoordinator*)coordinator {
  CHECK(!_managedConfirmationAlertCoordinator, base::NotFatalUntil::M136);
  CHECK(!_managedConfirmationScreenCoordinator, base::NotFatalUntil::M136);
  CHECK(_errorAlertCoordinator, base::NotFatalUntil::M136);
  [_errorAlertCoordinator stop];
  _errorAlertCoordinator = nil;
}

// Called when `_managedConfirmationAlertCoordinator` is finished.
// `accepted` is YES when the user confirmed or NO if the user canceled.
- (void)managedConfirmationAlertAccepted:(BOOL)accepted {
  CHECK(_managedConfirmationAlertCoordinator, base::NotFatalUntil::M136);
  CHECK(!_errorAlertCoordinator, base::NotFatalUntil::M136);
  CHECK(!_managedConfirmationScreenCoordinator, base::NotFatalUntil::M136);
  Browser* browser = _managedConfirmationAlertCoordinator.browser;
  [_managedConfirmationAlertCoordinator stop];
  _managedConfirmationAlertCoordinator = nil;
  [self managedConfirmationDidAccept:accepted
                             browser:browser
            keepBrowsingDataSeparate:
                AreSeparateProfilesForManagedAccountsEnabled()];
}

// Called when the user accepted to continue to sign-in with a managed account.
// `accepted` is YES when the user confirmed or NO if the user canceled.
// If `keepBrowsingDataSeparate` is `YES`, the managed account gets signed in to
// a new empty work profile. This must only be specified if
// AreSeparateProfilesForManagedAccountsEnabled() is true.
// If `keepBrowsingDataSeparate` is `NO`, the account gets signed in to the
// current profile. If AreSeparateProfilesForManagedAccountsEnabled() is true,
// this involves converting the current profile into a work profile.
- (void)managedConfirmationDidAccept:(BOOL)accepted
                             browser:(Browser*)browser
            keepBrowsingDataSeparate:(BOOL)keepBrowsingDataSeparate {
  if (!accepted) {
    base::RecordAction(
        base::UserMetricsAction("Signin_AuthenticationFlowPerformer_"
                                "ManagedConfirmationDialog_Canceled"));
    [self.delegate didCancelManagedConfirmation];
    return;
  }
  CHECK(AreSeparateProfilesForManagedAccountsEnabled() ||
        !keepBrowsingDataSeparate);
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
  [self.delegate didAcceptManagedConfirmation:keepBrowsingDataSeparate];
}

// Displays the identity confirmation snackbar with `identity`.
- (void)triggerAccountSwitchSnackbarWithIdentity:(id<SystemIdentity>)identity
                                         browser:(Browser*)browser {
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
  UIImage* avatar = ChromeAccountManagerServiceFactory::GetForProfile(profile)
                        ->GetIdentityAvatarWithIdentity(
                            identity, IdentityAvatarSize::Regular);
  ManagementState managementState =
      GetManagementState(IdentityManagerFactory::GetForProfile(profile),
                         AuthenticationServiceFactory::GetForProfile(profile),
                         profile->GetPrefs());
  MDCSnackbarMessage* snackbarTitle = [[IdentitySnackbarMessage alloc]
      initWithName:identity.userGivenName
             email:identity.userEmail
            avatar:avatar
           managed:managementState.is_profile_managed()];
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarCommandsHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarCommandsHandler showSnackbarMessageOverBrowserToolbar:snackbarTitle];
}

#pragma mark - ManagedProfileCreationCoordinatorDelegate

- (void)managedProfileCreationCoordinator:
            (ManagedProfileCreationCoordinator*)coordinator
                                didAccept:(BOOL)accepted
                 keepBrowsingDataSeparate:(BOOL)keepBrowsingDataSeparate {
  CHECK(!_managedConfirmationAlertCoordinator, base::NotFatalUntil::M136);
  CHECK(!_errorAlertCoordinator, base::NotFatalUntil::M136);
  CHECK_EQ(_managedConfirmationScreenCoordinator, coordinator);
  Browser* browser = _managedConfirmationScreenCoordinator.browser;
  [_managedConfirmationScreenCoordinator stop];
  _managedConfirmationScreenCoordinator = nil;
  [self managedConfirmationDidAccept:accepted
                             browser:browser
            keepBrowsingDataSeparate:keepBrowsingDataSeparate];
}

- (void)showHistorySyncScreenAfterProfileSwitch:(Browser*)browser
                                    accessPoint:(signin_metrics::AccessPoint)
                                                    accessPoint {
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kHistorySync
               identity:nil
            accessPoint:accessPoint
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
             completion:nil];
  command.optionalHistorySync = YES;

  UIViewController* viewController =
      browser->GetSceneState().rootViewController;
  while (viewController.presentedViewController) {
    viewController = viewController.presentedViewController;
  }

  [browser->GetSceneState().controller showSignin:command
                               baseViewController:viewController];
}

@end
