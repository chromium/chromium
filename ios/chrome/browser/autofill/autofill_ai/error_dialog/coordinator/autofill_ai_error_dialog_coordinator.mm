// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/error_dialog/coordinator/autofill_ai_error_dialog_coordinator.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/location.h"
#import "base/time/time.h"
#import "base/timer/timer.h"
#import "ios/chrome/browser/autofill/autofill_ai/error_dialog/coordinator/autofill_ai_error_dialog_mediator.h"
#import "ios/chrome/browser/autofill/autofill_ai/error_dialog/coordinator/autofill_ai_error_dialog_mediator_delegate.h"
#import "ios/chrome/browser/autofill/autofill_ai/error_dialog/model/autofill_ai_error_dialog_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/autofill_commands.h"

namespace {
// Delay for retrying to present error dialog when its presenting view
// controller already has a presented view controller.
inline constexpr base::TimeDelta kErrorDialogPresentationRetryDelay =
    base::Milliseconds(500);
}  // namespace

@interface AutofillAiPendingAlert : NSObject
@property(nonatomic, readonly, copy) NSString* title;
@property(nonatomic, readonly, copy) NSString* message;
@property(nonatomic, readonly, copy) NSString* buttonLabel;
@property(nonatomic, readonly, assign) BOOL showImmediately;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithTitle:(NSString*)title
                      message:(NSString*)message
                  buttonLabel:(NSString*)buttonLabel
              showImmediately:(BOOL)showImmediately
                   completion:(base::OnceClosure)completion
    NS_DESIGNATED_INITIALIZER;

- (void)runCompletion;
@end

@implementation AutofillAiPendingAlert {
  base::OnceClosure _completion;
}

- (instancetype)initWithTitle:(NSString*)title
                      message:(NSString*)message
                  buttonLabel:(NSString*)buttonLabel
              showImmediately:(BOOL)showImmediately
                   completion:(base::OnceClosure)completion {
  self = [super init];
  if (self) {
    _title = [title copy];
    _message = [message copy];
    _buttonLabel = [buttonLabel copy];
    _showImmediately = showImmediately;
    _completion = std::move(completion);
  }
  return self;
}

- (void)runCompletion {
  if (!_completion.is_null()) {
    std::move(_completion).Run();
  }
}

@end

@interface AutofillAiErrorDialogCoordinator () <
    AutofillAiErrorDialogMediatorDelegate>

@end

@implementation AutofillAiErrorDialogCoordinator {
  // The C++ mediator class that connects the context data to the IOS view
  // implementation.
  std::unique_ptr<AutofillAiErrorDialogMediator> _mediator;

  __weak UIAlertController* _alertController;

  // Pending alerts. Processed as a FIFO queue. New alerts are inserted at index
  // 0. Alerts are processed from the last object.
  NSMutableArray<AutofillAiPendingAlert*>* _pendingAlerts;

  // Used to schedule trying to present pending alerts.
  base::OneShotTimer _pendingAlertsTimer;

  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              errorContext:
                                  (autofill::AutofillAiErrorDialogContext)
                                      errorContext {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _mediator = std::make_unique<AutofillAiErrorDialogMediator>(
        std::move(errorContext), self);
    _pendingAlerts = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)dealloc {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
}

#pragma mark - ChromeCoordinator

- (void)start {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _mediator->Show();
}

- (void)stop {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _pendingAlertsTimer.Stop();
  [_pendingAlerts removeAllObjects];
  _pendingAlerts = nil;

  // It is safe to send messages to nil in Objective-C.
  [_alertController dismissViewControllerAnimated:YES completion:nil];
  _alertController = nil;
}

#pragma mark - AutofillAiErrorDialogMediatorDelegate

- (void)showErrorDialog:(NSString*)title
                message:(NSString*)message
            buttonLabel:(NSString*)buttonLabel
        showImmediately:(BOOL)showImmediately
             completion:(base::OnceClosure)completion {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  AutofillAiPendingAlert* alert =
      [[AutofillAiPendingAlert alloc] initWithTitle:title
                                            message:message
                                        buttonLabel:buttonLabel
                                    showImmediately:showImmediately
                                         completion:std::move(completion)];

  // Use _pendingAlerts as a FIFO queue: insert at index 0 (front), process from
  // lastObject (back).
  if (showImmediately) {
    [_pendingAlerts addObject:alert];
  } else {
    [_pendingAlerts insertObject:alert atIndex:0];
  }
  [self tryPresentPendingAlert];
}

#pragma mark - Private

- (void)scheduleTryPresentPendingAlert {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  // Already scheduled, ignore.
  if (_pendingAlertsTimer.IsRunning()) {
    return;
  }

  // Ensure weakSelf is used in the BindOnce callback.
  __weak __typeof__(self) weakSelf = self;
  _pendingAlertsTimer.Start(FROM_HERE, kErrorDialogPresentationRetryDelay,
                            base::BindOnce(^{
                              [weakSelf tryPresentPendingAlert];
                            }));
}

- (void)tryPresentPendingAlert {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  // If no pending alerts, or no baseViewController, do nothing.
  if (_pendingAlerts.count == 0 || !self.baseViewController) {
    return;
  }

  // Presenter is not busy. Get the next alert to show.
  AutofillAiPendingAlert* alertToShow = [_pendingAlerts lastObject];

  UIViewController* presenter = self.baseViewController;
  if (alertToShow.showImmediately) {
    // Bypass the queue delay. Find the top-most view controller to present
    // over.
    while (presenter.presentedViewController &&
           !presenter.presentedViewController.isBeingDismissed) {
      presenter = presenter.presentedViewController;
    }
  } else if (presenter.presentedViewController) {
    [self scheduleTryPresentPendingAlert];
    return;
  }

  UIAlertController* alertController =
      [UIAlertController alertControllerWithTitle:alertToShow.title
                                          message:alertToShow.message
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak __typeof__(self) weakSelf = self;
  UIAlertAction* buttonAction =
      [UIAlertAction actionWithTitle:alertToShow.buttonLabel
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* action) {
                               [weakSelf userDismissedAlert];
                               if (alertToShow) {
                                 [alertToShow runCompletion];
                               }
                             }];
  [alertController addAction:buttonAction];
  alertController.modalPresentationStyle = UIModalPresentationOverFullScreen;
  [_pendingAlerts removeLastObject];

  [presenter presentViewController:alertController animated:YES completion:nil];
  _alertController = alertController;
}

- (void)userDismissedAlert {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  _alertController = nil;

  // If there are any more pending alerts, try to display the next one.
  if (_pendingAlerts.count > 0) {
    [self tryPresentPendingAlert];
  } else {
    [self dismissViewController];
  }
}

- (void)dismissViewController {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  [_autofillCommandsHandler dismissAutofillAiErrorDialog];
}

@end
