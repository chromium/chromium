// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/coordinator/lens_promo_coordinator.h"

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/new_tab_page_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/tips_notifications/ui/lens_promo_instructions_view_controller.h"
#import "ios/chrome/browser/tips_notifications/ui/lens_promo_view_controller.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface LensPromoCoordinator () <ConfirmationAlertActionHandler,
                                    PromoStyleViewControllerDelegate,
                                    UIAdaptivePresentationControllerDelegate>
@end

@implementation LensPromoCoordinator {
  LensPromoViewController* _viewController;
  LensPromoInstructionsViewController* _instructionsViewController;
  BOOL _presentBubbleOnDismiss;
  BOOL _goToLensOnDismiss;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[LensPromoViewController alloc] init];
  _viewController.delegate = self;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
  navigationController.presentationController.delegate = self;
  _presentBubbleOnDismiss = NO;
  _goToLensOnDismiss = NO;
}

- (void)stop {
  _instructionsViewController.actionHandler = nil;
  _instructionsViewController = nil;
  ProceduralBlock completion = nil;
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  if (_goToLensOnDismiss) {
    OpenLensInputSelectionCommand* command =
        [self openLensInputSelectionCommand];
    id<LensCommands> handler = HandlerForProtocol(dispatcher, LensCommands);
    completion = ^{
      [handler openLensInputSelection:command];
    };
  } else if (_presentBubbleOnDismiss) {
    id<NewTabPageCommands> handler =
        HandlerForProtocol(dispatcher, NewTabPageCommands);
    completion = ^{
      [handler presentLensIconBubble];
    };
  }
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completion];
  _viewController = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  _goToLensOnDismiss = YES;
  [self dismissScreen];
}

- (void)didTapSecondaryActionButton {
  _instructionsViewController =
      [[LensPromoInstructionsViewController alloc] init];
  _instructionsViewController.actionHandler = self;
  _instructionsViewController.presentationController.delegate = self;
  [_viewController presentViewController:_instructionsViewController
                                animated:YES
                              completion:nil];
}

- (void)didDismissViewController {
  _presentBubbleOnDismiss = YES;
  [self dismissScreen];
}

#pragma mark - ConfirmationAlertPrimaryAction

- (void)confirmationAlertPrimaryAction {
  _goToLensOnDismiss = YES;
  [self dismissScreen];
}

- (void)confirmationAlertDismissAction {
  [_instructionsViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _instructionsViewController = nil;
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  if (presentationController.presentedViewController ==
      _instructionsViewController) {
    _instructionsViewController = nil;
  } else {
    // The UINavigationController was dismissed.
    [self dismissScreen];
  }
}
#pragma mark - Private methods

// Sends a command that will stop this coordinator and dismiss the screen.
- (void)dismissScreen {
  id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [handler dismissLensPromo];
}

// Returns a command object used to open Lens.
- (OpenLensInputSelectionCommand*)openLensInputSelectionCommand {
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:LensEntrypoint::NewTabPage
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromRight
      presentationCompletion:nil];
  command.presentNTPLensIconBubbleOnDismiss = YES;
  return command;
}

@end
