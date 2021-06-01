// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/app/signin_test_util.h"

#include "base/check.h"
#import "base/test/ios/wait_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "google_apis/gaia/gaia_constants.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

namespace {

// Starts forgetting all identities from the ChromeIdentity services.
//
// Note: Forgetting an identity is a asynchronous operation. This function does
// not wait for the forget identity operation to finish.
void StartForgetAllIdentities(PrefService* pref_service) {
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  NSArray* identities_to_remove =
      [NSArray arrayWithArray:identity_service->GetAllIdentities(pref_service)];
  for (ChromeIdentity* identity in identities_to_remove) {
    identity_service->ForgetIdentity(identity, ^(NSError* error) {
      if (error) {
        NSLog(@"ForgetIdentity failed: [identity = %@, error = %@]",
              identity.userEmail, [error localizedDescription]);
      }
    });
  }
}

}  // namespace

void SetUpMockAuthentication() {
  ios::ChromeBrowserProvider* provider = ios::GetChromeBrowserProvider();
  std::unique_ptr<ios::FakeChromeIdentityService> service(
      new ios::FakeChromeIdentityService());
  service->SetUpForIntegrationTests();
  provider->SetChromeIdentityServiceForTesting(std::move(service));
  AuthenticationServiceFactory::GetForBrowserState(GetOriginalBrowserState())
      ->ResetChromeIdentityServiceObserverForTesting();
}

void TearDownMockAuthentication() {
  ios::ChromeBrowserProvider* provider = ios::GetChromeBrowserProvider();
  provider->SetChromeIdentityServiceForTesting(nullptr);
  AuthenticationServiceFactory::GetForBrowserState(GetOriginalBrowserState())
      ->ResetChromeIdentityServiceObserverForTesting();
}

void SignOutAndClearIdentities() {
  // EarlGrey monitors network requests by swizzling internal iOS network
  // objects and expects them to be dealloced before the tear down. It is
  // important to autorelease all objects that make network requests to avoid
  // EarlGrey being confused about on-going network traffic..
  @autoreleasepool {
    ChromeBrowserState* browser_state = GetOriginalBrowserState();
    DCHECK(browser_state);

    // Sign out current user.
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state);
    if (authentication_service->IsAuthenticated()) {
      authentication_service->SignOut(signin_metrics::SIGNOUT_TEST,
                                      /*force_clear_browsing_data=*/false, nil);
    }

    // Clear last signed in user preference.
    browser_state->GetPrefs()->ClearPref(prefs::kGoogleServicesLastAccountId);
    browser_state->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);

    // |SignOutAndClearIdentities()| is called during shutdown. Commit all pref
    // changes to ensure that clearing the last signed in account is saved on
    // disk in case Chrome crashes during shutdown.
    browser_state->GetPrefs()->CommitPendingWrite();

    // Once the browser was signed out, start clearing all identities from the
    // ChromeIdentityService.
    StartForgetAllIdentities(browser_state->GetPrefs());
  }
}

bool HasIdentities() {
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider()->GetChromeIdentityService();
  return identity_service->HasIdentities();
}

void ResetMockAuthentication() {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->SetFakeMDMError(false);
}

void ResetSigninPromoPreferences() {
  ChromeBrowserState* browser_state = GetOriginalBrowserState();
  PrefService* prefs = browser_state->GetPrefs();
  prefs->SetInteger(prefs::kIosBookmarkSigninPromoDisplayedCount, 0);
  prefs->SetBoolean(prefs::kIosBookmarkPromoAlreadySeen, false);
  prefs->SetInteger(prefs::kIosSettingsSigninPromoDisplayedCount, 0);
  prefs->SetBoolean(prefs::kIosSettingsPromoAlreadySeen, false);
  prefs->SetBoolean(prefs::kSigninShouldPromptForSigninAgain, false);
}

void RevokeSyncConsent() {
  ChromeBrowserState* browser_state = GetOriginalBrowserState();
  PrefService* prefs = browser_state->GetPrefs();
  prefs->SetBoolean(prefs::kGoogleServicesConsentedToSync, false);
}

}  // namespace chrome_test_util
