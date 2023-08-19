// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_table_view_controller.h"

@interface TabsSettingsCoordinator () <TabsSettingsNavigationCommands>
@end

@implementation TabsSettingsCoordinator {
  // Mediator for the tab settings
  TabsSettingsMediator* _mediator;
  // View controller for the tabs settings.
  TabsSettingsTableViewController* _viewController;
  // Coordinator for the inactive tabs settings.
  InactiveTabsSettingsCoordinator* _inactiveTabsSettingsCoordinator;
  // Coordinator for the tab pickup settings.
  TabPickupSettingsCoordinator* _tabPickupSettingsCoordinator;
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
  _viewController = [[TabsSettingsTableViewController alloc] init];
  _mediator = [[TabsSettingsMediator alloc]
      initWithUserLocalPrefService:GetApplicationContext()->GetLocalState()
                       syncService:SyncServiceFactory::GetForBrowserState(
                                       self.browser->GetBrowserState())
                          consumer:_viewController];

  _viewController.delegate = _mediator;
  _mediator.handler = self;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_inactiveTabsSettingsCoordinator stop];
  _inactiveTabsSettingsCoordinator = nil;

  _viewController.delegate = nil;
  _viewController = nil;

  [_mediator disconnect];
  _mediator.handler = nil;
  _mediator = nil;
}

#pragma mark - TabsSettingsNavigationCommands

- (void)showInactiveTabsSettings {
  _inactiveTabsSettingsCoordinator = [[InactiveTabsSettingsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  [_inactiveTabsSettingsCoordinator start];
}

- (void)showTabPickupSettings {
  _tabPickupSettingsCoordinator = [[TabPickupSettingsCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  [_tabPickupSettingsCoordinator start];
}

@end
