// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "components/prefs/pref_service.h"
#import "components/reading_list/features/reading_list_switches.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/account_pref_utils.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_capabilities_fetcher.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/policy/model/cloud/user_policy_switch.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_error_api.h"
#import "ui/base/l10n/l10n_util.h"

using signin_ui::SigninCompletionCallback;

namespace {

// The states of the sign-in flow state machine.
// TODO(crbug.com/375605482): Need to remove steps from `kSignOutIfNeeded` to
// `kCompleteWithFailure` can be replaced with `AuthenticationFlowInProfile`
// even without multi profile.
enum class AuthenticationState {
  kBegin,
  // Check if there are unsynced data with the primary account, in the current
  // profile.
  kCheckUnsyncedData,
  // Display confirmation dialog when the user is already signed in, based on
  // unsynced data and if the primary account is a managed account.
  kShowLeavingPrimaryAccountConfirmationIfNeeded,
  kFetchManagedStatus,
  kFetchProfileSeparationPoliciesIfNeeded,
  kShowManagedConfirmationIfNeeded,
  kConvertPersonalProfileToManagedIfNeeded,
  kSwitchProfileIfNeeded,
  kSignOutIfNeeded,
  kSignIn,
  kRegisterForUserPolicy,
  kFetchUserPolicy,
  kFetchCapabilities,
  kCompleteWithSuccess,
  kCompleteWithFailure,
  kCleanupBeforeDone,
  kDone,
};

enum class CancelationReason {
  // Not canceled.
  kNotCanceled,
  // Canceled by the user.
  kUserCanceled,
  // Canceled, but not by the user.
  kFailed,
};

// Used by `RecordUnsyncedDataHistogramIfNeeded()` to know which histogram to
// record the unsynced data types.
enum class UnsyncedDataTypeHistogram {
  // `Sync.UnsyncedDataOnAccountSwitching` histogram.
  kUnsyncedDataOnAccountSwitching,
  // `Sync.UnsyncedDataOnProfileSwitching` histogram.
  kUnsyncedDataOnProfileSwitching,
};

// Name for `Signin.IOSIdentityAvailableInProfile` histogram.
const char kIOSIdentityAvailableInProfileHistogram[] =
    "Signin.IOSIdentityAvailableInProfile";

// Enum for `Signin.IOSIdentityAvailableInProfile` histogram.
// Entries should not be renumbered and numeric values should never be reused.
enum class IOSIdentityAvailableInProfile : int {
  kNotAvailableInProfileMapperNotAvailableInIdentityManager = 0,
  kNotAvailableInProfileMapperAvailableInIdentityManager = 1,
  kAvailableInProfileMapperNotAvailableInIdentityManager = 2,
  kAvailableInProfileMapperAvailableInIdentityManager = 3,
  kMaxValue = kAvailableInProfileMapperAvailableInIdentityManager,
};

// Returns `true` if any of the following holds:
// * we are at the FRE step,
// * there is already a profile that has been fully initialized for gaia_id, or
// * a policy forces the browsing data to stay separated.
bool ShouldSkipBrowsingDataMigration(signin_metrics::AccessPoint access_point,
                                     NSString* gaia_id,
                                     PrefService* pref_service) {
  bool always_separate_browsing_data_per_policy =
      pref_service->GetInteger(
          prefs::kProfileSeparationDataMigrationSettings) ==
      policy::ALWAYS_SEPARATE;
  return always_separate_browsing_data_per_policy ||
         access_point == signin_metrics::AccessPoint::kStartPage ||
         GetApplicationContext()
             ->GetAccountProfileMapper()
             ->IsProfileForGaiaIDFullyInitialized(GaiaId(gaia_id));
}

// Returns `true` if the browsing data migration is not available because it is
// disabled by policy and not because of another reason.
bool IsBrowsingDataMigrationDisabledByPolicy(
    signin_metrics::AccessPoint access_point,
    NSString* gaia_id,
    PrefService* pref_service,
    policy::ProfileSeparationDataMigrationSettings
        profileSeparationDataMigrationSettings) {
  return access_point != signin_metrics::AccessPoint::kStartPage &&
         !GetApplicationContext()
              ->GetAccountProfileMapper()
              ->IsProfileForGaiaIDFullyInitialized(GaiaId(gaia_id)) &&
         (profileSeparationDataMigrationSettings == policy::ALWAYS_SEPARATE ||
          pref_service->GetInteger(
              prefs::kProfileSeparationDataMigrationSettings) ==
              policy::ALWAYS_SEPARATE);
}

// Returns if `identity` is available by AccountProfileMapper and if it is
// available by IdentityManager.
IOSIdentityAvailableInProfile IdentityAvailableInProfileStatus(
    NSString* gaia_id,
    signin::IdentityManager* identity_manager,
    std::string_view profile_name) {
  bool is_identity_available_in_profile_mapper = false;
  AccountProfileMapper::IdentityIteratorCallback callback = base::BindRepeating(
      [](BOOL* isIdentityAvailableInProfileMapper,
         NSString* signinIdentityGaiaID, id<SystemIdentity> identity) {
        *isIdentityAvailableInProfileMapper =
            [identity.gaiaID isEqualToString:signinIdentityGaiaID];
        return *isIdentityAvailableInProfileMapper
                   ? AccountProfileMapper::IteratorResult::kInterruptIteration
                   : AccountProfileMapper::IteratorResult::kContinueIteration;
      },
      &is_identity_available_in_profile_mapper, gaia_id);
  GetApplicationContext()->GetAccountProfileMapper()->IterateOverIdentities(
      callback, profile_name);
  std::vector<CoreAccountInfo> accounts_in_profile =
      identity_manager->GetAccountsWithRefreshTokens();
  bool is_identity_available_in_identity_manager = base::Contains(
      accounts_in_profile, GaiaId(gaia_id), &CoreAccountInfo::gaia);
  if (!is_identity_available_in_profile_mapper &&
      !is_identity_available_in_identity_manager) {
    return IOSIdentityAvailableInProfile::
        kNotAvailableInProfileMapperNotAvailableInIdentityManager;
  } else if (is_identity_available_in_profile_mapper &&
             !is_identity_available_in_identity_manager) {
    return IOSIdentityAvailableInProfile::
        kAvailableInProfileMapperNotAvailableInIdentityManager;
  } else if (!is_identity_available_in_profile_mapper &&
             is_identity_available_in_identity_manager) {
    return IOSIdentityAvailableInProfile::
        kNotAvailableInProfileMapperAvailableInIdentityManager;
  }
  return IOSIdentityAvailableInProfile::
      kAvailableInProfileMapperAvailableInIdentityManager;
}

// Records `Signin.IOSIdentityAvailableInProfile` histogram.
void RecordIOSIdentityAvailableInProfile(
    NSString* gaia_id,
    signin::IdentityManager* identity_manager,
    std::string_view profile_name) {
  IOSIdentityAvailableInProfile identity_available =
      IdentityAvailableInProfileStatus(gaia_id, identity_manager, profile_name);
  base::UmaHistogramEnumeration(kIOSIdentityAvailableInProfileHistogram,
                                identity_available);
}

// Records histogram for the unsync data.
void RecordUnsyncedDataHistogramIfNeeded(UnsyncedDataTypeHistogram histogram,
                                         syncer::DataTypeSet set) {
  const char* histogram_name = nullptr;
  switch (histogram) {
    case UnsyncedDataTypeHistogram::kUnsyncedDataOnAccountSwitching:
      histogram_name = "Sync.UnsyncedDataOnAccountSwitching";
      break;
    case UnsyncedDataTypeHistogram::kUnsyncedDataOnProfileSwitching:
      histogram_name = "Sync.UnsyncedDataOnProfileSwitching";
      break;
  }
  CHECK(histogram_name);
  for (syncer::DataType type : set) {
    base::UmaHistogramEnumeration(histogram_name,
                                  syncer::DataTypeHistogramValue(type));
  }
}

}  // namespace

@interface AuthenticationFlow ()

// Whether this flow is curently handling an error.
@property(nonatomic, assign) BOOL handlingError;

// The actions to perform following account sign-in.
@property(nonatomic, assign) PostSignInActionSet postSignInActions;

@end

@implementation AuthenticationFlow {
  // View used to display dialogs.
  UIViewController* _presentingViewController;
  // Anchor based on the sign-in button that triggered sign-in.
  // Used to display popover dialog (like the unsynced data confirmation dialog)
  // with a regular window size (like iPad).
  UIView* _anchorView;
  CGRect _anchorRect;
  SigninCompletionCallback _signInCompletion;
  AuthenticationFlowPerformer* _performer;

  // State machine tracking.
  AuthenticationState _state;
  BOOL _didSignIn;
  CancelationReason _cancelationReason;
  // YES if the personal profile should be converted to a managed (work) profile
  // as part of the signin flow. Can only be true if the to-be-signed-in account
  // is managed.
  BOOL _shouldConvertPersonalProfileToManaged;

  raw_ptr<Browser> _browser;
  id<SystemIdentity> _identityToSignIn;
  signin_metrics::AccessPoint _accessPoint;
  NSString* _identityToSignInHostedDomain;

  // Token to have access to user policies from dmserver.
  NSString* _dmToken;
  // ID of the client that is registered for user policy.
  NSString* _clientID;
  // List of IDs that represents the domain of the user. The list will be used
  // to compare with a similiar list from device mangement to understand whether
  // user and device are managed by the same domain.
  NSArray<NSString*>* _userAffiliationIDs;

  // This AuthenticationFlow keeps a reference to `self` while a sign-in flow is
  // is in progress to ensure it outlives any attempt to destroy it in
  // `_signInCompletion`.
  AuthenticationFlow* _selfRetainer;

  // Capabilities fetcher for the subsequent History Sync Opt-In screen.
  HistorySyncCapabilitiesFetcher* _capabilitiesFetcher;

  // Value of the ProfileSeparationDataMigrationSettings for
  // `_identityToSignin`. This is used to know if the user can convert an
  // existing profile into a managed profile.
  policy::ProfileSeparationDataMigrationSettings
      _profileSeparationDataMigrationSettings;

  // `YES` if the profile switching is done.
  BOOL _didSwitchProfile;
  // List of unsynced data types in the current profile. If there is no primary
  // account the set is empty.
  // The value is set during `kCheckUnsyncedData` step.
  std::optional<syncer::DataTypeSet> _unsyncedDataTypes;
  // The lifetime of this ScopedClosureRunner denotes a batch of primary account
  // changes. UI listens to batched changes to avoid visual artifacts during an
  // account switch.
  base::ScopedClosureRunner _accountSwitchingBatchClosureRunner;
}

@synthesize handlingError = _handlingError;
@synthesize identity = _identityToSignIn;

#pragma mark - Public methods

- (instancetype)initWithBrowser:(Browser*)browser
                       identity:(id<SystemIdentity>)identity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
              postSignInActions:(PostSignInActionSet)postSignInActions
       presentingViewController:(UIViewController*)presentingViewController
                     anchorView:(UIView*)anchorView
                     anchorRect:(CGRect)anchorRect {
  if ((self = [super init])) {
    DCHECK(browser);
    DCHECK(presentingViewController);
    DCHECK(identity);
    _browser = browser;
    _identityToSignIn = identity;
    _accessPoint = accessPoint;
    _postSignInActions = postSignInActions;
    _presentingViewController = presentingViewController;
    _anchorView = anchorView;
    _anchorRect = anchorRect;
    _state = AuthenticationState::kBegin;
    _cancelationReason = CancelationReason::kNotCanceled;
    _profileSeparationDataMigrationSettings =
        policy::ProfileSeparationDataMigrationSettings::USER_OPT_IN;
  }
  return self;
}

- (void)startSignInWithCompletion:(SigninCompletionCallback)completion {
  DCHECK_EQ(AuthenticationState::kBegin, _state);
  DCHECK(!_signInCompletion);
  DCHECK(completion);
  _signInCompletion = [completion copy];
  _selfRetainer = self;
  // Kick off the state machine.
  if (!_performer) {
    id<ChangeProfileCommands> changeProfileHandler = HandlerForProtocol(
        _browser->GetSceneState().profileState.appState.appCommandDispatcher,
        ChangeProfileCommands);
    _performer = [[AuthenticationFlowPerformer alloc]
            initWithDelegate:self
        changeProfileHandler:changeProfileHandler];
  }
  // Make sure -[AuthenticationFlow startSignInWithCompletion:] doesn't call
  // the completion block synchronously.
  // Related to http://crbug.com/1246480.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf continueFlow];
  });
}

- (void)interrupt {
  if (_state == AuthenticationState::kDone) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  [_performer interruptWithCompletion:^() {
    [weakSelf performerInterrupted];
  }];
}

- (void)performerInterrupted {
  if (_state != AuthenticationState::kDone) {
    // The performer might not have been able to continue the flow if it was
    // waiting for a callback (e.g. waiting for AccountReconcilor). In this
    // case, we force the flow to finish synchronously.
    [self cancelFlowWithReason:CancelationReason::kFailed];
  }
  DCHECK_EQ(AuthenticationState::kDone, _state);
}

- (void)setPresentingViewController:
    (UIViewController*)presentingViewController {
  _presentingViewController = presentingViewController;
}

#pragma mark - State machine management

- (AuthenticationState)nextStateFailedOrCanceled {
  DCHECK([self canceled]);
  switch (_state) {
    case AuthenticationState::kBegin:
    case AuthenticationState::kCheckUnsyncedData:
    case AuthenticationState::kShowLeavingPrimaryAccountConfirmationIfNeeded:
    case AuthenticationState::kFetchManagedStatus:
    case AuthenticationState::kFetchProfileSeparationPoliciesIfNeeded:
    case AuthenticationState::kShowManagedConfirmationIfNeeded:
    case AuthenticationState::kConvertPersonalProfileToManagedIfNeeded:
    case AuthenticationState::kSwitchProfileIfNeeded:
    case AuthenticationState::kSignOutIfNeeded:
    case AuthenticationState::kSignIn:
    case AuthenticationState::kRegisterForUserPolicy:
    case AuthenticationState::kFetchUserPolicy:
    case AuthenticationState::kFetchCapabilities:
      return AuthenticationState::kCompleteWithFailure;
    case AuthenticationState::kCompleteWithSuccess:
    case AuthenticationState::kCompleteWithFailure:
      return AuthenticationState::kCleanupBeforeDone;
    case AuthenticationState::kCleanupBeforeDone:
    case AuthenticationState::kDone:
      return AuthenticationState::kDone;
  }
}

- (AuthenticationState)nextState {
  DCHECK(!self.handlingError);
  if ([self canceled]) {
    return [self nextStateFailedOrCanceled];
  }
  DCHECK(![self canceled]);
  switch (_state) {
    case AuthenticationState::kBegin:
      return AuthenticationState::kCheckUnsyncedData;
    case AuthenticationState::kCheckUnsyncedData:
      return AuthenticationState::
          kShowLeavingPrimaryAccountConfirmationIfNeeded;
    case AuthenticationState::kShowLeavingPrimaryAccountConfirmationIfNeeded:
      return AuthenticationState::kFetchManagedStatus;
    case AuthenticationState::kFetchManagedStatus:
      return AuthenticationState::kFetchProfileSeparationPoliciesIfNeeded;
    case AuthenticationState::kFetchProfileSeparationPoliciesIfNeeded:
      return AuthenticationState::kShowManagedConfirmationIfNeeded;
    case AuthenticationState::kShowManagedConfirmationIfNeeded:
      return AuthenticationState::kConvertPersonalProfileToManagedIfNeeded;
    case AuthenticationState::kConvertPersonalProfileToManagedIfNeeded:
      return AuthenticationState::kSwitchProfileIfNeeded;
    case AuthenticationState::kSwitchProfileIfNeeded:
      if (_didSwitchProfile) {
        // Once the profile switch is done, there is nothing more to do in this
        // profile. The `kCompleteWithSuccess` should be skipped. The completion
        // block has been passed to `AuthenticationFlowInProfile`.
        CHECK(!_signInCompletion);
        return AuthenticationState::kCleanupBeforeDone;
      }
      return AuthenticationState::kSignOutIfNeeded;
    case AuthenticationState::kSignOutIfNeeded:
      return AuthenticationState::kSignIn;
    case AuthenticationState::kSignIn:
      if (policy::IsAnyUserPolicyFeatureEnabled() &&
          _identityToSignInHostedDomain.length > 0) {
        return AuthenticationState::kRegisterForUserPolicy;
      } else if ([self shouldFetchCapabilities]) {
        return AuthenticationState::kFetchCapabilities;
      } else {
        return AuthenticationState::kCompleteWithSuccess;
      }
    case AuthenticationState::kRegisterForUserPolicy:
      if (!_dmToken.length || !_clientID.length) {
        // Skip fetching user policies when registration failed.
        if ([self shouldFetchCapabilities]) {
          return AuthenticationState::kFetchCapabilities;
        } else {
          return AuthenticationState::kCompleteWithSuccess;
        }
      }
      // Fetch user policies when registration is successful.
      return AuthenticationState::kFetchUserPolicy;
    case AuthenticationState::kFetchUserPolicy:
      if ([self shouldFetchCapabilities]) {
        return AuthenticationState::kFetchCapabilities;
      } else {
        return AuthenticationState::kCompleteWithSuccess;
      }
    case AuthenticationState::kFetchCapabilities:
      return AuthenticationState::kCompleteWithSuccess;
    case AuthenticationState::kCompleteWithSuccess:
    case AuthenticationState::kCompleteWithFailure:
      return AuthenticationState::kCleanupBeforeDone;
    case AuthenticationState::kCleanupBeforeDone:
    case AuthenticationState::kDone:
      return AuthenticationState::kDone;
  }
}

// Continues the sign-in state machine starting from `_state` and invokes
// `_signInCompletion` when finished.
- (void)continueFlow {
  ProfileIOS* profile = [self originalProfile];
  if (self.handlingError) {
    // The flow should not continue while the error is being handled, e.g. while
    // the user is being informed of an issue.
    return;
  }
  _state = [self nextState];
  switch (_state) {
    case AuthenticationState::kBegin:
      NOTREACHED();

    case AuthenticationState::kCheckUnsyncedData:
      [self checkUnsyncedDataStep];
      return;

    case AuthenticationState::kShowLeavingPrimaryAccountConfirmationIfNeeded:
      [self showLeavingPrimaryAccountConfirmationIfNeededStep];
      return;

    case AuthenticationState::kFetchManagedStatus:
      [_performer fetchManagedStatus:profile forIdentity:_identityToSignIn];
      return;

    case AuthenticationState::kFetchProfileSeparationPoliciesIfNeeded:
      [self fetchProfileSeparationPoliciesIfNeededStep];
      return;

    case AuthenticationState::kShowManagedConfirmationIfNeeded:
      [self showManagedConfirmationIfNeededStep];
      return;

    case AuthenticationState::kConvertPersonalProfileToManagedIfNeeded:
      [self convertPersonalProfileToManagedIfNeededStep];
      return;

    case AuthenticationState::kSwitchProfileIfNeeded:
      [self switchProfileIfNeededStep];
      return;

    case AuthenticationState::kSignOutIfNeeded:
      [self signOutIfNeededStep];
      return;

    case AuthenticationState::kSignIn:
      [self signInStep];
      return;

    case AuthenticationState::kRegisterForUserPolicy:
      [_performer registerUserPolicy:profile forIdentity:_identityToSignIn];
      return;

    case AuthenticationState::kFetchUserPolicy:
      [_performer fetchUserPolicy:profile
                      withDmToken:_dmToken
                         clientID:_clientID
               userAffiliationIDs:_userAffiliationIDs
                         identity:_identityToSignIn];
      return;
    case AuthenticationState::kFetchCapabilities:
      [self fetchCapabilities];
      return;
    case AuthenticationState::kCompleteWithSuccess:
      [self completeWithSuccessStep];
      return;
    case AuthenticationState::kCompleteWithFailure:
      [self completeWithFailureStep];
      return;
    case AuthenticationState::kCleanupBeforeDone: {
      // Clean up asynchronously to ensure that `self` does not die while
      // the flow is running.
      DCHECK([NSThread isMainThread]);
      dispatch_async(dispatch_get_main_queue(), ^{
        self->_selfRetainer = nil;
      });
      [self continueFlow];
      return;
    }
    case AuthenticationState::kDone:
      return;
  }
  NOTREACHED();
}

- (void)checkUnsyncedDataStep {
  ProfileIOS* profile = [self originalProfile];
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> currentIdentity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  // AuthenticationFlow should not switch to the same identity.
  CHECK(![currentIdentity isEqual:_identityToSignIn],
        base::NotFatalUntil::M140);
  if (!currentIdentity) {
    _unsyncedDataTypes = syncer::DataTypeSet();
    [self continueFlow];
    return;
  }
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile([self originalProfile]);
  [_performer fetchUnsyncedDataWithSyncService:syncService];
}

- (void)showLeavingPrimaryAccountConfirmationIfNeededStep {
  CHECK(_unsyncedDataTypes.has_value(), base::NotFatalUntil::M140);
  ProfileIOS* profile = _browser->GetProfile()->GetOriginalProfile();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  PrefService* profilePrefService = profile->GetPrefs();
  SignedInUserState signedInUserState = GetSignedInUserState(
      authenticationService, identityManager, profilePrefService);
  if (!ForceLeavingPrimaryAccountConfirmationDialog(signedInUserState) &&
      _unsyncedDataTypes.value().empty()) {
    [self continueFlow];
    return;
  }
  [_performer
      showLeavingPrimaryAccountConfirmationWithBaseViewController:
          _presentingViewController
                                                          browser:_browser
                                                signedInUserState:
                                                    signedInUserState
                                                       anchorView:_anchorView
                                                       anchorRect:_anchorRect];
}

// Fetches ManagedAccountsSigninRestriction policy, if needed.
- (void)fetchProfileSeparationPoliciesIfNeededStep {
  if (!ShouldShowManagedConfirmationForHostedDomain(
          _identityToSignInHostedDomain, _accessPoint, _identityToSignIn.gaiaID,
          [self prefs])) {
    // The managed confirmation dialog can be skipped, therefore, there is no
    // need to fetch the policy.
    [self continueFlow];
    return;
  }
  if (!AreSeparateProfilesForManagedAccountsEnabled() ||
      ShouldSkipBrowsingDataMigration(_accessPoint, _identityToSignIn.gaiaID,
                                      [self prefs])) {
    // The profile-separation policy affects whether browsing-data-migration
    // is offered, so it's only needed if the migration isn't skipped.
    [self continueFlow];
    return;
  }
  ProfileIOS* profile = [self originalProfile];
  [_performer fetchProfileSeparationPolicies:profile
                                 forIdentity:_identityToSignIn];
}

// Shows a confirmation dialog for signing in to an account managed.
- (void)showManagedConfirmationIfNeededStep {
  if (!ShouldShowManagedConfirmationForHostedDomain(
          _identityToSignInHostedDomain, _accessPoint, _identityToSignIn.gaiaID,
          [self prefs])) {
    [self continueFlow];
    return;
  }
  // These value are not used if
  // `AreSeparateProfilesForManagedAccountsEnabled()` is false.
  BOOL skipBrowsingDataMigration = NO;
  BOOL mergeBrowsingDataByDefault = NO;
  BOOL browsingDataMigrationDisabledByPolicy = NO;
  if (AreSeparateProfilesForManagedAccountsEnabled()) {
    // Skip browsing data migration if we are at the first run screen or if
    // there is already a profile that exists with the account we are trying
    // to signin with.
    PrefService* prefService = [self prefs];
    skipBrowsingDataMigration =
        _profileSeparationDataMigrationSettings == policy::ALWAYS_SEPARATE ||
        ShouldSkipBrowsingDataMigration(_accessPoint, _identityToSignIn.gaiaID,
                                        prefService);
    browsingDataMigrationDisabledByPolicy =
        IsBrowsingDataMigrationDisabledByPolicy(
            _accessPoint, _identityToSignIn.gaiaID, prefService,
            _profileSeparationDataMigrationSettings);

    // Merge browsing data by default if the data migration screen is shown to
    // the user and if a policy was set by the admin to merge the browsing data
    // by default.
    mergeBrowsingDataByDefault =
        !skipBrowsingDataMigration &&
        prefService->GetInteger(
            prefs::kProfileSeparationDataMigrationSettings) ==
            policy::USER_OPT_OUT;
  }
  [_performer
      showManagedConfirmationForHostedDomain:_identityToSignInHostedDomain
                                   userEmail:_identityToSignIn.userEmail
                              viewController:_presentingViewController
                                     browser:_browser
                   skipBrowsingDataMigration:skipBrowsingDataMigration
                  mergeBrowsingDataByDefault:mergeBrowsingDataByDefault
       browsingDataMigrationDisabledByPolicy:
           browsingDataMigrationDisabledByPolicy];
}

// Converts the personal profile to a managed profile, if needed.
- (void)convertPersonalProfileToManagedIfNeededStep {
  if (!_shouldConvertPersonalProfileToManaged) {
    [self continueFlow];
    return;
  }
  [_performer makePersonalProfileManagedWithIdentity:_identityToSignIn];
}

// Switches profile if `_identityToSignIn` is assigned to another profile.
// If `_identityToSignIn` doesn't exist anymore, an error is generated.
// If the identity is assigned to the current profile this step is a no-op.
- (void)switchProfileIfNeededStep {
  CHECK(!_didSwitchProfile);
  CHECK(_unsyncedDataTypes.has_value());
  ProfileIOS* profile = [self originalProfile];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  RecordIOSIdentityAvailableInProfile(_identityToSignIn.gaiaID, identityManager,
                                      profile->GetProfileName());
  if (!AreSeparateProfilesForManagedAccountsEnabled()) {
    [self continueFlow];
    return;
  }
  std::vector<AccountInfo> accountsOnDevice =
      identityManager->GetAccountsOnDevice();
  BOOL isValidIdentityOnDevice = base::Contains(
      accountsOnDevice, GaiaId(_identityToSignIn.gaiaID), &AccountInfo::gaia);
  if (!isValidIdentityOnDevice) {
    // Handle the case where the identity is no longer valid.
    NSError* error = ios::provider::CreateMissingIdentitySigninError();
    [self handleAuthenticationError:error];
    return;
  }
  std::vector<CoreAccountInfo> accountsInProfile =
      identityManager->GetAccountsWithRefreshTokens();
  BOOL isValidIdentityInProfile =
      base::Contains(accountsInProfile, GaiaId(_identityToSignIn.gaiaID),
                     &CoreAccountInfo::gaia);
  if (isValidIdentityInProfile) {
    // If the identity is in the current profile, the flow should continue,
    // without switching profile.
    RecordUnsyncedDataHistogramIfNeeded(
        UnsyncedDataTypeHistogram::kUnsyncedDataOnAccountSwitching,
        _unsyncedDataTypes.value());
    [self continueFlow];
    return;
  }
  RecordUnsyncedDataHistogramIfNeeded(
      UnsyncedDataTypeHistogram::kUnsyncedDataOnProfileSwitching,
      _unsyncedDataTypes.value());
  SceneState* sceneState = _browser->GetSceneState();
  __weak __typeof(self) weakSelf = self;
  OnProfileSwitchCompletion completion = base::BindOnce(
      [](__typeof(self) strong_self, bool success,
         Browser* new_profile_browser) {
        [strong_self onSwitchToProfileWithSuccess:success
                                newProfileBrowser:new_profile_browser];
      },
      weakSelf);
  [_performer switchToProfileWithIdentity:_identityToSignIn
                               sceneState:sceneState
                               completion:std::move(completion)];
}

// Signs out, if the user is already signed in with a different identity.
// Otherwise, this step does nothing and the flow continues to the next step.
- (void)signOutIfNeededStep {
  ProfileIOS* profile = [self originalProfile];
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> currentIdentity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!currentIdentity || [currentIdentity isEqual:_identityToSignIn]) {
    // No need to sign out.
    [self continueFlow];
    return;
  }
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  _accountSwitchingBatchClosureRunner =
      identityManager->StartBatchOfPrimaryAccountChanges();
  [_performer signOutForAccountSwitchWithProfile:profile];
}

// Sets the primary identity for the current profile.
- (void)signInStep {
  ProfileIOS* profile = [self originalProfile];
  id<SystemIdentity> currentIdentity =
      AuthenticationServiceFactory::GetForProfile(profile)->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin);
  if ([currentIdentity isEqual:_identityToSignIn]) {
    // The user is already signed in with the right identity.
    [self continueFlow];
    return;
  }
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  std::vector<CoreAccountInfo> accountsInProfile =
      identityManager->GetAccountsWithRefreshTokens();
  BOOL isValidIdentityInProfile =
      base::Contains(accountsInProfile, GaiaId(_identityToSignIn.gaiaID),
                     &CoreAccountInfo::gaia);
  if (isValidIdentityInProfile) {
    [_performer signInIdentity:_identityToSignIn
                 atAccessPoint:self.accessPoint
                currentProfile:profile];
    _didSignIn = YES;
    [self continueFlow];
  } else {
    // Handle the case where the identity is no longer valid.
    NSError* error = ios::provider::CreateMissingIdentitySigninError();
    [self handleAuthenticationError:error];
  }
}

// Fetches capabilities on successful authentication for the upcoming History
// Sync Opt-In screen.
- (void)fetchCapabilities {
  CHECK([self shouldFetchCapabilities]);
  ProfileIOS* profile = [self originalProfile];

  // Create the capability fetcher and start fetching capabilities.
  __weak __typeof(self) weakSelf = self;
  _capabilitiesFetcher = [[HistorySyncCapabilitiesFetcher alloc]
      initWithIdentityManager:IdentityManagerFactory::GetForProfile(profile)];

  [_capabilitiesFetcher
      startFetchingRestrictionCapabilityWithCallback:base::BindOnce(^(
                                                         signin::Tribool
                                                             capability) {
        // The capability value is ignored.
        [weakSelf continueFlow];
      })];
}

// Runs `_signInCompletion` asynchronously when the flow is successful.
- (void)completeWithSuccessStep {
  DCHECK(_signInCompletion)
      << "`completeSignInWithResult` should not be called twice.";
  _accountSwitchingBatchClosureRunner.RunAndReset();
  signin_metrics::SigninAccountType accountType =
      (_identityToSignInHostedDomain.length > 0)
          ? signin_metrics::SigninAccountType::kManaged
          : signin_metrics::SigninAccountType::kRegular;
  signin_metrics::LogSigninWithAccountType(accountType);
  SigninCompletionCallback signInCompletion = _signInCompletion;
  _signInCompletion = nil;
  signInCompletion(SigninCoordinatorResult::SigninCoordinatorResultSuccess);
  [_performer completePostSignInActions:_postSignInActions
                           withIdentity:_identityToSignIn
                                browser:_browser];
  [self continueFlow];
}

// Runs `_signInCompletion` asynchronously when the flow failed.
- (void)completeWithFailureStep {
  // TODO(crbug.com/375605482): If there was a primary identity at the beginning
  // of the flow, this primary identity should be restored if possible.
  DCHECK(_signInCompletion)
      << "`completeSignInWithResult` should not be called twice.";
  if (_didSignIn) {
    ProfileIOS* profile = [self originalProfile];
    [_performer signOutImmediatelyFromProfile:profile];
  }
  _accountSwitchingBatchClosureRunner.RunAndReset();
  SigninCoordinatorResult result;
  switch (_cancelationReason) {
    case CancelationReason::kFailed:
      result = SigninCoordinatorResult::SigninCoordinatorResultInterrupted;
      break;
    case CancelationReason::kUserCanceled:
      result = SigninCoordinatorResult::SigninCoordinatorResultCanceledByUser;
      break;
    case CancelationReason::kNotCanceled:
      NOTREACHED();
  }
  SigninCompletionCallback signInCompletion = _signInCompletion;
  _signInCompletion = nil;
  signInCompletion(result);
  [self continueFlow];
}

- (BOOL)canceled {
  return _cancelationReason != CancelationReason::kNotCanceled;
}

// Cancels the current sign-in flow.
- (void)cancelFlowWithReason:(CancelationReason)reason {
  CHECK_NE(reason, CancelationReason::kNotCanceled);
  if ([self canceled]) {
    // Avoid double handling of cancel or error.
    return;
  }
  _cancelationReason = reason;
  [self continueFlow];
}

// Handles an authentication error and show an alert to the user.
- (void)handleAuthenticationError:(NSError*)error {
  if ([self canceled]) {
    // Avoid double handling of cancel or error.
    return;
  }
  DCHECK(error);
  _cancelationReason = CancelationReason::kFailed;
  self.handlingError = YES;
  __weak AuthenticationFlow* weakSelf = self;
  [_performer showAuthenticationError:error
                       withCompletion:^{
                         AuthenticationFlow* strongSelf = weakSelf;
                         if (!strongSelf) {
                           return;
                         }
                         [strongSelf setHandlingError:NO];
                         [strongSelf continueFlow];
                       }
                       viewController:_presentingViewController
                              browser:_browser];
}

#pragma mark AuthenticationFlowPerformerDelegate

- (void)didSignOutForAccountSwitch {
  [self continueFlow];
}

- (void)didClearData {
  [self continueFlow];
}

- (void)didFetchUnsyncedDataWithUnsyncedDataTypes:
    (syncer::DataTypeSet)unsyncedDataTypes {
  _unsyncedDataTypes = unsyncedDataTypes;
  [self continueFlow];
}

- (void)didAcceptToLeavePrimaryAccount:(BOOL)acceptToContinue {
  if (acceptToContinue) {
    [self continueFlow];
  } else {
    [self cancelFlowWithReason:CancelationReason::kUserCanceled];
  }
}

- (void)didFetchManagedStatus:(NSString*)hostedDomain {
  DCHECK_EQ(AuthenticationState::kFetchManagedStatus, _state);
  _identityToSignInHostedDomain = hostedDomain;
  [self continueFlow];
}

- (void)didFailFetchManagedStatus:(NSError*)error {
  DCHECK_EQ(AuthenticationState::kFetchManagedStatus, _state);
  NSError* flowError =
      [NSError errorWithDomain:kAuthenticationErrorDomain
                          code:AUTHENTICATION_FLOW_ERROR
                      userInfo:@{
                        NSLocalizedDescriptionKey :
                            l10n_util::GetNSString(IDS_IOS_SIGN_IN_FAILED),
                        NSUnderlyingErrorKey : error
                      }];
  [self handleAuthenticationError:flowError];
}

- (void)didFetchProfileSeparationPolicies:
    (policy::ProfileSeparationDataMigrationSettings)
        profile_separation_data_migration_settings {
  _profileSeparationDataMigrationSettings =
      profile_separation_data_migration_settings;
  [self continueFlow];
}

- (void)didAcceptManagedConfirmation:(BOOL)keepBrowsingDataSeparate {
  if (base::FeatureList::IsEnabled(kIdentityDiscAccountMenu)) {
    // Only show the dialog once per account.
    signin::GaiaIdHash gaiaIDHash =
        signin::GaiaIdHash::FromGaiaId(GaiaId(_identityToSignIn.gaiaID));
    syncer::SetAccountKeyedPrefValue([self prefs],
                                     prefs::kSigninHasAcceptedManagementDialog,
                                     gaiaIDHash, base::Value(true));
  }

  _shouldConvertPersonalProfileToManaged =
      AreSeparateProfilesForManagedAccountsEnabled() &&
      (!keepBrowsingDataSeparate ||
       _accessPoint == signin_metrics::AccessPoint::kStartPage);

  [self continueFlow];
}

- (void)didCancelManagedConfirmation {
  [self cancelFlowWithReason:CancelationReason::kUserCanceled];
}

- (void)didRegisterForUserPolicyWithDMToken:(NSString*)dmToken
                                   clientID:(NSString*)clientID
                         userAffiliationIDs:
                             (NSArray<NSString*>*)userAffiliationIDs {
  DCHECK_EQ(AuthenticationState::kRegisterForUserPolicy, _state);

  _dmToken = dmToken;
  _clientID = clientID;
  _userAffiliationIDs = userAffiliationIDs;
  [self continueFlow];
}

- (void)didFetchUserPolicyWithSuccess:(BOOL)success {
  DCHECK_EQ(AuthenticationState::kFetchUserPolicy, _state);
  DLOG_IF(ERROR, !success) << "Error fetching policy for user";
  [self continueFlow];
}

- (void)didMakePersonalProfileManaged {
  [self continueFlow];
}

#pragma mark - Private methods

// The original profile used for services that don't exist in incognito mode.
- (ProfileIOS*)originalProfile {
  if (!_browser) {
    return nullptr;
  }
  return _browser->GetProfile()->GetOriginalProfile();
}

- (PrefService*)prefs {
  return [self originalProfile]->GetPrefs();
}

// Return YES if capabilities should be fetched for the History Sync screen.
- (BOOL)shouldFetchCapabilities {
  if (!self.precedingHistorySync) {
    return NO;
  }

  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile([self originalProfile]);
  syncer::SyncUserSettings* userSettings = syncService->GetUserSettings();

  if (userSettings->GetSelectedTypes().HasAll(
          {syncer::UserSelectableType::kHistory,
           syncer::UserSelectableType::kTabs})) {
    // History Opt-In is already set and the screen won't be shown.
    return NO;
  }

  return YES;
}

// Called when the profile switching succeeded or failed (according to
// `success`).
- (void)onSwitchToProfileWithSuccess:(BOOL)success
                   newProfileBrowser:(Browser*)newProfileBrowser {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  CHECK(!_didSwitchProfile);
  if (!success) {
    NSError* error = ios::provider::CreateMissingIdentitySigninError();
    [self handleAuthenticationError:error];
    return;
  }
  // TODO(crbug.com/375605482): Need to block user until
  // `AuthenticationFlowInProfile` is done? Probably with a blur animation.
  // With the profile switching `_browser` and `_presentingViewController` are
  // not valid anymore.
  _browser = nullptr;
  _presentingViewController = nil;
  // The sign-in flow is passed to `authenticationFlowInProfile`, with the
  // completion block. `AuthenticationFlowInProfile` retains itself until the
  // sign-in is done. There is no need to own this instance.
  AuthenticationFlowInProfile* authenticationFlowInProfile =
      [[AuthenticationFlowInProfile alloc]
            initWithBrowser:newProfileBrowser
                   identity:_identityToSignIn
          isManagedIdentity:_identityToSignInHostedDomain.length > 0
                accessPoint:_accessPoint
          postSignInActions:self.postSignInActions];
  authenticationFlowInProfile.precedingHistorySync = self.precedingHistorySync;
  [authenticationFlowInProfile startSignInWithCompletion:_signInCompletion];
  _signInCompletion = nil;
  _didSwitchProfile = YES;
  [self continueFlow];
}

#pragma mark - Used for testing

- (void)setPerformerForTesting:(AuthenticationFlowPerformer*)performer {
  _performer = performer;
}

@end
