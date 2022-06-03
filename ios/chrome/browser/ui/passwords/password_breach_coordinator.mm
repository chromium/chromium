// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_coordinator.h"

#include "base/metrics/histogram_macros.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "components/password_manager/core/browser/ui/password_check_referrer.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/password_breach_commands.h"
#import "ios/chrome/browser/ui/passwords/password_breach_mediator.h"
#import "ios/chrome/browser/ui/passwords/password_breach_presenter.h"
#import "ios/chrome/browser/ui/passwords/password_breach_view_controller.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::CredentialLeakType;

@interface PasswordBreachCoordinator () <PasswordBreachPresenter>

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordBreachViewController* viewController;

// Popover used to show learn more info, not nil when presented.
@property(nonatomic, strong)
    PopoverLabelViewController* learnMoreViewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordBreachMediator* mediator;

// Leak type of the dialog.
@property(nonatomic, assign) CredentialLeakType leakType;

// Url needed for the dialog.
@property(nonatomic, assign) GURL url;

@end

@implementation PasswordBreachCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                  leakType:(CredentialLeakType)leakType {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _leakType = leakType;
  }
  return self;
}

- (void)start {
  self.viewController = [[PasswordBreachViewController alloc] init];
  self.viewController.modalPresentationStyle = UIModalPresentationFormSheet;
  self.viewController.modalInPresentation = YES;
  self.mediator =
      [[PasswordBreachMediator alloc] initWithConsumer:self.viewController
                                             presenter:self
                                              leakType:self.leakType];
  self.viewController.actionHandler = self.mediator;

  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  [super stop];
}

#pragma mark - PasswordBreachPresenter

- (void)presentLearnMore {
  NSString* message =
      l10n_util::GetNSString(IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE);
  self.learnMoreViewController =
      [[PopoverLabelViewController alloc] initWithMessage:message];
  [self.viewController presentViewController:self.learnMoreViewController
                                    animated:YES
                                  completion:nil];
  self.learnMoreViewController.popoverPresentationController.barButtonItem =
      self.viewController.helpButton;
  self.learnMoreViewController.popoverPresentationController
      .permittedArrowDirections = UIPopoverArrowDirectionUp;
}

- (void)startPasswordCheck {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  password_manager::LogPasswordCheckReferrer(
      password_manager::PasswordCheckReferrer::kPasswordBreachDialog);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kPasswordBreachDialog);
  [handler showSavedPasswordsSettingsAndStartPasswordCheckFromViewController:
               self.baseViewController];
}

@end
