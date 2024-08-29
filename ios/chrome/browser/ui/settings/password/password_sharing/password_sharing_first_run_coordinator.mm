// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_first_run_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_first_run_action_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_first_run_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_first_run_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "url/gurl.h"

@interface PasswordSharingFirstRunCoordinator () <
    PasswordSharingFirstRunActionHandler>

// Main view controller for this coordinator.
@property(nonatomic, strong)
    PasswordSharingFirstRunViewController* viewController;

@end

@implementation PasswordSharingFirstRunCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  return self;
}

- (void)start {
  [super start];

  PasswordSharingFirstRunViewController* viewController =
      [[PasswordSharingFirstRunViewController alloc] init];
  viewController.actionHandler = self;
  viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  viewController.sheetPresentationController.detents = @[
    viewController.preferredHeightDetent,
    UISheetPresentationControllerDetent.largeDetent
  ];
  self.viewController = viewController;

  [self.baseViewController presentViewController:viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self stopWithCompletion:nil];
}

- (void)stopWithCompletion:(ProceduralBlock)completion {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completion];
  self.viewController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  LogPasswordSharingInteraction(
      PasswordSharingInteraction::kFirstRunShareClicked);

  [self.delegate passwordSharingFirstRunCoordinatorDidAccept:self];
}

- (void)confirmationAlertSecondaryAction {
  LogPasswordSharingInteraction(
      PasswordSharingInteraction::kFirstRunCancelClicked);

  [self.delegate passwordSharingFirstRunCoordinatorWasDismissed:self];
}

- (void)learnMoreLinkWasTapped {
  LogPasswordSharingInteraction(
      PasswordSharingInteraction::kFirstRunLearnMoreClicked);

  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:GURL(kPasswordSharingLearnMoreURL)];
  [handler closeSettingsUIAndOpenURL:command];

  [self.delegate passwordSharingFirstRunCoordinatorWasDismissed:self];
}

@end
