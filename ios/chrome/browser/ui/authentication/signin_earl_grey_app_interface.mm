// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"

#import <map>
#import <string>

#import "base/apple/foundation_util.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/titled_url_match.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/supervised_user/core/browser/supervised_user_preferences.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_interaction_manager.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_cell.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "net/base/mac/url_conversions.h"
#import "url/gurl.h"

@implementation SigninEarlGreyAppInterface

+ (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  systemIdentityManager->AddIdentity(fakeIdentity);
}

+ (void)addFakeIdentityForSSOAuthAddAccountFlow:
    (FakeSystemIdentity*)fakeIdentity {
  FakeSystemIdentityInteractionManager.identity = fakeIdentity;
}

+ (void)forgetFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  systemIdentityManager->ForgetIdentity(fakeIdentity, base::DoNothing());
}

+ (NSString*)primaryAccountGaiaID {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  CoreAccountInfo info =
      IdentityManagerFactory::GetForBrowserState(browserState)
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  return base::SysUTF8ToNSString(info.gaia);
}

+ (NSString*)primaryAccountEmailWithConsent:(signin::ConsentLevel)consentLevel {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  CoreAccountInfo info =
      IdentityManagerFactory::GetForBrowserState(browserState)
          ->GetPrimaryAccountInfo(consentLevel);

  return base::SysUTF8ToNSString(info.email);
}

+ (BOOL)isSignedOut {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  return !IdentityManagerFactory::GetForBrowserState(browserState)
              ->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

+ (void)signOut {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  authentication_service->SignOut(signin_metrics::ProfileSignout::kTest,
                                  /*force_clear_browsing_data=*/false, nil);
}

+ (void)triggerReauthDialogWithFakeIdentity:(FakeSystemIdentity*)identity {
  FakeSystemIdentityInteractionManager.identity = identity;
  std::string emailAddress = base::SysNSStringToUTF8(identity.userEmail);
  PrefService* prefService =
      chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefService->SetString(prefs::kGoogleServicesLastSyncingUsername,
                         emailAddress);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kSigninAndSyncReauth
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_RESIGNIN_INFOBAR];
  UIViewController* baseViewController =
      chrome_test_util::GetActiveViewController();
  SceneController* sceneController =
      chrome_test_util::GetForegroundActiveSceneController();
  [sceneController showSignin:command baseViewController:baseViewController];
}

+ (void)triggerConsistencyPromoSigninDialogWithURL:(NSURL*)url {
  const GURL gURL = net::GURLWithNSURL(url);
  UIViewController* baseViewController =
      chrome_test_util::GetActiveViewController();
  SceneController* sceneController =
      chrome_test_util::GetForegroundActiveSceneController();
  [sceneController showWebSigninPromoFromViewController:baseViewController
                                                    URL:gURL];
}

+ (void)clearLastSignedInAccounts {
  PrefService* prefService =
      chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefService->ClearPref(prefs::kSigninLastAccounts);
}

+ (void)presentSignInAccountsViewControllerIfNecessary {
  chrome_test_util::PresentSignInAccountsViewControllerIfNecessary();
}

#pragma mark - Capability Setters

+ (void)setIsSubjectToParentalControls:(BOOL)value
                           forIdentity:(FakeSystemIdentity*)fakeIdentity {
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  AccountCapabilitiesTestMutator* mutator =
      systemIdentityManager->GetCapabilitiesMutator(fakeIdentity);
  mutator->set_is_subject_to_parental_controls(value);

  // Update child account status to reflect parental controls support.
  // TODO(b/276899041): Add support for test classes to listen to extended
  // account info changes and reflect the new state in services.
  PrefService* prefService =
      chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  if (value) {
    supervised_user::EnableParentalControls(*prefService);
  } else {
    supervised_user::DisableParentalControls(*prefService);
  }
  systemIdentityManager->FireIdentityUpdatedNotification(fakeIdentity);
}

+ (void)setCanHaveEmailAddressDisplayed:(BOOL)value
                            forIdentity:(FakeSystemIdentity*)fakeIdentity {
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  AccountCapabilitiesTestMutator* mutator =
      systemIdentityManager->GetCapabilitiesMutator(fakeIdentity);
  mutator->set_can_have_email_address_displayed(value);
}

+ (void)setCanOfferExtendedChromeSyncPromos:(BOOL)value
                                forIdentity:(FakeSystemIdentity*)fakeIdentity {
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  AccountCapabilitiesTestMutator* mutator =
      systemIdentityManager->GetCapabilitiesMutator(fakeIdentity);
  mutator->set_can_offer_extended_chrome_sync_promos(value);
}

+ (void)setSelectedType:(syncer::UserSelectableType)type enabled:(BOOL)enabled {
  syncer::SyncUserSettings* settings =
      SyncServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState())
          ->GetUserSettings();
  settings->SetSelectedTypes(/*sync_everything=*/false,
                             settings->GetSelectedTypes());
  settings->SetSelectedType(type, enabled);
}

+ (BOOL)isSelectedTypeEnabled:(syncer::UserSelectableType)type {
  syncer::SyncUserSettings* settings =
      SyncServiceFactory::GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState())
          ->GetUserSettings();
  return settings->GetSelectedTypes().Has(type) ? YES : NO;
}

@end
