// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_user_activity.h"

#import "base/check.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/task_request_private.h"
#import "ios/chrome/app/unexpected_mode_toast_util.h"
#import "ios/chrome/browser/intents/model/user_activity_browser_agent.h"
#import "ios/chrome/browser/intents/model/user_activity_compatibility_util.h"
#import "ios/chrome/browser/policy/model/policy_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation TaskRequestForUserActivity {
  NSUserActivity* _userActivity;
}

- (instancetype)initWithUserActivity:(NSUserActivity*)userActivity
                          sceneState:(SceneState*)sceneState
                         isColdStart:(BOOL)isColdStart {
  if ((self = [super initWithSceneState:sceneState isColdStart:isColdStart])) {
    _userActivity = userActivity;
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
  Browser* browser =
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  PrefService* prefs = sceneState.profileState.profile->GetPrefs();
  if (!ProceedWithUserActivity(_userActivity, prefs)) {
    ApplicationModeForTabOpening targetMode =
        IsIncognitoModeForced(prefs) ? ApplicationModeForTabOpening::INCOGNITO
                                     : ApplicationModeForTabOpening::NORMAL;
    ShowToastWhenOpenInUnexpectedMode(sceneState, targetMode);
    return;
  }

  UserActivityBrowserAgent* userActivityBrowserAgent =
      UserActivityBrowserAgent::FromBrowser(browser);
  userActivityBrowserAgent->ContinueUserActivity(_userActivity, YES);
}

- (void)executeFromColdStart {
  // TODO(crbug.com/462018636): Handle cold start with userActivity.
}

@end
