// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/common/bookmark_features.h"
#import "components/policy/core/browser/signin/profile_separation_policies.h"
#import "components/prefs/pref_service.h"
#import "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/gaia_id_hash.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/account_pref_utils.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_in_profile.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_performer.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_ui_util.h"
#import "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/browser_policy_connector_ios.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
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
  kHandOverToAuthenticationFlowInProfile,
  kCompleteWithFailure,
  kCleanupBeforeDone,
  kDone,
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
                                     const GaiaId& gaia_id,
                                     PrefService* pref_service) {
  bool always_separate_browsing_data_per_policy =
      pref_service->GetInteger(
          prefs::kProfileSeparationDataMigrationSettings) ==
      policy::ALWAYS_SEPARATE;
  return always_separate_browsing_data_per_policy ||
         access_point == signin_metrics::AccessPoint::kStartPage ||
         GetApplicationContext()
             ->GetAccountProfileMapper()
             ->IsProfileForGaiaIDFullyInitialized(gaia_id);
}

// Returns `true` if the browsing data migration is not available because it is
// disabled by policy and not because of another reason.
bool IsBrowsingDataMigrationDisabledByPolicy(
    signin_metrics::AccessPoint access_point,
    const GaiaId& gaia_id,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    policy::ProfileSeparationDataMigrationSettings
        profileSeparationDataMigrationSettings) {
  bool isSignedProfile =
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  return !isSignedProfile &&
         access_point != signin_metrics::AccessPoint::kStartPage &&
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
    const GaiaId& gaia_id,
    signin::IdentityManager* identity_manager,
    std::string_view profile_name) {
  bool is_identity_available_in_profile_mapper = false;
  AccountProfileMapper::IdentityIteratorCallback callback = base::BindRepeating(
      [](BOOL* isIdentityAvailableInProfileMapper, GaiaId signinIdentityGaiaID,
         id<SystemIdentity> identity) {
        *isIdentityAvailableInProfileMapper =
            identity.gaiaId == signinIdentityGaiaID;
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
    const GaiaId& gaia_id,
    signin::IdentityManager* identity_manager,
    std::string_view profile_name) {
  IOSIdentityAvailableInProfile identity_available =
      IdentityAvailableInProfileStatus(gaia_id, identity_manager, profile_name);
  base::UmaHistogramEnumeration(kIOSIdentityAvailableInProfileHistogram,
                                identity_available);
}

// Enum for `Signin.IOSAccountSwitchType` histogram.
// Entries should not be renumbered and numeric values should never be reused.
// LINT.IfChange(IOSAccountSwitchType)
enum class IOSAccountSwitchType : int {
  kPersonalToPersonal = 0,
  kPersonalToManaged = 1,
  kManagedToPersonal = 2,
  kManagedToManaged = 3,
  kMaxValue = kManagedToManaged
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/signin/enums.xml:IOSAccountSwitchType)

IOSAccountSwitchType GetAccountSwitchType(bool originalIdentityWasManaged,
                                          bool newIdentityIsManaged) {
  if (!originalIdentityWasManaged && !newIdentityIsManaged) {
    return IOSAccountSwitchType::kPersonalToPersonal;
  } else if (!originalIdentityWasManaged && newIdentityIsManaged) {
    return IOSAccountSwitchType::kPersonalToManaged;
  } else if (originalIdentityWasManaged && !newIdentityIsManaged) {
    return IOSAccountSwitchType::kManagedToPersonal;
  } else {
    return IOSAccountSwitchType::kManagedToManaged;
  }
}

void RecordAccountSwitchTypeMetric(bool originalIdentityWasManaged,
                                   bool newIdentityIsManaged) {
  base::UmaHistogramEnumeration(
      "Signin.IOSAccountSwitchType",
      GetAccountSwitchType(originalIdentityWasManaged, newIdentityIsManaged));
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

@end

@implementation AuthenticationFlow {
  // View used to display dialogs.
  UIViewController* _presentingViewController;
  // Anchor based on the sign-in button that triggered sign-in.
  // Used to display popover dialog (like the unsynced data confirmation dialog)
  // with a regular window size (like iPad).
  UIView* _anchorView;
  CGRect _anchorRect;
  // One of the method of the delegate, depending on whether a profile switch
  // occurred.
  SigninCompletionCallback _signInInProfileCompletion;
  AuthenticationFlowPerformer* _performer;

  // State machine tracking.
  AuthenticationState _state;
  signin_ui::CancelationReason _cancelationReason;
  // YES if the personal profile should be converted to a managed (work) profile
  // as part of the signin flow. Can only be true if the to-be-signed-in account
  // is managed.
  BOOL _shouldConvertPersonalProfileToManaged;

  // The regular browser of the scene from which the sign-in was started.
  raw_ptr<Browser> _browser;
  id<SystemIdentity> _identityToSignIn;
  signin_metrics::AccessPoint _accessPoint;
  BOOL _precedingHistorySync;
  NSString* _identityToSignInHostedDomain;

  // The browser the user will use after sign-in.
  // It may be incognito if the last time the profile was used, it was in
  // incognito mode.
  raw_ptr<Browser> _browserForAuthenticationFlowInProfile;

  // This AuthenticationFlow keeps a reference to `self` while a sign-in flow is
  // is in progress to ensure it outlives any attempt to destroy it in
  // `self.delegate`’s method.
  AuthenticationFlow* _selfRetainer;

  // Value of the ProfileSeparationDataMigrationSettings for
  // `_identityToSignin`. This is used to know if the user can convert an
  // existing profile into a managed profile.
  policy::ProfileSeparationDataMigrationSettings
      _profileSeparationDataMigrationSettings;

  // List of unsynced data types in the current profile. If there is no primary
  // account the set is empty.
  // The value is set during `kCheckUnsyncedData` step.
  std::optional<syncer::DataTypeSet> _unsyncedDataTypes;

  // For metrics: Whether there was a managed primary account at the beginning
  // of the flow. Set to nullopt if there was no primary account at all.
  std::optional<bool> _wasPrimaryAccountManaged;

  // The actions to perform following account sign-in.
  PostSignInActionSet _postSignInActions;
}

@synthesize handlingError = _handlingError;
@synthesize identity = _identityToSignIn;

#pragma mark - Public methods

- (instancetype)initWithBrowser:(Browser*)browser
                       identity:(id<SystemIdentity>)identity
                    accessPoint:(signin_metrics::AccessPoint)accessPoint
           precedingHistorySync:(BOOL)precedingHistorySync
              postSignInActions:(PostSignInActionSet)postSignInActions
       presentingViewController:(UIViewController*)presentingViewController
                     anchorView:(UIView*)anchorView
                     anchorRect:(CGRect)anchorRect {
  if ((self = [super init])) {
    CHECK(browser);
    // Sign-in related work should be done on regular browser.
    CHECK_EQ(browser->type(), Browser::Type::kRegular,
             base::NotFatalUntil::M145);
    CHECK(presentingViewController, base::NotFatalUntil::M145);
    CHECK(identity, base::NotFatalUntil::M142);
    _browser = browser;

    _identityToSignIn = identity;
    _accessPoint = accessPoint;
    _precedingHistorySync = precedingHistorySync;
    _postSignInActions = postSignInActions;
    _presentingViewController = presentingViewController;
    _anchorView = anchorView;
    _anchorRect = anchorRect;
    _state = AuthenticationState::kBegin;
    _cancelationReason = signin_ui::CancelationReason::kNotCanceled;
    _profileSeparationDataMigrationSettings =
        policy::ProfileSeparationDataMigrationSettings::USER_OPT_IN;

    ProfileIOS* profile = [self profile];
    AuthenticationService* authenticationService =
        AuthenticationServiceFactory::GetForProfile(profile);
    id<SystemIdentity> current_primary_identity =
        authenticationService->GetPrimaryIdentity(
            signin::ConsentLevel::kSignin);
    // The user should not be allowed to sign-in to the current primary
    // identity.
    CHECK(![current_primary_identity isEqual:identity],
          base::NotFatalUntil::M142);
    if (current_primary_identity) {
      _wasPrimaryAccountManaged =
          authenticationService->HasPrimaryIdentityManaged(
              signin::ConsentLevel::kSignin);
    }
  }
  return self;
}

- (void)startSignIn {
  DCHECK_EQ(AuthenticationState::kBegin, _state);
  _selfRetainer = self;
  // Kick off the state machine.
  id<ChangeProfileCommands> changeProfileHandler = HandlerForProtocol(
      _browser->GetSceneState().profileState.appState.appCommandDispatcher,
      ChangeProfileCommands);
  _performer = [[AuthenticationFlowPerformer alloc]
          initWithDelegate:self
      changeProfileHandler:changeProfileHandler];

  // Make sure -[AuthenticationFlow startSignIn] doesn't call
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
  [_performer interrupt];
  if (_state != AuthenticationState::kDone) {
    // The performer might not have been able to continue the flow if it was
    // waiting for a callback (e.g. waiting for AccountReconcilor). In this
    // case, we force the flow to finish synchronously.
    [self cancelFlowWithReason:signin_ui::CancelationReason::kFailed];
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
    case AuthenticationState::kHandOverToAuthenticationFlowInProfile:
      return AuthenticationState::kCompleteWithFailure;
    case AuthenticationState::kCompleteWithFailure:
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
      return AuthenticationState::kHandOverToAuthenticationFlowInProfile;
    case AuthenticationState::kHandOverToAuthenticationFlowInProfile:
      return AuthenticationState::kCleanupBeforeDone;
    case AuthenticationState::kCompleteWithFailure:
      return AuthenticationState::kCleanupBeforeDone;
    case AuthenticationState::kCleanupBeforeDone:
    case AuthenticationState::kDone:
      return AuthenticationState::kDone;
  }
}

// Continues the sign-in state machine starting from `_state` and invokes
// a `self.delegate`’s method when finished.
- (void)continueFlow {
  ProfileIOS* profile = [self profile];
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

    case AuthenticationState::kHandOverToAuthenticationFlowInProfile:
      [self handOverToAuthenticationFlowInProfileStep];
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
    case AuthenticationState::kDone: {
      [self doneStep];
      return;
    }
  }
  NOTREACHED();
}

- (void)checkUnsyncedDataStep {
  ProfileIOS* profile = [self profile];
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  id<SystemIdentity> currentIdentity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  // AuthenticationFlow should not switch to the same identity.
  CHECK(![currentIdentity isEqual:_identityToSignIn],
        base::NotFatalUntil::M145);
  if (!currentIdentity) {
    _unsyncedDataTypes = syncer::DataTypeSet();
    [self continueFlow];
    return;
  }
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile([self profile]);
  [_performer fetchUnsyncedDataWithSyncService:syncService];
}

- (void)showLeavingPrimaryAccountConfirmationIfNeededStep {
  CHECK(_unsyncedDataTypes.has_value(), base::NotFatalUntil::M140);
  ProfileIOS* profile = [self profile];
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  PrefService* profilePrefService = profile->GetPrefs();
  SignedInUserState signedInUserState = GetSignedInUserState(
      authenticationService, identityManager, profilePrefService);
  if (!ForceLeavingPrimaryAccountConfirmationDialog(signedInUserState,
                                                    profile) &&
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
          _identityToSignInHostedDomain, _accessPoint, _identityToSignIn.gaiaId,
          [self prefs])) {
    // The managed confirmation dialog can be skipped, therefore, there is no
    // need to fetch the policy.
    [self continueFlow];
    return;
  }
  if (!AreSeparateProfilesForManagedAccountsEnabled() ||
      ShouldSkipBrowsingDataMigration(_accessPoint, _identityToSignIn.gaiaId,
                                      [self prefs])) {
    // The profile-separation policy affects whether browsing-data-migration
    // is offered, so it's only needed if the migration isn't skipped.
    [self continueFlow];
    return;
  }

  ProfileIOS* profile = [self profile];
  [_performer fetchProfileSeparationPolicies:profile
                                 forIdentity:_identityToSignIn];
}

// Shows a confirmation dialog for signing in to an account managed.
- (void)showManagedConfirmationIfNeededStep {
  if (!ShouldShowManagedConfirmationForHostedDomain(
          _identityToSignInHostedDomain, _accessPoint, _identityToSignIn.gaiaId,
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
        ShouldSkipBrowsingDataMigration(_accessPoint, _identityToSignIn.gaiaId,
                                        prefService);

    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile([self profile]);

    browsingDataMigrationDisabledByPolicy =
        IsBrowsingDataMigrationDisabledByPolicy(
            _accessPoint, _identityToSignIn.gaiaId, prefService,
            identityManager, _profileSeparationDataMigrationSettings);

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
                                    identity:_identityToSignIn
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
  CHECK(_unsyncedDataTypes.has_value());
  ProfileIOS* profile = [self profile];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  RecordIOSIdentityAvailableInProfile(_identityToSignIn.gaiaId, identityManager,
                                      profile->GetProfileName());
  std::vector<AccountInfo> accountsOnDevice =
      identityManager->GetAccountsOnDevice();
  BOOL isValidIdentityOnDevice = base::Contains(
      accountsOnDevice, _identityToSignIn.gaiaId, &AccountInfo::gaia);
  std::vector<CoreAccountInfo> accountsInProfile =
      identityManager->GetAccountsWithRefreshTokens();
  BOOL isValidIdentityInCurrentProfile = base::Contains(
      accountsInProfile, _identityToSignIn.gaiaId, &CoreAccountInfo::gaia);
  if (!isValidIdentityOnDevice ||
      (!isValidIdentityInCurrentProfile &&
       !AreSeparateProfilesForManagedAccountsEnabled())) {
    // Handle the case where the identity is no longer valid.
    NSError* error = ios::provider::CreateMissingIdentitySigninError();
    [self handleAuthenticationError:error];
    return;
  }
  if (isValidIdentityInCurrentProfile) {
    // If the identity is in the current profile, the flow should continue,
    // without switching profile.
    RecordUnsyncedDataHistogramIfNeeded(
        UnsyncedDataTypeHistogram::kUnsyncedDataOnAccountSwitching,
        _unsyncedDataTypes.value());
    _browserForAuthenticationFlowInProfile = _browser;
    CHECK(!_signInInProfileCompletion);
    PostSignInActionSet postSignInActions = _postSignInActions;
    id<SystemIdentity> identityToSignIn = _identityToSignIn;
    signin_metrics::AccessPoint accessPoint = _accessPoint;
    // In case of sign-in in same profile, we can reuse the same browser.
    base::WeakPtr<Browser> weakBrowser = _browser->AsWeakPtr();
    // In case of same profile signin, the delegate simply allows
    // to update the view that started the authentication. If it gets
    // deallocated, it means the view is closed, so it’s acceptable
    // not to call its method.
    __weak id<AuthenticationFlowDelegate> delegate = [self takeDelegate];
    // Not using a call to a method on self, because self will be
    // deallocated by the time the `signinCompletion` is executed.
    _signInInProfileCompletion = ^(
        signin_ui::CancelationReason cancelationReason) {
      [delegate
          authenticationFlowDidSignInInSameProfileWithCancelationReason:
              cancelationReason
                                                               identity:
                                                                   identityToSignIn];
      if (Browser* browser = weakBrowser.get()) {
        CompletePostSignInActions(postSignInActions, identityToSignIn, browser,
                                  accessPoint);
      }
    };
    [self continueFlow];
    return;
  }
  BOOL isValidIdentityInSomeProfile =
      GetApplicationContext()
          ->GetAccountProfileMapper()
          ->FindProfileNameForGaiaID(_identityToSignIn.gaiaId)
          .has_value();
  if (!isValidIdentityInSomeProfile) {
    __weak __typeof(self) weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](__typeof(self) weakSelf) {
                         [weakSelf didFailToSwitchToProfile];
                       },
                       weakSelf));
    return;
  }

  RecordUnsyncedDataHistogramIfNeeded(
      UnsyncedDataTypeHistogram::kUnsyncedDataOnProfileSwitching,
      _unsyncedDataTypes.value());
  SceneState* sceneState = _browser->GetSceneState();
  // Determine the reason for the profile switch. In general, AuthenticationFlow
  // handles cases where the user has chosen a new account to use. This can be
  // either a signin or a change-account:
  // * If the *current* profile (pre-switch) has a primary account, then the
  //   profile switch must be due to an account change.
  // * Otherwise, it must be due to a signin with a managed account (because a
  //   signin with a non-managed account wouldn't cause a profile switch - that
  //   case is handled in the `isValidIdentityInCurrentProfile` check above).
  ChangeProfileReason reason =
      identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)
          ? ChangeProfileReason::kSwitchAccounts
          : ChangeProfileReason::kManagedAccountSignIn;

  // Calling switchToProfileWithIdentity will shutdown the BrowserViewWrangler
  // and clear the browser.
  _browser = nullptr;
  _browserForAuthenticationFlowInProfile = nullptr;
  _presentingViewController = nil;

  [_performer switchToProfileWithIdentity:_identityToSignIn
                               sceneState:sceneState
                                   reason:reason
                                 delegate:[self takeDelegate]
                        postSignInActions:_postSignInActions
                              accessPoint:_accessPoint];
}

// Hands the sign-in flow over to `AuthenticationFlowInProfile`. This step is
// only reached when the identity is in the current profile - either because it
// was there in the first place, or `AuthenticationFlow` has switched to the
// appropriate profile.
- (void)handOverToAuthenticationFlowInProfileStep {
  CHECK(_browserForAuthenticationFlowInProfile);
  BOOL isManagedIdentity = _identityToSignInHostedDomain.length > 0;
  // If `_wasPrimaryAccountManaged` is unset, then this was a signin, not an
  // account switch.
  if (_wasPrimaryAccountManaged.has_value()) {
    if (signin::ActivePrimaryAccountsMetricsRecorder* metricsRecorder =
            GetApplicationContext()
                ->GetActivePrimaryAccountsMetricsRecorder()) {
      metricsRecorder->AccountWasSwitched();
    }
    RecordAccountSwitchTypeMetric(_wasPrimaryAccountManaged.value(),
                                  isManagedIdentity);
  }

  // The sign-in flow is passed to `authenticationFlowInProfile`.
  // `AuthenticationFlowInProfile` retains itself until the sign-in is done.
  // There is no need to own this instance.
  AuthenticationFlowInProfile* authenticationFlowInProfile =
      [[AuthenticationFlowInProfile alloc]
               initWithBrowser:_browserForAuthenticationFlowInProfile
                      identity:_identityToSignIn
             isManagedIdentity:isManagedIdentity
                   accessPoint:_accessPoint
          precedingHistorySync:_precedingHistorySync
             postSignInActions:_postSignInActions];

  [authenticationFlowInProfile
      startSignInWithCompletion:_signInInProfileCompletion];
  _signInInProfileCompletion = nil;
  [self continueFlow];
}

// Runs `[self.delegate
// authenticationFlowDidSignInInSameProfile:withResult:]` synchronously when the
// flow failed.
- (void)completeWithFailureStep {
  CHECK_NE(_cancelationReason, signin_ui::CancelationReason::kNotCanceled);
  [[self takeDelegate]
      authenticationFlowDidSignInInSameProfileWithCancelationReason:
          _cancelationReason
                                                           identity:nil];
  [self continueFlow];
}

- (void)doneStep {
  _presentingViewController = nil;
  _anchorView = nil;
  _signInInProfileCompletion = nil;
  _performer = nil;
  _browser = nullptr;
  _identityToSignIn = nil;
  _identityToSignInHostedDomain = nil;
  _browserForAuthenticationFlowInProfile = nullptr;
}

- (BOOL)canceled {
  return _cancelationReason != signin_ui::CancelationReason::kNotCanceled;
}

// Cancels the current sign-in flow.
- (void)cancelFlowWithReason:(signin_ui::CancelationReason)reason {
  CHECK_NE(reason, signin_ui::CancelationReason::kNotCanceled);
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
  CHECK(_browser, base::NotFatalUntil::M150);
  _cancelationReason = signin_ui::CancelationReason::kFailed;
  self.handlingError = YES;
  __weak AuthenticationFlow* weakSelf = self;
  [_performer showAuthenticationError:error
                       withCompletion:^{
                         [weakSelf authenticationErrorDismissed];
                       }
                       viewController:_presentingViewController
                              browser:_browser];
}

#pragma mark AuthenticationFlowPerformerDelegate

- (void)didFetchUnsyncedDataWithUnsyncedDataTypes:
    (syncer::DataTypeSet)unsyncedDataTypes {
  _unsyncedDataTypes = unsyncedDataTypes;
  [self continueFlow];
}

- (void)didAcceptToLeavePrimaryAccount:(BOOL)acceptToContinue {
  if (acceptToContinue) {
    [self continueFlow];
  } else {
    [self cancelFlowWithReason:signin_ui::CancelationReason::kUserCanceled];
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

- (void)didAcceptManagedConfirmationWithBrowsingDataSeparate:
    (BOOL)browsingDataSeparate {
  // Only show the dialog once per account.
  signin::GaiaIdHash gaiaIDHash =
      signin::GaiaIdHash::FromGaiaId(_identityToSignIn.gaiaId);
  syncer::SetAccountKeyedPrefValue([self prefs],
                                   prefs::kSigninHasAcceptedManagementDialog,
                                   gaiaIDHash, base::Value(true));

  _shouldConvertPersonalProfileToManaged =
      AreSeparateProfilesForManagedAccountsEnabled() &&
      (!browsingDataSeparate ||
       _accessPoint == signin_metrics::AccessPoint::kStartPage);

  // When we show the managed profile screen, the profile is a new one, ensure
  // the history sync screen is shown then in case a separate profile is
  // created because the caller who would show the history sync screen gets
  // destroyed.
  // TODO(crbug.com/403183877): Make sure that only one entity is responsible
  // for showing the sync screen.
  if (AreSeparateProfilesForManagedAccountsEnabled() &&
      !_shouldConvertPersonalProfileToManaged) {
    // Note that the history sync screen may not be displayed for any reason
    // considered in `GetSkipReason`.
    _postSignInActions.Put(
        PostSignInAction::kShowHistorySyncScreenAfterProfileSwitch);
  }

  [self continueFlow];
}

- (void)didCancelManagedConfirmation {
  [self cancelFlowWithReason:signin_ui::CancelationReason::kUserCanceled];
}

- (void)didFailToSwitchToProfile {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  NSError* error = ios::provider::CreateMissingIdentitySigninError();
  [self handleAuthenticationError:error];
}

- (void)didSwitchToProfileWithNewProfileBrowser:(Browser*)newProfileBrowser
                                     completion:(base::OnceClosure)completion {
  CHECK(AreSeparateProfilesForManagedAccountsEnabled());
  CHECK(completion);
  CHECK(newProfileBrowser);
  // Sign-in related work should be done on regular browser.
  CHECK_EQ(newProfileBrowser->type(), Browser::Type::kRegular,
           base::NotFatalUntil::M145);
  // With the profile switching `_browser` and `_presentingViewController` are
  // not valid anymore.
  CHECK(!_browser, base::NotFatalUntil::M145);
  CHECK(!_presentingViewController, base::NotFatalUntil::M145);
  CHECK(!_browserForAuthenticationFlowInProfile, base::NotFatalUntil::M145);
  _browserForAuthenticationFlowInProfile = newProfileBrowser;
  CHECK(!_signInInProfileCompletion);
  _signInInProfileCompletion = base::CallbackToBlock(
      base::IgnoreArgs<signin_ui::CancelationReason>(std::move(completion)));

  [self continueFlow];
}

- (void)didMakePersonalProfileManaged {
  [self continueFlow];
}

#pragma mark - Private methods

- (void)authenticationErrorDismissed {
  [self setHandlingError:NO];
  [self continueFlow];
}

// Returns the delegate at most once.
- (id<AuthenticationFlowDelegate>)takeDelegate {
  id<AuthenticationFlowDelegate> delegate = self.delegate;
  self.delegate = nil;
  return delegate;
}

// The original profile used for services that don't exist in incognito mode. Or
// nullptr if there is no _browser.
- (ProfileIOS*)profile {
  if (!_browser) {
    return nullptr;
  }
  return _browser->GetProfile();
}

- (PrefService*)prefs {
  return [self profile]->GetPrefs();
}

@end
