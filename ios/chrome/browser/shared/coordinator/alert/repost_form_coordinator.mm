// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator.h"

#import "base/check.h"
#import "base/memory/weak_ptr.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/repost_form_coordinator_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/dialogs/completion_block_util.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

using completion_block_util::DecidePolicyCallback;
using completion_block_util::GetSafeDecidePolicyCompletion;

@interface RepostFormCoordinator () {
  // WebState which requested this dialog.
  base::WeakPtr<web::WebState> _webState;
  // View Controller representing the dialog.
  UIAlertController* _dialogController;
  // Number of attempts to show the repost form action sheet.
  NSUInteger _repostAttemptCount;
  // A completion handler to be called when the dialog is dismissed.
  void (^_dismissCompletionHandler)(void);
}

// Creates a new UIAlertController to use for the dialog.
+ (UIAlertController*)newDialogControllerForSourceView:(UIView*)sourceView
                                            sourceRect:(CGRect)sourceRect
                                     completionHandler:
                                         (void (^)(BOOL))completionHandler;

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
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _webState = webState->GetWeakPtr();
    CGRect sourceRect = CGRectMake(dialogLocation.x, dialogLocation.y, 1, 1);
    DecidePolicyCallback safeCallback =
        GetSafeDecidePolicyCompletion(completionHandler);
    _dialogController =
        [[self class] newDialogControllerForSourceView:viewController.view
                                            sourceRect:sourceRect
                                     completionHandler:safeCallback];
    // The dialog may be dimissed when a new navigation starts while the dialog
    // is still presenting. This should be treated as a NO from user.
    // See https://crbug.com/854750 for a case why this matters.
    _dismissCompletionHandler = ^{
      safeCallback(NO);
    };
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
  [_dialogController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:_dismissCompletionHandler];
  _repostAttemptCount = 0;
  _dismissCompletionHandler = nil;
  _webState.reset();
}

#pragma mark - Private

+ (UIAlertController*)newDialogControllerForSourceView:(UIView*)sourceView
                                            sourceRect:(CGRect)sourceRect
                                     completionHandler:
                                         (void (^)(BOOL))completionHandler {
  NSString* message = [NSString
      stringWithFormat:@"%@\n\n%@",
                       l10n_util::GetNSString(IDS_HTTP_POST_WARNING_TITLE),
                       l10n_util::GetNSString(IDS_HTTP_POST_WARNING)];
  NSString* buttonTitle = l10n_util::GetNSString(IDS_HTTP_POST_WARNING_RESEND);
  NSString* cancelTitle = l10n_util::GetNSString(IDS_CANCEL);

  UIAlertController* result = [UIAlertController
      alertControllerWithTitle:nil
                       message:message
                preferredStyle:UIAlertControllerStyleActionSheet];
  // Make sure the block is located on the heap.
  completionHandler = [completionHandler copy];

  UIAlertAction* cancelAction =
      [UIAlertAction actionWithTitle:cancelTitle
                               style:UIAlertActionStyleCancel
                             handler:^(UIAlertAction* _Nonnull action) {
                               completionHandler(NO);
                             }];
  [result addAction:cancelAction];
  UIAlertAction* continueAction =
      [UIAlertAction actionWithTitle:buttonTitle
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* _Nonnull action) {
                               completionHandler(YES);
                             }];
  [result addAction:continueAction];

  result.modalPresentationStyle = UIModalPresentationPopover;
  result.popoverPresentationController.sourceView = sourceView;
  result.popoverPresentationController.sourceRect = sourceRect;

  return result;
}

@end
