// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/coordinator/quick_delete_other_data_coordinator.h"

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/ui/quick_delete_other_data_view_controller.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@implementation QuickDeleteOtherDataCoordinator {
  // The view controller for this coordinator.
  QuickDeleteOtherDataViewController* _viewController;
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

#pragma mark - ChromeCoordinator

- (void)start {
  // The "Quick Delete Other Data" page is only available on the regular
  // browser.
  CHECK(!self.profile->IsOffTheRecord());

  _viewController = [[QuickDeleteOtherDataViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.quickDeleteOtherDataHandler =
      self.quickDeleteOtherDataHandler;
  [_baseNavigationController pushViewController:_viewController animated:YES];
}

- (void)stop {
  _viewController.quickDeleteOtherDataHandler = nil;
  _viewController = nil;
}

@end
