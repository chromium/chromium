// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

TEST(PhysicalSizeTest, FitToAspectRatioShrink) {
  PhysicalSize aspect_ratio(50000, 40000);
  EXPECT_EQ(PhysicalSize(1250, 1000),
            PhysicalSize(2000, 1000)
                .FitToAspectRatio(aspect_ratio, kAspectRatioFitShrink));
  EXPECT_EQ(PhysicalSize(1000, 800),
            PhysicalSize(1000, 2000)
                .FitToAspectRatio(aspect_ratio, kAspectRatioFitShrink));
}

TEST(PhysicalSizeTest, FitToAspectRatioGrow) {
  PhysicalSize aspect_ratio(50000, 40000);
  EXPECT_EQ(PhysicalSize(2000, 1600),
            PhysicalSize(2000, 1000)
                .FitToAspectRatio(aspect_ratio, kAspectRatioFitGrow));
  EXPECT_EQ(PhysicalSize(2500, 2000),
            PhysicalSize(1000, 2000)
                .FitToAspectRatio(aspect_ratio, kAspectRatioFitGrow));
}

}  // namespace blink
