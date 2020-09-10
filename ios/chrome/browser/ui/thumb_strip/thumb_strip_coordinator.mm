// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_coordinator.h"

#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Height of the view that is revealed. The thumb strip has a height equal to a
// small grid cell + edge insets (top and bottm) from thumb strip layout.
const CGFloat kThumbStripHeight = 168.0f + 22.0f + 22.0f;
// The height of the BVC that remains visible after transitioning from thumb
// strip to tab grid.
// TODO(crbug.com/1123048): Change this hardcoded number into a value calculated
// by the runtime toolbar height and any other inputs.
const CGFloat kBVCHeightTabGrid = 108.0f;
}  // namespace

@implementation ThumbStripCoordinator

#pragma mark - ChromeCoordinator

- (void)start {
  CGFloat baseViewHeight = self.baseViewController.view.frame.size.height;
  self.panHandler = [[ViewRevealingVerticalPanHandler alloc]
      initWithPeekedHeight:kThumbStripHeight
       revealedCoverHeight:kBVCHeightTabGrid
            baseViewHeight:baseViewHeight];
}

- (void)stop {
  self.panHandler = nil;
}

@end
