// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_presentation_controller.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_banner_positioner.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The presented view outer horizontal margins.
const CGFloat kContainerHorizontalPadding = 8;
// The presented view maximum width.
const CGFloat kContainerMaxWidth = 398;
// The presented view maximum height.
const CGFloat kContainerMaxHeight = 200;
}

@implementation InfobarBannerPresentationController

- (void)presentationTransitionWillBegin {
  self.containerView.frame = [self viewForPresentedView].frame;
}

- (void)containerViewWillLayoutSubviews {
  self.containerView.frame = [self viewForPresentedView].frame;
  self.presentedView.frame = [self viewForPresentedView].bounds;
}

- (UIView*)viewForPresentedView {
  DCHECK(self.bannerPositioner);
  UIWindow* window = UIApplication.sharedApplication.keyWindow;

  // Calculate the Banner container width.
  CGFloat safeAreaWidth = CGRectGetWidth(window.bounds);
  CGFloat maxAvailableWidth = safeAreaWidth - 2 * kContainerHorizontalPadding;
  CGFloat frameWidth = fmin(maxAvailableWidth, kContainerMaxWidth);

  // Based on the container width, calculate the value in order to center the
  // Banner in the X axis.
  CGFloat bannerXPosition = (safeAreaWidth / 2) - (frameWidth / 2);
  CGFloat bannerYPosition = [self.bannerPositioner bannerYPosition];

  // Calculate the Banner height needed to fit its content with frameWidth.
  UIView* bannerView = [self.bannerPositioner bannerView];
  [bannerView setNeedsLayout];
  [bannerView layoutIfNeeded];
  CGSize frameThatFits =
      [bannerView systemLayoutSizeFittingSize:CGSizeMake(frameWidth, 0)
                withHorizontalFittingPriority:UILayoutPriorityRequired
                      verticalFittingPriority:1];
  CGFloat frameHeight = fmin(kContainerMaxHeight, frameThatFits.height);

  return
      [[UIView alloc] initWithFrame:CGRectMake(bannerXPosition, bannerYPosition,
                                               frameWidth, frameHeight)];
}

@end
