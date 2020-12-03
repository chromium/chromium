// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"

#include "base/feature_list.h"
#include "base/ios/ios_util.h"
#include "base/version.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/whats_new_commands.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DefaultBrowserSceneAgent ()

// Command Dispatcher.
@property(nonatomic, weak) CommandDispatcher* dispatcher;

@end

@implementation DefaultBrowserSceneAgent

- (instancetype)initWithCommandDispatcher:(CommandDispatcher*)dispatcher {
  if ([super init])
    _dispatcher = dispatcher;
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (!base::FeatureList::IsEnabled(kDefaultBrowserFullscreenPromo)) {
    // Do nothing if the Default Browser Fullscreen Promo feature flag is not
    // on.
    return;
  }
  std::map<std::string, std::string> params;
  if (!base::GetFieldTrialParamsByFeature(kDefaultBrowserFullscreenPromo,
                                          &params)) {
    // No FieldTrial in the Finch Config.
    return;
  } else {
    base::Version min_os_version;
    for (const auto& param : params) {
      if (param.first == "min_os_version") {
        min_os_version = base::Version(param.second);
      }
    }

    if (min_os_version.components().size() != 3) {
      return;
    }
    // Do not show fullscreen promo if the min_os_version set in the Finch
    // Config is higher than the device os version. This is to protect users
    // on 14.0.0 (which has the default browser reset bug) from seeing this
    // promo.
    std::vector<uint32_t> components = min_os_version.components();
    if (!base::ios::IsRunningOnOrLater(components[0], components[1],
                                       components[2])) {
      return;
    }
  }

  AppState* appState = self.sceneState.appState;
  // Can only present UI when activation level is
  // SceneActivationLevelForegroundActive. Show the UI if user has met the
  // qualifications to be shown the promo.
  if (level == SceneActivationLevelForegroundActive &&
      appState.shouldShowDefaultBrowserPromo && !appState.currentUIBlocker) {
    [HandlerForProtocol(self.dispatcher, WhatsNewCommands)
        showDefaultBrowserFullscreenPromo];
    appState.shouldShowDefaultBrowserPromo = NO;
  }
}

@end
