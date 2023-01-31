// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/account_storage_notice/passwords_account_storage_notice_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/passwords_account_storage_notice_commands.h"
#import "ios/chrome/browser/ui/passwords/account_storage_notice/passwords_account_storage_notice_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordsAccountStorageNoticeCoordinator () <
    PasswordsAccountStorageNoticeActionHandler,
    PasswordSettingsCoordinatorDelegate>

// Dismissal handler, never nil.
@property(nonatomic, copy, readonly) void (^dismissalHandler)(void);

// Nil if it's not being displayed.
@property(nonatomic, strong)
    PasswordsAccountStorageNoticeViewController* sheetViewController;

// Nil if it's not being displayed.
@property(nonatomic, strong)
    PasswordSettingsCoordinator* passwordSettingsCoordinator;

@end

@implementation PasswordsAccountStorageNoticeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          dismissalHandler:(void (^)())dismissalHandler {
  DCHECK(dismissalHandler);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _dismissalHandler = dismissalHandler;
  }
  return self;
}

- (void)start {
  [super start];

  const std::string account =
      IdentityManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  DCHECK(!account.empty()) << "Account storage notice triggered when there "
                              "was no signed in account";
  self.sheetViewController =
      [[PasswordsAccountStorageNoticeViewController alloc]
            initWithActionHandler:self
          accountStoringPasswords:base::SysUTF8ToNSString(account)];
  [self.baseViewController presentViewController:self.sheetViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  if (self.sheetViewController) {
    //  This usually shouldn't happen: stop() called while the controller was
    // still showing, dismiss immediately.
    [self.sheetViewController dismissViewControllerAnimated:NO completion:nil];
    self.sheetViewController = nil;
  }
  if (self.passwordSettingsCoordinator) {
    //  This usually shouldn't happen: stop() called while the child coordinator
    // was still showing, stop it.
    [self.passwordSettingsCoordinator stop];
    self.passwordSettingsCoordinator = nil;
  }
}

#pragma mark - PasswordsAccountStorageNoticeActionHandler

- (void)confirmationAlertPrimaryAction {
  __weak __typeof(self) weakSelf = self;
  [self.sheetViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf confirmationAlertPrimaryActionCompletion];
                         }];
}

- (void)confirmationAlertSettingsAction {
  __weak __typeof(self) weakSelf = self;
  // Close the sheet before showing password settings, for a number of reasons:
  // - If the user disables the account storage switch in settings, the sheet
  // string about saving passwords to an account no longer makes sense.
  // - If the user clicked "settings", they read the notice, no need to bother
  // them with an additional button click when they close settings. Also,
  // `self.dismissalHandler` has no privacy delta per se, it shows more UI
  // requiring confirmation.
  [self.sheetViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf confirmationAlertSettingsActionCompletion];
                         }];
}

- (void)confirmationAlertSwipeDismissAction {
  // No call to dismissViewControllerAnimated(), that's done.
  self.sheetViewController = nil;
  self.dismissalHandler();
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      PasswordsAccountStorageNoticeCommands)
      hidePasswordsAccountStorageNotice];
  // `self` is deleted.
}

#pragma mark - Private

- (void)confirmationAlertPrimaryActionCompletion {
  self.sheetViewController = nil;
  self.dismissalHandler();
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      PasswordsAccountStorageNoticeCommands)
      hidePasswordsAccountStorageNotice];
  // `self` is deleted.
}

- (void)confirmationAlertSettingsActionCompletion {
  self.sheetViewController = nil;
  self.passwordSettingsCoordinator = [[PasswordSettingsCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser];
  self.passwordSettingsCoordinator.delegate = self;
  [self.passwordSettingsCoordinator start];
}

#pragma mark - PasswordSettingsCoordinatorDelegate

- (void)passwordSettingsCoordinatorDidRemove:
    (PasswordSettingsCoordinator*)coordinator {
  [self.passwordSettingsCoordinator stop];
  self.passwordSettingsCoordinator = nil;
  self.dismissalHandler();
  [HandlerForProtocol(self.browser->GetCommandDispatcher(),
                      PasswordsAccountStorageNoticeCommands)
      hidePasswordsAccountStorageNotice];
  // `self` is deleted.
}

@end
