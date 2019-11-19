// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutSVGTextTest : public RenderingTest {
 public:
  LayoutSVGTextTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(LayoutSVGTextTest, RectBasedHitTest) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id=svg width="300" height="300">
      <a id="link">
        <text id="text" y="20">text</text>
      </a>
    </svg>
  )HTML");

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& text = *GetDocument().getElementById("text")->firstChild();

  // Rect based hit testing
  auto results = RectBasedHitTest(PhysicalRect(0, 0, 300, 300));
  int count = 0;
  EXPECT_EQ(2u, results.size());
  for (auto result : results) {
    Node* node = result.Get();
    if (node == svg || node == text)
      count++;
  }
  EXPECT_EQ(2, count);
}

}  // namespace blink
