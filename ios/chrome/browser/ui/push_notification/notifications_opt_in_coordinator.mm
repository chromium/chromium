// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_coordinator.h"

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_coordinator_delegate.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_view_controller.h"

@interface NotificationsOptInCoordinator () <
    UIAdaptivePresentationControllerDelegate>

@end

@implementation NotificationsOptInCoordinator {
  NotificationsOptInViewController* _viewController;
}

- (void)start {
  _viewController = [[NotificationsOptInViewController alloc] init];
  _viewController.delegate = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [_viewController.presentingViewController dismissViewControllerAnimated:NO
                                                               completion:nil];
  _viewController = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/1519599): Check if user has enabled push notifications.
  __weak __typeof(self) weakSelf = self;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf.delegate
                               notificationsOptInScreenDidFinish:weakSelf];
                         }];
}

- (void)didTapSecondaryActionButton {
  __weak __typeof(self) weakSelf = self;
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf.delegate
                               notificationsOptInScreenDidFinish:weakSelf];
                         }];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate notificationsOptInScreenDidFinish:self];
}

@end
