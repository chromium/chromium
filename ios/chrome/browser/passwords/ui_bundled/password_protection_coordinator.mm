// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/password_protection_coordinator.h"

#import "base/check.h"
#import "base/notreached.h"
#import "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_protection_coordinator_delegate.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_protection_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface PasswordProtectionCoordinator () <ConfirmationAlertActionHandler>

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordProtectionViewController* viewController;

// The warning text to display.
@property(nonatomic, copy) NSString* warningText;

// The completion block.
@property(nonatomic, copy) void (^completion)(safe_browsing::WarningAction);

@end

@implementation PasswordProtectionCoordinator {
  // Navigation controller for the view controller.
  UINavigationController* _navigationController;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                               warningText:(NSString*)warningText {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _warningText = warningText;
  }
  return self;
}

- (void)startWithCompletion:(void (^)(safe_browsing::WarningAction))completion {
  DCHECK(completion);
  self.completion = completion;
  self.viewController = [[PasswordProtectionViewController alloc] init];
  self.viewController.subtitleString = self.warningText;
  self.viewController.actionHandler = self;

  self.viewController.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc]
          initWithBarButtonSystemItem:UIBarButtonSystemItemClose
                               target:self
                               action:@selector(dismiss)];

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.viewController];

  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.modalInPresentation = YES;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  _navigationController = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  self.completion(safe_browsing::WarningAction::CHANGE_PASSWORD);
  // Opening Password page will stop the presentation. No need to send `stop`.
  [self openSavedPasswordsSettings];
}

#pragma mark - Private

- (void)openSavedPasswordsSettings {
  id<SettingsCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);

  [handler
      showSavedPasswordsSettingsFromViewController:self.baseViewController];
}

// Dismisses the sheet.
- (void)dismiss {
  self.completion(safe_browsing::WarningAction::CLOSE);
  [self.delegate passwordProtectionCoordinatorWantsToBeStopped:self];
}

@end
