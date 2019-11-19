// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/alert_coordinator/repost_form_coordinator.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/dialogs/completion_block_util.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using completion_block_util::DecidePolicyCallback;
using completion_block_util::GetSafeDecidePolicyCompletion;

@interface RepostFormCoordinator () {
  // WebState which requested this dialog.
  web::WebState* _webState;
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
                            dialogLocation:(CGPoint)dialogLocation
                                  webState:(web::WebState*)webState
                         completionHandler:(void (^)(BOOL))completionHandler {
  DCHECK(webState);
  DCHECK(completionHandler);
  self = [super
      initWithBaseViewController:viewController
                    browserState:ios::ChromeBrowserState::FromBrowserState(
                                     webState->GetBrowserState())];
  if (self) {
    _webState = webState;
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
  if (!_webState->IsWebUsageEnabled())
    return;

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
  // yet added to the window. Retry after |kDelayBetweenAttemptsNanoSecs|.
  // TODO(crbug.com/227868): The strategy to poll until the resubmit data action
  // sheet can be presented is a temporary workaround. This needs to be
  // refactored to match the Chromium implementation:
  // * web_controller should notify/ the BVC once an action sheet should be
  //   shown.
  // * BVC should present the action sheet and then trigger the reload
  const NSUInteger kMaximumNumberAttempts = 10;
  // 400 milliseconds
  const int64_t kDelayBetweenAttemptsNanoSecs = 0.4 * NSEC_PER_SEC;
  if (_repostAttemptCount >= kMaximumNumberAttempts) {
    NOTREACHED();
    [self stop];
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
