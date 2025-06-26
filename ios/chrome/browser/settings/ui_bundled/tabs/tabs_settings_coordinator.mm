// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_coordinator.h"

#import "ios/chrome/browser/settings/ui_bundled/tabs/inactive_tabs/inactive_tabs_settings_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_navigation_commands.h"
#import "ios/chrome/browser/settings/ui_bundled/tabs/tabs_settings_table_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@interface TabsSettingsCoordinator () <
    TabsSettingsNavigationCommands,
    TabsSettingsTableViewControllerDismissalDelegate>
@end

@implementation TabsSettingsCoordinator {
  // Mediator for the tab settings
  TabsSettingsMediator* _mediator;
  // View controller for the tabs settings.
  TabsSettingsTableViewController* _viewController;
  // Coordinator for the inactive tabs settings.
  InactiveTabsSettingsCoordinator* _inactiveTabsSettingsCoordinator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  _viewController = [[TabsSettingsTableViewController alloc] init];
  _viewController.dismissalDelegate = self;
  _mediator = [[TabsSettingsMediator alloc]
      initWithProfilePrefService:self.profile->GetPrefs()
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

#pragma mark - TabsSettingsTableViewControllerDismissalDelegate

- (void)tabsSettingsTableViewControllerDidDisappear:
    (TabsSettingsTableViewController*)controller {
  [self.delegate tabsSettingsCoordinatorDidRemove:self];
}

@end
