// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/following_feed_overlay/following_feed_overlay_view_controller.h"

#import "ios/chrome/browser/ntp/ui_bundled/following_feed_overlay/following_feed_overlay_view_controller_delegate.h"

@implementation FollowingFeedOverlayViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.yellowColor;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate followingFeedOverlayViewControllerDidDismiss:self];
  }
}

@end
