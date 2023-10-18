// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_coordinator.h"

#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_commands.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_table_view_controller.h"

@interface TabPickupSettingsCoordinator () <TabPickupSettingsCommands>

@end

@implementation TabPickupSettingsCoordinator {
  // Mediator for the tab pickup settings.
  TabPickupSettingsMediator* _mediator;
  // View controller for the tab pickup settings.
  TabPickupSettingsTableViewController* _viewController;
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
  _viewController = [[TabPickupSettingsTableViewController alloc] init];
  _mediator = [[TabPickupSettingsMediator alloc]
      initWithUserLocalPrefService:GetApplicationContext()->GetLocalState()
                       syncService:SyncServiceFactory::GetForBrowserState(
                                       self.browser->GetBrowserState())
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
  _viewController.delegate = nil;
  _viewController = nil;

  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - TabPickupSettingsCommands

- (void)showSign {
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  __weak TabPickupSettingsTableViewController* weakViewController =
      _viewController;

  ShowSigninCommandCompletionCallback completion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo* completionInfo) {
        // Reset the switch if the user cancels the sign-in flow.
        [weakViewController reloadSwitchItem];
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

@end
