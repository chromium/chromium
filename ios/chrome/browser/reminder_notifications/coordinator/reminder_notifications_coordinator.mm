// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reminder_notifications_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface ReminderNotificationsCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation ReminderNotificationsCoordinator {
  ReminderNotificationsViewController* _viewController;
}

// Initializes and presents the "Set a reminder" UI.
- (void)start {
  _viewController = [[ReminderNotificationsViewController alloc] init];
  _viewController.actionHandler = self;
  _viewController.presentationController.delegate = self;
  _viewController.sheetPresentationController
      .prefersEdgeAttachedInCompactHeight = YES;
  _viewController.sheetPresentationController.detents = @[
    [UISheetPresentationControllerDetent mediumDetent],
    [UISheetPresentationControllerDetent largeDetent],
  ];

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

// Cleans up and dismisses the "Set a reminder" UI.
- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

// Handles the primary action ("Set reminder") of the confirmation alert.
- (void)confirmationAlertPrimaryAction {
  // TODO(crbug.com/389911697): Create and schedule the Reminder notification.

  [self dismissScreen];
}

// Handles the secondary action ("Cancel") of the confirmation alert.
- (void)confirmationAlertSecondaryAction {
  [self dismissScreen];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

// Sends a command to stop this coordinator if certain conditions are met.
- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  if (presentationController.presentedViewController == _viewController) {
    _viewController = nil;
  }

  [self dismissScreen];
}

#pragma mark - Private methods

// Triggers dismissal of the reminder notifications UI by sending a command
// through the dispatcher. This ensures proper cleanup of the UI state.
- (void)dismissScreen {
  id<ReminderNotificationsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ReminderNotificationsCommands);
  [handler dismissSetTabReminderUI];
}

@end
