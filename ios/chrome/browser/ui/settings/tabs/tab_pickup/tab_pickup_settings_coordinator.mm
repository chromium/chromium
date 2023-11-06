// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_coordinator.h"

#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_popup_coordinator.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_commands.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_table_view_controller.h"

@interface TabPickupSettingsCoordinator () <HistorySyncPopupCoordinatorDelegate,
                                            TabPickupSettingsCommands>

@end

@implementation TabPickupSettingsCoordinator {
  // Mediator for the tab pickup settings.
  TabPickupSettingsMediator* _mediator;
  // View controller for the tab pickup settings.
  TabPickupSettingsTableViewController* _viewController;
  // Coordinator for the history sync opt-in screen that should appear after
  // sign-in.
  HistorySyncPopupCoordinator* _historySyncPopupCoordinator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:navigationController
                                       browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();

  _viewController = [[TabPickupSettingsTableViewController alloc] init];
  _mediator = [[TabPickupSettingsMediator alloc]
      initWithUserLocalPrefService:GetApplicationContext()->GetLocalState()
                browserPrefService:browserState->GetPrefs()
             authenticationService:AuthenticationServiceFactory::
                                       GetForBrowserState(browserState)
                       syncService:SyncServiceFactory::GetForBrowserState(
                                       browserState)
                          consumer:_viewController];
  _viewController.delegate = _mediator;
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  _viewController.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  _viewController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  _viewController.browsingDataHandler =
      HandlerForProtocol(dispatcher, BrowsingDataCommands);
  _viewController.settingsHandler =
      HandlerForProtocol(dispatcher, ApplicationSettingsCommands);
  _viewController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  _viewController.tabPickupSettingsHandler = self;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_historySyncPopupCoordinator stop];
  _historySyncPopupCoordinator = nil;
  _viewController.delegate = nil;
  _viewController = nil;
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - TabPickupSettingsCommands

- (void)showSign {
  if ([self shouldShowHistorySync]) {
    [self showHistorySyncOptInAfterDedicatedSignIn:NO];
    return;
  }
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  __weak __typeof(self) weakSelf = self;
  ShowSigninCommandCompletionCallback completion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* completionInfo) {
        [weakSelf handleSigninCompletionWithResult:result];
      };

  AuthenticationOperation operation =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? AuthenticationOperation::kSheetSigninAndHistorySync
          : AuthenticationOperation::kSigninAndSync;
  ShowSigninCommand* const showSigninCommand = [[ShowSigninCommand alloc]
      initWithOperation:operation
               identity:nil
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:completion];
  [handler showSignin:showSigninCommand
      baseViewController:self.baseViewController];
}

#pragma mark - Private

// Handles the sign in completion callback.
- (void)handleSigninCompletionWithResult:(SigninCoordinatorResult)result {
  if (result == SigninCoordinatorResultSuccess) {
    if ([self shouldShowHistorySync]) {
      [self showHistorySyncOptInAfterDedicatedSignIn:YES];
      return;
    }
  }
  // Reset the switch if the user cancels the sign-in flow.
  [_viewController reloadSwitchItem];
}

// Shows the History Sync Opt-In screen.
- (void)showHistorySyncOptInAfterDedicatedSignIn:(BOOL)dedicatedSignInDone {
  // Stop the previous coordinator since the user can tap on the switch item
  // to open a new History Sync Page while the dismiss animation of the previous
  // one is in progress.
  _historySyncPopupCoordinator.delegate = nil;
  [_historySyncPopupCoordinator stop];
  _historySyncPopupCoordinator = nil;
  // Show the History Sync Opt-In screen. The coordinator will dismiss itself
  // if there is no signed-in account (eg. if sign-in unsuccessful) or if sync
  // is disabled by policies.
  _historySyncPopupCoordinator = [[HistorySyncPopupCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                   showUserEmail:!dedicatedSignInDone
               signOutIfDeclined:dedicatedSignInDone
                      isOptional:NO
                     accessPoint:signin_metrics::AccessPoint::
                                     ACCESS_POINT_SETTINGS];
  _historySyncPopupCoordinator.delegate = self;
  [_historySyncPopupCoordinator start];
}

// Returns YES if the History Sync Opt-In should be shown.
- (BOOL)shouldShowHistorySync {
  if (!base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return NO;
  }
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  // TODO(crbug.com/1466884): Delete the usage of ConsentLevel::kSync after
  // Phase 2 on iOS is launched. See ConsentLevel::kSync documentation for
  // details.
  if (authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSync)) {
    return NO;
  }
  // Check if History Sync Opt-In should be skipped.
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  HistorySyncSkipReason skipReason = [HistorySyncCoordinator
      getHistorySyncOptInSkipReason:syncService
              authenticationService:authenticationService
                        prefService:browserState->GetPrefs()
              isHistorySyncOptional:NO];
  return skipReason == HistorySyncSkipReason::kNone;
}

#pragma mark - HistorySyncPopupCoordinatorDelegate

- (void)historySyncPopupCoordinator:(HistorySyncPopupCoordinator*)coordinator
                didFinishWithResult:(SigninCoordinatorResult)result {
  _historySyncPopupCoordinator.delegate = nil;
  [_historySyncPopupCoordinator stop];
  _historySyncPopupCoordinator = nil;
  [_viewController reloadSwitchItem];
}

@end
