// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/undo_signout/coordinator/undo_signout_coordinator.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow_delegate.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator+subclassing.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

@interface UndoSignoutCoordinator () <AuthenticationFlowDelegate>
@end

@implementation UndoSignoutCoordinator {
  id<SystemIdentity> _identity;
  AuthenticationFlow* _authenticationFlow;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                     regularBrowserOfScene:(SceneState*)sceneState
                                  identity:(id<SystemIdentity>)identity {
  CHECK(viewController);
  CHECK(identity);
  if ((self = [super initWithBaseViewController:viewController
                          regularBrowserOfScene:sceneState])) {
    _identity = identity;
  }
  return self;
}

- (void)start {
  CHECK(!_authenticationFlow);
  _authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:self.browser
                      identity:_identity
                   accessPoint:signin_metrics::AccessPoint::kSignoutUndoSnackbar
          precedingHistorySync:NO
             postSignInActions:PostSignInActionSet()
      presentingViewController:self.baseViewController
                    anchorView:nil
                    anchorRect:CGRectNull];
  _authenticationFlow.delegate = self;
  [_authenticationFlow startSignIn];
}

- (void)stop {
  [super stop];
  [_authenticationFlow interrupt];
  _authenticationFlow = nil;
}

#pragma mark - AuthenticationFlowDelegate

- (void)authenticationFlowDidSignInInSameProfileWithIdentity:
            (id<SystemIdentity>)identity
                                           cancelationReason:
                                               (signin_ui::CancelationReason)
                                                   cancelationReason
                                                  completion:(ProceduralBlock)
                                                                 completion {
  _authenticationFlow = nil;
  [self.delegate undoSignoutCoordinatorDidFinish:self];
  if (completion) {
    completion();
  }
}

- (void)authenticationFlowWillSwitchProfileWithReadyCompletion:
    (ReadyForProfileSwitchingCompletion)readyCompletion {
  _authenticationFlow = nil;
  [self.delegate undoSignoutCoordinatorDidFinish:self];
  std::move(readyCompletion).Run(ChangeProfileContinuation());
}

@end
