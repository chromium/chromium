// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/policy_change_commands.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "url/gurl.h"

namespace {
constexpr CGFloat kHalfSheetCornerRadius = 20;
}  // namespace

@interface EnterprisePromptCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate>

// ViewController that contains enterprise prompt information.
@property(nonatomic, strong) EnterprisePromptViewController* viewController;

// PromptType that contains the type of the prompt to display.
@property(nonatomic, assign) EnterprisePromptType promptType;

@end

@implementation EnterprisePromptCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                promptType:(EnterprisePromptType)promptType {
  if ((self = [super initWithBaseViewController:baseViewController
                                        browser:browser])) {
    _promptType = promptType;
  }
  return self;
}

- (void)start {
  [super start];

  self.viewController = [[EnterprisePromptViewController alloc]
      initWithpromptType:self.promptType];
  self.viewController.presentationController.delegate = self;
  self.viewController.actionHandler = self;
  self.viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  UISheetPresentationController* presentationController =
      self.viewController.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.detents = @[
    UISheetPresentationControllerDetent.mediumDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  presentationController.preferredCornerRadius = kHalfSheetCornerRadius;

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self dismissSignOutViewController];
  self.viewController = nil;
  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.delegate hideEnterprisePrompForLearnMore:NO];
}

- (void)confirmationAlertSecondaryAction {
  [self.delegate hideEnterprisePrompForLearnMore:YES];
  [self openManagementPage];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate hideEnterprisePrompForLearnMore:NO];
}

#pragma mark - Private

// Removes view controller from display.
- (void)dismissSignOutViewController {
  if (self.viewController) {
    [self.baseViewController.presentedViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    self.viewController = nil;
  }
}

// Opens the management page in a new tab.
- (void)openManagementPage {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL(kChromeUIManagementURL)];
  id<ApplicationCommands> applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationHandler openURLInNewTab:command];
}

@end
