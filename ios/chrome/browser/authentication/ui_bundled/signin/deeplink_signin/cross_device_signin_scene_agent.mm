// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/deeplink_signin/cross_device_signin_scene_agent.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/deeplink_signin/cross_device_signin_url_interceptor.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"
#import "url/gurl.h"

@implementation CrossDeviceSigninSceneAgent {
  __weak id<SceneCommands> _sceneHandler;
}

- (instancetype)initWithSceneURLLoadingService:
                    (SceneUrlLoadingService*)sceneURLLoadingService
                                  sceneHandler:(id<SceneCommands>)sceneHandler {
  self = [super init];
  if (self) {
    _sceneHandler = sceneHandler;

    GURL base_url(switches::kCrossDeviceSigninUrl.Get());
    if (base_url.is_valid()) {
      __weak __typeof(self) weakSelf = self;
      auto interceptor = std::make_unique<CrossDeviceSigninURLInterceptor>(
          base::BindRepeating(^(const std::string& email) {
            [weakSelf handleCrossDeviceSigninWithIdentity:email];
          }));
      bool success = sceneURLLoadingService->AddInterceptor(
          base_url, std::move(interceptor));
      CHECK(success);
    }
  }
  return self;
}

#pragma mark - Private

// Dispatched when the URL is intercepted.
- (void)handleCrossDeviceSigninWithIdentity:(const std::string&)email {
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
       initWithOperation:AuthenticationOperation::kDeepLinkSignin
      targetAccountEmail:base::SysUTF8ToNSString(email)
             accessPoint:signin_metrics::AccessPoint::kDeepLinkDefault
             promoAction:signin_metrics::PromoAction::
                             PROMO_ACTION_NO_SIGNIN_PROMO];
  [_sceneHandler showSignin:command baseViewController:nil];
}

@end
