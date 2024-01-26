// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_configuration.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_coordinator.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/save_to_drive/file_destination_picker_view_controller.h"
#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_mediator.h"
#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_util.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/web_state.h"

@interface SaveToDriveCoordinator () <AccountPickerCoordinatorDelegate>

@end

@implementation SaveToDriveCoordinator {
  raw_ptr<web::DownloadTask> _downloadTask;
  SaveToDriveMediator* _mediator;
  AccountPickerCoordinator* _accountPickerCoordinator;
  id<SystemIdentity> _selectedIdentity;
  FileDestinationPickerViewController* _destinationPicker;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              downloadTask:(web::DownloadTask*)downloadTask {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _downloadTask = downloadTask;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  id<SaveToDriveCommands> saveToDriveCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SaveToDriveCommands);
  _mediator = [[SaveToDriveMediator alloc]
            initWithDownloadTask:_downloadTask
      saveToDriveCommandsHandler:saveToDriveCommandsHandler];

  AccountPickerConfiguration* accountPickerConfiguration =
      drive::GetAccountPickerConfiguration(_downloadTask);
  _accountPickerCoordinator = [[AccountPickerCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                   configuration:accountPickerConfiguration];
  _accountPickerCoordinator.delegate = self;
  _destinationPicker = [[FileDestinationPickerViewController alloc] init];
  _accountPickerCoordinator.accountConfirmationChildViewController =
      _destinationPicker;
  [_accountPickerCoordinator start];

  _destinationPicker.actionDelegate = _mediator;
  _mediator.accountPickerConsumer = _accountPickerCoordinator;
  _mediator.destinationPickerConsumer = _destinationPicker;
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  [_destinationPicker willMoveToParentViewController:nil];
  [_destinationPicker removeFromParentViewController];
  _destinationPicker = nil;
  [_accountPickerCoordinator stop];
  _accountPickerCoordinator = nil;
}

#pragma mark - AccountPickerCoordinatorDelegate

- (void)accountPickerCoordinator:
            (AccountPickerCoordinator*)accountPickerCoordinator
    openAddAccountWithCompletion:(void (^)(id<SystemIdentity>))completion {
  id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  ShowSigninCommand* addAccountCommand = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_SAVE_TO_DRIVE_IOS
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(SigninCoordinatorResult result,
                          SigninCompletionInfo* completionInfo) {
                 if (completion) {
                   completion(completionInfo.identity);
                 }
               }];
  [applicationCommandsHandler
              showSignin:addAccountCommand
      baseViewController:accountPickerCoordinator.viewController];
}

- (void)accountPickerCoordinator:
            (AccountPickerCoordinator*)accountPickerCoordinator
               didSelectIdentity:(id<SystemIdentity>)identity
                    askEveryTime:(BOOL)askEveryTime {
  _selectedIdentity = identity;
  [_accountPickerCoordinator startValidationSpinner];
  [_accountPickerCoordinator stopAnimated:YES];
}

- (void)accountPickerCoordinatorCancel:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  [_accountPickerCoordinator stopAnimated:YES];
}

- (void)accountPickerCoordinatorAllIdentityRemoved:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  [_accountPickerCoordinator stopAnimated:YES];
}

- (void)accountPickerCoordinatorDidStop:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  _accountPickerCoordinator = nil;

  // If an identity was selected, start the download and save to Drive.
  if (_selectedIdentity) {
    [_mediator startDownloadWithIdentity:_selectedIdentity];
  }

  id<SaveToDriveCommands> saveToDriveCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SaveToDriveCommands);
  [saveToDriveCommandsHandler hideSaveToDrive];
}

@end
