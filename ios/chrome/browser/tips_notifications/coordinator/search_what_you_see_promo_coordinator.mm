// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tips_notifications/coordinator/search_what_you_see_promo_coordinator.h"

#import "base/notreached.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/tips_notifications/ui/search_what_you_see_promo_instructions_view_controller.h"
#import "ios/chrome/browser/tips_notifications/ui/search_what_you_see_promo_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "url/gurl.h"

@interface SearchWhatYouSeePromoCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation SearchWhatYouSeePromoCoordinator {
  SearchWhatYouSeePromoViewController* _viewController;
  SearchWhatYouSeePromoInstructionsViewController* _instructionsViewController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _viewController = [[SearchWhatYouSeePromoViewController alloc] init];
  _viewController.actionHandler = self;

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
  navigationController.presentationController.delegate = self;
}

- (void)stop {
  _instructionsViewController.actionHandler = nil;
  _viewController.actionHandler = nil;

  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];

  _instructionsViewController = nil;
  _viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  NOTREACHED();
}

- (void)confirmationAlertSecondaryAction {
  if (_viewController.presentedViewController &&
      _viewController.presentedViewController == _instructionsViewController) {
    [self openURLInNewTab:GURL(kLearnMoreLensURL)];

    return;
  }

  _instructionsViewController =
      [[SearchWhatYouSeePromoInstructionsViewController alloc] init];
  _instructionsViewController.actionHandler = self;
  _instructionsViewController.presentationController.delegate = self;
  [_viewController presentViewController:_instructionsViewController
                                animated:YES
                              completion:nil];
}

- (void)confirmationAlertDismissAction {
  if (_viewController.presentedViewController &&
      _viewController.presentedViewController == _instructionsViewController) {
    _instructionsViewController.actionHandler = nil;
    _instructionsViewController = nil;

    [_viewController dismissViewControllerAnimated:YES completion:nil];

    return;
  }

  [self dismiss];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  if (presentationController.presentedViewController &&
      presentationController.presentedViewController ==
          _instructionsViewController) {
    _instructionsViewController.actionHandler = nil;
    _instructionsViewController = nil;

    return;
  }

  [self dismiss];
}

#pragma mark - Private methods

// Sends a command that will stop this coordinator and dismiss the promo.
- (void)dismiss {
  id<BrowserCoordinatorCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [handler dismissSearchWhatYouSeePromo];
}

// Opens a given URL in a new tab.
- (void)openURLInNewTab:(GURL)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];

  [HandlerForProtocol(self.browser->GetCommandDispatcher(), ApplicationCommands)
      openURLInNewTab:command];
}

@end
