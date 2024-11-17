// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/enterprise/idle/idle_pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service.h"
#import "ios/chrome/browser/enterprise/model/idle/idle_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_cofirmation_presenter.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_coordinator_delegate.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_mediator.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_view_controller.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_policy_utils.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20.0;
constexpr base::TimeDelta kDialogTimeout = base::Seconds(30);
}  // namespace

@interface IdleTimeoutConfirmationCoordinator () <
    IdleTimeoutConfirmationPresenter>
@end

@implementation IdleTimeoutConfirmationCoordinator {
  // View controller for the idle timeout confirmation dialog.
  IdleTimeoutConfirmationViewController* _presentedViewController;

  // Mediator for the idle timeout confirmation dialog ro respond to cancel and
  // UI timeout.
  IdleTimeoutConfirmationMediator* _mediator;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _mediator = [[IdleTimeoutConfirmationMediator alloc]
      initWithPresenter:self
         dialogDuration:[self countDownStart]];

  enterprise_idle::IdleService* idleService = [self idleService];
  AuthenticationService* authService = [self authService];

  enterprise_idle::ActionSet actions = idleService->GetLastActionSet();
  std::optional<int> titleId =
      enterprise_idle::GetIdleTimeoutActionsTitleId(actions);
  CHECK(titleId)
      << "The idle timeout confirmation dialog title id should not be empty";
  int subtitleId = enterprise_idle::GetIdleTimeoutActionsSubtitleId(
      actions, authService->ShouldClearDataForSignedInPeriodOnSignOut());

  _presentedViewController = [[IdleTimeoutConfirmationViewController alloc]
      initWithIdleTimeoutTitleId:*titleId
           idleTimeoutSubtitleId:subtitleId
            idleTimeoutThreshold:[self idleTimeout]];
  _presentedViewController.actionHandler = _mediator;
  _mediator.consumer = _presentedViewController;

  _presentedViewController.modalPresentationStyle =
      UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      _presentedViewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;

  _presentedViewController.modalInPresentation = YES;
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController presentViewController:_presentedViewController
                                        animated:YES
                                      completion:^{
                                        [weakSelf setInitialVoiceOverFocus];
                                      }];
}

- (void)stop {
  // Reset the timers before dismising the view. This is needed for the case
  // when the app is backgrounded because the mediator does not stop the timers
  // in this case before this funciton is called.
  [_mediator stop];
  if (_presentedViewController) {
    [self.baseViewController.presentedViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    _presentedViewController = nil;
  }
}

#pragma mark - IdleTimeoutConfirmationPresenter

- (void)stopPresentingAfterUserClickedContinue {
  [self.delegate stopPresentingAndRunActionsAfterwards:false];
}

- (void)stopPresentingAfterDialogExpired {
  [self.delegate stopPresentingAndRunActionsAfterwards:true];
}

#pragma mark - Private

// Returns the idle timeout the admin has set.
- (int)idleTimeout {
  return self.browser->GetProfile()
      ->GetPrefs()
      ->GetTimeDelta(enterprise_idle::prefs::kIdleTimeout)
      .InMinutes();
}

// Returns the time delta from which the remaining time countdown should start.
// The time is usually 30 seconds. However, this is used to cover the
// multi-window case when the dialog is reshown on a new window after the first
// one has been closed.
- (base::TimeDelta)countDownStart {
  return kDialogTimeout - (base::Time::Now() - self.triggerTime);
}

- (enterprise_idle::IdleService*)idleService {
  return enterprise_idle::IdleServiceFactory::GetForProfile(
      self.browser->GetProfile());
}

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForProfile(
      self.browser->GetProfile());
}

- (void)setInitialVoiceOverFocus {
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  _presentedViewController.image);
}

@end
