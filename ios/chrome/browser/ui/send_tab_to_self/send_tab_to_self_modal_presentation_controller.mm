// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_presentation_controller.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_positioner.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The presented view outer margins.
const CGFloat kPresentedViewMargin = 10.0;
// The presented view maximum width.
const CGFloat kPresentedViewMaxWidth = 394.0;
// The rounded corner radius for the container view.
const CGFloat kContainerCornerRadius = 13.0;
}  // namespace

@implementation SendTabToSelfModalPresentationController

- (void)containerViewWillLayoutSubviews {
  self.presentedView.frame = [self frameForPresentedView];

  // Style the presented and container views.
  self.presentedView.layer.cornerRadius = kContainerCornerRadius;
  self.presentedView.layer.masksToBounds = YES;
  self.presentedView.clipsToBounds = YES;
  self.containerView.backgroundColor =
      [UIColor colorNamed:kScrimBackgroundColor];
}

- (CGRect)frameForPresentedView {
  DCHECK(self.modalPositioner);
  CGRect safeAreaBounds = self.containerView.safeAreaLayoutGuide.layoutFrame;
  CGFloat safeAreaWidth = CGRectGetWidth(safeAreaBounds);
  CGFloat safeAreaHeight = CGRectGetHeight(safeAreaBounds);

  // Calculate the frame width.
  CGFloat maxAvailableWidth = safeAreaWidth - 2 * kPresentedViewMargin;
  CGFloat frameWidth = fmin(maxAvailableWidth, kPresentedViewMaxWidth);

  CGFloat modalTargetHeight = [self.modalPositioner modalHeight];
  CGFloat maxAvailableHeight = safeAreaHeight - 2 * kPresentedViewMargin;
  CGFloat frameHeight = fmin(maxAvailableHeight, modalTargetHeight);

  // Based on the container width calculate the values in order to center the
  // frame in the X and Y axis.
  CGFloat containerWidth = CGRectGetWidth(self.containerView.bounds);
  CGFloat containerHeight = CGRectGetHeight(self.containerView.bounds);
  CGFloat modalXPosition = (containerWidth / 2) - (frameWidth / 2);
  CGFloat modalYPosition = (containerHeight / 2) - (frameHeight / 2);

  return CGRectMake(modalXPosition, modalYPosition, frameWidth, frameHeight);
}

@end
