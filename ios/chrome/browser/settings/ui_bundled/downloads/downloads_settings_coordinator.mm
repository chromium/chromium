// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/downloads/downloads_settings_coordinator.h"

#import "components/prefs/pref_service.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/download/coordinator/auto_deletion/auto_deletion_settings_mediator.h"
#import "ios/chrome/browser/photos/model/photos_service.h"
#import "ios/chrome/browser/photos/model/photos_service_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/downloads_settings_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/downloads_settings_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/downloads_settings_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/downloads_settings_table_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_action_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_mediator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

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

  // Auto deletion settings mediator.
  AutoDeletionSettingsMediator* _autoDeletionSettingsMediator;
  // The signin coordinator, if it is opened.
  SigninCoordinator* _signinCoordinator;
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
  ProfileIOS* profile = self.profile;
  PhotosService* photosService = PhotosServiceFactory::GetForProfile(profile);
  _saveToPhotosSettingsMediator = [[SaveToPhotosSettingsMediator alloc]
      initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                        GetForProfile(profile)
                        prefService:profile->GetPrefs()
                    identityManager:IdentityManagerFactory::GetForProfile(
                                        profile)
                      photosService:photosService];

  _downloadsSettingsTableViewController =
      [[DownloadsSettingsTableViewController alloc] init];
  _downloadsSettingsTableViewController.actionDelegate = self;
  _downloadsSettingsTableViewController.presentationDelegate = self;

  _saveToPhotosSettingsMediator.accountConfirmationConsumer =
      _downloadsSettingsTableViewController;
  _downloadsSettingsTableViewController.saveToPhotosSettingsMutator =
      _saveToPhotosSettingsMediator;

  if (IsDownloadAutoDeletionFeatureEnabled()) {
    PrefService* localState = GetApplicationContext()->GetLocalState();
    _autoDeletionSettingsMediator =
        [[AutoDeletionSettingsMediator alloc] initWithLocalState:localState];
    _autoDeletionSettingsMediator.autoDeletionConsumer =
        _downloadsSettingsTableViewController;
    _downloadsSettingsTableViewController.autoDeletionSettingsMutator =
        _autoDeletionSettingsMediator;
  }

  [self.baseNavigationController
      pushViewController:_downloadsSettingsTableViewController
                animated:YES];
}

- (void)stop {
  [self stopSigninCoordinator];
  [_saveToPhotosSettingsMediator disconnect];
  _saveToPhotosSettingsMediator = nil;

  if (IsDownloadAutoDeletionFeatureEnabled()) {
    [_autoDeletionSettingsMediator disconnect];
    _autoDeletionSettingsMediator = nil;
  }

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
  if (_signinCoordinator.viewWillPersist) {
    return;
  }
  [_signinCoordinator stop];
  SigninContextStyle contextStyle = SigninContextStyle::kDefault;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::kSaveToPhotosIos;

  __weak __typeof(self) weakSelf = self;
  _signinCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.baseViewController
                                          browser:signin::GetRegularBrowser(
                                                      self.browser)
                                     contextStyle:contextStyle
                                      accessPoint:accessPoint
                                   prefilledEmail:nil
                             continuationProvider:
                                 DoNothingContinuationProvider()];
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> signinIdentity) {
        [weakSelf signinCoordinatorCompletionWithCoordinator:coordinator
                                                      result:result
                                              signinIdentity:signinIdentity];
      };
  [_signinCoordinator start];
}

#pragma mark - Private

- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

- (void)
    signinCoordinatorCompletionWithCoordinator:(SigninCoordinator*)coordinator
                                        result:(SigninCoordinatorResult)result
                                signinIdentity:
                                    (id<SystemIdentity>)signinIdentity {
  CHECK_EQ(_signinCoordinator, coordinator, base::NotFatalUntil::M151);
  [self stopSigninCoordinator];
  if (result == SigninCoordinatorResultSuccess && signinIdentity) {
    GaiaId gaiaID = signinIdentity.gaiaId;
    [_saveToPhotosSettingsMediator setSelectedIdentityGaiaID:&gaiaID];
  }
}

@end
