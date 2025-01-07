// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_signout_continuation.h"

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

namespace {

// Called by ChangeProfileSignoutContinuation once the sign-out is complete.
void SignoutDone(base::WeakPtr<Browser> weak_browser,
                 bool force_snackbar_over_toolbar,
                 MDCSnackbarMessage* snackbar_message) {
  Browser* browser = weak_browser.get();
  if (!browser) {
    return;
  }

  id<SnackbarCommands> snackbar_commands_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);
  if (force_snackbar_over_toolbar) {
    [snackbar_commands_handler
        showSnackbarMessageOverBrowserToolbar:snackbar_message];
  } else {
    [snackbar_commands_handler showSnackbarMessage:snackbar_message
                                      bottomOffset:0];
  }
}

}  // namespace

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
      sceneState.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  // Create the closure corresponding to the action to perform once the signout
  // action completes, chaining `_signoutCompletion` and `completion` if needed.
  base::OnceClosure closure =
      base::BindOnce(&SignoutDone, browser->AsWeakPtr(),
                     _forceSnackbarOverToolbar, _snackbarMessage);

  if (_signoutCompletion) {
    closure = std::move(closure).Then(base::BindOnce(_signoutCompletion));
  }
  if (completion) {
    closure = std::move(closure).Then(base::BindOnce(completion));
  }

  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(browser->GetProfile());

  authenticationService->SignOut(_signoutSourceMetric, _forceClearData,
                                 base::CallbackToBlock(std::move(closure)));

  signin_metrics::RecordSignoutForceClearDataChoice(_forceClearData);
  signin_metrics::RecordSignoutUserAction(_forceClearData);
}

@end
