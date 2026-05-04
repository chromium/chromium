// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/coordinator/gemini_entry_flow_coordinator.h"

#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"

@implementation GeminiEntryFlowCoordinator {
  // The sign-in coordinator presented when the user is signed out.
  SigninCoordinator* _signinCoordinator;
  // The startup state for the Gemini session.
  GeminiStartupState* _startupState;
  // The sign-in access point for metrics.
  signin_metrics::AccessPoint _accessPoint;
  // Whether to show a snackbar on ineligible completion.
  BOOL _showSnackbarOnCompletion;
  // Called with the final result of the flow.
  GeminiEntryFlowCompletion _completion;
}

#pragma mark - ChromeCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)baseViewController
                       browser:(Browser*)browser
                  startupState:(GeminiStartupState*)startupState
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
      showSnackbarOnCompletion:(BOOL)showSnackbarOnCompletion
                    completion:(GeminiEntryFlowCompletion)completion {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _startupState = startupState;
    _accessPoint = accessPoint;
    _showSnackbarOnCompletion = showSnackbarOnCompletion;
    _completion = [completion copy];
  }
  return self;
}

- (void)start {
  [super start];

  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());

  // If the user is already signed in, proceed to the next step.
  if (authService->HasPrimaryIdentity()) {
    // TODO(crbug.com/507509815): Add eligibility check.
    [self finishWithResult:kGeminiEntryFlowResultSuccess];
    return;
  }

  // User is signed out, present sign-in.
  [self presentSignIn];
}

- (void)stop {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
  [super stop];
}

#pragma mark - Private

// Presents the sign-in sheet.
- (void)presentSignIn {
  _signinCoordinator = [SigninCoordinator
      signinAndHistorySyncCoordinatorWithBaseViewController:
          self.baseViewController
                                                    browser:self.browser
                                               contextStyle:SigninContextStyle::
                                                                kDefault
                                                accessPoint:_accessPoint
                                                promoAction:
                                                    signin_metrics::PromoAction::
                                                        PROMO_ACTION_NO_SIGNIN_PROMO
                                        optionalHistorySync:YES
                                            fullscreenPromo:NO
                                       continuationProvider:
                                           DoNothingContinuationProvider()];
  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        [weakSelf signinDidFinishWithResult:result];
      };
  [_signinCoordinator start];
}

// Called when sign-in completes or is cancelled.
- (void)signinDidFinishWithResult:(SigninCoordinatorResult)result {
  [_signinCoordinator stop];
  _signinCoordinator = nil;

  if (result != SigninCoordinatorResultSuccess) {
    [self finishWithResult:kGeminiEntryFlowResultCancelled];
    return;
  }

  // TODO(crbug.com/507509815): Add eligibility check.
  [self finishWithResult:kGeminiEntryFlowResultSuccess];
}

// Completes the flow and calls the completion block.
- (void)finishWithResult:(GeminiEntryFlowResult)result {
  if (_completion) {
    _completion(result);
  }
}

@end
