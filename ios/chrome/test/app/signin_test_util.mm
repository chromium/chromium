// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/signin_test_util.h"

#import "base/check.h"
#import "base/test/ios/wait_util.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_user_settings.h"
#import "google_apis/gaia/gaia_constants.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

namespace {

// Starts forgetting all identities from the ChromeAccountManagerService.
//
// Note: Forgetting an identity is a asynchronous operation. This function does
// not wait for the forget identity operation to finish.
void StartForgetAllIdentities(ChromeBrowserState* browser_state) {
  ChromeAccountManagerService* account_manager_service =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state);
  NSArray* identities_to_remove = account_manager_service->GetAllIdentities();
  ios::ChromeIdentityService* identity_service =
      ios::GetChromeBrowserProvider().GetChromeIdentityService();
  for (id<SystemIdentity> identity in identities_to_remove) {
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
  std::unique_ptr<ios::FakeChromeIdentityService> service(
      new ios::FakeChromeIdentityService());
  ios::GetChromeBrowserProvider().SetChromeIdentityServiceForTesting(
      std::move(service));
}

void TearDownMockAuthentication() {
  ios::GetChromeBrowserProvider().SetChromeIdentityServiceForTesting(nullptr);
}

void SignOutAndClearIdentities() {
  // EarlGrey monitors network requests by swizzling internal iOS network
  // objects and expects them to be dealloced before the tear down. It is
  // important to autorelease all objects that make network requests to avoid
  // EarlGrey being confused about on-going network traffic..
  @autoreleasepool {
    ChromeBrowserState* browser_state = GetOriginalBrowserState();
    DCHECK(browser_state);

    // Sign out current user and clear all browsing data on the device.
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state);
    if (authentication_service->HasPrimaryIdentity(
            signin::ConsentLevel::kSignin)) {
      authentication_service->SignOut(signin_metrics::SIGNOUT_TEST,
                                      /*force_clear_browsing_data=*/true, nil);
    }

    // Clear last signed in user preference.
    browser_state->GetPrefs()->ClearPref(prefs::kGoogleServicesLastGaiaId);
    browser_state->GetPrefs()->ClearPref(prefs::kGoogleServicesLastUsername);

    // `SignOutAndClearIdentities()` is called during shutdown. Commit all pref
    // changes to ensure that clearing the last signed in account is saved on
    // disk in case Chrome crashes during shutdown.
    browser_state->GetPrefs()->CommitPendingWrite();

    // Once the browser was signed out, start clearing all identities from the
    // ChromeIdentityService.
    StartForgetAllIdentities(browser_state);
  }
}

bool HasIdentities() {
  ChromeAccountManagerService* account_manager_service =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          GetOriginalBrowserState());
  return account_manager_service->HasIdentities();
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
  prefs->SetInteger(prefs::kIosNtpFeedTopSigninPromoDisplayedCount, 0);
  prefs->SetBoolean(prefs::kIosNtpFeedTopPromoAlreadySeen, false);
  prefs->SetBoolean(prefs::kSigninShouldPromptForSigninAgain, false);
}

void ResetUserApprovedAccountListManager() {
  ChromeBrowserState* browser_state = GetOriginalBrowserState();
  PrefService* prefs = browser_state->GetPrefs();
  prefs->ClearPref(prefs::kSigninLastAccounts);
}

void SignInWithoutSync(id<SystemIdentity> identity) {
  Browser* browser = GetMainBrowser();
  UIViewController* viewController = GetActiveViewController();
  __block AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:browser
                                         identity:identity
                                 postSignInAction:POST_SIGNIN_ACTION_NONE
                         presentingViewController:viewController];
  authenticationFlow.dispatcher = (id<BrowsingDataCommands>)GetMainController();
  [authenticationFlow startSignInWithCompletion:^(BOOL success) {
    authenticationFlow = nil;
  }];
}

void ResetSyncSelectedDataTypes() {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  syncService->GetUserSettings()->SetSelectedTypes(/*sync_everything=*/true,
                                                   {});
}

}  // namespace chrome_test_util
