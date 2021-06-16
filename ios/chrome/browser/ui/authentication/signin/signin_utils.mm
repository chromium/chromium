// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "base/version.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#import "components/policy/policy_constants.h"
#import "components/prefs/pref_service.h"
#import "components/signin/ios/browser/features.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_constants.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "net/base/network_change_notifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Converts an array of identities to a set of gaia ids.
NSSet<NSString*>* GaiaIdSetWithIdentities(
    NSArray<ChromeIdentity*>* identities) {
  NSMutableSet* gaia_id_set = [NSMutableSet set];
  for (ChromeIdentity* identity in identities) {
    [gaia_id_set addObject:identity.gaiaID];
  }
  return [gaia_id_set copy];
}

// Returns whether the gaia ids |recorded_gaia_ids| is a strict subset of the
// current |identities| (i.e. all the gaia ids are in identities but there is
// at least one new identity).
bool IsStrictSubset(NSArray<NSString*>* recorded_gaia_ids,
                    NSArray<ChromeIdentity*>* identities) {
  // Optimisation for the case of a nil or empty |recorded_gaia_ids|.
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

bool ShouldPresentUserSigninUpgrade(ChromeBrowserState* browser_state,
                                    const base::Version& current_version) {
  DCHECK(browser_state);
  DCHECK(current_version.IsValid());

  if (tests_hook::DisableSigninRecallPromo())
    return false;

  if (browser_state->IsOffTheRecord())
    return false;

  // There will be an error shown if the user chooses to sign in or select
  // another account while offline.
  if (net::NetworkChangeNotifier::IsOffline())
    return false;

  // Sign-in can be disabled by policy or through user Settings.
  if (!signin::IsSigninAllowed(browser_state->GetPrefs()))
    return false;

  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(browser_state);
  // Do not show the SSO promo if the user is already logged in.
  if (auth_service->IsAuthenticated())
    return false;

  // Used for testing purposes only.
  if (signin::ForceStartupSigninPromo())
    return true;

  // Show the promo at most every two major versions.
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSString* version_string =
      [defaults stringForKey:kDisplayedSSORecallForMajorVersionKey];

  if (version_string) {
    const base::Version version_shown(base::SysNSStringToUTF8(version_string));
    if (version_shown.IsValid()) {
      if (current_version.components()[0] - version_shown.components()[0] < 2)
        return false;
    }
  }

  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();

  // Don't show the promo if there are no identities.
  NSArray* identities = identity_service->GetAllIdentitiesSortedForDisplay(
      browser_state->GetPrefs());
  if (identities.count == 0)
    return false;

  // The SSO promo should not be disabled if it is force disabled.
  if (signin::ForceDisableExtendedSyncPromos())
    return false;

  // Don't show the SSO promo if the default primary account cannot display
  // extended sync promos.
  bool canOfferExtendedSyncPromos =
      identity_service->CanOfferExtendedSyncPromos(identities[0]);
  if (signin::ExtendedSyncPromosCapabilityEnabled() &&
      !canOfferExtendedSyncPromos)
    return false;

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

void RecordVersionSeen(PrefService* pref_service,
                       const base::Version& current_version) {
  DCHECK(pref_service);
  DCHECK(current_version.IsValid());

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setObject:base::SysUTF8ToNSString(current_version.GetString())
               forKey:kDisplayedSSORecallForMajorVersionKey];
  NSArray<ChromeIdentity*>* identities =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->GetAllIdentitiesSortedForDisplay(pref_service);
  NSSet<NSString*>* gaia_id_set = GaiaIdSetWithIdentities(identities);
  [defaults setObject:gaia_id_set.allObjects
               forKey:kLastShownAccountGaiaIdVersionKey];
  NSInteger display_count =
      [defaults integerForKey:kSigninPromoViewDisplayCountKey];
  ++display_count;
  [defaults setInteger:display_count forKey:kSigninPromoViewDisplayCountKey];
}

bool IsSigninAllowed(const PrefService* prefs) {
  DCHECK(signin::IsMobileIdentityConsistencyEnabled() ||
         prefs->GetBoolean(prefs::kSigninAllowed));
  return prefs->GetBoolean(prefs::kSigninAllowed) &&
         prefs->GetBoolean(prefs::kSigninAllowedByPolicy);
}

bool IsSigninAllowedByPolicy(const PrefService* prefs) {
  return prefs->GetBoolean(prefs::kSigninAllowedByPolicy);
}

}  // namespace signin
