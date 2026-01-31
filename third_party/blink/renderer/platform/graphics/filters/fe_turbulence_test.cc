// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/filters/fe_turbulence.h"

#include "cc/paint/paint_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class FETurbulenceTest : public testing::Test {};

TEST_F(FETurbulenceTest, LargeFilterPrimitiveSubregion) {
  auto* filter = MakeGarbageCollected<Filter>(gfx::RectF(0, 0, 100, 100),
                                              gfx::RectF(0, 0, 100, 100), 1.0f,
                                              Filter::UnitScaling::kUserSpace);
  auto* effect = MakeGarbageCollected<FETurbulence>(
      filter, FETURBULENCE_TYPE_TURBULENCE, 0.1f, 0.1f, 1, 0, true);
  effect->SetFilterPrimitiveSubregion(gfx::RectF(0, 0, 3e9f, 3e9f));
  auto image_filter = effect->CreateImageFilterWithoutValidation();
  auto* t = static_cast<cc::TurbulencePaintFilter*>(image_filter.get());
  // Ensure the big filter primitive subregion was clamped and did not overflow.
  EXPECT_EQ(t->tile_size().width(), 2147483647);
  EXPECT_EQ(t->tile_size().height(), 2147483647);
}

}  // namespace blink
