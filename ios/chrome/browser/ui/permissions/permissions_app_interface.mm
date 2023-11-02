// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/permissions/permissions_app_interface.h"

#import "ios/chrome/app/main_controller.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PermissionsAppInterface

+ (NSDictionary<NSNumber*, NSNumber*>*)statesForAllPermissions {
  web::WebState* activeWebState =
      chrome_test_util::GetMainController()
          .interfaceProvider.currentInterface.browser->GetWebStateList()
          ->GetActiveWebState();
  if (activeWebState != nil) {
    return activeWebState->GetStatesForAllPermissions();
  }
  return [NSDictionary dictionary];
}

@end
