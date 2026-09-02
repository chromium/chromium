// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_consumer.h"
#import "ios/chrome/browser/enterprise/data_protection/ui/watermark_view_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using WatermarkViewTest = PlatformTest;

// Tests that WatermarkView is initialized correctly with default values.
TEST_F(WatermarkViewTest, TestInitialization) {
  WatermarkView* view = [[WatermarkView alloc] initWithFrame:CGRectZero];
  EXPECT_NE(view, nil);
  EXPECT_EQ(view.text, nil);
  EXPECT_NEAR(view.fillOpacity, 0.10, 0.01);
  EXPECT_NEAR(view.outlineOpacity, 0.15, 0.01);
  EXPECT_NEAR(view.fontSize, 16.0, 0.01);
  EXPECT_NSEQ(view.backgroundColor, [UIColor clearColor]);
  EXPECT_FALSE(view.userInteractionEnabled);
}

// Tests that setting properties correctly updates their values.
TEST_F(WatermarkViewTest, TestProperties) {
  WatermarkView* view = [[WatermarkView alloc] initWithFrame:CGRectZero];

  view.text = @"Confidential";
  EXPECT_NSEQ(view.text, @"Confidential");

  view.fillOpacity = 0.5;
  EXPECT_NEAR(view.fillOpacity, 0.5, 0.01);

  view.outlineOpacity = 0.8;
  EXPECT_NEAR(view.outlineOpacity, 0.8, 0.01);

  view.fontSize = 24.0;
  EXPECT_NEAR(view.fontSize, 24.0, 0.01);
}

// Tests that the expanded size required to cover viewport corners after
// rotation is calculated correctly via the standalone utility function.
TEST_F(WatermarkViewTest, TestExpandedSizeForSizeAngle) {
  CGSize originalSize = CGSizeMake(100.0, 200.0);

  // No rotation should yield the original bounds (100, 200).
  CGSize sizeZero = GetWatermarkExpandedSizeForRotation(originalSize, 0.0);
  EXPECT_NEAR(sizeZero.width, 100.0, 0.01);
  EXPECT_NEAR(sizeZero.height, 200.0, 0.01);

  // 90-degree rotation (PI/2) should swap the dimensions to (200, 100).
  CGSize size90 = GetWatermarkExpandedSizeForRotation(originalSize, M_PI_2);
  EXPECT_NEAR(size90.width, 200.0, 0.01);
  EXPECT_NEAR(size90.height, 100.0, 0.01);

  // 45-degree rotation (PI/4).
  // W' = W * cos(45) + H * sin(45) = 100 * 0.7071 + 200 * 0.7071 ≈ 212.13
  // H' = H * cos(45) + W * sin(45) = 200 * 0.7071 + 100 * 0.7071 ≈ 212.13
  CGSize size45 = GetWatermarkExpandedSizeForRotation(originalSize, M_PI_4);
  EXPECT_NEAR(size45.width, 212.13, 0.1);
  EXPECT_NEAR(size45.height, 212.13, 0.1);

  // Negative angle -45 degrees (-PI/4).
  // Absolute value of cos and sin means the dimensions should be identical to
  // +45 degrees.
  CGSize sizeNeg45 = GetWatermarkExpandedSizeForRotation(originalSize, -M_PI_4);
  EXPECT_NEAR(sizeNeg45.width, 212.13, 0.1);
  EXPECT_NEAR(sizeNeg45.height, 212.13, 0.1);
}

// Tests that updating the view through the WatermarkConsumer protocol
// correctly sets its text and style parameters.
TEST_F(WatermarkViewTest, TestWatermarkConsumerProtocol) {
  WatermarkView* view = [[WatermarkView alloc] initWithFrame:CGRectZero];

  WatermarkStyle style;
  style.fill_opacity = 0.45;
  style.outline_opacity = 0.75;
  style.font_size = 32;

  id<WatermarkConsumer> consumer = (id<WatermarkConsumer>)view;
  [consumer updateWatermarkWithText:@"ProtocolText" style:style];

  EXPECT_NSEQ(view.text, @"ProtocolText");
  EXPECT_NEAR(view.fillOpacity, 0.45, 0.01);
  EXPECT_NEAR(view.outlineOpacity, 0.75, 0.01);
  EXPECT_NEAR(view.fontSize, 32.0, 0.01);
}

}  // namespace
