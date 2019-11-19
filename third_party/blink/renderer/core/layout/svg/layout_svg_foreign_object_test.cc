// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutSVGForeignObjectTest : public RenderingTest {
 public:
  LayoutSVGForeignObjectTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}
};

TEST_F(LayoutSVGForeignObjectTest, DivInForeignObject) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id='svg' style='width: 500px; height: 400px'>
      <foreignObject id='foreign' x='100' y='100' width='300' height='200'>
        <div id='div' style='margin: 50px; width: 200px; height: 100px'>
        </div>
      </foreignObject>
    </svg>
  )HTML");

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& foreign = *GetDocument().getElementById("foreign");
  const auto& foreign_object = *GetLayoutObjectByElementId("foreign");
  const auto& div = *GetLayoutObjectByElementId("div");

  EXPECT_EQ(FloatRect(100, 100, 300, 200), foreign_object.ObjectBoundingBox());
  EXPECT_EQ(AffineTransform(), foreign_object.LocalSVGTransform());
  EXPECT_EQ(AffineTransform(), foreign_object.LocalToSVGParentTransform());

  // MapToVisualRectInAncestorSpace
  PhysicalRect div_rect(0, 0, 100, 50);
  EXPECT_TRUE(div.MapToVisualRectInAncestorSpace(&GetLayoutView(), div_rect));
  EXPECT_EQ(PhysicalRect(150, 150, 100, 50), div_rect);

  // LocalToAncestorPoint
  EXPECT_EQ(PhysicalOffset(150, 150),
            div.LocalToAncestorPoint(PhysicalOffset(), &GetLayoutView(),
                                     kTraverseDocumentBoundaries));

  // MapAncestorToLocal
  EXPECT_EQ(PhysicalOffset(-150, -150),
            div.AncestorToLocalPoint(&GetLayoutView(), PhysicalOffset(),
                                     kTraverseDocumentBoundaries));

  // PushMappingToContainer
  LayoutGeometryMap rgm(kTraverseDocumentBoundaries);
  rgm.PushMappingsToAncestor(&div, nullptr);
  EXPECT_EQ(PhysicalRect(150, 150, 1, 2),
            rgm.MapToAncestor(PhysicalRect(0, 0, 1, 2), nullptr));

  // Hit testing
  EXPECT_EQ(svg, HitTest(1, 1));
  EXPECT_EQ(foreign, HitTest(149, 149));
  EXPECT_EQ(div.GetNode(), HitTest(150, 150));
  EXPECT_EQ(div.GetNode(), HitTest(349, 249));
  EXPECT_EQ(foreign, HitTest(350, 250));
  EXPECT_EQ(svg, HitTest(450, 350));

  // Rect based hit testing
  auto results = RectBasedHitTest(PhysicalRect(0, 0, 300, 300));
  int count = 0;
  EXPECT_EQ(3u, results.size());
  for (auto result : results) {
    Node* node = result.Get();
    if (node == svg || node == div.GetNode() || node == foreign)
      count++;
  }
  EXPECT_EQ(3, count);
}

TEST_F(LayoutSVGForeignObjectTest, IframeInForeignObject) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <svg id='svg' style='width: 500px; height: 450px'>
      <foreignObject id='foreign' x='100' y='100' width='300' height='250'>
        <iframe id=iframe style='border: none; margin: 30px;
             width: 240px; height: 190px'></iframe>
      </foreignObject>
    </svg>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      body { margin: 0 }
      * { background: white; }
    </style>
    <div id='div' style='margin: 70px; width: 100px; height: 50px'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& foreign = *GetDocument().getElementById("foreign");
  const auto& foreign_object = *GetLayoutObjectByElementId("foreign");
  const auto& iframe = *GetDocument().getElementById("iframe");
  const auto& div = *ChildDocument().getElementById("div")->GetLayoutObject();

  EXPECT_EQ(FloatRect(100, 100, 300, 250), foreign_object.ObjectBoundingBox());
  EXPECT_EQ(AffineTransform(), foreign_object.LocalSVGTransform());
  EXPECT_EQ(AffineTransform(), foreign_object.LocalToSVGParentTransform());

  // MapToVisualRectInAncestorSpace
  PhysicalRect div_rect(0, 0, 100, 50);
  EXPECT_TRUE(div.MapToVisualRectInAncestorSpace(&GetLayoutView(), div_rect));
  EXPECT_EQ(PhysicalRect(200, 200, 100, 50), div_rect);

  // LocalToAncestorPoint
  EXPECT_EQ(PhysicalOffset(200, 200),
            div.LocalToAncestorPoint(PhysicalOffset(), &GetLayoutView(),
                                     kTraverseDocumentBoundaries));

  // AncestorToLocalPoint
  EXPECT_EQ(PhysicalOffset(-200, -200),
            div.AncestorToLocalPoint(&GetLayoutView(), PhysicalOffset(),
                                     kTraverseDocumentBoundaries));

  // PushMappingToContainer
  LayoutGeometryMap rgm(kTraverseDocumentBoundaries);
  rgm.PushMappingsToAncestor(&div, nullptr);
  EXPECT_EQ(PhysicalRect(200, 200, 1, 2),
            rgm.MapToAncestor(PhysicalRect(0, 0, 1, 2), nullptr));

  // Hit testing
  EXPECT_EQ(svg, HitTest(90, 90));
  EXPECT_EQ(foreign, HitTest(129, 129));
  EXPECT_EQ(ChildDocument().documentElement(), HitTest(130, 130));
  EXPECT_EQ(ChildDocument().documentElement(), HitTest(199, 199));
  EXPECT_EQ(div.GetNode(), HitTest(200, 200));
  EXPECT_EQ(div.GetNode(), HitTest(299, 249));
  EXPECT_EQ(ChildDocument().documentElement(), HitTest(300, 250));
  EXPECT_EQ(ChildDocument().documentElement(), HitTest(369, 319));
  EXPECT_EQ(foreign, HitTest(370, 320));
  EXPECT_EQ(svg, HitTest(450, 400));

  // Rect based hit testing
  auto results = RectBasedHitTest(PhysicalRect(0, 0, 300, 300));
  int count = 0;
  EXPECT_EQ(7u, results.size());
  for (auto result : results) {
    Node* node = result.Get();
    if (node == svg || node == div.GetNode() || node == foreign ||
        node == iframe)
      count++;
  }
  EXPECT_EQ(4, count);
}

TEST_F(LayoutSVGForeignObjectTest, HitTestZoomedForeignObject) {
  SetBodyInnerHTML(R"HTML(
    <style>* { margin: 0; zoom: 150% }</style>
    <svg id='svg' style='width: 200px; height: 200px'>
      <foreignObject id='foreign' x='10' y='10' width='100' height='150' style='overflow: visible'>
        <div id='div' style='margin: 50px; width: 50px; height: 50px'>
        </div>
      </foreignObject>
    </svg>
  )HTML");

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& foreign = *GetDocument().getElementById("foreign");
  const auto& foreign_object = *GetLayoutObjectByElementId("foreign");
  const auto& div = *GetDocument().getElementById("div");

  EXPECT_EQ(FloatRect(10, 10, 100, 150), foreign_object.ObjectBoundingBox());
  EXPECT_EQ(AffineTransform(), foreign_object.LocalSVGTransform());
  EXPECT_EQ(AffineTransform(), foreign_object.LocalToSVGParentTransform());

  // MapToVisualRectInAncestorSpace
  PhysicalRect div_rect(0, 0, 100, 50);
  EXPECT_TRUE(div.GetLayoutObject()->MapToVisualRectInAncestorSpace(
      &GetLayoutView(), div_rect));
  EXPECT_EQ(PhysicalRect(286, 286, 339, 170), div_rect);

  // LocalToAncestorPoint
  EXPECT_EQ(
      PhysicalOffset(LayoutUnit(286.875), LayoutUnit(286.875)),
      div.GetLayoutObject()->LocalToAncestorPoint(
          PhysicalOffset(), &GetLayoutView(), kTraverseDocumentBoundaries));

  // AncestorToLocalPoint
  EXPECT_EQ(PhysicalOffset(),
            div.GetLayoutObject()->AncestorToLocalPoint(
                &GetLayoutView(),
                PhysicalOffset(LayoutUnit(286.875), LayoutUnit(286.875)),
                kTraverseDocumentBoundaries));

  EXPECT_EQ(svg, HitTest(20, 20));
  EXPECT_EQ(foreign, HitTest(280, 280));
  EXPECT_EQ(div, HitTest(290, 290));

  // Rect based hit testing
  auto results = RectBasedHitTest(PhysicalRect(0, 0, 300, 300));
  int count = 0;
  EXPECT_EQ(3u, results.size());
  for (auto result : results) {
    Node* node = result.Get();
    if (node == svg || node == &div || node == foreign)
      count++;
  }
  EXPECT_EQ(3, count);
}

TEST_F(LayoutSVGForeignObjectTest, HitTestViewBoxForeignObject) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' style='width: 200px; height: 200px' viewBox='0 0 100 100'>
      <foreignObject id='foreign' x='10' y='10' width='100' height='150'>
        <div id='div' style='margin: 50px; width: 50px; height: 50px'>
        </div>
      </foreignObject>
    </svg>
  )HTML");

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& foreign = *GetDocument().getElementById("foreign");
  const auto& div = *GetDocument().getElementById("div");

  // LocalToAncestorPoint
  EXPECT_EQ(
      PhysicalOffset(128, 128),
      div.GetLayoutObject()->LocalToAncestorPoint(
          PhysicalOffset(), &GetLayoutView(), kTraverseDocumentBoundaries));

  // AncestorToLocalPoint
  EXPECT_EQ(PhysicalOffset(), div.GetLayoutObject()->AncestorToLocalPoint(
                                  &GetLayoutView(), PhysicalOffset(128, 128),
                                  kTraverseDocumentBoundaries));

  EXPECT_EQ(svg, HitTest(20, 20));
  EXPECT_EQ(foreign, HitTest(120, 110));
  EXPECT_EQ(div, HitTest(160, 160));
}

TEST_F(LayoutSVGForeignObjectTest, HitTestUnderClipPath) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0
      }
      #target {
         width: 500px;
         height: 500px;
         background-color: blue;
      }
      #target:hover {
        background-color: green;
      }
    </style>
    <svg id="svg" style="width: 500px; height: 500px">
      <clipPath id="c">
        <circle cx="250" cy="250" r="200"/>
      </clipPath>
      <g clip-path="url(#c)">
        <foreignObject id="foreignObject" width="100%" height="100%">
        </foreignObject>
      </g>
    </svg>
  )HTML");

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& foreignObject = *GetDocument().getElementById("foreignObject");

  // The fist and the third return |svg| because the circle clip-path
  // clips out the foreignObject.
  EXPECT_EQ(svg, GetDocument().ElementFromPoint(20, 20));
  EXPECT_EQ(foreignObject, GetDocument().ElementFromPoint(250, 250));
  EXPECT_EQ(svg, GetDocument().ElementFromPoint(400, 400));
}

TEST_F(LayoutSVGForeignObjectTest,
       HitTestUnderClippedPositionedForeignObjectDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0
      }
    </style>
    <svg id="svg" style="width: 600px; height: 600px">
      <foreignObject id="foreignObject" x="200" y="200" width="100"
          height="100">
        <div id="target" style="overflow: hidden; position: relative;
            width: 100px; height: 50px; left: 5px"></div>
      </foreignObject>
    </svg>
  )HTML");

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& target = *GetDocument().getElementById("target");
  const auto& foreignObject = *GetDocument().getElementById("foreignObject");

  EXPECT_EQ(svg, GetDocument().ElementFromPoint(1, 1));
  EXPECT_EQ(foreignObject, GetDocument().ElementFromPoint(201, 201));
  EXPECT_EQ(target, GetDocument().ElementFromPoint(206, 206));
  EXPECT_EQ(foreignObject, GetDocument().ElementFromPoint(205, 255));

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location((PhysicalOffset(206, 206)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(206, 206), result.PointInInnerNodeFrame());
}

TEST_F(LayoutSVGForeignObjectTest,
       HitTestUnderTransformedForeignObjectDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0
      }
    </style>
    <svg id="svg" style="width: 600px; height: 600px">
      <foreignObject id="foreignObject" x="200" y="200" width="100"
          height="100" transform="translate(30)">
        <div id="target" style="overflow: hidden; position: relative;
            width: 100px; height: 50px; left: 5px"></div>
      </foreignObject>
    </svg>
  )HTML");

  const auto& svg = *GetDocument().getElementById("svg");
  const auto& target = *GetDocument().getElementById("target");
  const auto& foreign_object = *GetDocument().getElementById("foreignObject");

  EXPECT_EQ(svg, GetDocument().ElementFromPoint(1, 1));
  EXPECT_EQ(foreign_object, GetDocument().ElementFromPoint(231, 201));
  EXPECT_EQ(target, GetDocument().ElementFromPoint(236, 206));
  EXPECT_EQ(foreign_object, GetDocument().ElementFromPoint(235, 255));

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location((PhysicalOffset(236, 206)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(236, 206), result.PointInInnerNodeFrame());
}

TEST_F(LayoutSVGForeignObjectTest, HitTestUnderScrollingAncestor) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0
      }
    </style>
    <div id=scroller style="width: 500px; height: 500px; overflow: auto">
      <svg width="3000" height="3000">
        <foreignObject width="3000" height="3000">
          <div id="target" style="width: 3000px; height: 3000px; background: red">
          </div>
        </foreignObject>
      </svg>
    </div>
  )HTML");

  auto& scroller = *GetDocument().getElementById("scroller");
  const auto& target = *GetDocument().getElementById("target");

  EXPECT_EQ(target, GetDocument().ElementFromPoint(450, 450));

  HitTestRequest request(HitTestRequest::kReadOnly | HitTestRequest::kActive);
  HitTestLocation location((PhysicalOffset(450, 450)));
  HitTestResult result(request, location);
  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(450, 450), result.PointInInnerNodeFrame());

  scroller.setScrollTop(3000);

  EXPECT_EQ(target, GetDocument().ElementFromPoint(450, 450));

  GetDocument().GetLayoutView()->HitTest(location, result);
  EXPECT_EQ(target, result.InnerNode());
  EXPECT_EQ(PhysicalOffset(450, 450), result.PointInInnerNodeFrame());
}

}  // namespace blink
