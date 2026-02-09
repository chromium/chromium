// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_mediator.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_mediator_delegate.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/passkey_creation_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/webauthn/public/scoped_passkey_reauth_module_override.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/web/public/web_state.h"

@interface PasskeyCreationBottomSheetCoordinator () <
    ConfirmationAlertActionHandler,
    PasskeyCreationBottomSheetMediatorDelegate> {
  // The Passkey Creation Bottom Sheet's view controller.
  PasskeyCreationBottomSheetViewController* _viewController;

  // The Passkey Creation Bottom Sheet's mediator.
  PasskeyCreationBottomSheetMediator* _mediator;

  // The passkey request's ID, originating from PasskeyTabHelper.
  std::optional<std::string> _pendingRequestID;

  // Module for biometric authentication.
  ReauthenticationModule* _reauthModule;
}

@end

@implementation PasskeyCreationBottomSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                 requestID:(std::string)requestID {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _pendingRequestID = requestID;
  }
  return self;
}

- (void)start {
  WebStateList* webStateList = self.browser->GetWebStateList();
  _reauthModule = ScopedPasskeyReauthModuleOverride::Get()
                      ?: [[ReauthenticationModule alloc] init];
  _mediator = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:webStateList
                 requestID:std::move(*_pendingRequestID)
          accountForSaving:[self accountForSaving]
              reauthModule:_reauthModule
                  delegate:self];

  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(self.profile);

  _viewController = [[PasskeyCreationBottomSheetViewController alloc]
      initWithHandler:self.browserCoordinatorCommandsHandler
        faviconLoader:faviconLoader];

  _viewController.actionHandler = self;

  _mediator.consumer = _viewController;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;

  [_mediator disconnect];
  _mediator = nil;
  _reauthModule = nil;
}

#pragma mark - PasskeyCreationBottomSheetMediatorDelegate

- (void)endPresentation {
  // Dismiss the bottom sheet, then the presentation will be fully torn down
  // upon calling -didMoveToParentViewController.
  [_viewController dismissViewControllerAnimated:NO completion:nil];
}

- (void)dismissPasskeyCreation {
  [self.browserCoordinatorCommandsHandler dismissPasskeyCreation];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [_mediator createPasskey];
}

- (void)confirmationAlertSecondaryAction {
  [_mediator deferPasskeyCreationToRenderer];
  [self dismissPasskeyCreation];
}

#pragma mark - Private

- (NSString*)accountForSaving {
  // Use GetOriginalProfile so that it uses the signed in profile even when in
  // incognito mode, since the user is later asked to make an explicit choice
  // about saving this data to their account.
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForProfile(self.profile->GetOriginalProfile());
  return base::SysUTF8ToNSString(
      syncService ? syncService->GetAccountInfo().email : "");
}

@end
