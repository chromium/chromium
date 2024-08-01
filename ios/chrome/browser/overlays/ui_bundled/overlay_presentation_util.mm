// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/ui_bundled/overlay_presentation_util.h"

#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_positioner.h"

namespace {

// The presented view outer margins.
const CGFloat kPresentedViewMargin = 10.0;
// The presented view maximum width.
const CGFloat kPresentedViewMaxWidth = 394.0;

}  // namespace

CGRect ContainedModalFrameThatFit(id<InfobarModalPositioner> modalPositioner,
                                  UIView* containerView) {
  CGRect safeAreaBounds = containerView.safeAreaLayoutGuide.layoutFrame;

  CGFloat safeAreaWidth = CGRectGetWidth(safeAreaBounds);
  CGFloat maxAvailableWidth = safeAreaWidth - 2 * kPresentedViewMargin;
  CGFloat frameWidth = fmin(maxAvailableWidth, kPresentedViewMaxWidth);

  CGFloat targetHeight = [modalPositioner modalHeightForWidth:frameWidth];
  CGFloat safeAreaHeight = CGRectGetHeight(safeAreaBounds);
  CGFloat maxAvailableHeight = safeAreaHeight - 2 * kPresentedViewMargin;
  CGFloat frameHeight = fmin(maxAvailableHeight, targetHeight);

  CGFloat containerWidth = CGRectGetWidth(containerView.bounds);
  CGFloat containerHeight = CGRectGetHeight(containerView.bounds);
  CGFloat modalXPosition = (containerWidth / 2) - (frameWidth / 2);
  CGFloat modalYPosition = (containerHeight / 2) - (frameHeight / 2);

  return CGRectMake(modalXPosition, modalYPosition, frameWidth, frameHeight);
}
