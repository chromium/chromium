// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/account_storage_notice/passwords_account_storage_notice_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/passwords_account_storage_notice_commands.h"
#import "ios/chrome/browser/ui/passwords/account_storage_notice/passwords_account_storage_notice_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_coordinator_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const char kDismissalReasonHistogramPrefix[] =
    "PasswordManager.AccountStorageNoticeDismissalReason";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with PasswordsAccountStorageNoticeDismissalReason in
// tools/metrics/histograms/enums.xml.
enum class DismissalReason {
  // - stop called before the user interacted with the sheet.
  kUnknown = 0,
  kPrimaryButton = 1,
  kSettingsLink = 2,
  kSwipe = 3,
  kMaxValue = kSwipe,
};

}  // namespace

@interface PasswordsAccountStorageNoticeCoordinator () <
    PasswordsAccountStorageNoticeActionHandler,
    PasswordSettingsCoordinatorDelegate>

// Entry point for metrics.
@property(nonatomic, assign, readonly)
    PasswordsAccountStorageNoticeEntryPoint entryPoint;

// Dismissal handler, never nil.
@property(nonatomic, copy, readonly) void (^dismissalHandler)(void);

// Nil if it's not being displayed.
@property(nonatomic, strong)
    PasswordsAccountStorageNoticeViewController* sheetViewController;

// Nil if it's not being displayed.
@property(nonatomic, strong)
    PasswordSettingsCoordinator* passwordSettingsCoordinator;

// Dismissal reason for metrics. Zero-initialized to kUnknown.
@property(nonatomic, assign) DismissalReason dismissalReason;

@end

@implementation PasswordsAccountStorageNoticeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                entryPoint:
                                    (PasswordsAccountStorageNoticeEntryPoint)
                                        entryPoint
                          dismissalHandler:(void (^)())dismissalHandler {
  DCHECK(dismissalHandler);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _entryPoint = entryPoint;
    _dismissalHandler = dismissalHandler;
  }
  return self;
}

- (void)start {
  [super start];

  self.sheetViewController =
      [[PasswordsAccountStorageNoticeViewController alloc]
          initWithActionHandler:self];
  [self.baseViewController presentViewController:self.sheetViewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  const char* histogramSuffix = nullptr;
  switch (self.entryPoint) {
    case PasswordsAccountStorageNoticeEntryPoint::kSave:
      histogramSuffix = ".Save";
      break;
    case PasswordsAccountStorageNoticeEntryPoint::kFill:
      histogramSuffix = ".Fill";
      break;
    case PasswordsAccountStorageNoticeEntryPoint::kUpdate:
      histogramSuffix = ".Update";
      break;
  }
  base::UmaHistogramEnumeration(kDismissalReasonHistogramPrefix,
                                self.dismissalReason);
  base::UmaHistogramEnumeration(
      base::StrCat({kDismissalReasonHistogramPrefix, histogramSuffix}),
      self.dismissalReason);
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
  self.dismissalReason = DismissalReason::kPrimaryButton;
  __weak __typeof(self) weakSelf = self;
  [self.sheetViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf confirmationAlertPrimaryActionCompletion];
                         }];
}

- (void)confirmationAlertSettingsAction {
  self.dismissalReason = DismissalReason::kSettingsLink;
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
  self.dismissalReason = DismissalReason::kSwipe;
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
