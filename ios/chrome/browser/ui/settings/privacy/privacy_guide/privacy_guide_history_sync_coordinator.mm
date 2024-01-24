// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_history_sync_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/check.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_history_sync_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface PrivacyGuideHistorySyncCoordinator () <
    PromoStyleViewControllerDelegate>
@end

@implementation PrivacyGuideHistorySyncCoordinator {
  PrivacyGuideHistorySyncViewController* _viewController;
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
  // TODO(crbug.com/1520481): Implement History Sync view controller.
  _viewController = [[PrivacyGuideHistorySyncViewController alloc] init];

  CHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:_viewController
                                           animated:YES];
}

- (void)stop {
  _viewController = nil;
}

@end
