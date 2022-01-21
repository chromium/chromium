// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin_earl_grey_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/titled_url_match.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/bookmarks/bookmarks_utils.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_cell.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/main/scene_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "net/base/mac/url_conversions.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SigninEarlGreyAppInterface

+ (void)addFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()->AddIdentity(
      fakeIdentity);
}

+ (void)setCapabilities:(NSDictionary*)capabilities
            forIdentity:(FakeChromeIdentity*)fakeIdentity {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->SetCapabilities(fakeIdentity, capabilities);
}

+ (void)forgetFakeIdentity:(FakeChromeIdentity*)fakeIdentity {
  ios::FakeChromeIdentityService::GetInstanceFromChromeProvider()
      ->ForgetIdentity(fakeIdentity, nil);
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

+ (BOOL)hasPrimaryIdentity {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  return authentication_service->HasPrimaryIdentity(
      signin::ConsentLevel::kSignin);
}

+ (void)signOut {
  ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  authentication_service->SignOut(signin_metrics::SIGNOUT_TEST,
                                  /*force_clear_browsing_data=*/false, nil);
}

+ (void)triggerReauthDialogWithFakeIdentity:(FakeChromeIdentity*)identity {
  FakeChromeIdentityInteractionManager.identity = identity;
  std::string emailAddress = base::SysNSStringToUTF8(identity.userEmail);
  PrefService* prefService =
      chrome_test_util::GetOriginalBrowserState()->GetPrefs();
  prefService->SetString(prefs::kGoogleServicesLastUsername, emailAddress);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AUTHENTICATION_OPERATION_REAUTHENTICATE
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_RESIGNIN_INFOBAR];
  UIViewController* baseViewController =
      chrome_test_util::GetActiveViewController();
  SceneController* sceneController =
      chrome_test_util::GetForegroundActiveSceneController();
  [sceneController showSignin:command baseViewController:baseViewController];
}

+ (void)triggerConsistencyPromoSigninDialog {
  NSURL* url = [NSURL URLWithString:@"http://www.example.com"];
  const GURL gURL = net::GURLWithNSURL(url);
  UIViewController* baseViewController =
      chrome_test_util::GetActiveViewController();
  SceneController* sceneController =
      chrome_test_util::GetForegroundActiveSceneController();
  [sceneController showConsistencyPromoFromViewController:baseViewController
                                                      URL:gURL];
}

@end
