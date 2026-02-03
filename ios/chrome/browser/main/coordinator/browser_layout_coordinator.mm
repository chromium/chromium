// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/coordinator/browser_layout_coordinator.h"

#import "ios/chrome/browser/main/ui/browser_layout_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

@implementation BrowserLayoutCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  _viewController = [[BrowserLayoutViewController alloc] init];
  _viewController.incognito = self.browser->GetProfile()->IsOffTheRecord();
}

- (void)stop {
  _viewController = nil;
}

#pragma mark - Properties

- (void)setBrowserViewController:(UIViewController*)browserViewController {
  _viewController.currentBVC = browserViewController;
}

- (UIViewController*)browserViewController {
  return _viewController.currentBVC;
}

@end
