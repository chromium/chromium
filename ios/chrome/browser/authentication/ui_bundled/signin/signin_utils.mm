// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"

#import "base/barrier_closure.h"
#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/version.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/policy/policy_constants.h"
#import "components/prefs/pref_service.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_settings_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_signout_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/history_sync/history_sync_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/features.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/model/profile/profile_attributes_storage_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "net/base/network_change_notifier.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace {

// Initiate synchronously the change to `profile`, then run `continuation`
// when the change completes asynchronously. The UI (thus `scene_state`)
// will be destroyed synchronously, so this function should not be called
// directly, instead it should be posted as a task.
//
// Destroying the UI will destroy the SceneState, the SceneController and
// the Browser. As the SceneState is an Objective-C class and the Browser
// is a C++ class, this method take a SceneState* as parameter to avoid
// risking accessing a dangling pointer to a C++ object.
void SwitchToProfileSynchronously(const std::string& profile_name,
                                  __weak SceneState* weak_scene_state,
                                  ChangeProfileReason reason,
                                  ChangeProfileContinuation continuation) {
  if (SceneState* scene_state = weak_scene_state) {
    id<ChangeProfileCommands> change_profile_handler = HandlerForProtocol(
        scene_state.profileState.appState.appCommandDispatcher,
        ChangeProfileCommands);

    [change_profile_handler changeProfile:profile_name
                                 forScene:scene_state
                                   reason:reason
                             continuation:std::move(continuation)];
  }
}

// Maximum delay to wait for fetching the account capabilities before showing
// the sign-in upgrade promo. If fetching the account capabilities takes more
// than the delay, then the promo is suppressed - it may be shown on the next
// start-up.
constexpr base::TimeDelta kShowSigninUpgradePromoMaxDelay =
    base::Milliseconds(200);

// Converts an array of AccountInfos to a set of gaia ids.
NSSet<NSString*>* GaiaIdSetWithAccountInfos(
    const std::vector<AccountInfo>& account_infos) {
  NSMutableSet* gaia_id_set = [NSMutableSet set];
  for (const AccountInfo& account_info : account_infos) {
    [gaia_id_set addObject:account_info.gaia.ToNSString()];
  }
  return [gaia_id_set copy];
}

// Returns whether the gaia ids `recorded_gaia_ids` is a strict subset of the
// current `identities_on_device_gaia_ids` (i.e. all the recorded gaia IDs are
// (still) on the device, but there is at least one new identity on the device).
bool IsStrictSubset(NSArray<NSString*>* recorded_gaia_ids,
                    NSSet<NSString*>* identities_on_device_gaia_ids) {
  // Optimisation for the case of a nil or empty `recorded_gaia_ids`.
  // This allow not special casing the construction of the NSSet (as
  // -[NSSet setWithArray:] does not support nil for the array).
  if (recorded_gaia_ids.count == 0) {
    return identities_on_device_gaia_ids.count > 0;
  }

  NSSet<NSString*>* recorded_gaia_ids_set =
      [NSSet setWithArray:recorded_gaia_ids];
  return [recorded_gaia_ids_set isSubsetOfSet:identities_on_device_gaia_ids] &&
         ![recorded_gaia_ids_set isEqualToSet:identities_on_device_gaia_ids];
}

// Returns true if profile separation is enabled and the current profile is not
// the personal one (a managed profile).
bool ShouldSwitchProfileAtSignout(AuthenticationService* authentication_service,
                                  const std::string& profile_name) {
  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();
  bool is_work_profile = profile_manager->GetProfileAttributesStorage()
                             ->GetPersonalProfileName() != profile_name;
  return AreSeparateProfilesForManagedAccountsEnabled() &&
         authentication_service->HasPrimaryIdentityManaged(
             signin::ConsentLevel::kSignin) &&
         is_work_profile;
}

// Post an asynchronous request to switch to `profile`, running `continuation`
// when the change completes.
void SwitchToProfile(Browser* browser,
                     const std::string& profile_name,
                     ChangeProfileReason reason,
                     ChangeProfileContinuation continuation) {
  __weak SceneState* weak_scene_state = browser->GetSceneState();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SwitchToProfileSynchronously, profile_name,
                     weak_scene_state, reason, std::move(continuation)));
}

// Post an asynchronous request to switch from a managed profile to the
// personal profile, running `continuation` when the change completes.
void SwitchToPersonalProfile(Browser* browser,
                             ChangeProfileReason reason,
                             ChangeProfileContinuation continuation) {
  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();
  std::string personal_profile_name =
      profile_manager->GetProfileAttributesStorage()->GetPersonalProfileName();
  CHECK(profile_manager->HasProfileWithName(personal_profile_name));

  SwitchToProfile(browser, personal_profile_name, reason,
                  std::move(continuation));
}

syncer::DataTypeSet DataCountsMapToDataTypeSet(
    absl::flat_hash_map<syncer::DataType, size_t> type_counts) {
  syncer::DataTypeSet types;
  for (const auto& [type, count] : type_counts) {
    types.Put(type);
  }
  return types;
}

}  // namespace

#pragma mark - Public

namespace signin {

base::TimeDelta GetWaitThresholdForCapabilities() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          signin::kWaitThresholdMillisecondsForCapabilitiesApi)) {
    std::string delayString = command_line->GetSwitchValueASCII(
        signin::kWaitThresholdMillisecondsForCapabilitiesApi);
    int commandLineDelay = 0;
    if (base::StringToInt(delayString, &commandLineDelay)) {
      return base::Milliseconds(commandLineDelay);
    }
  }
  return kShowSigninUpgradePromoMaxDelay;
}

bool ShouldPresentUserSigninUpgrade(ProfileIOS* profile,
                                    const base::Version& current_version) {
  DCHECK(profile);
  DCHECK(current_version.IsValid());

  if (tests_hook::DisableUpgradeSigninPromo()) {
    return false;
  }

  if (profile->IsOffTheRecord()) {
    return false;
  }

  // There will be an error shown if the user chooses to sign in or select
  // another account while offline.
  if (net::NetworkChangeNotifier::IsOffline()) {
    return false;
  }

  // Sign-in can be disabled by policy or through user Settings.
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  switch (authentication_service->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
      return false;
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed:
      break;
  }

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    syncer::SyncService* sync_service =
        SyncServiceFactory::GetForProfile(profile);
    switch (history_sync::GetSkipReason(sync_service, auth_service,
                                        profile->GetPrefs(), YES)) {
      case history_sync::HistorySyncSkipReason::kNone:
        // Need to show the upgrade promo, to show the history sync opt-in.
        break;
      case history_sync::HistorySyncSkipReason::kNotSignedIn:
        NOTREACHED();
      case history_sync::HistorySyncSkipReason::kAlreadyOptedIn:
      case history_sync::HistorySyncSkipReason::kSyncForbiddenByPolicies:
      case history_sync::HistorySyncSkipReason::kDeclinedTooOften:
        return false;
    }
  }

  // Avoid showing the upgrade sign-in promo when the device restore sign-in
  // promo should be shown instead.
  if (GetPreRestoreIdentity(profile->GetPrefs()).has_value()) {
    return false;
  }

  // Don't show the promo if there are no identities. This should be tested
  // before ForceStartupSigninPromo() to avoid any DCHECK failures if
  // ForceStartupSigninPromo() returns true.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  NSSet<NSString*>* identities_on_device_gaia_ids =
      GaiaIdSetWithAccountInfos(identity_manager->GetAccountsOnDevice());
  if (identities_on_device_gaia_ids.count == 0) {
    return false;
  }

  // Used for testing purposes only.
  if (signin::ForceStartupSigninPromo() ||
      experimental_flags::AlwaysDisplayUpgradePromo()) {
    return true;
  }

  // Show the promo at most every two major versions.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* version_string =
      [defaults stringForKey:kDisplayedSSORecallForMajorVersionKey];
  const base::Version version_shown(base::SysNSStringToUTF8(version_string));

  // If the version was not set, we need to set it in order to wait 2 major
  // releases to show the sign-in promo.
  if (!version_shown.IsValid()) {
    [defaults setObject:base::SysUTF8ToNSString(current_version.GetString())
                 forKey:kDisplayedSSORecallForMajorVersionKey];
    return false;
  }

  // Wait 2 major releases to show the sign-in promo.
  if (current_version.components()[0] - version_shown.components()[0] < 2) {
    return false;
  }

  // TODO(crbug.com/416634715): Replace user defaults interaction count with FET
  // event count. The sign-in promo should be shown twice, even if no account
  // has been added.
  NSInteger display_count =
      [defaults integerForKey:kSigninPromoViewDisplayCountKey];
  if (display_count <= 1) {
    return true;
  }

  // Otherwise, it can be shown only if a new account has been added.
  NSArray<NSString*>* last_known_gaia_id_list =
      [defaults arrayForKey:kLastShownAccountGaiaIdVersionKey];
  return IsStrictSubset(last_known_gaia_id_list, identities_on_device_gaia_ids);
}

bool ShouldPresentWebSignin(ProfileIOS* profile) {
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (authentication_service->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // For some reasons, Gaia might ask for the web sign-in while the user is
    // already signed in. It might be a race conditions with a token already
    // disabled on Gaia, and Chrome not aware of it yet?
    // To avoid a crash (hitting CHECK() to sign-in while already being signed
    // in), we need to skip the web sign-in dialog.
    // Related to crbug.com/1308448.
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::
            SUPPRESSED_ALREADY_SIGNED_IN,
        signin_metrics::AccessPoint::kWebSignin);
    return false;
  }
  signin_metrics::AccessPoint web_signin_access_point =
      signin_metrics::AccessPoint::kWebSignin;
  // Skip the bottom sheet sign-in dialog if the user cannot sign-in.
  switch (authentication_service->GetServiceStatus()) {
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByUser:
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
      RecordConsistencyPromoUserAction(
          signin_metrics::AccountConsistencyPromoAction::
              SUPPRESSED_SIGNIN_NOT_ALLOWED,
          web_signin_access_point);
      return false;
    case AuthenticationService::ServiceStatus::SigninAllowed:
      break;
  }
  // Show the sign-in dialog less than `kSigninWebSignDismissalCount` times.
  PrefService* user_pref_service = profile->GetPrefs();
  const int current_dismissal_count =
      user_pref_service->GetInteger(prefs::kSigninWebSignDismissalCount);
  if (current_dismissal_count >= kDefaultWebSignInDismissalCount) {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::
            SUPPRESSED_CONSECUTIVE_DISMISSALS,
        web_signin_access_point);
    return false;
  }
  return true;
}

void RecordUpgradePromoSigninStarted(
    signin::IdentityManager* identity_manager,
    ChromeAccountManagerService* account_manager_service,
    const base::Version& current_version) {
  DCHECK(identity_manager);
  DCHECK(account_manager_service);
  DCHECK(current_version.IsValid());

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:base::SysUTF8ToNSString(current_version.GetString())
               forKey:kDisplayedSSORecallForMajorVersionKey];
  std::vector<AccountInfo> account_infos =
      identity_manager->GetAccountsOnDevice();
  NSSet<NSString*>* gaia_id_on_device_set =
      GaiaIdSetWithAccountInfos(account_infos);
  [defaults setObject:gaia_id_on_device_set.allObjects
               forKey:kLastShownAccountGaiaIdVersionKey];
  NSInteger display_count =
      [defaults integerForKey:kSigninPromoViewDisplayCountKey];
  ++display_count;
  [defaults setInteger:display_count forKey:kSigninPromoViewDisplayCountKey];
}

Tribool TriboolFromCapabilityResult(SystemIdentityCapabilityResult result) {
  switch (result) {
    case SystemIdentityCapabilityResult::kTrue:
      return Tribool::kTrue;
    case SystemIdentityCapabilityResult::kFalse:
      return Tribool::kFalse;
    case SystemIdentityCapabilityResult::kUnknown:
      return Tribool::kUnknown;
  }
  NOTREACHED();
}

NSArray<id<SystemIdentity>>* GetIdentitiesOnDevice(
    signin::IdentityManager* identityManager,
    ChromeAccountManagerService* accountManagerService) {
  std::vector<AccountInfo> accountInfos =
      identityManager->GetAccountsOnDevice();
  return accountManagerService->GetIdentitiesOnDeviceWithGaiaIDs(accountInfos);
}

NSArray<id<SystemIdentity>>* GetIdentitiesOnDevice(ProfileIOS* profile) {
  return GetIdentitiesOnDevice(
      IdentityManagerFactory::GetForProfile(profile),
      ChromeAccountManagerServiceFactory::GetForProfile(profile));
}

id<SystemIdentity> GetDefaultIdentityOnDevice(
    signin::IdentityManager* identityManager,
    ChromeAccountManagerService* accountManagerService) {
  NSArray<id<SystemIdentity>>* identitiesOnDevice =
      GetIdentitiesOnDevice(identityManager, accountManagerService);
  return [identitiesOnDevice firstObject];
}

id<SystemIdentity> GetDefaultIdentityOnDevice(ProfileIOS* profile) {
  return GetDefaultIdentityOnDevice(
      IdentityManagerFactory::GetForProfile(profile),
      ChromeAccountManagerServiceFactory::GetForProfile(profile));
}

ProfileSignoutRequest::ProfileSignoutRequest(
    signin_metrics::ProfileSignout source)
    : source_(source),
      prepare_callback_(base::DoNothing()),
      completion_callback_(base::DoNothing()) {}

ProfileSignoutRequest::~ProfileSignoutRequest() {
  CHECK(run_has_been_called_);
}

ProfileSignoutRequest&& ProfileSignoutRequest::SetSnackbarMessage(
    MDCSnackbarMessage* snackbar_message,
    bool force_snackbar_over_toolbar) && {
  CHECK(!run_has_been_called_);
  snackbar_message_ = snackbar_message;
  force_snackbar_over_toolbar_ = force_snackbar_over_toolbar;
  return std::move(*this);
}

ProfileSignoutRequest&& ProfileSignoutRequest::SetPrepareCallback(
    PrepareCallback prepare_callback) && {
  CHECK(!run_has_been_called_);
  CHECK(!prepare_callback.is_null());
  prepare_callback_ = std::move(prepare_callback);
  return std::move(*this);
}

ProfileSignoutRequest&& ProfileSignoutRequest::SetCompletionCallback(
    CompletionCallback completion_callback) && {
  CHECK(!run_has_been_called_);
  CHECK(!completion_callback.is_null());
  completion_callback_ = std::move(completion_callback);
  return std::move(*this);
}

ProfileSignoutRequest&& ProfileSignoutRequest::SetShouldRecordMetrics(
    bool value) && {
  CHECK(!run_has_been_called_);
  should_record_metrics_ = value;
  return std::move(*this);
}

void ProfileSignoutRequest::Run(Browser* browser) && {
  CHECK(!run_has_been_called_);
  run_has_been_called_ = true;

  // The regular browser should be used to execute the signout.
  CHECK_EQ(browser->type(), Browser::Type::kRegular);
  SceneState* scene_state = browser->GetSceneState();

  ChangeProfileContinuation continuation =
      CreateChangeProfileSignoutContinuation(
          source_, force_snackbar_over_toolbar_, should_record_metrics_,
          snackbar_message_, std::move(completion_callback_));
  ProfileIOS* profile = browser->GetProfile();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile);

  if (source_ == signin_metrics::ProfileSignout::kPrefChanged) {
    ChangeProfileContinuation postSignoutContinuation =
        CreateChangeProfileForceSignoutContinuation();
    continuation = ChainChangeProfileContinuations(
        std::move(continuation), std::move(postSignoutContinuation));
  }

  if (!ShouldSwitchProfileAtSignout(authentication_service,
                                    profile->GetProfileName())) {
    std::move(prepare_callback_).Run(/*will_change_profile=*/false);
    std::move(continuation).Run(scene_state, base::DoNothing());
    return;
  }

  if (source_ == signin_metrics::ProfileSignout::kUserClickedSignoutSettings) {
    ChangeProfileContinuation postSignoutContinuation =
        CreateChangeProfileSettingsContinuation();
    continuation = ChainChangeProfileContinuations(
        std::move(continuation), std::move(postSignoutContinuation));
  }

  std::move(prepare_callback_).Run(/*will_change_profile=*/true);
  SwitchToPersonalProfile(browser, ChangeProfileReason::kManagedAccountSignOut,
                          std::move(continuation));
}

void MultiProfileSignOutForProfile(
    ProfileIOS* profile,
    signin_metrics::ProfileSignout signout_source,
    base::OnceClosure signout_completion_closure) {
  // Simply sign out if no profile switching is needed.
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  if (!ShouldSwitchProfileAtSignout(authentication_service,
                                    profile->GetProfileName())) {
    authentication_service->SignOut(
        signout_source,
        base::CallbackToBlock(std::move(signout_completion_closure)));
    return;
  }

  // The API to change a profile work on a `SceneState`. Each `SceneState` has a
  // regular, inactive and incognito browser associated.
  // All three Browser points to the same `SceneState`, so this code only need
  // to consider the regular Browser.
  auto browser_list =
      BrowserListFactory::GetForProfile(profile)->BrowsersOfType(
          BrowserList::BrowserType::kRegular);

  // Only call `signout_completion_closure` after all browsers have switched to
  // the personal profile.
  base::RepeatingClosure barrier = base::BarrierClosure(
      browser_list.size(), std::move(signout_completion_closure));

  // Sign the user out in all browsers.
  for (Browser* browser : browser_list) {
    ChangeProfileContinuation continuation =
        CreateChangeProfileSignoutContinuation(
            signout_source, /*force_snackbar_over_toolbar=*/false,
            /*should_record_metrics=*/false, /*snackbar_message =*/nil,
            base::IgnoreArgs<SceneState*>(barrier));
    SwitchToPersonalProfile(browser,
                            ChangeProfileReason::kManagedAccountSignOut,
                            std::move(continuation));
  }
}

bool IsFullscreenSigninPromoManagerMigrationDone() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  return [defaults boolForKey:kFullscreenSigninPromoManagerMigrationDone];
}

void LogFullscreenSigninPromoManagerMigrationDone() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setBool:YES forKey:kFullscreenSigninPromoManagerMigrationDone];
}

void FetchUnsyncedDataForSignOutOrProfileSwitching(
    syncer::SyncService* sync_service,
    UnsyncedDataForSignoutOrProfileSwitchingCallback callback) {
  constexpr syncer::DataTypeSet kDataTypesToQuery =
      syncer::TypesRequiringUnsyncedDataCheckOnSignout();
  sync_service->GetTypesWithUnsyncedData(
      kDataTypesToQuery,
      base::BindOnce(&DataCountsMapToDataTypeSet).Then(std::move(callback)));
}

}  // namespace signin
