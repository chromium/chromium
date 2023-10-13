// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_all_password_coordinator.h"

#import "base/ios/block_types.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/password_store_interface.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_all_password_coordinator_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_list_navigator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_manager_ui_features.h"
#import "ios/chrome/browser/ui/settings/password/reauthentication/reauthentication_coordinator.h"

@interface ManualFillAllPasswordCoordinator () <
    ManualFillPasswordMediatorDelegate,
    PasswordViewControllerDelegate,
    ReauthenticationCoordinatorDelegate>

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
}

- (void)start {
  [super start];
  UISearchController* searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.passwordViewController = [[PasswordViewController alloc]
      initWithSearchController:searchController];
  self.passwordViewController.delegate = self;

  auto profilePasswordStore =
      IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
          self.browser->GetBrowserState(), ServiceAccessType::EXPLICIT_ACCESS);
  auto accountPasswordStore =
      IOSChromeAccountPasswordStoreFactory::GetForBrowserState(
          self.browser->GetBrowserState(), ServiceAccessType::EXPLICIT_ACCESS);
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  self.passwordMediator = [[ManualFillPasswordMediator alloc]
      initWithProfilePasswordStore:profilePasswordStore
              accountPasswordStore:accountPasswordStore
                     faviconLoader:faviconLoader
                          webState:webState
                       syncService:syncService
                               URL:GURL::EmptyGURL()
            invokedOnPasswordField:NO];
  [self.passwordMediator fetchPasswords];
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

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];

  if (password_manager::features::IsAuthOnEntryV2Enabled()) {
    [self startReauthCoordinator];
  }
}

- (void)stop {
  [self stopReauthCoordinator];

  [self.passwordViewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.passwordViewController = nil;
  self.passwordMediator = nil;
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

- (void)willPushReauthenticationViewController {
  // No-op.
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
