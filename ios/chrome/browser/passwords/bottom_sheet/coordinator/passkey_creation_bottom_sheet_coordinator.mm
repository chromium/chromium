// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/service/sync_service.h"
#import "components/webauthn/ios/ios_passkey_client_commands.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_exit_reason.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_mediator.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_creation_bottom_sheet_mediator_delegate.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/passkey_creation_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/web/public/web_state.h"

@interface PasskeyCreationBottomSheetCoordinator () <
    ConfirmationAlertActionHandler,
    PasskeyCreationBottomSheetMediatorDelegate,
    UIAdaptivePresentationControllerDelegate> {
  // The Passkey Creation Bottom Sheet's view controller.
  PasskeyCreationBottomSheetViewController* _viewController;

  // The Passkey Creation Bottom Sheet's mediator.
  PasskeyCreationBottomSheetMediator* _mediator;

  // Information about the pending passkey request.
  std::optional<webauthn::IOSPasskeyClient::RequestInfo> _requestInfo;

  // Whether the user has already initiated a terminal action (button tap or
  // interactive dismissal) on the sheet.
  BOOL _actionStarted;

  // Whether the coordinator is currently dismissing / stopping.
  BOOL _dismissing;
}

@end

@implementation PasskeyCreationBottomSheetCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               requestInfo:
                                   (webauthn::IOSPasskeyClient::RequestInfo)
                                       requestInfo {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _requestInfo = std::move(requestInfo);
  }
  return self;
}

- (void)start {
  WebStateList* webStateList = self.browser->GetWebStateList();
  id<ReauthenticationProtocol> reauthModule =
      ReauthenticationServiceFactory::GetForProfile(self.profile)
          ->GetReauthModule();
  _mediator = [[PasskeyCreationBottomSheetMediator alloc]
      initWithWebStateList:webStateList
               requestInfo:std::move(*_requestInfo)
          accountForSaving:[self accountForSaving]
              reauthModule:reauthModule
                  delegate:self];

  _requestInfo.reset();

  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(self.profile);

  _viewController = [[PasskeyCreationBottomSheetViewController alloc]
      initWithFaviconLoader:faviconLoader];

  _viewController.actionHandler = self;
  _viewController.modalPresentationStyle = UIModalPresentationPageSheet;
  _viewController.presentationController.delegate = self;

  _mediator.consumer = _viewController;

  if (!_mediator.consumer) {
    // Dismiss the coordinator right away if the request metadata is missing or
    // expired. Do not access `self` past this point because the command handler
    // synchronously stops and releases this coordinator.
    [self dismissPasskeyCreation];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];

  // Dismiss right away if the presentation failed to avoid having a zombie
  // coordinator.
  ExecuteWhenTransitionsComplete(
      ^{
        [weakSelf verifyPresentation];
      },
      self.baseViewController);
}

- (void)stop {
  _dismissing = YES;
  _viewController.presentationController.delegate = nil;
  [_viewController disconnect];
  [_viewController.presentingViewController dismissViewControllerAnimated:NO
                                                               completion:nil];
  _viewController = nil;

  [_mediator
      logExitReason:PasskeyCreationBottomSheetExitReason::kDismissedExternally];
  [_mediator cancelPasskeyCreation];
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
  [super stop];
}

- (BOOL)hasPendingRequest:
    (const webauthn::IOSPasskeyClient::RequestInfo&)requestInfo {
  if (_mediator) {
    return [_mediator hasPendingRequest:requestInfo];
  }
  return _requestInfo.has_value() && *_requestInfo == requestInfo;
}

#pragma mark - PasskeyCreationBottomSheetMediatorDelegate

- (void)endPresentation {
  if (_dismissing) {
    return;
  }
  [self dismissPasskeyCreation];
}

- (void)dismissPasskeyCreation {
  if (_dismissing) {
    return;
  }
  _dismissing = YES;
  id<IOSPasskeyClientCommands> passkeyClientHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), IOSPasskeyClientCommands);
  [passkeyClientHandler dismissPasskeyCreation];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  if (_dismissing || _actionStarted) {
    return;
  }
  _actionStarted = YES;
  [_mediator logExitReason:PasskeyCreationBottomSheetExitReason::kDismissed];
  [_mediator cancelPasskeyCreation];
  [self dismissPasskeyCreation];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  if (_dismissing || _actionStarted) {
    return;
  }
  _actionStarted = YES;
  _viewController.view.userInteractionEnabled = NO;
  [_mediator createPasskey];
}

- (void)confirmationAlertSecondaryAction {
  if (_dismissing || _actionStarted) {
    return;
  }
  _actionStarted = YES;
  _viewController.view.userInteractionEnabled = NO;
  [_mediator
      logExitReason:PasskeyCreationBottomSheetExitReason::kSaveAnotherWay];
  [_mediator deferPasskeyCreationToRenderer];
  [self dismissPasskeyCreation];
}

#pragma mark - Private

// Verifies whether the bottom sheet view controller was successfully presented.
// If not, defers the passkey creation request to the renderer and dismisses
// the coordinator.
- (void)verifyPresentation {
  if (_dismissing || _actionStarted) {
    return;
  }
  if (!_viewController.presentingViewController &&
      !_viewController.beingPresented) {
    _actionStarted = YES;
    [_mediator
        logExitReason:PasskeyCreationBottomSheetExitReason::kCouldNotPresent];
    [_mediator deferPasskeyCreationToRenderer];
    [self dismissPasskeyCreation];
  }
}

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
