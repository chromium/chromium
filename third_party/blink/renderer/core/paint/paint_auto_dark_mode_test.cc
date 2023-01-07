// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

class PaintAutoDarkModeTest : public RenderingTest {
 public:
  PaintAutoDarkModeTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(PaintAutoDarkModeTest, ShouldApplyFilterToImage) {
  DarkModeSettings settings;
  settings.mode = DarkModeInversionAlgorithm::kSimpleInvertForTesting;
  settings.image_policy = DarkModeImagePolicy::kFilterSmart;
  DarkModeFilter filter(settings);
  LocalFrame* frame = GetDocument().GetFrame();

  // |dst| is smaller than threshold size.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(*frame, gfx::RectF(50, 50),
                                                    gfx::RectF(50, 50))));

  // |dst| is smaller than threshold size, even |src| is larger.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(*frame, gfx::RectF(50, 50),
                                                    gfx::RectF(200, 200))));

  // |dst| is smaller than threshold size, |src| is smaller.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(*frame, gfx::RectF(50, 50),
                                                    gfx::RectF(20, 20))));

  // |src| having very smaller width, even |dst| is larger than threshold size.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(*frame, gfx::RectF(200, 5),
                                                    gfx::RectF(200, 5))));

  // |src| having very smaller height, even |dst| is larger than threshold size.
  EXPECT_TRUE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(*frame, gfx::RectF(5, 200),
                                                    gfx::RectF(5, 200))));

  // |dst| is larger than threshold size.
  EXPECT_FALSE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(
          *frame, gfx::RectF(200, 200), gfx::RectF(20, 20))));

  // |dst| is larger than threshold size.
  EXPECT_FALSE(filter.ShouldApplyFilterToImage(
      ImageClassifierHelper::GetImageTypeForTesting(*frame, gfx::RectF(20, 200),
                                                    gfx::RectF(20, 200))));
}

}  // namespace blink
