// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"

#include <gtest/gtest.h>
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {
namespace {

using ClipPathClipperTest = RenderingTest;

TEST_F(ClipPathClipperTest, ClipPathBoundingBoxClamped) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div id="e" style="width:1000px; height:1000px; will-change:transform;
                       clip-path:circle(1000000000%);">
    </div>
  )HTML");
  auto& object = *GetLayoutObjectByElementId("e");
  std::optional<gfx::RectF> bounding_box =
      ClipPathClipper::LocalClipPathBoundingBox(object);
  ASSERT_TRUE(bounding_box.has_value());
  EXPECT_EQ(gfx::RectF(InfiniteIntRect()), *bounding_box);
}

TEST_F(ClipPathClipperTest, ClipPathOnLayoutTableColNoCrash) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <clipPath id="clip">
        <rect width="10" height="10"/>
      </clipPath>
    </svg>
    <table>
      <colgroup>
        <col id="col" style="clip-path: url(#clip);">
        <col id="col2" style="clip-path: circle(50px);">
      </colgroup>
    </table>
  )HTML");
  auto* object = GetLayoutObjectByElementId("col");
  ASSERT_TRUE(object);
  // Verify that calling LocalClipPathBoundingBox on a LayoutTableColumn
  // (which has kNoPaintLayer) doesn't crash.
  EXPECT_FALSE(ClipPathClipper::LocalClipPathBoundingBox(*object));

  auto* object2 = GetLayoutObjectByElementId("col2");
  ASSERT_TRUE(object2);
  // Verify that shape clip-paths also return nullopt because the column doesn't
  // support clip path.
  EXPECT_FALSE(ClipPathClipper::LocalClipPathBoundingBox(*object2));
}

}  // unnamed namespace
}  // namespace blink
