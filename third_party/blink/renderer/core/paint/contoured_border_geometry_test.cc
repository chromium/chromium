// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/contoured_border_geometry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"

namespace blink {

using ContouredBorderGeometryTest = RenderingTest;

TEST_F(ContouredBorderGeometryTest, NearlyTouchingNotchCorners) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 300px; height: 300px;
         border: 2px solid; border-radius: 120px; corner-shape: notch;
         border-top-right-radius: 184.1px"></div>
  )HTML");

  const ComputedStyle& style = GetLayoutObjectByElementId("target")->StyleRef();
  const ContouredRect border = ContouredBorderGeometry::ContouredBorder(
      style, PhysicalRect(0, 0, 304, 304));

  EXPECT_NEAR(304,
              border.GetRadii().BottomLeft().width() +
                  border.GetRadii().TopRight().width(),
              0.001);
}

}  // namespace blink
