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

}  // unnamed namespace
}  // namespace blink
