// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

#import "base/command_line.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "base/version.h"
#import "components/policy/policy_constants.h"
#import "components/prefs/pref_service.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
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

// Returns whether the gaia ids `recorded_gaia_ids` is a strict subset of the
// current `identities` (i.e. all the gaia ids are in identities but there is
// at least one new identity).
bool IsStrictSubset(NSArray<NSString*>* recorded_gaia_ids,
                    NSArray<id<SystemIdentity>>* identities) {
  // Optimisation for the case of a nil or empty `recorded_gaia_ids`.
  // This allow not special casing the construction of the NSSet (as
  // -[NSSet setWithArray:] does not support nil for the array).
  if (recorded_gaia_ids.count == 0)
    return identities.count > 0;

  NSSet<NSString*>* recorded_gaia_ids_set =
      [NSSet setWithArray:recorded_gaia_ids];
  NSSet<NSString*>* identities_gaia_ids_set =
      GaiaIdSetWithIdentities(identities);
  return [recorded_gaia_ids_set isSubsetOfSet:identities_gaia_ids_set] &&
         ![recorded_gaia_ids_set isEqualToSet:identities_gaia_ids_set];
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

  if (tests_hook::DisableUpgradeSigninPromo())
    return false;

  if (profile->IsOffTheRecord()) {
    return false;
  }

  // There will be an error shown if the user chooses to sign in or select
  // another account while offline.
  if (net::NetworkChangeNotifier::IsOffline())
    return false;

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
    HistorySyncSkipReason skip_reason = [HistorySyncCoordinator
        getHistorySyncOptInSkipReason:sync_service
                authenticationService:auth_service
                          prefService:profile->GetPrefs()
                isHistorySyncOptional:YES];
    switch (skip_reason) {
      case HistorySyncSkipReason::kNone:
        // Need to show the upgrade promo, to show the history sync opt-in.
        break;
      case HistorySyncSkipReason::kNotSignedIn:
        NOTREACHED();
      case HistorySyncSkipReason::kAlreadyOptedIn:
      case HistorySyncSkipReason::kSyncForbiddenByPolicies:
      case HistorySyncSkipReason::kDeclinedTooOften:
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
  ChromeAccountManagerService* account_manager_service =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  NSArray<id<SystemIdentity>>* identities =
      account_manager_service->GetAllIdentities();
  if (identities.count == 0)
    return false;

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

  // The sign-in promo should be shown twice, even if no account has been added.
  NSInteger display_count =
      [defaults integerForKey:kSigninPromoViewDisplayCountKey];
  if (display_count <= 1)
    return true;

  // Otherwise, it can be shown only if a new account has been added.
  NSArray<NSString*>* last_known_gaia_id_list =
      [defaults arrayForKey:kLastShownAccountGaiaIdVersionKey];
  return IsStrictSubset(last_known_gaia_id_list, identities);
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
        signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN);
    return false;
  }
  signin_metrics::AccessPoint web_signin_access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN;
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
    ChromeAccountManagerService* account_manager_service,
    const base::Version& current_version) {
  DCHECK(account_manager_service);
  DCHECK(current_version.IsValid());

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:base::SysUTF8ToNSString(current_version.GetString())
               forKey:kDisplayedSSORecallForMajorVersionKey];
  NSArray<id<SystemIdentity>>* identities =
      account_manager_service->GetAllIdentities();
  NSSet<NSString*>* gaia_id_set = GaiaIdSetWithIdentities(identities);
  [defaults setObject:gaia_id_set.allObjects
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

}  // namespace signin
