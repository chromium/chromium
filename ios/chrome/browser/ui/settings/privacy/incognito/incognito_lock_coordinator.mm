// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/incognito/incognito_lock_view_controller_presentation_delegate.h"

@interface IncognitoLockCoordinator () <
    IncognitoLockViewControllerPresentationDelegate>

@end

@implementation IncognitoLockCoordinator {
  // View controller presented by this coordinator.
  IncognitoLockViewController* _viewController;
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
  _viewController = [[IncognitoLockViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.presentationDelegate = self;
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

#pragma mark - IncognitoLockViewControllerPresentationDelegate

- (void)incognitoLockViewControllerDidRemove:
    (IncognitoLockViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate incognitoLockCoordinatorDidRemove:self];
}

@end
