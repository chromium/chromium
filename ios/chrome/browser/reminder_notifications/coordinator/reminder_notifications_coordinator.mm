// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_coordinator.h"

#import <UIKit/UIKit.h>

#import <algorithm>

#import "base/check.h"
#import "base/check_op.h"
#import "base/time/time.h"
#import "ios/chrome/browser/reminder_notifications/coordinator/reminder_notifications_mediator.h"
#import "ios/chrome/browser/reminder_notifications/ui/reminder_notifications_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/reminder_notifications_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"

@interface ReminderNotificationsCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation ReminderNotificationsCoordinator {
  ReminderNotificationsViewController* _viewController;
  ReminderNotificationsMediator* _mediator;
}

// Initializes and presents the "Set a reminder" UI.
- (void)start {
  _mediator = [[ReminderNotificationsMediator alloc]
      initWithProfilePrefService:self.profile->GetPrefs()];

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
  [_mediator disconnect];
  _mediator = nil;

  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

// Handles the primary action ("Set reminder") of the confirmation alert.
- (void)confirmationAlertPrimaryAction {
  web::WebState* webState = [self activeWebState];
  CHECK(webState);

  const GURL& url = webState->GetLastCommittedURL();

  // Check if the URL is valid and not an app-specific URL, with exceptions for
  // downloaded files or external file references.
  bool isURLValidForReminder =
      url.is_valid() &&
      (UrlIsDownloadedFile(url) || UrlIsExternalFileReference(url) ||
       !web::GetWebClient()->IsAppSpecificURL(url));

  if (!isURLValidForReminder) {
    // TODO(crbug.com/422449238): Consider adding UMA/logging for this failure
    // case.
    [self dismissScreen];
    return;
  }

  NSDate* selectedDate = _viewController.date;

  // Ensure the reminder time is not in the past. If the selected date is
  // before the current time, use the current time instead.
  const base::Time reminderTime =
      std::max(base::Time::FromNSDate(selectedDate), base::Time::Now());

  [_mediator setReminderForURL:url time:reminderTime];

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

// Returns the active web state. May return `nullptr`.
- (web::WebState*)activeWebState {
  WebStateList* webStateList = self.browser->GetWebStateList();

  return webStateList ? webStateList->GetActiveWebState() : nullptr;
}

@end
