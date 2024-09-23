// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/page/page_zoom.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(PageZoomTest, ZoomValuesEqual) {
  // Test two identical values.
  EXPECT_TRUE(blink::ZoomValuesEqual(1.5, 1.5));

  // Test two values that are close enough to be considered equal.
  EXPECT_TRUE(blink::ZoomValuesEqual(1.5, 1.49999999));

  // Test two values that are close, but should not be considered equal.
  EXPECT_FALSE(blink::ZoomValuesEqual(1.5, 1.4));
}
