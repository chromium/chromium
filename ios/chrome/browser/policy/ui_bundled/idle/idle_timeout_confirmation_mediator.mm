// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_confirmation_mediator.h"

#import "base/timer/timer.h"
#import "ios/chrome/browser/policy/ui_bundled/idle/idle_timeout_cofirmation_presenter.h"

@implementation IdleTimeoutConfirmationMediator {
  // Repeating timer for updating the countdown every 1 second.
  base::RepeatingTimer _updateTimer;
  // Timer for dialog expiry.
  base::OneShotTimer _dialogTimer;
  // Time when the dialog is expected to expire.
  base::TimeTicks _deadline;
  // The time the dialog needs to be shown for. This is needed in case the
  // dialog needs to continue being displayed on a different scene after the
  // scene displaying the dialog is closed and not just minimized.
  base::TimeDelta _dialogDuration;

  // Presenter of the confirmation dialog. The presenter needs to stop showing
  // the dialog when it times out.
  id<IdleTimeoutConfirmationPresenter> _presenter;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // Stop all timers and trigger the presenter to dismiss the dialog.
  [self stop];
  [_presenter stopPresentingAfterUserClickedContinue];
}

#pragma mark - IdleTimeoutConfirmationConsumer

- (void)setConsumer:(id<IdleTimeoutConfirmationConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  __weak __typeof(self) weakSelf = self;
  _dialogTimer.Start(FROM_HERE, _dialogDuration, base::BindOnce(^{
                       [weakSelf OnDialogExpired];
                     }));
  _deadline = base::TimeTicks::Now() + _dialogDuration;
  // Update the dialog with the remaining time before it is presented or else it
  // will be empty when the dialog is first presented.
  [self updateConsumerCountdown];
  _updateTimer.Start(FROM_HERE, base::Seconds(1), base::BindRepeating((^{
                       [weakSelf updateConsumerCountdown];
                     })));
}

#pragma mark - Internal

- (instancetype)initWithPresenter:
                    (id<IdleTimeoutConfirmationPresenter>)presenter
                   dialogDuration:(base::TimeDelta)duration {
  if ((self = [super init])) {
    _presenter = presenter;
    _dialogDuration = duration;
  }
  return self;
}

- (void)updateConsumerCountdown {
  base::TimeDelta delay =
      std::max(base::TimeDelta(), _deadline - base::TimeTicks::Now());
  [_consumer setCountdown:delay];
}

- (void)OnDialogExpired {
  // Stop all timers and trigger the presenter to dismiss the dialog after no
  // user action.
  [self stop];
  [_presenter stopPresentingAfterDialogExpired];
}

- (void)stop {
  // No-op if timers are already stopped.
  _updateTimer.Stop();
  _dialogTimer.Stop();
}

@end
