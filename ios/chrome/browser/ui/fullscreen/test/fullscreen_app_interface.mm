// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_app_interface.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/web_state.h"

@implementation FullscreenAppInterface

+ (UIEdgeInsets)currentViewportInsets {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();
  if (!webState)
    return UIEdgeInsetsZero;
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(webState->GetBrowserState());
  // TODO: (crbug.com/1063516): Retrieve Browser-scoped FullscreenController
  // in a better way.
  std::set<Browser*> browsers =
      BrowserListFactory::GetForProfile(profile)->BrowsersOfType(
          BrowserList::BrowserType::kRegularAndInactive);
  // There is regular browser and inactive browser. More means multi-window.
  // NOTE: The inactive browser is always created even if the feature is
  // disabled, in order to ensure to restore all saved tabs.
  DCHECK(browsers.size() == 2);
  std::set<Browser*>::iterator iterator = base::ranges::find_if(
      browsers, [](Browser* browser) { return !browser->IsInactive(); });
  DCHECK(iterator != browsers.end());
  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(*iterator);

  if (!fullscreenController)
    return UIEdgeInsetsZero;
  return fullscreenController->GetCurrentViewportInsets();
}

@end
