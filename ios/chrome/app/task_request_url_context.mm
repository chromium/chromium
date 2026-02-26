// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_url_context.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

@implementation TaskRequestForURLContext {
  UIOpenURLContext* _URLContext;
}

- (instancetype)initWithURLContext:(UIOpenURLContext*)URLContext
                        sceneState:(SceneState*)sceneState
                       isColdStart:(BOOL)isColdStart {
  if ((self = [super initWithSceneState:sceneState isColdStart:isColdStart])) {
    _URLContext = URLContext;
    [self extractGaiaID];
  }
  return self;
}

- (void)execute {
  if (self.isColdStart) {
    [self executeFromColdStart];
  } else {
    [self executeFromWarmStart];
  }
}

#pragma mark - Private

- (void)executeFromWarmStart {
  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);

  NSSet* URLContextSet = [NSSet setWithObject:_URLContext];
  // If the SystemIdentityManager handles the URL context, return early to avoid
  // opening the URL twice.
  if (GetApplicationContext()
          ->GetSystemIdentityManager()
          ->HandleSessionOpenURLContexts(sceneState.scene, URLContextSet)) {
    return;
  }
  ProfileState* profileState = sceneState.profileState;
  URLOpenerParams* options =
      [[URLOpenerParams alloc] initWithUIOpenURLContext:_URLContext];
  [URLOpener openURL:options
          applicationActive:YES
                  tabOpener:sceneState.controller
      connectionInformation:sceneState.controller
         startupInformation:profileState.startupInformation
                prefService:profileState.profile->GetPrefs()
                  initStage:profileState.initStage];
}

- (void)executeFromColdStart {
  SceneState* sceneState = [self sceneStateFromSessionID];
  CHECK(sceneState);

  URLOpenerParams* options =
      [[URLOpenerParams alloc] initWithUIOpenURLContext:_URLContext];
  ProfileState* profileState = sceneState.profileState;

  [URLOpener handleLaunchOptions:options
                       tabOpener:sceneState.controller
           connectionInformation:sceneState.controller
              startupInformation:profileState.startupInformation
                     prefService:profileState.profile->GetPrefs()
                       initStage:profileState.initStage];
}

- (void)extractGaiaID {
  NSURL* URL = _URLContext.URL;

  // Only widgets and share extension support profile/account switching when
  // handling an intent.
  bool isWidget = [URL.scheme isEqualToString:@"chromewidgetkit"];
  bool isShareExtension = [URL.path
      isEqualToString:
          [NSString
              stringWithFormat:@"/%s",
                               app_group::kChromeAppGroupXCallbackCommand]];

  if (!isWidget && !isShareExtension) {
    return;
  }

  std::string gaiaID;
  if (net::GetValueForKeyInQuery(net::GURLWithNSURL(URL),
                                 app_group::kGaiaIDQueryItemName, &gaiaID)) {
    self.gaiaID = base::SysUTF8ToNSString(gaiaID);
  }
}

@end
