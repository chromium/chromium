// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/change_profile/change_profile_signout_continuation.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

@implementation ChangeProfileSignoutContinuation {
  signin_metrics::ProfileSignout _signoutSourceMetric;
  BOOL _forceClearData;
  BOOL _forceSnackbarOverToolbar;
  MDCSnackbarMessage* _snackbarMessage;
}

- (instancetype)initWithSignoutSourceMetric:
                    (signin_metrics::ProfileSignout)signoutSourceMetric
                             forceClearData:(BOOL)forceClearData
                   forceSnackbarOverToolbar:(BOOL)forceSnackbarOverToolbar
                            snackbarMessage:
                                (MDCSnackbarMessage*)snackbarMessage {
  self = [super init];
  if (self) {
    _signoutSourceMetric = signoutSourceMetric;
    _forceClearData = forceClearData;
    _forceSnackbarOverToolbar = forceSnackbarOverToolbar;
    _snackbarMessage = snackbarMessage;
  }
  return self;
}

#pragma mark - ChangeProfileContinuation

- (void)executeWithSceneState:(SceneState*)sceneState
                   completion:(ProceduralBlock)completion {
  Browser* browser =
      sceneState.browserProviderInterface.mainBrowserProvider.browser;
  CHECK(browser);
  // TODO(crbug.com/375605174): Complete the sign-out action.
}

@end
