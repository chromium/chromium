// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/coordinator/tab_groups_promo_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/tips_notifications/model/utils.h"
#import "ios/chrome/browser/tips_notifications/ui/tab_groups_promo_view_controller.h"
#import "ios/chrome/browser/tips_notifications/ui/tips_promo_view_controller.h"
#import "ios/chrome/common/ui/button_stack/button_stack_action_delegate.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface TabGroupsPromoCoordinator () <
    ButtonStackActionDelegate,
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation TabGroupsPromoCoordinator {
  TabGroupsPromoViewController* _viewController;
  BOOL _showTabGroupsPageOnDismiss;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[TabGroupsPromoViewController alloc] init];
  _viewController.actionDelegate = self;

  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissViewController)];
  _viewController.navigationItem.rightBarButtonItem = dismissButton;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
  _showTabGroupsPageOnDismiss = NO;
}

- (void)stop {
  ProceduralBlock completion = nil;
  if (_showTabGroupsPageOnDismiss) {
    CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
    id<SceneCommands> handler = HandlerForProtocol(dispatcher, SceneCommands);
    completion = ^{
      [handler displayTabGridInMode:TabGridOpeningMode::kTabGroups];
    };
  }

  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completion];
  _viewController = nil;
  _showTabGroupsPageOnDismiss = NO;
}

#pragma mark - ButtonStackActionDelegate

- (void)didTapPrimaryActionButton {
  _showTabGroupsPageOnDismiss = YES;
  LogTipsNotificationPromoAction(TipsNotificationType::kTabGroups,
                                 TipsNotificationPromoAction::kPrimary);
  [self dismissScreen];
}

- (void)didTapSecondaryActionButton {
  LogTipsNotificationPromoAction(TipsNotificationType::kTabGroups,
                                 TipsNotificationPromoAction::kSecondary);
  [self dismissScreen];
}

- (void)didTapTertiaryActionButton {
  // Not used.
}

#pragma mark - ConfirmationAlertPrimaryAction

- (void)confirmationAlertPrimaryAction {
  _showTabGroupsPageOnDismiss = YES;
  [self dismissScreen];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self dismissScreen];
}

#pragma mark - Private methods

// Dismisses the coordinator and its view controller.
- (void)dismissViewController {
  [self dismissScreen];
}

// Sends a command that will stop this coordinator and dismiss the screen.
- (void)dismissScreen {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> handler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  [handler dismissTabGroupsPromo];
}

@end
