// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_view_controller.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

@interface ReauthenticationCoordinator () <
    ReauthenticationViewControllerDelegate>

// Module used for requesting Local Authentication.
@property(nonatomic, strong) id<ReauthenticationProtocol> reauthModule;

// Application Commands dispatcher for closing settings ui and opening tabs.
@property(nonatomic, strong) id<ApplicationCommands> dispatcher;

// The view controller presented by the coordinator.
@property(nonatomic, strong)
    ReauthenticationViewController* reauthViewController;

// Coordinator for displaying an alert requesting the user to set up a
// passcode.
@property(nonatomic, strong) AlertCoordinator* passcodeRequestAlertCoordinator;

@end

@implementation ReauthenticationCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                          reauthenticationModule:(id<ReauthenticationProtocol>)
                                                     reauthenticationModule {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(reauthenticationModule);
    _reauthModule = reauthenticationModule;
    _baseNavigationController = navigationController;
    _dispatcher =
        static_cast<id<ApplicationCommands>>(browser->GetCommandDispatcher());
  }

  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  _reauthViewController = [[ReauthenticationViewController alloc]
      initWithReauthenticationModule:_reauthModule];
  _reauthViewController.delegate = self;

  // Don't animate presentation to block top view controller right away.
  [_baseNavigationController pushViewController:_reauthViewController
                                       animated:NO];
}

- (void)stop {
  _reauthViewController.delegate = nil;
  _reauthViewController = nil;
}

#pragma mark - ReauthenticationViewControllerDelegate

// Creates and displays an alert requesting the user to set a passcode.
- (void)showSetUpPasscodeDialog {
  // TODO(crbug.com/1462419): Open iOS Passcode Settings for phase 2 launch in
  // M118. See i/p/p/c/b/password_auto_fill/password_auto_fill_api.h for
  // reference.
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE);
  NSString* message =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_CONTENT);
  _passcodeRequestAlertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:_reauthViewController
                                                   browser:self.browser
                                                     title:title
                                                   message:message];

  __weak __typeof(self) weakSelf = self;

  // Action OK -> Close settings.
  [_passcodeRequestAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_OK)
                action:^{
                  [weakSelf.dispatcher closeSettingsUI];
                }
                 style:UIAlertActionStyleCancel];

  // Action Learn How -> Close settings and open passcode help page.
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL(kPasscodeArticleURL)];

  [_passcodeRequestAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                action:^{
                  [weakSelf.dispatcher closeSettingsUIAndOpenURL:command];
                }
                 style:UIAlertActionStyleDefault];

  [_passcodeRequestAlertCoordinator start];
}

- (void)reauthenticationDidFinishWithSuccess:(BOOL)success {
  if (success) {
    [_baseNavigationController popViewControllerAnimated:NO];
    [_delegate successfulReauthenticationWithCoordinator:self];

  } else {
    [_dispatcher closeSettingsUI];
  }
}

@end
