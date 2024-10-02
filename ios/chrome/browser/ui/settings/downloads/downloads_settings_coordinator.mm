// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mediator.h"

@interface DownloadsSettingsCoordinator () <
    DownloadsSettingsTableViewControllerActionDelegate,
    DownloadsSettingsTableViewControllerPresentationDelegate,
    SaveToPhotosSettingsAccountSelectionViewControllerActionDelegate,
    SaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate>

@end

@implementation DownloadsSettingsCoordinator {
  DownloadsSettingsTableViewController* _downloadsSettingsTableViewController;

  // Save to Photos settings mediator and account selection view controller.
  SaveToPhotosSettingsMediator* _saveToPhotosSettingsMediator;
  SaveToPhotosSettingsAccountSelectionViewController*
      _saveToPhotosAccountSelectionViewController;
}

@synthesize baseNavigationController = _baseNavigationController;

#pragma mark - Initialization

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile();
  _saveToPhotosSettingsMediator = [[SaveToPhotosSettingsMediator alloc]
      initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                        GetForProfile(profile)
                        prefService:profile->GetPrefs()
                    identityManager:IdentityManagerFactory::GetForProfile(
                                        profile)];

  _downloadsSettingsTableViewController =
      [[DownloadsSettingsTableViewController alloc] init];
  _downloadsSettingsTableViewController.actionDelegate = self;
  _downloadsSettingsTableViewController.presentationDelegate = self;

  _saveToPhotosSettingsMediator.accountConfirmationConsumer =
      _downloadsSettingsTableViewController;
  _downloadsSettingsTableViewController.saveToPhotosSettingsMutator =
      _saveToPhotosSettingsMediator;

  [self.baseNavigationController
      pushViewController:_downloadsSettingsTableViewController
                animated:YES];
}

- (void)stop {
  [_saveToPhotosSettingsMediator disconnect];
  _saveToPhotosSettingsMediator = nil;

  [_saveToPhotosAccountSelectionViewController.navigationController
      popToViewController:_saveToPhotosAccountSelectionViewController
                 animated:NO];
  [_saveToPhotosAccountSelectionViewController.navigationController
      popViewControllerAnimated:NO];
  _saveToPhotosAccountSelectionViewController = nil;

  [_downloadsSettingsTableViewController.navigationController
      popToViewController:_downloadsSettingsTableViewController
                 animated:NO];
  [_downloadsSettingsTableViewController.navigationController
      popViewControllerAnimated:NO];
  _downloadsSettingsTableViewController = nil;
}

#pragma mark - DownloadsSettingsTableViewControllerPresentationDelegate

- (void)downloadsSettingsTableViewControllerWasRemoved:
    (DownloadsSettingsTableViewController*)controller {
  [self.delegate downloadsSettingsCoordinatorWasRemoved:self];
}

#pragma mark - DownloadsSettingsTableViewControllerActionDelegate

- (void)downloadsSettingsTableViewControllerOpenSaveToPhotosAccountSelection:
    (DownloadsSettingsTableViewController*)controller {
  _saveToPhotosAccountSelectionViewController =
      [[SaveToPhotosSettingsAccountSelectionViewController alloc] init];
  _saveToPhotosAccountSelectionViewController.actionDelegate = self;
  _saveToPhotosAccountSelectionViewController.presentationDelegate = self;
  _saveToPhotosAccountSelectionViewController.mutator =
      _saveToPhotosSettingsMediator;
  _saveToPhotosSettingsMediator.accountSelectionConsumer =
      _saveToPhotosAccountSelectionViewController;
  [self.baseNavigationController
      pushViewController:_saveToPhotosAccountSelectionViewController
                animated:YES];
}

#pragma mark - SaveToPhotosSettingsAccountSelectionViewControllerPresentationDelegate

- (void)saveToPhotosSettingsAccountSelectionViewControllerWasRemoved {
  _saveToPhotosSettingsMediator.accountSelectionConsumer = nil;
  _saveToPhotosAccountSelectionViewController = nil;
}

#pragma mark - SaveToPhotosSettingsAccountSelectionViewControllerActionDelegate

- (void)saveToPhotosSettingsAccountSelectionViewControllerAddAccount {
  id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  __weak __typeof(self) weakSelf = self;
  ShowSigninCommand* addAccountCommand = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
               identity:nil
            accessPoint:signin_metrics::AccessPoint::
                            ACCESS_POINT_SAVE_TO_PHOTOS_IOS
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(SigninCoordinatorResult result,
                          SigninCompletionInfo* info) {
                 __strong __typeof(weakSelf) strongSelf = weakSelf;
                 if (strongSelf && result == SigninCoordinatorResultSuccess &&
                     info.identity) {
                   [strongSelf->_saveToPhotosSettingsMediator
                       setSelectedIdentityGaiaID:info.identity.gaiaID];
                 }
               }];
  [applicationCommandsHandler showSignin:addAccountCommand
                      baseViewController:self.baseViewController];
}

@end
