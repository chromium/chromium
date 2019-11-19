// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_app_interface.h"

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller_factory.h"
#import "ios/chrome/test/app/tab_test_util.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/ui/fullscreen_provider.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FullscreenAppInterface

+ (BOOL)isFullscreenInitialized {
  return ios::GetChromeBrowserProvider()
      ->GetFullscreenProvider()
      ->IsInitialized();
}

+ (UIEdgeInsets)currentViewportInsets {
  web::WebState* webState = chrome_test_util::GetCurrentWebState();
  if (!webState)
    return UIEdgeInsetsZero;
  ios::ChromeBrowserState* browserState =
      ios::ChromeBrowserState::FromBrowserState(webState->GetBrowserState());
  FullscreenController* fullscreenController =
      FullscreenControllerFactory::GetForBrowserState(browserState);
  if (!fullscreenController)
    return UIEdgeInsetsZero;
  return fullscreenController->GetCurrentViewportInsets();
}

@end
