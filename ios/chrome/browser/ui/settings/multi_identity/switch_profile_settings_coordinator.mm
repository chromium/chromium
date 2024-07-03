// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_view_controller.h"

@implementation SwitchProfileSettingsCoordinator {
  // View controller for the tabs settings.
  SwitchProfileSettingsTableViewController* _viewController;
  // The ChromeBrowserState instance passed to the initializer.
  ChromeBrowserState* _browserState;
  // Mediator for the switch profile settings.
  SwitchProfileSettingsMediator* _mediator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:navigationController
                                       browser:browser]) {
    _baseNavigationController = navigationController;
    _browserState = browser->GetBrowserState();
  }
  return self;
}

- (void)start {
  _mediator = [[SwitchProfileSettingsMediator alloc] init];
  _viewController = [[SwitchProfileSettingsTableViewController alloc] init];
  _viewController.delegate = _mediator;
  _viewController.activeBrowserStateName =
      base::SysUTF8ToNSString(_browserState->GetBrowserStateName());
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;
}

@end
