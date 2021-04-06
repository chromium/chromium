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
#import "components/version_info/version_info.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/browser_signin_policy_handler.h"
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

using base::SysNSStringToUTF8;
using base::Version;

namespace {
Version* g_current_version_for_test = nullptr;

Version CurrentVersion() {
  if (g_current_version_for_test) {
    return *g_current_version_for_test;
  }
  Version currentVersion = version_info::GetVersion();
  DCHECK(currentVersion.IsValid());
  return currentVersion;
}

NSSet* GaiaIdSetWithIdentities(NSArray* identities) {
  NSMutableSet* gaiaIdSet = [NSMutableSet set];
  for (ChromeIdentity* identity in identities) {
    [gaiaIdSet addObject:identity.gaiaID];
  }
  return [gaiaIdSet copy];
}
}  // namespace

#pragma mark - Public

namespace signin {

bool ShouldPresentUserSigninUpgrade(ChromeBrowserState* browserState) {
  if (tests_hook::DisableSigninRecallPromo())
    return false;

  if (browserState->IsOffTheRecord())
    return false;

  // There will be an error shown if the user chooses to sign in or select
  // another account while offline.
  if (net::NetworkChangeNotifier::IsOffline())
    return false;

  // Sign-in can be disabled by policy.
  if (!signin::IsSigninAllowed(browserState->GetPrefs()))
    return false;

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  // Do not show the SSO promo if the user is already logged in.
  if (authService->IsAuthenticated())
    return false;

  if (signin::ForceStartupSigninPromo())
    return true;

  // Show the promo at most every two major versions.
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  NSString* versionShown =
      [standardDefaults stringForKey:kDisplayedSSORecallForMajorVersionKey];
  if (versionShown) {
    Version seenVersion(SysNSStringToUTF8(versionShown));
    Version currentVersion = CurrentVersion();
    if (currentVersion.components()[0] - seenVersion.components()[0] < 2)
      return false;
  }
  // Don't show the promo if there is no identities.
  NSArray* identities = ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->GetAllIdentitiesSortedForDisplay();
  if ([identities count] == 0)
    return false;
  // The sign-in promo should be shown twice, even if no account has been added.
  NSInteger displayCount =
      [standardDefaults integerForKey:kSigninPromoViewDisplayCountKey];
  if (displayCount <= 1)
    return true;
  // Otherwise, it can be shown only if a new account has been added.
  NSArray* lastKnownGaiaIdList =
      [standardDefaults arrayForKey:kLastShownAccountGaiaIdVersionKey];
  NSSet* lastKnownGaiaIdSet = lastKnownGaiaIdList
                                  ? [NSSet setWithArray:lastKnownGaiaIdList]
                                  : [NSSet set];
  NSSet* currentGaiaIdSet = GaiaIdSetWithIdentities(identities);
  return [lastKnownGaiaIdSet isSubsetOfSet:currentGaiaIdSet] &&
         ![lastKnownGaiaIdSet isEqualToSet:currentGaiaIdSet];
}

void RecordVersionSeen() {
  NSUserDefaults* standardDefaults = [NSUserDefaults standardUserDefaults];
  [standardDefaults
      setObject:base::SysUTF8ToNSString(CurrentVersion().GetString())
         forKey:kDisplayedSSORecallForMajorVersionKey];
  NSArray* identities = ios::GetChromeBrowserProvider()
                            ->GetChromeIdentityService()
                            ->GetAllIdentitiesSortedForDisplay();
  NSArray* gaiaIdList = GaiaIdSetWithIdentities(identities).allObjects;
  [standardDefaults setObject:gaiaIdList
                       forKey:kLastShownAccountGaiaIdVersionKey];
  NSInteger displayCount =
      [standardDefaults integerForKey:kSigninPromoViewDisplayCountKey];
  ++displayCount;
  [standardDefaults setInteger:displayCount
                        forKey:kSigninPromoViewDisplayCountKey];
}

void SetCurrentVersionForTesting(Version* version) {
  g_current_version_for_test = version;
}

bool IsSigninAllowed(const PrefService* prefs) {
  return prefs->GetBoolean(prefs::kSigninAllowed);
}

bool IsSigninAllowedByPolicy() {
  NSDictionary* configuration = [[NSUserDefaults standardUserDefaults]
      dictionaryForKey:kPolicyLoaderIOSConfigurationKey];

  NSValue* value = [configuration
      valueForKey:base::SysUTF8ToNSString(policy::key::kBrowserSignin)];
  if (!value) {
    return true;
  }

  policy::BrowserSigninMode signin_mode;
  [value getValue:&signin_mode];
  return signin_mode == policy::BrowserSigninMode::kEnabled;
}

}  // namespace signin
