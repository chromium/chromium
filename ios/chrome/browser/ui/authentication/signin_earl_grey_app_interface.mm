// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"

#import <map>
#import <string>

#import "base/functional/callback_helpers.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/titled_url_match.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_interaction_manager.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_cell.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "net/base/mac/url_conversions.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SigninEarlGreyAppInterface

+ (void)addFakeIdentity:(FakeSystemIdentity*)fakeIdentity {
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  systemIdentityManager->AddIdentity(fakeIdentity);
}

+ (void)setCapabilities:(ios::CapabilitiesDict*)capabilities
            forIdentity:(FakeSystemIdentity*)fakeIdentity {
  using CapabilityResult = SystemIdentityCapabilityResult;
  std::map<std::string, CapabilityResult> capabilitiesMap;
  for (NSString* name in capabilities) {
    const std::string key = base::SysNSStringToUTF8(name);
    NSNumber* number = base::mac::ObjCCastStrict<NSNumber>(capabilities[name]);
    switch (number.intValue) {
      case static_cast<int>(CapabilityResult::kFalse):
      case static_cast<int>(CapabilityResult::kTrue):
      case static_cast<int>(CapabilityResult::kUnknown):
        capabilitiesMap.insert(
            {key, static_cast<CapabilityResult>(number.intValue)});
        break;

      default:
        NOTREACHED() << "unexpected capability value: " << number.intValue;
        break;
    }
  }
  FakeSystemIdentityManager* systemIdentityManager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  systemIdentityManager->SetCapabilities(fakeIdentity, capabilitiesMap);
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
  prefService->SetString(prefs::kGoogleServicesLastUsername, emailAddress);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperationReauthenticate
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

@end
