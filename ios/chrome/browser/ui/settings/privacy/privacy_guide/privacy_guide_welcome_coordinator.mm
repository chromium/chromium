// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_coordinator.h"

#import "base/check_op.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_welcome_view_controller_presentation_delegate.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface PrivacyGuideWelcomeCoordinator () <
    PrivacyGuideWelcomeViewControllerPresentationDelegate,
    PromoStyleViewControllerDelegate>
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
  _viewController = [[PrivacyGuideWelcomeViewController alloc] init];
  _viewController.presentationDelegate = self;
  _viewController.delegate = self;

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController.delegate = nil;
  _viewController.presentationDelegate = nil;
  _viewController = nil;
}

#pragma mark - PrivacyGuideWelcomeViewControllerPresentationDelegate

- (void)privacyGuideWelcomeViewControllerDidRemove:
    (PrivacyGuideWelcomeViewController*)controller {
  CHECK_EQ(controller, _viewController);
  [self.delegate privacyGuideWelcomeCoordinatorDidRemove:self];
}

// TODO(crbug.com/1494887): Implement the WelcomeViewController actions.

@end
