// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check_op.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_view_controller_presentation_delegate.h"

@interface PrivacyGuideURLUsageCoordinator () <
    PrivacyGuideURLUsageViewControllerPresentationDelegate>
@end

@implementation PrivacyGuideURLUsageCoordinator {
  PrivacyGuideURLUsageViewController* _viewController;
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
  _viewController = [[PrivacyGuideURLUsageViewController alloc] init];
  _viewController.presentationDelegate = self;

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController.presentationDelegate = nil;
  _viewController = nil;
}

#pragma mark - PrivacyGuideURLUsageViewControllerPresentationDelegate

- (void)privacyGuideURLUsageViewControllerDidRemove:
    (PrivacyGuideURLUsageViewController*)controller {
  CHECK_EQ(_viewController, controller);
  [self.delegate privacyGuideURLUsageCoordinatorDidRemove:self];
}

@end
