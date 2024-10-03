// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/partial_translate/partial_translate_app_interface.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/partial_translate/partial_translate_mediator.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"

@implementation PartialTranslateAppInterface

+ (BOOL)installedPartialTranslate {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  bool incognito = chrome_test_util::IsIncognitoMode();
  if (incognito) {
    profile = chrome_test_util::GetCurrentIncognitoProfile();
  }
  PrefService* prefService = profile->GetPrefs();
  UIViewController* viewController =
      chrome_test_util::GetActiveViewController();
  Browser* browser = chrome_test_util::GetMainBrowser();
  WebStateList* webStateList = browser ? browser->GetWebStateList() : nullptr;
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(browser);

  PartialTranslateMediator* partialTranslateMediator =
      [[PartialTranslateMediator alloc]
            initWithWebStateList:webStateList
          withBaseViewController:viewController
                     prefService:prefService
            fullscreenController:fullscreenController
                       incognito:incognito];
  BOOL shouldInstallPartialTranslate =
      [partialTranslateMediator shouldInstallPartialTranslate];
  [partialTranslateMediator shutdown];
  return shouldInstallPartialTranslate;
}

@end
