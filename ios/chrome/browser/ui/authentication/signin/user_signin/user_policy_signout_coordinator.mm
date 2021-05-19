// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_policy_signout_coordinator.h"

#include "base/notreached.h"
#import "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_policy_signout_view_controller.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/policy_signout_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface UserPolicySignoutCoordinator () <ConfirmationAlertActionHandler>

// The fullscreen sign-out prompt view controller this coordiantor
// manages.
@property(nonatomic, strong)
    UserPolicySignoutViewController* policySignoutViewController;

@end

@implementation UserPolicySignoutCoordinator

#pragma mark - Public Methods.

- (void)start {
  [super start];
  self.policySignoutViewController =
      [[UserPolicySignoutViewController alloc] init];
  self.policySignoutViewController.actionHandler = self;
  self.policySignoutViewController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  [self.baseViewController
      presentViewController:self.policySignoutViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [self.policySignoutViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.policySignoutViewController = nil;
  [super stop];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertDismissAction {
  // There should be no cancel toolbar button for this UI.
  NOTREACHED();
}

- (void)confirmationAlertPrimaryAction {
  [self.signoutPromptHandler hidePolicySignoutPrompt];
}

- (void)confirmationAlertSecondaryAction {
  [self.signoutPromptHandler hidePolicySignoutPrompt];
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL(kChromeUIManagementURL)];
  command.userInitiated = YES;
  [self.applicationHandler openURLInNewTab:command];
}

- (void)confirmationAlertTertiaryAction {
  // There should be no tertiary action button for this UI.
  NOTREACHED();
}

- (void)confirmationAlertLearnMoreAction {
  // There should be no Learn More action button for this UI.
  NOTREACHED();
}

@end