// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/policy/user_policy/user_policy_prompt_mediator.h"

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/ui/policy/user_policy/user_policy_prompt_presenter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UserPolicyPromptMediator ()

// Presenter of the User Policy prompt view.
@property(nonatomic, weak, readonly) id<UserPolicyPromptPresenter> presenter;

// AuthenticationService for handling authentication (e.g. sign out).
@property(nonatomic, assign, readonly) AuthenticationService* authService;

@end

@implementation UserPolicyPromptMediator

- (instancetype)initWithPresenter:(id<UserPolicyPromptPresenter>)presenter
                      authService:(AuthenticationService*)authService {
  if (self = [super init]) {
    _presenter = presenter;
    _authService = authService;
  }
  return self;
}

- (void)stopPresentation {
  [self.presenter stopPresenting];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self stopPresentation];
}

- (void)confirmationAlertSecondaryAction {
  DCHECK(self.authService);
  __weak __typeof(self) weakSelf = self;
  [self.presenter showActivityOverlay];
  self.authService->SignOut(
      signin_metrics::ProfileSignout::
          kUserClickedSignoutFromUserPolicyNotificationDialog,
      false, ^{
        [weakSelf stopPresentation];
      });
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self stopPresentation];
}

@end
