// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/tracking_protections/script_blocking/script_blocking_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/settings/ui_bundled/privacy/tracking_protections/script_blocking/script_blocking_view_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@interface ScriptBlockingCoordinator () <
    ScriptBlockingViewControllerPresentationDelegate>

@end

@implementation ScriptBlockingCoordinator {
  // View controller presented by this coordinator.
  ScriptBlockingViewController* _viewController;
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
  _viewController = [[ScriptBlockingViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.presentationDelegate = self;
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;
}

#pragma mark - ScriptBlockingViewControllerPresentationDelegate

- (void)scriptBlockingViewControllerDidRemove:
    (ScriptBlockingViewController*)controller {
  [self.delegate scriptBlockingCoordinatorDidRemove:self];
}

@end
