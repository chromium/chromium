// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_all_password_coordinator.h"

#import "base/ios/block_types.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/affiliations/model/ios_chrome_affiliation_service_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_all_password_coordinator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"
#import "ios/chrome/browser/webauthn/model/ios_passkey_model_factory.h"

@interface ManualFillAllPasswordCoordinator () <
    ManualFillPasswordMediatorDelegate,
    PasswordViewControllerDelegate,
    ReauthenticationCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>

// Fetches and filters the passwords for the view controller.
@property(nonatomic, strong) ManualFillPasswordMediator* passwordMediator;

// The view controller presented above the keyboard where the user can select
// one of their passwords.
@property(nonatomic, strong) PasswordViewController* passwordViewController;

@end

@implementation ManualFillAllPasswordCoordinator {
  // Used for requiring Local Authentication before revealing the password list.
  // Authentication is also required when the app is backgrounded/foregrounded
  // with this surface opened.
  ReauthenticationCoordinator* _reauthCoordinator;

  // Navigation controller presented by this coordinator.
  TableViewNavigationController* _navigationController;

  // Service which gives us a view on users' saved passwords.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      _savedPasswordsPresenter;
}

- (void)start {
  [super start];
  UISearchController* searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.passwordViewController = [[PasswordViewController alloc]
      initWithSearchController:searchController];
  self.passwordViewController.delegate = self;

  ProfileIOS* profile = self.browser->GetProfile();
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(profile);

  _savedPasswordsPresenter =
      std::make_unique<password_manager::SavedPasswordsPresenter>(
          IOSChromeAffiliationServiceFactory::GetForBrowserState(profile),
          IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          IOSChromeAccountPasswordStoreFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS),
          IOSPasskeyModelFactory::GetForProfile(profile));

  _savedPasswordsPresenter->Init();

  // Initialize `passwordMediator` with a nil `profilePasswordStore` and
  // 'accountPasswordStore` as these arguments are only used to create a
  // PasswordCounterObserver, which is not needed in this case.
  self.passwordMediator = [[ManualFillPasswordMediator alloc]
         initWithFaviconLoader:faviconLoader
                      webState:webState
                   syncService:syncService
                           URL:GURL()
      invokedOnObfuscatedField:NO
          profilePasswordStore:nil
          accountPasswordStore:nil
        showAutofillFormButton:IsKeyboardAccessoryUpgradeEnabled() &&
                               [self.injectionHandler
                                       isActiveFormAPasswordForm]];
  [self.passwordMediator
      setSavedPasswordsPresenter:_savedPasswordsPresenter.get()];
  [self.passwordMediator fetchAllPasswords];
  self.passwordMediator.actionSectionEnabled = NO;
  self.passwordMediator.consumer = self.passwordViewController;
  self.passwordMediator.contentInjector = self.injectionHandler;
  self.passwordMediator.delegate = self;

  self.passwordViewController.imageDataSource = self.passwordMediator;

  searchController.searchResultsUpdater = self.passwordMediator;

  _navigationController = [[TableViewNavigationController alloc]
      initWithTable:self.passwordViewController];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.modalTransitionStyle =
      UIModalTransitionStyleCoverVertical;
  _navigationController.presentationController.delegate = self;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
  [self startReauthCoordinator];
}

- (void)stop {
  [self stopReauthCoordinator];

  [self.passwordViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.passwordViewController = nil;
  [self.passwordMediator disconnect];
  self.passwordMediator.consumer = nil;
  self.passwordMediator = nil;
  _savedPasswordsPresenter.reset();
  [super stop];
}

#pragma mark - FallbackCoordinator

- (UIViewController*)viewController {
  return self.passwordViewController;
}

#pragma mark - ManualFillPasswordMediatorDelegate

- (void)manualFillPasswordMediatorWillInjectContent:
    (ManualFillPasswordMediator*)mediator {
  [self.manualFillAllPasswordCoordinatorDelegate
      manualFillAllPasswordCoordinatorWantsToBeDismissed:self];  // The job is
                                                                 // done.
}

- (void)manualFillPasswordMediator:(ManualFillPasswordMediator*)mediator
    didTriggerOpenPasswordDetailsInEditMode:
        (password_manager::CredentialUIEntry)credential {
  [self.manualFillAllPasswordCoordinatorDelegate
             manualFillAllPasswordCoordinator:self
      didTriggerOpenPasswordDetailsInEditMode:credential];
}

#pragma mark - PasswordViewControllerDelegate

- (void)passwordViewControllerDidTapDoneButton:
    (PasswordViewController*)passwordViewController {
  [self.manualFillAllPasswordCoordinatorDelegate
      manualFillAllPasswordCoordinatorWantsToBeDismissed:self];  // The job is
                                                                 // done.
}

- (void)didTapLinkURL:(CrURL*)URL {
  // Dismiss `passwordViewController` and open header link in a new tab.
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:URL.gurl];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [self.manualFillAllPasswordCoordinatorDelegate
      manualFillAllPasswordCoordinatorWantsToBeDismissed:self];
  [handler openURLInNewTab:command];
}

#pragma mark - ReauthenticationCoordinatorDelegate

- (void)successfulReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  // No-op.
}

- (void)dismissUIAfterFailedReauthenticationWithCoordinator:
    (ReauthenticationCoordinator*)coordinator {
  CHECK_EQ(_reauthCoordinator, coordinator);
  [self.manualFillAllPasswordCoordinatorDelegate
      manualFillAllPasswordCoordinatorWantsToBeDismissed:self];
}

- (void)willPushReauthenticationViewController {
  // No-op.
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.manualFillAllPasswordCoordinatorDelegate
      manualFillAllPasswordCoordinatorWantsToBeDismissed:self];
}

#pragma mark - Private

// Starts reauthCoordinator and Local Authentication before revealing the
// password list. Once started reauthCoordinator observes scene state changes
// and requires authentication when the scene is backgrounded and then
// foregrounded while the surface is is opened.
- (void)startReauthCoordinator {
  _reauthCoordinator = [[ReauthenticationCoordinator alloc]
      initWithBaseNavigationController:_navigationController
                               browser:self.browser
                reauthenticationModule:nil
                           authOnStart:YES];
  _reauthCoordinator.delegate = self;
  [_reauthCoordinator start];
}

// Stops reauthCoordinator.
- (void)stopReauthCoordinator {
  [_reauthCoordinator stop];
  _reauthCoordinator = nil;
}

@end
