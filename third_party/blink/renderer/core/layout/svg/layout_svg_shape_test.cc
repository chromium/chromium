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

TEST_F(LayoutSVGShapeTest, RectBasedHitTest_CircleEllipse) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id="svg" width="400" height="400">
      <circle id="stroked" cx="100" cy="100" r="50" stroke="blue"
              stroke-width="10" fill="none"/>
      <ellipse id="filled" cx="300" cy="300" rx="75" ry="50"/>
      <ellipse id="filled-xfrm" cx="100" cy="100" rx="75" ry="50"
               transform="translate(0 200) rotate(45, 100, 100)"/>
    </svg>
  )HTML");

  auto* svg = GetElementById("svg");
  auto* filled = GetElementById("filled");
  auto* filled_xfrm = GetElementById("filled-xfrm");
  auto* stroked = GetElementById("stroked");

  {
    // Touching all the shapes.
    auto results = RectBasedHitTest(PhysicalRect(100, 100, 200, 200));
    EXPECT_EQ(4u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(filled));
    EXPECT_TRUE(results.Contains(filled_xfrm));
    EXPECT_TRUE(results.Contains(stroked));
  }
  {
    // Inside #stroked.
    auto results = RectBasedHitTest(PhysicalRect(70, 70, 60, 60));
    EXPECT_EQ(1u, results.size());
    EXPECT_TRUE(results.Contains(svg));
  }
  {
    // Covering #stroked.
    auto results = RectBasedHitTest(PhysicalRect(44, 44, 112, 112));
    EXPECT_EQ(2u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(stroked));
  }
  {
    // Covering #filled-xfrm.
    auto results = RectBasedHitTest(PhysicalRect(30, 230, 140, 145));
    EXPECT_EQ(2u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(filled_xfrm));
  }
  {
    // Overlapping #stroked's bounding box but not intersecting.
    auto results = RectBasedHitTest(PhysicalRect(30, 30, 30, 30));
    EXPECT_EQ(1u, results.size());
    EXPECT_TRUE(results.Contains(svg));
  }
}

TEST_F(LayoutSVGShapeTest, RectBasedHitTest_Rect) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id="svg" width="200" height="200">
      <rect id="filled" x="10" y="25" width="80" height="50"/>
      <rect id="stroked" x="110" y="125" width="80" height="50"
            stroke="blue" stroke-width="10" fill="none"/>
      <rect id="filled-xfrm" x="10" y="25" width="80" height="50"
            transform="translate(100 0) rotate(45, 50, 50)"/>
      <rect id="stroked-xfrm" x="10" y="25" width="80" height="50"
            stroke="blue" stroke-width="10" fill="none"
            transform="translate(0 100) rotate(45, 50, 50)"/>
    </svg>
  )HTML");

  auto* svg = GetElementById("svg");
  auto* filled = GetElementById("filled");
  auto* filled_xfrm = GetElementById("filled-xfrm");
  auto* stroked = GetElementById("stroked");
  auto* stroked_xfrm = GetElementById("stroked-xfrm");

  {
    // Touching all the shapes.
    auto results = RectBasedHitTest(PhysicalRect(50, 50, 100, 100));
    EXPECT_EQ(5u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(filled));
    EXPECT_TRUE(results.Contains(filled_xfrm));
    EXPECT_TRUE(results.Contains(stroked));
    EXPECT_TRUE(results.Contains(stroked_xfrm));
  }
  {
    // Inside #stroked-xfrm.
    auto results = RectBasedHitTest(PhysicalRect(40, 140, 20, 20));
    EXPECT_EQ(1u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_FALSE(results.Contains(stroked_xfrm));
  }
  {
    // Covering #filled-xfrm.
    auto results = RectBasedHitTest(PhysicalRect(100, 0, 100, 100));
    EXPECT_EQ(2u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(filled_xfrm));
  }
  {
    // Covering #stroked.
    auto results = RectBasedHitTest(PhysicalRect(104, 119, 92, 62));
    EXPECT_EQ(2u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(stroked));
  }
  {
    // Outside all shapes.
    auto results = RectBasedHitTest(PhysicalRect(75, 77, 50, 40));
    EXPECT_EQ(1u, results.size());
    EXPECT_TRUE(results.Contains(svg));
  }
}

TEST_F(LayoutSVGShapeTest, RectBasedHitTest_Path) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id="svg" width="200" height="200">
      <path d="M30,50 Q0,0 50,30 100,0 70,50 100,100 50,70 0,100 30,50z"
            id="filled" fill-rule="evenodd"/>
      <path d="M30,50 Q0,0 50,30 100,0 70,50 100,100 50,70 0,100 30,50z"
            transform="translate(100 100) rotate(25, 50, 50)"
            id="filled-xfrm"/>
    </svg>
  )HTML");

  auto* svg = GetElementById("svg");
  auto* filled = GetElementById("filled");
  auto* filled_xfrm = GetElementById("filled-xfrm");

  {
    // Touching all the shapes.
    auto results = RectBasedHitTest(PhysicalRect(50, 50, 100, 100));
    EXPECT_EQ(3u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(filled));
    EXPECT_TRUE(results.Contains(filled_xfrm));
  }
  {
    // Inside #filled.
    auto results = RectBasedHitTest(PhysicalRect(35, 35, 30, 30));
    EXPECT_EQ(2u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(filled));
  }
  {
    // Covering #filled-xfrm.
    auto results = RectBasedHitTest(PhysicalRect(105, 105, 90, 90));
    EXPECT_EQ(2u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(filled_xfrm));
  }
  {
    // Intersecting #filled.
    auto results = RectBasedHitTest(PhysicalRect(25, 25, 50, 50));
    EXPECT_EQ(2u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(filled));
  }
  {
    // Intersecting #filled-xfrm.
    auto results = RectBasedHitTest(PhysicalRect(125, 125, 50, 50));
    EXPECT_EQ(2u, results.size());
    EXPECT_TRUE(results.Contains(svg));
    EXPECT_TRUE(results.Contains(filled_xfrm));
  }
}

}  // namespace blink
