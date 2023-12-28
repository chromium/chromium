// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

TEST(MinMaxSizesTest, ShrinkToFit) {
  test::TaskEnvironment task_environment;
  MinMaxSizes sizes;

  sizes.min_size = LayoutUnit(100);
  sizes.max_size = LayoutUnit(200);
  EXPECT_EQ(LayoutUnit(200), sizes.ShrinkToFit(LayoutUnit(300)));

  sizes.min_size = LayoutUnit(100);
  sizes.max_size = LayoutUnit(300);
  EXPECT_EQ(LayoutUnit(200), sizes.ShrinkToFit(LayoutUnit(200)));

  sizes.min_size = LayoutUnit(200);
  sizes.max_size = LayoutUnit(300);
  EXPECT_EQ(LayoutUnit(200), sizes.ShrinkToFit(LayoutUnit(100)));
}

}  // namespace

}  // namespace blink
