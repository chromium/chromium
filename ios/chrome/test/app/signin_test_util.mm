// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/signin_test_util.h"

#import "base/check.h"
#import "base/feature_list.h"
#import "base/notreached.h"
#import "base/test/ios/wait_util.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "google_apis/gaia/gaia_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace chrome_test_util {

namespace {

// Starts forgetting all identities from the ChromeAccountManagerService.
//
// Note: Forgetting an identity is a asynchronous operation. This function does
// not wait for the forget identity operation to finish.
void StartForgetAllIdentities(ChromeBrowserState* browser_state,
                              ProceduralBlock completion) {
  SystemIdentityManager* system_identity_manager =
      GetApplicationContext()->GetSystemIdentityManager();
  ChromeAccountManagerService* account_manager_service =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browser_state);

  NSArray* identities_to_remove = account_manager_service->GetAllIdentities();
  if (identities_to_remove.count == 0) {
    if (completion) {
      completion();
    }
    return;
  }

  __block int pending_tasks_count =
      static_cast<int>(identities_to_remove.count);
  ProceduralBlock tasks_completion = ^{
    DCHECK_GT(pending_tasks_count, 0);
    if (--pending_tasks_count == 0 && completion) {
      completion();
    }
  };
  for (id<SystemIdentity> identity in identities_to_remove) {
    system_identity_manager->ForgetIdentity(
        identity, base::BindOnce(^(NSError* error) {
          if (error) {
            NSLog(@"ForgetIdentity failed: [identity = %@, error = %@]",
                  identity.userEmail, [error localizedDescription]);
          }
          tasks_completion();
        }));
  }
}

}  // namespace

void SetUpMockAuthentication() {
  // Should we do something here?
}

void TearDownMockAuthentication() {
  // Should we do something here?
}

void SignOutAndClearIdentities(ProceduralBlock completion) {
  // EarlGrey monitors network requests by swizzling internal iOS network
  // objects and expects them to be dealloced before the tear down. It is
  // important to autorelease all objects that make network requests to avoid
  // EarlGrey being confused about on-going network traffic..
  @autoreleasepool {
    ChromeBrowserState* browser_state = GetOriginalBrowserState();
    DCHECK(browser_state);

    // Needs to wait for two tasks to complete:
    // - Sign-out & clean browsing data (skipped if the user is already
    // signed-out, but callback is still called)
    // - Forgetting all identities
    __block int pending_tasks_count = 2;
    ProceduralBlock tasks_completion = ^{
      DCHECK_GT(pending_tasks_count, 0);
      if (--pending_tasks_count == 0 && completion) {
        completion();
      }
    };

    // Sign out current user and clear all browsing data on the device.
    AuthenticationService* authentication_service =
        AuthenticationServiceFactory::GetForBrowserState(browser_state);
    if (authentication_service->HasPrimaryIdentity(
            signin::ConsentLevel::kSignin)) {
      authentication_service->SignOut(signin_metrics::ProfileSignout::kTest,
                                      /*force_clear_browsing_data=*/true,
                                      tasks_completion);
    } else {
      tasks_completion();
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
    StartForgetAllIdentities(browser_state, tasks_completion);
  }
}

bool HasIdentities() {
  ChromeAccountManagerService* account_manager_service =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          GetOriginalBrowserState());
  return account_manager_service->HasIdentities();
}

void ResetMockAuthentication() {
  // Should we do something here?
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
  prefs->SetInteger(prefs::kIosReadingListSigninPromoDisplayedCount, 0);
  prefs->SetBoolean(prefs::kIosReadingListPromoAlreadySeen, false);
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
  __block AuthenticationFlow* authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:browser
                      identity:identity
                   accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN
              postSignInAction:PostSignInAction::kNone
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
  syncer::SyncUserSettings* settings = syncService->GetUserSettings();
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    // Clear the new per-account settings.
    settings->KeepAccountSettingsPrefsOnlyForUsers({});
  } else {
    // Explicitly enable all selectable types, this ensures that
    // BookmarksAndReadingListAccountStorageOptIn works as expected.
    settings->SetSelectedTypes(
        /*sync_everything=*/true, settings->GetRegisteredSelectableTypes());
  }
}

}  // namespace chrome_test_util
