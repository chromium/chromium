// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/bwg_settings_coordinator.h"

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/bwg_settings_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/bwg/ui/bwg_settings_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@implementation BWGSettingsCoordinator {
  // View controller presented by this coordinator.
  BWGSettingsViewController* _viewController;
  // Mediator used by this coordinator.
  BWGSettingsMediator* _mediator;
}

@synthesize baseNavigationController = _baseNavigationController;

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

- (void)start {
  CommandDispatcher* commandDispatcher = self.browser->GetCommandDispatcher();
  _mediator = [[BWGSettingsMediator alloc]
      initWithPrefService:self.profile->GetPrefs()];
  _mediator.applicationHandler =
      HandlerForProtocol(commandDispatcher, ApplicationCommands);

  _viewController =
      [[BWGSettingsViewController alloc] initWithStyle:ChromeTableViewStyle()];
  _viewController.mutator = _mediator;
  _mediator.consumer = _viewController;
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

@end
