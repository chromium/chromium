// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator.h"

#import "base/check.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/weak_ptr.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/web/public/web_state.h"
#import "mojo/public/cpp/bindings/callback_helpers.h"
#import "ui/base/l10n/l10n_util.h"

@interface RepostFormCoordinator () {
  // WebState which requested this dialog.
  base::WeakPtr<web::WebState> _webState;
  // View Controller representing the dialog.
  UIAlertController* _dialogController;
  // Number of attempts to show the repost form action sheet.
  NSUInteger _repostAttemptCount;
  // A completion handler to be called when the dialog is dismissed.
  base::OnceCallback<void(BOOL)> _completion;
}

// Creates a new UIAlertController to use for the dialog. The returned
// alert controller invokes -onDialogCompletion: with YES/NO depending
// on which action has been selected by the user.
- (UIAlertController*)newDialogControllerForSourceView:(UIView*)sourceView
                                            sourceRect:(CGRect)sourceRect;

// Method invoked when the user tap on an action in the UIAlerController
// returned by -newDialogControllerForSourceView:sourceRect:.
- (void)onDialogCompletion:(BOOL)success;

@end

@implementation RepostFormCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                            dialogLocation:(CGPoint)dialogLocation
                                  webState:(web::WebState*)webState
                         completionHandler:(void (^)(BOOL))completionHandler {
  DCHECK(browser);
  DCHECK(webState);
  DCHECK(completionHandler);

  // The `completionHandler` must be called even if the object is destroyed.
  // Wrap it with `mojo::WrapCallbackWithDefaultInvokeIfNotRun(...)` to get
  // a callback that will be called if destroyed without calling `Run(...)`
  // on it. Create the wrapper before calling the super class initializer,
  // so that the callback is invoked even if the initializer fails.
  base::OnceCallback<void(BOOL)> completion =
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(completionHandler), NO);

  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _webState = webState->GetWeakPtr();
    _completion = std::move(completion);

    CGRect sourceRect = CGRectMake(dialogLocation.x, dialogLocation.y, 1, 1);
    _dialogController =
        [self newDialogControllerForSourceView:viewController.view
                                    sourceRect:sourceRect];
  }
  return self;
}

- (void)start {
  // The WebState may have been destroyed since the RepostFormCoordinator was
  // created, in that case, there is nothing to do (as the tab would have been
  // closed).
  web::WebState* webState = _webState.get();
  if (!webState || !webState->IsWebUsageEnabled()) {
    return;
  }

  // Check to see if an action sheet can be shown.
  if (self.baseViewController.view.window &&
      !self.baseViewController.presentedViewController) {
    [self.baseViewController presentViewController:_dialogController
                                          animated:YES
                                        completion:nil];
    _repostAttemptCount = 0;
    return;
  }

  // The resubmit data action cannot be presented as the view was not
  // yet added to the window or another VC is being presented. Retry after
  // `kDelayBetweenAttemptsNanoSecs`.
  const NSUInteger kMaximumNumberAttempts = 10;
  // 400 milliseconds
  const int64_t kDelayBetweenAttemptsNanoSecs = 0.4 * NSEC_PER_SEC;
  if (_repostAttemptCount >= kMaximumNumberAttempts) {
    [self.delegate repostFormCoordinatorWantsToBeDismissed:self];
    return;
  }
  __weak RepostFormCoordinator* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kDelayBetweenAttemptsNanoSecs),
      dispatch_get_main_queue(), ^{
        [weakSelf start];
      });
  _repostAttemptCount++;
}

- (void)stop {
  // If `-stop` is called before `-onDialogCompletion:`, creates a block that
  // will invoke `_completion` with `NO`. Otherwise, there is no need for a
  // completion block for the -dismissViewControllerAnimated:completion:.
  void (^completion)() = nil;
  if (_completion) {
    completion =
        base::CallbackToBlock(base::BindOnce(std::move(_completion), NO));
  }

  [_dialogController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completion];
  _repostAttemptCount = 0;
  _webState.reset();
}

#pragma mark - Private

- (UIAlertController*)newDialogControllerForSourceView:(UIView*)sourceView
                                            sourceRect:(CGRect)sourceRect {
  NSString* message = [NSString
      stringWithFormat:@"%@\n\n%@",
                       l10n_util::GetNSString(IDS_HTTP_POST_WARNING_TITLE),
                       l10n_util::GetNSString(IDS_HTTP_POST_WARNING)];

  UIAlertController* result = [UIAlertController
      alertControllerWithTitle:nil
                       message:message
                preferredStyle:UIAlertControllerStyleActionSheet];

  // The action must not extend the lifetime of `self`.
  __weak __typeof(self) weakSelf = self;

  NSString* cancelTitle = l10n_util::GetNSString(IDS_CANCEL);
  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:cancelTitle
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* _Nonnull action) {
                               [weakSelf onDialogCompletion:NO];
                             }];

  NSString* continueTitle =
      l10n_util::GetNSString(IDS_HTTP_POST_WARNING_RESEND);
  UIAlertAction* continueAction =
      [UIAlertAction actionWithTitle:continueTitle
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               [weakSelf onDialogCompletion:YES];
                             }];

  [result addAction:cancelAction];
  [result addAction:continueAction];

  result.modalPresentationStyle = UIModalPresentationPopover;
  result.popoverPresentationController.sourceView = sourceView;
  result.popoverPresentationController.sourceRect = sourceRect;

  return result;
}

- (void)onDialogCompletion:(BOOL)success {
  if (_completion) {
    std::move(_completion).Run(success);
  }
}

@end
