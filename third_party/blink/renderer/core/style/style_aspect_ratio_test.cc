// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_aspect_ratio.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(StyleAspectRatioTest, LayoutAspectRatio) {
  // Just test there is no precision loss when multiply/dividing through the
  // aspect-ratio.
  StyleAspectRatio ratio(EAspectRatioType::kRatio, gfx::SizeF(0.25f, 0.1f));
  PhysicalSize layout_ratio = ratio.GetLayoutRatio();
  EXPECT_EQ(LayoutUnit(250),
            LayoutUnit(100).MulDiv(layout_ratio.width, layout_ratio.height));

  ratio = StyleAspectRatio(EAspectRatioType::kRatio, gfx::SizeF(0.1f, 0.25f));
  layout_ratio = ratio.GetLayoutRatio();
  EXPECT_EQ(LayoutUnit(40),
            LayoutUnit(100).MulDiv(layout_ratio.width, layout_ratio.height));

  ratio = StyleAspectRatio(EAspectRatioType::kRatio, gfx::SizeF(2.0f, 0.01f));
  layout_ratio = ratio.GetLayoutRatio();
  EXPECT_EQ(LayoutUnit(20000),
            LayoutUnit(100).MulDiv(layout_ratio.width, layout_ratio.height));

  ratio = StyleAspectRatio(EAspectRatioType::kRatio, gfx::SizeF(0.01f, 2.0f));
  layout_ratio = ratio.GetLayoutRatio();
  EXPECT_EQ(LayoutUnit(0.5),
            LayoutUnit(100).MulDiv(layout_ratio.width, layout_ratio.height));

  ratio = StyleAspectRatio(EAspectRatioType::kRatio, gfx::SizeF(0.7f, 1.0f));
  layout_ratio = ratio.GetLayoutRatio();
  EXPECT_EQ(LayoutUnit(70),
            LayoutUnit(100).MulDiv(layout_ratio.width, layout_ratio.height));

  ratio = StyleAspectRatio(EAspectRatioType::kRatio,
                           gfx::SizeF(0.00001f, 0.00002f));
  layout_ratio = ratio.GetLayoutRatio();
  EXPECT_EQ(LayoutUnit(50),
            LayoutUnit(100).MulDiv(layout_ratio.width, layout_ratio.height));

  ratio = StyleAspectRatio(EAspectRatioType::kRatio, gfx::SizeF(1.6f, 1.0f));
  layout_ratio = ratio.GetLayoutRatio();
  EXPECT_EQ(LayoutUnit(160),
            LayoutUnit(100).MulDiv(layout_ratio.width, layout_ratio.height));
}

}  // namespace

}  // namespace blink
