// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_coordinator.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_table_view_controller.h"

@implementation InactiveTabsSettingsCoordinator {
  // Mediator for the inactive tabs settings.
  InactiveTabsSettingsMediator* _mediator;
  // View controller for the inactive tabs settings.
  InactiveTabsSettingsTableViewController* _viewController;
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
  _viewController = [[InactiveTabsSettingsTableViewController alloc] init];
  _mediator = [[InactiveTabsSettingsMediator alloc]
      initWithUserLocalPrefService:GetApplicationContext()->GetLocalState()
                           browser:(Browser*)self.browser
                          consumer:_viewController];
  _viewController.delegate = _mediator;

  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController.delegate = nil;
  _viewController = nil;

  [_mediator disconnect];
  _mediator = nil;
}

@end
