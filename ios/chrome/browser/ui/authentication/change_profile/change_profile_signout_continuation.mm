// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/change_profile/change_profile_signout_continuation.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

@implementation ChangeProfileSignoutContinuation {
  signin_metrics::ProfileSignout _signoutSourceMetric;
  BOOL _forceClearData;
  BOOL _forceSnackbarOverToolbar;
  MDCSnackbarMessage* _snackbarMessage;
  ProceduralBlock _signoutCompletion;
}

- (instancetype)initWithSignoutSourceMetric:
                    (signin_metrics::ProfileSignout)signoutSourceMetric
                             forceClearData:(BOOL)forceClearData
                   forceSnackbarOverToolbar:(BOOL)forceSnackbarOverToolbar
                            snackbarMessage:(MDCSnackbarMessage*)snackbarMessage
                          signoutCompletion:(ProceduralBlock)signoutCompletion {
  self = [super init];
  if (self) {
    _signoutSourceMetric = signoutSourceMetric;
    _forceClearData = forceClearData;
    _forceSnackbarOverToolbar = forceSnackbarOverToolbar;
    _snackbarMessage = snackbarMessage;
    _signoutCompletion = signoutCompletion;
  }
  return self;
}

#pragma mark - ChangeProfileContinuation

- (void)executeWithSceneState:(SceneState*)sceneState
                   completion:(ProceduralBlock)completion {
  Browser* browser =
      sceneState.browserProviderInterface.mainBrowserProvider.browser;
  CHECK(browser);

  id<SnackbarCommands> snackbarCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(browser->GetProfile());

  BOOL forceSnackbarOverToolbar = _forceSnackbarOverToolbar;
  MDCSnackbarMessage* snackbarMessage = _snackbarMessage;
  ProceduralBlock signoutCompletion = _signoutCompletion;

  authenticationService->SignOut(_signoutSourceMetric, _forceClearData, ^{
    if (forceSnackbarOverToolbar) {
      [snackbarCommandsHandler
          showSnackbarMessageOverBrowserToolbar:snackbarMessage];
    } else {
      [snackbarCommandsHandler showSnackbarMessage:snackbarMessage
                                      bottomOffset:0];
    }
    if (signoutCompletion) {
      signoutCompletion();
    }
  });

  signin_metrics::RecordSignoutForceClearDataChoice(_forceClearData);
  signin_metrics::RecordSignoutUserAction(_forceClearData);

  if (completion) {
    completion();
  }
}

@end
