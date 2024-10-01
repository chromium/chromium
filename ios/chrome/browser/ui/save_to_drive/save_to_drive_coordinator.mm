// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_configuration.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_coordinator.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_coordinator_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_logger.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/chrome/browser/drive/model/drive_metrics.h"
#import "ios/chrome/browser/drive/model/drive_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/account_picker_commands.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/manage_storage_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/save_to_drive_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/save_to_drive/file_destination_picker_view_controller.h"
#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_mediator.h"
#import "ios/chrome/browser/ui/save_to_drive/save_to_drive_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/download/download_task.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface SaveToDriveCoordinator () <AccountPickerCommands,
                                      AccountPickerCoordinatorDelegate,
                                      AccountPickerLogger,
                                      ManageStorageAlertCommands>

@end

@implementation SaveToDriveCoordinator {
  raw_ptr<web::DownloadTask> _downloadTask;
  SaveToDriveMediator* _mediator;
  AccountPickerCoordinator* _accountPickerCoordinator;
  FileDestinationPickerViewController* _destinationPicker;
  UIAlertController* _alertController;
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
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(AccountPickerCommands)];
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(ManageStorageAlertCommands)];
  ProfileIOS* profile = self.browser->GetProfile();
  drive::DriveService* driveService =
      drive::DriveServiceFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  PrefService* prefService = profile->GetPrefs();
  id<SaveToDriveCommands> saveToDriveHandler =
      HandlerForProtocol(dispatcher, SaveToDriveCommands);
  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  _mediator =
      [[SaveToDriveMediator alloc] initWithDownloadTask:_downloadTask
                                     saveToDriveHandler:saveToDriveHandler
                              manageStorageAlertHandler:self
                                     applicationHandler:applicationHandler
                                   accountPickerHandler:self
                                            prefService:prefService
                                  accountManagerService:accountManagerService
                                           driveService:driveService];

  AccountPickerConfiguration* accountPickerConfiguration =
      drive::GetAccountPickerConfiguration(_downloadTask);

  accountPickerConfiguration.dismissOnBackgroundTap =
      self.baseViewController.traitCollection.horizontalSizeClass ==
          UIUserInterfaceSizeClassRegular &&
      self.baseViewController.traitCollection.verticalSizeClass ==
          UIUserInterfaceSizeClassRegular;
  _accountPickerCoordinator = [[AccountPickerCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                   configuration:accountPickerConfiguration];
  _accountPickerCoordinator.delegate = self;
  _accountPickerCoordinator.logger = self;
  _destinationPicker = [[FileDestinationPickerViewController alloc] init];
  _accountPickerCoordinator.accountConfirmationChildViewController =
      _destinationPicker;
  [_accountPickerCoordinator start];

  _destinationPicker.actionDelegate = _mediator;
  _mediator.accountPickerConsumer = _accountPickerCoordinator;
  _mediator.destinationPickerConsumer = _destinationPicker;
}

- (void)stop {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher stopDispatchingToTarget:self];
  [_mediator disconnect];
  _mediator = nil;
  [_destinationPicker willMoveToParentViewController:nil];
  [_destinationPicker removeFromParentViewController];
  _destinationPicker = nil;
  [_alertController.presentingViewController dismissViewControllerAnimated:NO
                                                                completion:nil];
  _alertController = nil;
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
  CHECK(identity);
  [_mediator saveWithSelectedIdentity:identity];
}

- (void)accountPickerCoordinatorCancel:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  [_mediator cancelSaveToDrive];
}

- (void)accountPickerCoordinatorAllIdentityRemoved:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  [_accountPickerCoordinator stopAnimated:YES];
}

- (void)accountPickerCoordinatorDidStop:
    (AccountPickerCoordinator*)accountPickerCoordinator {
  _accountPickerCoordinator = nil;
  id<SaveToDriveCommands> saveToDriveCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SaveToDriveCommands);
  [saveToDriveCommandsHandler hideSaveToDrive];
}

#pragma mark - AccountPickerLogger

- (void)logAccountPickerSelectionScreenOpened {
  base::RecordAction(base::UserMetricsAction(
      "MobileSaveToDriveAccountPickerSelectionScreenOpened"));
}

- (void)logAccountPickerNewIdentitySelected {
  base::RecordAction(base::UserMetricsAction(
      "MobileSaveToDriveAccountPickerNewIdentitySelected"));
}

- (void)logAccountPickerSelectionScreenClosed {
  base::RecordAction(base::UserMetricsAction(
      "MobileSaveToDriveAccountPickerSelectionScreenClosed"));
}

- (void)logAccountPickerAddAccountScreenOpened {
  base::RecordAction(base::UserMetricsAction(
      "MobileSaveToDriveAccountPickerAddAccountScreenOpened"));
}

- (void)logAccountPickerAddAccountCompleted {
  base::RecordAction(base::UserMetricsAction(
      "MobileSaveToDriveAccountPickerAddAccountCompleted"));
}

#pragma mark - ManageStorageAlertCommands

- (void)showManageStorageAlertForIdentity:(id<SystemIdentity>)identity {
  if (_alertController) {
    [_alertController.presentingViewController
        dismissViewControllerAnimated:NO
                           completion:nil];
  }
  _alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_MANAGE_STORAGE_ALERT_TITLE)
                       message:l10n_util::GetNSString(
                                   IDS_IOS_MANAGE_STORAGE_ALERT_MESSAGE)
                preferredStyle:UIAlertControllerStyleAlert];
  __weak __typeof(_mediator) weakMediator = _mediator;
  UIAlertAction* manageStorageAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_MANAGE_STORAGE_ALERT_MANAGE_STORAGE_BUTTON)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                [weakMediator showManageStorageForIdentity:identity];
                base::UmaHistogramBoolean(
                    kSaveToDriveUIManageStorageAlertCanceled, false);
              }];
  UIAlertAction* cancelAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                style:UIAlertActionStyleCancel
              handler:^(UIAlertAction* action) {
                base::UmaHistogramBoolean(
                    kSaveToDriveUIManageStorageAlertCanceled, true);
              }];
  [_alertController addAction:manageStorageAction];
  [_alertController addAction:cancelAction];
  [_alertController setPreferredAction:manageStorageAction];
  CHECK(_accountPickerCoordinator.viewController);
  [_accountPickerCoordinator.viewController
      presentViewController:_alertController
                   animated:YES
                 completion:nil];
}

#pragma mark - AccountPickerCommands

- (void)hideAccountPickerAnimated:(BOOL)animated {
  [_accountPickerCoordinator stopAnimated:animated];
}

@end
