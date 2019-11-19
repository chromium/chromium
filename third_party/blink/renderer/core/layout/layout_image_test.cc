// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

using LayoutImageTest = RenderingTest;

TEST_F(LayoutImageTest, HitTestUnderTransform) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform: translateX(50px)'>
      <img id=target style='width: 20px; height: 20px'/>
    </div>
  )HTML");

  const auto& target = *GetDocument().getElementById("target");
  HitTestLocation location(PhysicalOffset(60, 10));
  HitTestResult result(
      HitTestRequest(HitTestRequest::kReadOnly | HitTestRequest::kActive |
                     HitTestRequest::kAllowChildFrameContent),
      location);
  GetLayoutView().HitTest(location, result);
  EXPECT_EQ(PhysicalOffset(60, 10), result.PointInInnerNodeFrame());
  EXPECT_EQ(target, result.InnerNode());
}

}  // namespace blink
