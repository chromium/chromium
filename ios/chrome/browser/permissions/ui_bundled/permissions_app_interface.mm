// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/permissions/ui_bundled/permissions_app_interface.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/web_state.h"

@implementation PermissionsAppInterface

+ (NSDictionary<NSNumber*, NSNumber*>*)statesForAllPermissions {
  SceneState* sceneState = chrome_test_util::GetForegroundActiveScene();
  web::WebState* activeWebState =
      sceneState.browserProviderInterface.currentBrowserProvider.browser
          ->GetWebStateList()
          ->GetActiveWebState();
  if (activeWebState != nil) {
    return activeWebState->GetStatesForAllPermissions();
  }
  return [NSDictionary dictionary];
}

@end
