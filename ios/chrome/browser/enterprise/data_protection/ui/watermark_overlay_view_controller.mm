// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_overlay_view_controller.h"

#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view.h"

@implementation WatermarkOverlayViewController

- (WatermarkView*)watermarkView {
  return (WatermarkView*)self.view;
}

#pragma mark - UIViewController

- (void)loadView {
  WatermarkView* watermarkView =
      [[WatermarkView alloc] initWithFrame:CGRectZero];
  self.view = watermarkView;
}

#pragma mark - WatermarkConsumer

- (void)updateWatermarkWithText:(NSString*)text style:(WatermarkStyle)style {
  [self.watermarkView updateWithText:text style:style];
}

@end
