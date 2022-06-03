// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/infobars/sc_infobar_container_view_controller.h"

#import "ios/chrome/browser/ui/infobars/banners/infobar_banner_view_controller.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_transition_driver.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_transition_driver.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ContainerViewController
- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.bannerViewController
      setModalPresentationStyle:UIModalPresentationCustom];
  self.bannerTransitionDriver = [[InfobarBannerTransitionDriver alloc] init];
  self.bannerTransitionDriver.bannerPositioner = self;
  self.bannerViewController.transitioningDelegate = self.bannerTransitionDriver;
  [self presentViewController:self.bannerViewController
                     animated:YES
                   completion:nil];
}

#pragma mark InfobarBannerPositioner

- (CGFloat)bannerYPosition {
  return 100;
}

- (UIView*)bannerView {
  return self.bannerViewController.view;
}

#pragma mark InfobarModalPositioner

- (CGFloat)modalHeightForWidth:(CGFloat)width {
  return 350;
}

@end
