// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/layout_unit_diffuser.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

constexpr LayoutUnit kEpsilon(LayoutUnit::Epsilon());

TEST(LayoutUnitDiffuserTest, ThreeBuckets) {
  {
    const LayoutUnit base = LayoutUnit(7) / 3u;
    LayoutUnitDiffuser diffuser(LayoutUnit(7), 3u);
    EXPECT_EQ(diffuser.Next(), base);
    EXPECT_EQ(diffuser.Next(), base + kEpsilon);
    EXPECT_EQ(diffuser.Next(), base);
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }

  {
    const LayoutUnit base = LayoutUnit(8) / 3u;
    LayoutUnitDiffuser diffuser(LayoutUnit(8), 3u);
    EXPECT_EQ(diffuser.Next(), base + kEpsilon);
    EXPECT_EQ(diffuser.Next(), base);
    EXPECT_EQ(diffuser.Next(), base + kEpsilon);
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }
}

TEST(LayoutUnitDiffuserTest, FourBuckets) {
  {
    LayoutUnitDiffuser diffuser(kEpsilon, 4u);
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }

  {
    LayoutUnitDiffuser diffuser(kEpsilon * 2, 4u);
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }

  {
    LayoutUnitDiffuser diffuser(kEpsilon * 3, 4u);
    EXPECT_TRUE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }
}

TEST(LayoutUnitDiffuserTest, SevenBuckets) {
  {
    LayoutUnitDiffuser diffuser(kEpsilon, 7u);
    EXPECT_FALSE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }

  {
    LayoutUnitDiffuser diffuser(kEpsilon * 2, 7u);
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }

  {
    LayoutUnitDiffuser diffuser(kEpsilon * 3, 7u);
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }

  {
    LayoutUnitDiffuser diffuser(kEpsilon * 4, 7u);
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }

  {
    LayoutUnitDiffuser diffuser(kEpsilon * 5, 7u);
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }
  {
    LayoutUnitDiffuser diffuser(kEpsilon * 6, 7u);
    EXPECT_TRUE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_FALSE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_TRUE(diffuser.Next());
    EXPECT_EQ(diffuser.Next(), LayoutUnit());
  }
}

}  // namespace

}  // namespace blink
