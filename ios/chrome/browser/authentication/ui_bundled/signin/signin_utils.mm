// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"

#import "base/barrier_closure.h"
#import "base/command_line.h"
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

namespace {

// Maximum delay to wait for fetching the account capabilities before showing
// the sign-in upgrade promo. If fetching the account capabilities takes more
// than the delay, then the promo is suppressed - it may be shown on the next
// start-up.
constexpr base::TimeDelta kShowSigninUpgradePromoMaxDelay =
    base::Milliseconds(200);

// Converts an array of identities to a set of gaia ids.
NSSet<NSString*>* GaiaIdSetWithIdentities(
    NSArray<id<SystemIdentity>>* identities) {
  NSMutableSet* gaia_id_set = [NSMutableSet set];
  for (id<SystemIdentity> identity in identities) {
    [gaia_id_set addObject:identity.gaiaID];
  }
  return [gaia_id_set copy];
}

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

// Switch from a managed profile to a personal profile then run `continuation`.
void SwitchToPersonalProfile(Browser* browser,
                             ChangeProfileContinuation continuation) {
  SceneState* scene_state = browser->GetSceneState();

  ProfileManagerIOS* profile_manager =
      GetApplicationContext()->GetProfileManager();
  std::string default_profile_name =
      profile_manager->GetProfileAttributesStorage()->GetPersonalProfileName();

  CHECK(profile_manager->HasProfileWithName(default_profile_name));

  id<ChangeProfileCommands> change_profile_handler =
      HandlerForProtocol(scene_state.profileState.appState.appCommandDispatcher,
                         ChangeProfileCommands);

  [change_profile_handler changeProfile:default_profile_name
                               forScene:scene_state
                           continuation:std::move(continuation)];
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
  if (auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSync)) {
    return false;
  }
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
  NSSet<NSString*>* identities_on_device_gaia_ids;
  if (IsUseAccountListFromIdentityManagerEnabled()) {
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile);
    identities_on_device_gaia_ids =
        GaiaIdSetWithAccountInfos(identity_manager->GetAccountsOnDevice());
  } else {
    ChromeAccountManagerService* account_manager_service =
        ChromeAccountManagerServiceFactory::GetForProfile(profile);
    identities_on_device_gaia_ids =
        GaiaIdSetWithIdentities(account_manager_service->GetAllIdentities());
  }
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

  if (IsFullscreenSigninPromoManagerMigrationEnabled()) {
    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForProfile(profile);
    unsigned int interactions = 0;
    std::vector<std::pair<feature_engagement::EventConfig, int>> events =
        tracker->ListEvents(
            feature_engagement::kIPHiOSPromoSigninFullscreenFeature);
    for (const auto& event : events) {
      if (event.first.name ==
          feature_engagement::events::kIOSSigninFullscreenPromoTrigger) {
        interactions = event.second;
        break;
      }
    }

    if (interactions <= 1) {
      return true;
    }

  } else {
    // The sign-in promo should be shown twice, even if no account has been
    // added.
    NSInteger display_count =
        [defaults integerForKey:kSigninPromoViewDisplayCountKey];
    if (display_count <= 1) {
      return true;
    }
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
  NSSet<NSString*>* gaia_id_on_device_set;
  if (IsUseAccountListFromIdentityManagerEnabled()) {
    std::vector<AccountInfo> account_infos =
        identity_manager->GetAccountsOnDevice();
    gaia_id_on_device_set = GaiaIdSetWithAccountInfos(account_infos);
  } else {
    NSArray<id<SystemIdentity>>* identities =
        account_manager_service->GetAllIdentities();
    gaia_id_on_device_set = GaiaIdSetWithIdentities(identities);
  }
  [defaults setObject:gaia_id_on_device_set.allObjects
               forKey:kLastShownAccountGaiaIdVersionKey];
  NSInteger display_count =
      [defaults integerForKey:kSigninPromoViewDisplayCountKey];
  ++display_count;
  [defaults setInteger:display_count forKey:kSigninPromoViewDisplayCountKey];
}

IdentitySigninState GetPrimaryIdentitySigninState(ProfileIOS* profile) {
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile);
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  // TODO(crbug.com/40066949): After phase 3 migration of kSync users, Remove
  // this usage.
  if (auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSync) &&
      syncService->GetUserSettings()->IsInitialSyncFeatureSetupComplete()) {
    return IdentitySigninStateSignedInWithSyncEnabled;
  } else if (auth_service->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    return IdentitySigninStateSignedInWithSyncDisabled;
  } else {
    return IdentitySigninStateSignedOut;
  }
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
  if (!IsUseAccountListFromIdentityManagerEnabled()) {
    return accountManagerService->GetAllIdentities();
  }
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

void MultiProfileSignOut(Browser* browser,
                         signin_metrics::ProfileSignout signout_source,
                         bool force_snackbar_over_toolbar,
                         MDCSnackbarMessage* snackbar_message,
                         ProceduralBlock signout_completion,
                         bool should_record_metrics) {
  // The regular browser should be used to execute the signout.
  CHECK_EQ(browser->type(), Browser::Type::kRegular);
  SceneState* scene_state = browser->GetSceneState();

  ChangeProfileContinuation continuation =
      CreateChangeProfileSignoutContinuation(
          signout_source, force_snackbar_over_toolbar, should_record_metrics,
          snackbar_message, signout_completion);
  ProfileIOS* profile = browser->GetProfile();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(profile);

  if (signout_source == signin_metrics::ProfileSignout::kPrefChanged) {
    ChangeProfileContinuation postSignoutContinuation =
        CreateChangeProfileForceSignoutContinuation();
    continuation = ChainChangeProfileContinuations(
        std::move(continuation), std::move(postSignoutContinuation));
  }

  if (!ShouldSwitchProfileAtSignout(authentication_service,
                                    profile->GetProfileName())) {
    std::move(continuation).Run(scene_state, base::DoNothing());
    return;
  }

  if (signout_source ==
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings) {
    // TODO(crbug.com/375605174): Verify that This signout source is only used
    // when signing out from Accounts settings page. For now, it is also used
    // in the signout button in ManageAccounts view, which will no longer be
    // shown once kSeparateProfilesForManagedAccounts is enabled.
    ChangeProfileContinuation postSignoutContinuation =
        CreateChangeProfileSettingsContinuation();
    continuation = ChainChangeProfileContinuations(
        std::move(continuation), std::move(postSignoutContinuation));
  }

  SwitchToPersonalProfile(browser, std::move(continuation));
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
  // the personal profile
  base::RepeatingClosure barrier = base::BarrierClosure(
      browser_list.size(), std::move(signout_completion_closure));

  // Sign the user out in all browsers
  for (Browser* browser : browser_list) {
    ChangeProfileContinuation continuation =
        CreateChangeProfileSignoutContinuation(
            signout_source, /*force_snackbar_over_toolbar=*/false,
            /*should_record_metrics=*/false, /*snackbar_message =*/nil,
            base::CallbackToBlock(barrier));
    SwitchToPersonalProfile(browser, std::move(continuation));
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
  sync_service->GetTypesWithUnsyncedData(kDataTypesToQuery,
                                         std::move(callback));
}

}  // namespace signin
