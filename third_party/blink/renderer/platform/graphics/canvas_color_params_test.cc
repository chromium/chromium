// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/color_correction_test_utils.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "ui/gfx/color_space.h"

namespace blink {

// When drawing a color managed canvas, the target SkColorSpace is obtained by
// calling CanvasColorParams::GetSkColorSpace(). When drawing media to the
// canvas, the target gfx::ColorSpace is returned by CanvasColorParams::
// GetStorageGfxColorSpace(). This test verifies that the two different color
// spaces are approximately the same for different CanvasColorParam objects.
TEST(CanvasColorParamsTest, MatchSkColorSpaceWithGfxColorSpace) {
  test::TaskEnvironment task_environment;
  PredefinedColorSpace canvas_color_spaces[] = {
      PredefinedColorSpace::kSRGB,
      PredefinedColorSpace::kRec2020,
      PredefinedColorSpace::kP3,
  };
  for (PredefinedColorSpace color_space : canvas_color_spaces) {
    CanvasColorParams color_params(color_space, CanvasPixelFormat::kF16,
                                   kNonOpaque);
    sk_sp<SkColorSpace> canvas_drawing_color_space =
        color_params.GetSkColorSpace();
    sk_sp<SkColorSpace> canvas_media_color_space =
        color_params.GetStorageGfxColorSpace().ToSkColorSpace();
    ASSERT_TRUE(ColorCorrectionTestUtils::MatchColorSpace(
        canvas_drawing_color_space, canvas_media_color_space));
  }
}

}  // namespace blink
