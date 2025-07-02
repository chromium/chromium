// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/coordinator/bwg_settings_coordinator.h"

#import "ios/chrome/browser/settings/ui_bundled/bwg//ui/bwg_settings_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@implementation BWGSettingsCoordinator {
  // View controller presented by this coordinator.
  BWGSettingsViewController* _viewController;
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
  _viewController =
      [[BWGSettingsViewController alloc] initWithStyle:ChromeTableViewStyle()];
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;
}

@end
