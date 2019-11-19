// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/page_viewport_state.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

using ViewportLengthTest = PlatformTest;

// Verifies viewport length construction for "device-width" and "device-height".
TEST_F(ViewportLengthTest, DeviceDimension) {
  ViewportLength device_width(@"device-width");
  EXPECT_TRUE(device_width.use_device_length());
  EXPECT_TRUE(isnan(device_width.value()));
  ViewportLength device_height(@"device-height");
  EXPECT_TRUE(device_height.use_device_length());
  EXPECT_TRUE(isnan(device_height.value()));
}

// Verifies viewport length construction for a hardcoded length value.
TEST_F(ViewportLengthTest, HardcodedDimension) {
  ViewportLength hardcoded_length(@"1024.0");
  EXPECT_FALSE(hardcoded_length.use_device_length());
  EXPECT_EQ(1024.0, hardcoded_length.value());
}

// Tests that malformed strings are handled correctly.
TEST_F(ViewportLengthTest, MalformedInput) {
  ViewportLength length(@"malformed input");
  EXPECT_FALSE(length.use_device_length());
  EXPECT_TRUE(isnan(length.value()));
}

using PageViewportStateTest = PlatformTest;

// Tests that a well-formed viewport tag is successfully parsed.
TEST_F(PageViewportStateTest, ValidInputParsing) {
  NSString* const kViewportContent =
      @"width=device-width, initial-scale=1.0, minimum-scale=1.0,"
       "maximum-scale=5.0,user-scalable=no";
  PageViewportState state(kViewportContent);
  EXPECT_TRUE(state.width().use_device_length());
  EXPECT_TRUE(isnan(state.width().value()));
  EXPECT_EQ(1.0, state.initial_zoom_scale());
  EXPECT_EQ(1.0, state.minimum_zoom_scale());
  EXPECT_EQ(5.0, state.maximum_zoom_scale());
  EXPECT_FALSE(state.user_scalable());
}

// Tests that malformed strings are handled correctly.
TEST_F(PageViewportStateTest, MalformedInput) {
  NSString* const kViewportContent =
      @"width=, initial-scale=not a valid value,, maximum-scale = ";
  PageViewportState state(kViewportContent);
  EXPECT_FALSE(state.width().use_device_length());
  EXPECT_TRUE(isnan(state.width().value()));
  EXPECT_TRUE(isnan(state.initial_zoom_scale()));
  EXPECT_TRUE(isnan(state.maximum_zoom_scale()));
}

// Tests parsing of the user-scalable property.
TEST_F(PageViewportStateTest, UserScalableParsing) {
  PageViewportState state(@"user-scalable=yes");
  EXPECT_TRUE(state.user_scalable());
  state = PageViewportState(@"user-scalable=1");
  EXPECT_TRUE(state.user_scalable());
  state = PageViewportState(@"user-scalable=no");
  EXPECT_FALSE(state.user_scalable());
  state = PageViewportState(@"user-scalable=0");
  EXPECT_FALSE(state.user_scalable());
}

}  // namespace web
