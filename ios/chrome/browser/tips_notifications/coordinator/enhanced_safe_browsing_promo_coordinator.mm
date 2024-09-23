// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/coordinator/enhanced_safe_browsing_promo_coordinator.h"

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/tips_notifications/ui/enhanced_safe_browsing_promo_instructions_view_controller.h"
#import "ios/chrome/browser/tips_notifications/ui/enhanced_safe_browsing_promo_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface EnhancedSafeBrowsingPromoCoordinator () <
    ConfirmationAlertActionHandler,
    PromoStyleViewControllerDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation EnhancedSafeBrowsingPromoCoordinator {
  EnhancedSafeBrowsingPromoViewController* _viewController;
  EnhancedSafeBrowsingPromoInstructionsViewController*
      _instructionsViewController;
  BOOL _showSettingsOnDismiss;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[EnhancedSafeBrowsingPromoViewController alloc] init];
  _viewController.delegate = self;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
  navigationController.presentationController.delegate = self;
  _showSettingsOnDismiss = NO;
}

- (void)stop {
  _instructionsViewController.actionHandler = nil;
  _instructionsViewController = nil;
  ProceduralBlock completion = nil;
  if (_showSettingsOnDismiss) {
    completion = ^{
      [self goToSettings];
    };
  }
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completion];
  _viewController = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  _showSettingsOnDismiss = YES;
  [self dismissScreen];
}

- (void)didTapSecondaryActionButton {
  _instructionsViewController =
      [[EnhancedSafeBrowsingPromoInstructionsViewController alloc] init];
  _instructionsViewController.actionHandler = self;
  _instructionsViewController.presentationController.delegate = self;
  [_viewController presentViewController:_instructionsViewController
                                animated:YES
                              completion:nil];
}

- (void)didDismissViewController {
  [self dismissScreen];
}

#pragma mark - ConfirmationAlertPrimaryAction

- (void)confirmationAlertPrimaryAction {
  _showSettingsOnDismiss = YES;
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
  [handler dismissEnhancedSafeBrowsingPromo];
}

// Opens the Safe Browsing Settings page in the app.
- (void)goToSettings {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [HandlerForProtocol(dispatcher, SettingsCommands) showSafeBrowsingSettings];
}

@end
