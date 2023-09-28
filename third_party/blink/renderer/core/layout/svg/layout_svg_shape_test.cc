// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using LayoutSVGShapeTest = RenderingTest;

TEST_F(LayoutSVGShapeTest, StrokeBoundingBoxOnEmptyShape) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <circle id="target" stroke="white" stroke-width="100"/>
    </svg>
  )HTML");

  auto* circle = GetLayoutObjectByElementId("target");
  EXPECT_EQ(circle->StrokeBoundingBox(), gfx::RectF(0, 0, 0, 0));
}

}  // namespace blink
