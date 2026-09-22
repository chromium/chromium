// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_overlay_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_consumer.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using WatermarkOverlayViewControllerTest = PlatformTest;

// Tests that WatermarkOverlayViewController correctly conforms to the
// WatermarkConsumer protocol and updates its view.
TEST_F(WatermarkOverlayViewControllerTest, TestWatermarkConsumerProtocol) {
  WatermarkOverlayViewController* viewController =
      [[WatermarkOverlayViewController alloc] init];
  [viewController loadViewIfNeeded];

  WatermarkStyle style;
  style.fill_opacity = 0.45;
  style.outline_opacity = 0.75;
  style.font_size = 32;

  id<WatermarkConsumer> consumer = (id<WatermarkConsumer>)viewController;
  [consumer updateWatermarkWithText:@"ProtocolText" style:style];
  WatermarkView* view = viewController.watermarkView;
  EXPECT_NSEQ(view.text, @"ProtocolText");
  EXPECT_NEAR(view.fillOpacity, 0.45, 0.01);
  EXPECT_NEAR(view.outlineOpacity, 0.75, 0.01);
  EXPECT_NEAR(view.fontSize, 32.0, 0.01);
}

}  // namespace
