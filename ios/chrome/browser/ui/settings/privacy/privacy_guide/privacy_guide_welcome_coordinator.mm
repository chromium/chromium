// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_view_controller.h"

@interface PrivacyGuideWelcomeCoordinator () <
    PrivacyGuideWelcomeViewControllerPresentationDelegate>
@end

@implementation PrivacyGuideWelcomeCoordinator {
  PrivacyGuideWelcomeViewController* _viewController;
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
  _viewController = [[PrivacyGuideWelcomeViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  _viewController.presentationDelegate = self;

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController.presentationDelegate = nil;
  _viewController = nil;
}

#pragma mark - PrivacyGuideWelcomeViewControllerPresentationDelegate

- (void)privacyGuideWelcomeViewControllerDidRemove:
    (PrivacyGuideWelcomeViewController*)controller {
  CHECK_EQ(controller, _viewController);
  [self.delegate privacyGuideWelcomeCoordinatorDidRemove:self];
}

@end
