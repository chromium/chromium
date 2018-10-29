// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/transforms/transform_state.h"

namespace blink {

class MapCoordinatesTest : public RenderingTest {
 public:
  MapCoordinatesTest() : RenderingTest(SingleChildLocalFrameClient::Create()) {}
  FloatPoint MapLocalToAncestor(const LayoutObject*,
                                const LayoutBoxModelObject* ancestor,
                                FloatPoint,
                                MapCoordinatesFlags = 0) const;
  FloatQuad MapLocalToAncestor(const LayoutObject*,
                               const LayoutBoxModelObject* ancestor,
                               FloatQuad,
                               MapCoordinatesFlags = 0) const;
  FloatPoint MapAncestorToLocal(const LayoutObject*,
                                const LayoutBoxModelObject* ancestor,
                                FloatPoint,
                                MapCoordinatesFlags = 0) const;
  FloatQuad MapAncestorToLocal(const LayoutObject*,
                               const LayoutBoxModelObject* ancestor,
                               FloatQuad,
                               MapCoordinatesFlags = 0) const;

  // Adjust point by the scroll offset of the LayoutView.  This only has an
  // effect if root layer scrolling is enabled.  The only reason for doing
  // this here is so the test expected values can be the same whether or not
  // root layer scrolling is enabled.  This is analogous to what
  // LayoutGeometryMapTest does; for more context, see:
  // https://codereview.chromium.org/2417103002/#msg11
  FloatPoint AdjustForFrameScroll(const FloatPoint&) const;
};

// One note about tests here that operate on LayoutInline and LayoutText
// objects: mapLocalToAncestor() expects such objects to pass their static
// location and size (relatively to the border edge of their container) to
// mapLocalToAncestor() via the TransformState argument. mapLocalToAncestor() is
// then only expected to make adjustments for relative-positioning,
// container-specific characteristics (such as writing mode roots, multicol),
// and so on. This in contrast to LayoutBox objects, where the TransformState
// passed is relative to the box itself, not the container.

FloatPoint MapCoordinatesTest::AdjustForFrameScroll(
    const FloatPoint& point) const {
  FloatPoint result(point);
  LayoutView* layout_view = GetDocument().GetLayoutView();
  if (layout_view->HasOverflowClip())
    result.Move(-layout_view->ScrolledContentOffset());
  return result;
}

FloatPoint MapCoordinatesTest::MapLocalToAncestor(
    const LayoutObject* object,
    const LayoutBoxModelObject* ancestor,
    FloatPoint point,
    MapCoordinatesFlags mode) const {
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 point);
  object->MapLocalToAncestor(ancestor, transform_state, mode);
  transform_state.Flatten();
  return transform_state.LastPlanarPoint();
}

FloatQuad MapCoordinatesTest::MapLocalToAncestor(
    const LayoutObject* object,
    const LayoutBoxModelObject* ancestor,
    FloatQuad quad,
    MapCoordinatesFlags mode) const {
  TransformState transform_state(TransformState::kApplyTransformDirection,
                                 quad.BoundingBox().Center(), quad);
  object->MapLocalToAncestor(ancestor, transform_state, mode);
  transform_state.Flatten();
  return transform_state.LastPlanarQuad();
}

FloatPoint MapCoordinatesTest::MapAncestorToLocal(
    const LayoutObject* object,
    const LayoutBoxModelObject* ancestor,
    FloatPoint point,
    MapCoordinatesFlags mode) const {
  TransformState transform_state(
      TransformState::kUnapplyInverseTransformDirection, point);
  object->MapAncestorToLocal(ancestor, transform_state, mode);
  transform_state.Flatten();
  return transform_state.LastPlanarPoint();
}

FloatQuad MapCoordinatesTest::MapAncestorToLocal(
    const LayoutObject* object,
    const LayoutBoxModelObject* ancestor,
    FloatQuad quad,
    MapCoordinatesFlags mode) const {
  TransformState transform_state(
      TransformState::kUnapplyInverseTransformDirection,
      quad.BoundingBox().Center(), quad);
  object->MapAncestorToLocal(ancestor, transform_state, mode);
  transform_state.Flatten();
  return transform_state.LastPlanarQuad();
}

TEST_F(MapCoordinatesTest, SimpleText) {
  SetBodyInnerHTML("<div id='container'><br>text</div>");

  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));
  LayoutObject* text = ToLayoutBlockFlow(container)->LastChild();
  ASSERT_TRUE(text->IsText());
  FloatPoint mapped_point =
      MapLocalToAncestor(text, container, FloatPoint(10, 30));
  EXPECT_EQ(FloatPoint(10, 30), mapped_point);
  mapped_point = MapAncestorToLocal(text, container, mapped_point);
  EXPECT_EQ(FloatPoint(10, 30), mapped_point);
}

TEST_F(MapCoordinatesTest, SimpleInline) {
  SetBodyInnerHTML("<div><span id='target'>text</span></div>");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  FloatPoint mapped_point = MapLocalToAncestor(
      target, ToLayoutBoxModelObject(target->Parent()), FloatPoint(10, 10));
  EXPECT_EQ(FloatPoint(10, 10), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, ToLayoutBoxModelObject(target->Parent()), mapped_point);
  EXPECT_EQ(FloatPoint(10, 10), mapped_point);
}

TEST_F(MapCoordinatesTest, SimpleBlock) {
  SetBodyInnerHTML(R"HTML(
    <div style='margin:666px; border:8px solid; padding:7px;'>
        <div id='target' style='margin:10px; border:666px;
    padding:666px;'></div>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  FloatPoint mapped_point = MapLocalToAncestor(
      target, ToLayoutBoxModelObject(target->Parent()), FloatPoint(100, 100));
  EXPECT_EQ(FloatPoint(125, 125), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, ToLayoutBoxModelObject(target->Parent()), mapped_point);
  EXPECT_EQ(FloatPoint(100, 100), mapped_point);
}

TEST_F(MapCoordinatesTest, OverflowClip) {
  SetBodyInnerHTML(R"HTML(
    <div id='overflow' style='height: 100px; width: 100px; border:8px
    solid; padding:7px; overflow:scroll'>
        <div style='height:200px; width:200px'></div>
        <div id='target' style='margin:10px; border:666px;
    padding:666px;'></div>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutObject* overflow = GetLayoutObjectByElementId("overflow");
  ToLayoutBox(overflow)->ScrollToPosition(FloatPoint(32, 54));

  FloatPoint mapped_point = MapLocalToAncestor(
      target, ToLayoutBoxModelObject(target->Parent()), FloatPoint(100, 100));
  EXPECT_EQ(FloatPoint(93, 271), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, ToLayoutBoxModelObject(target->Parent()), mapped_point);
  EXPECT_EQ(FloatPoint(100, 100), mapped_point);
}

TEST_F(MapCoordinatesTest, TextInRelPosInline) {
  SetBodyInnerHTML(
      "<div><span style='position:relative; left:7px; top:4px;'><br "
      "id='sibling'>text</span></div>");

  LayoutObject* br = GetLayoutObjectByElementId("sibling");
  LayoutObject* text = br->NextSibling();
  ASSERT_TRUE(text->IsText());
  FloatPoint mapped_point =
      MapLocalToAncestor(text, text->ContainingBlock(), FloatPoint(10, 30));
  EXPECT_EQ(FloatPoint(17, 34), mapped_point);
  mapped_point =
      MapAncestorToLocal(text, text->ContainingBlock(), mapped_point);
  EXPECT_EQ(FloatPoint(10, 30), mapped_point);
}

TEST_F(MapCoordinatesTest, RelposInline) {
  SetBodyInnerHTML(
      "<span id='target' style='position:relative; left:50px; "
      "top:100px;'>text</span>");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  FloatPoint mapped_point = MapLocalToAncestor(
      target, ToLayoutBoxModelObject(target->Parent()), FloatPoint(10, 10));
  EXPECT_EQ(FloatPoint(60, 110), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, ToLayoutBoxModelObject(target->Parent()), mapped_point);
  EXPECT_EQ(FloatPoint(10, 10), mapped_point);
}

TEST_F(MapCoordinatesTest, RelposInlineInRelposInline) {
  SetBodyInnerHTML(R"HTML(
    <div style='padding-left:10px;'>
        <span style='position:relative; left:5px; top:6px;'>
            <span id='target' style='position:relative; left:50px;
    top:100px;'>text</span>
        </span>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutInline* parent = ToLayoutInline(target->Parent());
  LayoutBlockFlow* containing_block = ToLayoutBlockFlow(parent->Parent());

  FloatPoint mapped_point =
      MapLocalToAncestor(target, containing_block, FloatPoint(20, 10));
  EXPECT_EQ(FloatPoint(75, 116), mapped_point);
  mapped_point = MapAncestorToLocal(target, containing_block, mapped_point);
  EXPECT_EQ(FloatPoint(20, 10), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  mapped_point = MapLocalToAncestor(target, parent, FloatPoint(20, 10));
  EXPECT_EQ(FloatPoint(70, 110), mapped_point);

  mapped_point = MapLocalToAncestor(parent, containing_block, mapped_point);
  EXPECT_EQ(FloatPoint(75, 116), mapped_point);

  mapped_point = MapAncestorToLocal(parent, containing_block, mapped_point);
  EXPECT_EQ(FloatPoint(70, 110), mapped_point);

  mapped_point = MapAncestorToLocal(target, parent, mapped_point);
  EXPECT_EQ(FloatPoint(20, 10), mapped_point);
}

TEST_F(MapCoordinatesTest, RelPosBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='margin:666px; border:8px solid;
    padding:7px;'>
        <div id='middle' style='margin:30px; border:1px solid;'>
            <div id='target' style='position:relative; left:50px; top:50px;
    margin:10px; border:666px; padding:666px;'></div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(106, 106), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, FloatPoint(110, 110));
  EXPECT_EQ(FloatPoint(4, 4), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* middle = ToLayoutBox(GetLayoutObjectByElementId("middle"));

  mapped_point = MapLocalToAncestor(target, middle, FloatPoint());
  EXPECT_EQ(FloatPoint(61, 61), mapped_point);

  mapped_point = MapLocalToAncestor(middle, container, mapped_point);
  EXPECT_EQ(FloatPoint(106, 106), mapped_point);

  mapped_point = MapAncestorToLocal(middle, container, mapped_point);
  EXPECT_EQ(FloatPoint(61, 61), mapped_point);

  mapped_point = MapAncestorToLocal(target, middle, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, AbsPos) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position:relative; margin:666px; border:8px
    solid; padding:7px;'>
        <div id='staticChild' style='margin:30px; padding-top:666px;'>
            <div style='padding-top:666px;'></div>
            <div id='target' style='position:absolute; left:-1px; top:-1px;
    margin:10px; border:666px; padding:666px;'></div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(17, 17), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, FloatPoint(18, 18));
  EXPECT_EQ(FloatPoint(1, 1), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* static_child =
      ToLayoutBox(GetLayoutObjectByElementId("staticChild"));

  mapped_point = MapLocalToAncestor(target, static_child, FloatPoint());
  EXPECT_EQ(FloatPoint(-28, -28), mapped_point);

  mapped_point = MapLocalToAncestor(static_child, container, mapped_point);
  EXPECT_EQ(FloatPoint(17, 17), mapped_point);

  mapped_point = MapAncestorToLocal(static_child, container, mapped_point);
  EXPECT_EQ(FloatPoint(-28, -28), mapped_point);

  mapped_point = MapAncestorToLocal(target, static_child, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, AbsPosAuto) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position:absolute; margin:666px; border:8px
    solid; padding:7px;'>
        <div id='staticChild' style='margin:30px; padding-top:5px;'>
            <div style='padding-top:20px;'></div>
            <div id='target' style='position:absolute; margin:10px;
    border:666px; padding:666px;'></div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(55, 80), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, FloatPoint(56, 82));
  EXPECT_EQ(FloatPoint(1, 2), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* static_child =
      ToLayoutBox(GetLayoutObjectByElementId("staticChild"));

  mapped_point = MapLocalToAncestor(target, static_child, FloatPoint());
  EXPECT_EQ(FloatPoint(10, 35), mapped_point);

  mapped_point = MapLocalToAncestor(static_child, container, mapped_point);
  EXPECT_EQ(FloatPoint(55, 80), mapped_point);

  mapped_point = MapAncestorToLocal(static_child, container, mapped_point);
  EXPECT_EQ(FloatPoint(10, 35), mapped_point);

  mapped_point = MapAncestorToLocal(target, static_child, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, FixedPos) {
  // Assuming BODY margin of 8px.
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position:absolute; margin:4px; border:5px
    solid; padding:7px;'>
        <div id='staticChild' style='padding-top:666px;'>
            <div style='padding-top:666px;'></div>
            <div id='target' style='position:fixed; left:-1px; top:-1px;
    margin:10px; border:666px; padding:666px;'></div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* static_child =
      ToLayoutBox(GetLayoutObjectByElementId("staticChild"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(view->IsLayoutView());

  FloatPoint mapped_point = MapLocalToAncestor(target, view, FloatPoint());
  EXPECT_EQ(FloatPoint(9, 9), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, FloatPoint(10, 11));
  EXPECT_EQ(FloatPoint(1, 2), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  mapped_point = MapLocalToAncestor(target, static_child, FloatPoint());
  EXPECT_EQ(FloatPoint(-15, -15), mapped_point);

  mapped_point = MapLocalToAncestor(static_child, container, mapped_point);
  EXPECT_EQ(FloatPoint(-3, -3), mapped_point);

  mapped_point = MapLocalToAncestor(container, body, mapped_point);
  EXPECT_EQ(FloatPoint(1, 1), mapped_point);

  mapped_point = MapLocalToAncestor(body, html, mapped_point);
  EXPECT_EQ(FloatPoint(9, 9), mapped_point);

  mapped_point = MapLocalToAncestor(html, view, mapped_point);
  EXPECT_EQ(FloatPoint(9, 9), mapped_point);

  mapped_point = MapAncestorToLocal(html, view, mapped_point);
  EXPECT_EQ(FloatPoint(9, 9), mapped_point);

  mapped_point = MapAncestorToLocal(body, html, mapped_point);
  EXPECT_EQ(FloatPoint(1, 1), mapped_point);

  mapped_point = MapAncestorToLocal(container, body, mapped_point);
  EXPECT_EQ(FloatPoint(-3, -3), mapped_point);

  mapped_point = MapAncestorToLocal(static_child, container, mapped_point);
  EXPECT_EQ(FloatPoint(-15, -15), mapped_point);

  mapped_point = MapAncestorToLocal(target, static_child, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, FixedPosAuto) {
  // Assuming BODY margin of 8px.
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position:absolute; margin:3px; border:8px
    solid; padding:7px;'>
        <div id='staticChild' style='padding-top:5px;'>
            <div style='padding-top:20px;'></div>
            <div id='target' style='position:fixed; margin:10px;
    border:666px; padding:666px;'></div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* static_child =
      ToLayoutBox(GetLayoutObjectByElementId("staticChild"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(view->IsLayoutView());

  FloatPoint mapped_point =
      MapLocalToAncestor(target, target->ContainingBlock(), FloatPoint());
  EXPECT_EQ(FloatPoint(36, 61), mapped_point);
  mapped_point =
      MapAncestorToLocal(target, target->ContainingBlock(), FloatPoint(36, 61));
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  mapped_point = MapLocalToAncestor(target, static_child, FloatPoint());
  EXPECT_EQ(FloatPoint(10, 35), mapped_point);

  mapped_point = MapLocalToAncestor(static_child, container, mapped_point);
  EXPECT_EQ(FloatPoint(25, 50), mapped_point);

  mapped_point = MapLocalToAncestor(container, body, mapped_point);
  EXPECT_EQ(FloatPoint(28, 53), mapped_point);

  mapped_point = MapLocalToAncestor(body, html, mapped_point);
  EXPECT_EQ(FloatPoint(36, 61), mapped_point);

  mapped_point = MapLocalToAncestor(html, view, mapped_point);
  EXPECT_EQ(FloatPoint(36, 61), mapped_point);

  mapped_point = MapAncestorToLocal(html, view, mapped_point);
  EXPECT_EQ(FloatPoint(36, 61), mapped_point);

  mapped_point = MapAncestorToLocal(body, html, mapped_point);
  EXPECT_EQ(FloatPoint(28, 53), mapped_point);

  mapped_point = MapAncestorToLocal(container, body, mapped_point);
  EXPECT_EQ(FloatPoint(25, 50), mapped_point);

  mapped_point = MapAncestorToLocal(static_child, container, mapped_point);
  EXPECT_EQ(FloatPoint(10, 35), mapped_point);

  mapped_point = MapAncestorToLocal(target, static_child, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, FixedPosInFixedPos) {
  // Assuming BODY margin of 8px.
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position:absolute; margin:4px; border:5px
    solid; padding:7px;'>
        <div id='staticChild' style='padding-top:666px;'>
            <div style='padding-top:666px;'></div>
            <div id='outerFixed' style='position:fixed; left:100px;
    top:100px; margin:10px; border:666px; padding:666px;'>
                <div id='target' style='position:fixed; left:-1px;
    top:-1px; margin:10px; border:666px; padding:666px;'></div>
            </div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* outer_fixed =
      ToLayoutBox(GetLayoutObjectByElementId("outerFixed"));
  LayoutBox* static_child =
      ToLayoutBox(GetLayoutObjectByElementId("staticChild"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(view->IsLayoutView());

  FloatPoint mapped_point = MapLocalToAncestor(target, view, FloatPoint());
  EXPECT_EQ(FloatPoint(9, 9), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, FloatPoint(9, 9));
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  mapped_point = MapLocalToAncestor(target, outer_fixed, FloatPoint());
  EXPECT_EQ(FloatPoint(-101, -101), mapped_point);

  mapped_point = MapLocalToAncestor(outer_fixed, static_child, mapped_point);
  EXPECT_EQ(FloatPoint(-15, -15), mapped_point);

  mapped_point = MapLocalToAncestor(static_child, container, mapped_point);
  EXPECT_EQ(FloatPoint(-3, -3), mapped_point);

  mapped_point = MapLocalToAncestor(container, body, mapped_point);
  EXPECT_EQ(FloatPoint(1, 1), mapped_point);

  mapped_point = MapLocalToAncestor(body, html, mapped_point);
  EXPECT_EQ(FloatPoint(9, 9), mapped_point);

  mapped_point = MapLocalToAncestor(html, view, mapped_point);
  EXPECT_EQ(FloatPoint(9, 9), mapped_point);

  mapped_point = MapAncestorToLocal(html, view, mapped_point);
  EXPECT_EQ(FloatPoint(9, 9), mapped_point);

  mapped_point = MapAncestorToLocal(body, html, mapped_point);
  EXPECT_EQ(FloatPoint(1, 1), mapped_point);

  mapped_point = MapAncestorToLocal(container, body, mapped_point);
  EXPECT_EQ(FloatPoint(-3, -3), mapped_point);

  mapped_point = MapAncestorToLocal(static_child, container, mapped_point);
  EXPECT_EQ(FloatPoint(-15, -15), mapped_point);

  mapped_point = MapAncestorToLocal(outer_fixed, static_child, mapped_point);
  EXPECT_EQ(FloatPoint(-101, -101), mapped_point);

  mapped_point = MapAncestorToLocal(target, outer_fixed, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, FixedPosInFixedPosScrollView) {
  SetBodyInnerHTML(R"HTML(
    <div style='height: 4000px'></div>
    <div id='container' style='position:fixed; top: 100px; left: 100px'>
      <div id='target' style='position:fixed; top: 200px; left: 200px'>
      </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(view->IsLayoutView());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0.0, 50),
                                                          kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(50,
            GetDocument().View()->LayoutViewport()->ScrollOffsetInt().Height());

  FloatPoint mapped_point = MapLocalToAncestor(target, view, FloatPoint());
  EXPECT_EQ(AdjustForFrameScroll(FloatPoint(200, 250)), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(100, 100), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, FloatPoint(100, 100));
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, FixedPosInAbsolutePosScrollView) {
  SetBodyInnerHTML(R"HTML(
    <div style='height: 4000px'></div>
    <div id='container' style='position:absolute; top: 100px; left: 100px'>
      <div id='target' style='position:fixed; top: 200px; left: 200px'>
      </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(view->IsLayoutView());

  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0.0, 50),
                                                          kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(50,
            GetDocument().View()->LayoutViewport()->ScrollOffsetInt().Height());

  FloatPoint mapped_point = MapLocalToAncestor(target, view, FloatPoint());
  EXPECT_EQ(AdjustForFrameScroll(FloatPoint(200, 250)), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(100, 150), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, FloatPoint(100, 150));
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, FixedPosInTransform) {
  SetBodyInnerHTML(R"HTML(
    <style>#container { transform: translateY(100px); position: absolute;
    left: 0; top: 100px; }
    .fixed { position: fixed; top: 0; }
    .spacer { height: 2000px; } </style>
    <div id='container'><div class='fixed' id='target'></div></div>
    <div class='spacer'></div>
  )HTML");

  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0.0, 50),
                                                          kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(50,
            GetDocument().View()->LayoutViewport()->ScrollOffsetInt().Height());

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(view->IsLayoutView());

  FloatPoint mapped_point = MapLocalToAncestor(target, view, FloatPoint());
  EXPECT_EQ(AdjustForFrameScroll(FloatPoint(0, 100)), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(0, 0), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, FloatPoint(0, 0));
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(container, view, FloatPoint());
  EXPECT_EQ(AdjustForFrameScroll(FloatPoint(0, 100)), mapped_point);
  mapped_point = MapAncestorToLocal(container, view, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, FixedPosInContainPaint) {
  SetBodyInnerHTML(R"HTML(
    <style>#container { contain: paint; position: absolute; left: 0; top:
    100px; }
    .fixed { position: fixed; top: 0; }
    .spacer { height: 2000px; } </style>
    <div id='container'><div class='fixed' id='target'></div></div>
    <div class='spacer'></div>
  )HTML");

  GetDocument().View()->LayoutViewport()->SetScrollOffset(ScrollOffset(0.0, 50),
                                                          kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(50,
            GetDocument().View()->LayoutViewport()->ScrollOffsetInt().Height());

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(view->IsLayoutView());

  FloatPoint mapped_point = MapLocalToAncestor(target, view, FloatPoint());
  EXPECT_EQ(AdjustForFrameScroll(FloatPoint(0, 100)), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(0, 0), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, FloatPoint(0, 0));
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(container, view, FloatPoint());
  EXPECT_EQ(AdjustForFrameScroll(FloatPoint(0, 100)), mapped_point);
  mapped_point = MapAncestorToLocal(container, view, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

// TODO(chrishtr): add more multi-frame tests.
TEST_F(MapCoordinatesTest, FixedPosInIFrameWhenMainFrameScrolled) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div style='width: 200; height: 8000px'></div>
    <iframe src='http://test.com' width='500' height='500'
    frameBorder='0'>
    </iframe>
  )HTML");
  SetChildFrameHTML(
      "<style>body { margin: 0; } #target { width: 200px; height: 200px; "
      "position:fixed}</style><div id=target></div>");

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 1000), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* target = ChildDocument().getElementById("target");
  ASSERT_TRUE(target);
  FloatPoint mapped_point =
      MapAncestorToLocal(target->GetLayoutObject(), nullptr, FloatPoint(10, 70),
                         kTraverseDocumentBoundaries);

  // y = 70 - 8000, since the iframe is offset by 8000px from the main frame.
  // The scroll is not taken into account because the element is not fixed to
  // the root LayoutView, and the space of the root LayoutView does not include
  // scroll.
  EXPECT_EQ(FloatPoint(10, -7930), AdjustForFrameScroll(mapped_point));
}

TEST_F(MapCoordinatesTest, IFrameTransformed) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <iframe style='transform: scale(2)' src='http://test.com'
    width='500' height='500' frameBorder='0'>
    </iframe>
  )HTML");
  SetChildFrameHTML(
      "<style>body { margin: 0; } #target { width: 200px; "
      "height: 8000px}</style><div id=target></div>");

  GetDocument().View()->UpdateAllLifecyclePhases();

  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 1000), kProgrammaticScroll);
  ChildDocument().View()->UpdateAllLifecyclePhases();

  Element* target = ChildDocument().getElementById("target");
  ASSERT_TRUE(target);
  FloatPoint mapped_point = MapAncestorToLocal(
      target->GetLayoutObject(), nullptr, FloatPoint(200, 200),
      kTraverseDocumentBoundaries | kUseTransforms);

  // Derivation:
  // (200, 200) -> (-50, -50)  (Adjust for transform origin of scale, which is
  //                           at the center of the 500x500 iframe)
  // (-50, -50) -> (-25, -25)  (Divide by 2 to invert the scale)
  // (-25, -25) -> (225, 225)  (Add the origin back in)
  // (225, 225) -> (225, 1225) (Adjust by scroll offset of y=1000)
  EXPECT_EQ(FloatPoint(225, 1225), mapped_point);
}

TEST_F(MapCoordinatesTest, FixedPosInScrolledIFrameWithTransform) {
  GetDocument().SetBaseURLOverride(KURL("http://test.com"));
  SetBodyInnerHTML(R"HTML(
    <style>* { margin: 0; }</style>
    <div style='position: absolute; left: 0px; top: 0px; width: 1024px;
    height: 768px; transform-origin: 0 0; transform: scale(0.5, 0.5);'>
        <iframe frameborder=0 src='http://test.com'
    sandbox='allow-same-origin' width='1024' height='768'></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>* { margin: 0; } #target { width: 200px; height: 200px;
    position:fixed}</style><div id=target></div>
    <div style='width: 200; height: 8000px'></div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhases();
  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 1000), kProgrammaticScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  Element* target = ChildDocument().getElementById("target");
  ASSERT_TRUE(target);
  FloatPoint mapped_point =
      MapAncestorToLocal(target->GetLayoutObject(), nullptr, FloatPoint(0, 0),
                         kUseTransforms | kTraverseDocumentBoundaries);

  EXPECT_EQ(FloatPoint(0, 0), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithText) {
  SetBodyInnerHTML(R"HTML(
    <div id='multicol' style='columns:2; column-gap:20px; width:400px;
    line-height:50px; padding:5px; orphans:1; widows:1;'>
        <br id='sibling'>
        text
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("sibling")->NextSibling();
  ASSERT_TRUE(target->IsText());
  LayoutBox* flow_thread = ToLayoutBox(target->Parent());
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());
  LayoutBox* multicol = ToLayoutBox(GetLayoutObjectByElementId("multicol"));

  FloatPoint mapped_point =
      MapLocalToAncestor(target, flow_thread, FloatPoint(10, 70));
  EXPECT_EQ(FloatPoint(10, 70), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(FloatPoint(10, 70), mapped_point);

  mapped_point = MapLocalToAncestor(flow_thread, multicol, FloatPoint(10, 70));
  EXPECT_EQ(FloatPoint(225, 25), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, multicol, mapped_point);
  EXPECT_EQ(FloatPoint(10, 70), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='multicol' style='columns:2; column-gap:20px; width:400px;
    line-height:50px; padding:5px; orphans:1; widows:1;'>
        <span id='target'><br>text</span>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutBox* flow_thread = ToLayoutBox(target->Parent());
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());
  LayoutBox* multicol = ToLayoutBox(GetLayoutObjectByElementId("multicol"));

  FloatPoint mapped_point =
      MapLocalToAncestor(target, flow_thread, FloatPoint(10, 70));
  EXPECT_EQ(FloatPoint(10, 70), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(FloatPoint(10, 70), mapped_point);

  mapped_point = MapLocalToAncestor(flow_thread, multicol, FloatPoint(10, 70));
  EXPECT_EQ(FloatPoint(225, 25), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, multicol, mapped_point);
  EXPECT_EQ(FloatPoint(10, 70), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='-webkit-columns:3; -webkit-column-gap:0;
    column-fill:auto; width:300px; height:100px; border:8px solid;
    padding:7px;'>
        <div style='height:110px;'></div>
        <div id='target' style='margin:10px; border:13px;
    padding:13px;'></div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(125, 35), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* flow_thread = target->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, flow_thread, FloatPoint());
  EXPECT_EQ(FloatPoint(10, 120), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point =
      MapLocalToAncestor(flow_thread, container, FloatPoint(10, 120));
  EXPECT_EQ(FloatPoint(125, 35), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, container, mapped_point);
  EXPECT_EQ(FloatPoint(10, 120), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithBlockAbove) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='columns:3; column-gap:0;
    column-fill:auto; width:300px; height:200px;'>
        <div id='target' style='margin-top:-50px; height:100px;'></div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(0, -50), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* flow_thread = target->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, flow_thread, FloatPoint());
  EXPECT_EQ(FloatPoint(0, -50), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(flow_thread, container, FloatPoint(0, -50));
  EXPECT_EQ(FloatPoint(0, -50), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, container, mapped_point);
  EXPECT_EQ(FloatPoint(0, -50), mapped_point);
}

TEST_F(MapCoordinatesTest, NestedMulticolWithBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id='outerMulticol' style='columns:2; column-gap:0;
    column-fill:auto; width:560px; height:215px; border:8px solid;
    padding:7px;'>
        <div style='height:10px;'></div>
        <div id='innerMulticol' style='columns:2; column-gap:0; border:8px
    solid; padding:7px;'>
            <div style='height:630px;'></div>
            <div id='target' style='width:50px; height:50px;'></div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* outer_multicol =
      ToLayoutBox(GetLayoutObjectByElementId("outerMulticol"));
  LayoutBox* inner_multicol =
      ToLayoutBox(GetLayoutObjectByElementId("innerMulticol"));
  LayoutBox* inner_flow_thread = target->ParentBox();
  ASSERT_TRUE(inner_flow_thread->IsLayoutFlowThread());
  LayoutBox* outer_flow_thread = inner_multicol->ParentBox();
  ASSERT_TRUE(outer_flow_thread->IsLayoutFlowThread());

  FloatPoint mapped_point =
      MapLocalToAncestor(target, outer_multicol, FloatPoint());
  EXPECT_EQ(FloatPoint(435, 115), mapped_point);
  mapped_point = MapAncestorToLocal(target, outer_multicol, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  mapped_point = MapLocalToAncestor(target, inner_flow_thread, FloatPoint());
  EXPECT_EQ(FloatPoint(0, 630), mapped_point);
  mapped_point = MapAncestorToLocal(target, inner_flow_thread, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point =
      MapLocalToAncestor(inner_flow_thread, inner_multicol, FloatPoint(0, 630));
  EXPECT_EQ(FloatPoint(140, 305), mapped_point);
  mapped_point =
      MapAncestorToLocal(inner_flow_thread, inner_multicol, mapped_point);
  EXPECT_EQ(FloatPoint(0, 630), mapped_point);

  mapped_point = MapLocalToAncestor(inner_multicol, outer_flow_thread,
                                    FloatPoint(140, 305));
  EXPECT_EQ(FloatPoint(140, 315), mapped_point);
  mapped_point =
      MapAncestorToLocal(inner_multicol, outer_flow_thread, mapped_point);
  EXPECT_EQ(FloatPoint(140, 305), mapped_point);

  mapped_point = MapLocalToAncestor(outer_flow_thread, outer_multicol,
                                    FloatPoint(140, 315));
  EXPECT_EQ(FloatPoint(435, 115), mapped_point);
  mapped_point =
      MapAncestorToLocal(outer_flow_thread, outer_multicol, mapped_point);
  EXPECT_EQ(FloatPoint(140, 315), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithAbsPosInRelPos) {
  SetBodyInnerHTML(R"HTML(
    <div id='multicol' style='-webkit-columns:3; -webkit-column-gap:0;
    column-fill:auto; width:300px; height:100px; border:8px solid;
    padding:7px;'>
        <div style='height:110px;'></div>
        <div id='relpos' style='position:relative; left:4px; top:4px;'>
            <div id='target' style='position:absolute; left:15px; top:15px;
    margin:10px; border:13px; padding:13px;'></div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* multicol = ToLayoutBox(GetLayoutObjectByElementId("multicol"));

  FloatPoint mapped_point = MapLocalToAncestor(target, multicol, FloatPoint());
  EXPECT_EQ(FloatPoint(144, 54), mapped_point);
  mapped_point = MapAncestorToLocal(target, multicol, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* relpos = ToLayoutBox(GetLayoutObjectByElementId("relpos"));
  LayoutBox* flow_thread = relpos->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, relpos, FloatPoint());
  EXPECT_EQ(FloatPoint(25, 25), mapped_point);
  mapped_point = MapAncestorToLocal(target, relpos, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(relpos, flow_thread, FloatPoint(25, 25));
  EXPECT_EQ(FloatPoint(29, 139), mapped_point);
  mapped_point = MapAncestorToLocal(relpos, flow_thread, mapped_point);
  EXPECT_EQ(FloatPoint(25, 25), mapped_point);

  mapped_point = MapLocalToAncestor(flow_thread, multicol, FloatPoint(29, 139));
  EXPECT_EQ(FloatPoint(144, 54), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, multicol, mapped_point);
  EXPECT_EQ(FloatPoint(29, 139), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithAbsPosNotContained) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position:relative; margin:666px; border:7px
    solid; padding:3px;'>
        <div id='multicol' style='-webkit-columns:3; -webkit-column-gap:0;
    column-fill:auto; width:300px; height:100px; border:8px solid;
    padding:7px;'>
            <div style='height:110px;'></div>
            <div id='target' style='position:absolute; left:-1px; top:-1px;
    margin:10px; border:13px; padding:13px;'></div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  // The multicol container isn't in the containing block chain of the abspos
  // #target.
  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(16, 16), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* multicol = ToLayoutBox(GetLayoutObjectByElementId("multicol"));
  LayoutBox* flow_thread = target->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, flow_thread, FloatPoint());
  EXPECT_EQ(FloatPoint(-9, -9), mapped_point);

  mapped_point = MapLocalToAncestor(flow_thread, multicol, mapped_point);
  EXPECT_EQ(FloatPoint(6, 6), mapped_point);

  mapped_point = MapLocalToAncestor(multicol, container, mapped_point);
  EXPECT_EQ(FloatPoint(16, 16), mapped_point);

  mapped_point = MapAncestorToLocal(multicol, container, mapped_point);
  EXPECT_EQ(FloatPoint(6, 6), mapped_point);

  mapped_point = MapAncestorToLocal(flow_thread, multicol, mapped_point);
  EXPECT_EQ(FloatPoint(-9, -9), mapped_point);

  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolRtl) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='columns:3; column-gap:0; column-fill:auto;
    width:300px; height:200px; direction:rtl;'>
        <div style='height:200px;'></div>
        <div id='target' style='height:50px;'></div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(100, 0), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* flow_thread = target->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, flow_thread, FloatPoint());
  EXPECT_EQ(FloatPoint(0, 200), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(flow_thread, container, FloatPoint(0, 200));
  EXPECT_EQ(FloatPoint(100, 0), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, container, mapped_point);
  EXPECT_EQ(FloatPoint(0, 200), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithLargeBorder) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='columns:3; column-gap:0; column-fill:auto;
    width:300px; height:200px; border:200px solid;'>
        <div style='height:200px;'></div>
        <div id='target' style='height:50px;'></div>
        <div style='height:200px;'></div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(300, 200), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* flow_thread = target->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, flow_thread, FloatPoint());
  EXPECT_EQ(FloatPoint(0, 200), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(flow_thread, container, FloatPoint(0, 200));
  EXPECT_EQ(FloatPoint(300, 200), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, container, mapped_point);
  EXPECT_EQ(FloatPoint(0, 200), mapped_point);
}

TEST_F(MapCoordinatesTest, FlippedBlocksWritingModeWithText) {
  SetBodyInnerHTML(R"HTML(
    <div style='-webkit-writing-mode:vertical-rl;'>
        <div style='width:13px;'></div>
        <div style='width:200px; height:400px; line-height:50px;'>
            <br id='sibling'>text
        </div>
        <div style='width:5px;'></div>
    </div>
  )HTML");

  LayoutObject* br = GetLayoutObjectByElementId("sibling");
  LayoutObject* text = br->NextSibling();
  ASSERT_TRUE(text->IsText());

  // Map to the nearest container. Flipping should occur.
  FloatPoint mapped_point = MapLocalToAncestor(
      text, text->ContainingBlock(), FloatPoint(75, 10), kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(125, 10), mapped_point);
  mapped_point = MapAncestorToLocal(text, text->ContainingBlock(), mapped_point,
                                    kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(75, 10), mapped_point);

  // Map to a container further up in the tree. Flipping should still occur on
  // the nearest container. LayoutObject::mapLocalToAncestor() is called
  // recursively until the ancestor is reached, and the ApplyContainerFlip flag
  // is cleared after having processed the innermost object.
  mapped_point =
      MapLocalToAncestor(text, text->ContainingBlock()->ContainingBlock(),
                         FloatPoint(75, 10), kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(130, 10), mapped_point);
  mapped_point =
      MapAncestorToLocal(text, text->ContainingBlock()->ContainingBlock(),
                         mapped_point, kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(75, 10), mapped_point);

  // If the ApplyContainerFlip flag isn't specified, no flipping should take
  // place.
  mapped_point = MapLocalToAncestor(
      text, text->ContainingBlock()->ContainingBlock(), FloatPoint(75, 10));
  EXPECT_EQ(FloatPoint(80, 10), mapped_point);
  mapped_point = MapAncestorToLocal(
      text, text->ContainingBlock()->ContainingBlock(), mapped_point);
  EXPECT_EQ(FloatPoint(75, 10), mapped_point);
}

TEST_F(MapCoordinatesTest, FlippedBlocksWritingModeWithInline) {
  SetBodyInnerHTML(R"HTML(
    <div style='-webkit-writing-mode:vertical-rl;'>
        <div style='width:13px;'></div>
        <div style='width:200px; height:400px; line-height:50px;'>
            <span>
                <span id='target'><br>text</span>
            </span>
        </div>
        <div style='width:7px;'></div>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  ASSERT_TRUE(target);

  // First map to the parent SPAN. Nothing special should happen, since flipping
  // occurs at the nearest container.
  FloatPoint mapped_point =
      MapLocalToAncestor(target, ToLayoutBoxModelObject(target->Parent()),
                         FloatPoint(75, 10), kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(75, 10), mapped_point);
  mapped_point =
      MapAncestorToLocal(target, ToLayoutBoxModelObject(target->Parent()),
                         mapped_point, kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(75, 10), mapped_point);

  // Continue to the nearest container. Flipping should occur.
  mapped_point = MapLocalToAncestor(ToLayoutBoxModelObject(target->Parent()),
                                    target->ContainingBlock(),
                                    FloatPoint(75, 10), kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(125, 10), mapped_point);
  mapped_point = MapAncestorToLocal(ToLayoutBoxModelObject(target->Parent()),
                                    target->ContainingBlock(), mapped_point,
                                    kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(75, 10), mapped_point);

  // Now map from the innermost inline to the nearest container in one go.
  mapped_point = MapLocalToAncestor(target, target->ContainingBlock(),
                                    FloatPoint(75, 10), kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(125, 10), mapped_point);
  mapped_point = MapAncestorToLocal(target, target->ContainingBlock(),
                                    mapped_point, kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(75, 10), mapped_point);

  // Map to a container further up in the tree. Flipping should still only occur
  // on the nearest container.
  mapped_point =
      MapLocalToAncestor(target, target->ContainingBlock()->ContainingBlock(),
                         FloatPoint(75, 10), kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(132, 10), mapped_point);
  mapped_point =
      MapAncestorToLocal(target, target->ContainingBlock()->ContainingBlock(),
                         mapped_point, kApplyContainerFlip);
  EXPECT_EQ(FloatPoint(75, 10), mapped_point);

  // If the ApplyContainerFlip flag isn't specified, no flipping should take
  // place.
  mapped_point = MapLocalToAncestor(
      target, target->ContainingBlock()->ContainingBlock(), FloatPoint(75, 10));
  EXPECT_EQ(FloatPoint(82, 10), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, target->ContainingBlock()->ContainingBlock(), mapped_point);
  EXPECT_EQ(FloatPoint(75, 10), mapped_point);
}

TEST_F(MapCoordinatesTest, FlippedBlocksWritingModeWithBlock) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='-webkit-writing-mode:vertical-rl; border:8px
    solid; padding:7px; width:200px; height:200px;'>
        <div id='middle' style='border:1px solid;'>
            <div style='width:30px;'></div>
            <div id='target' style='margin:6px; width:25px;'></div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(153, 22), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* middle = ToLayoutBox(GetLayoutObjectByElementId("middle"));

  mapped_point = MapLocalToAncestor(target, middle, FloatPoint());
  EXPECT_EQ(FloatPoint(7, 7), mapped_point);
  mapped_point = MapAncestorToLocal(target, middle, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  mapped_point = MapLocalToAncestor(middle, container, FloatPoint(7, 7));
  EXPECT_EQ(FloatPoint(153, 22), mapped_point);
  mapped_point = MapAncestorToLocal(middle, container, mapped_point);
  EXPECT_EQ(FloatPoint(7, 7), mapped_point);
}

TEST_F(MapCoordinatesTest, Table) {
  SetBodyInnerHTML(R"HTML(
    <style>td { padding: 2px; }</style>
    <div id='container' style='border:3px solid;'>
        <table style='margin:9px; border:5px solid; border-spacing:10px;'>
            <thead>
                <tr>
                    <td>
                        <div style='width:100px; height:100px;'></div>
                    </td>
                </tr>
            </thead>
            <tbody>
                <tr>
                    <td>
                        <div style='width:100px; height:100px;'></div>
                     </td>
                </tr>
                <tr>
                    <td>
                         <div style='width:100px; height:100px;'></div>
                    </td>
                    <td>
                        <div id='target' style='width:100px;
    height:10px;'></div>
                    </td>
                </tr>
            </tbody>
        </table>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(143, 302), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* td = target->ParentBox();
  ASSERT_TRUE(td->IsTableCell());
  mapped_point = MapLocalToAncestor(target, td, FloatPoint());
  // Cells are middle-aligned by default.
  EXPECT_EQ(FloatPoint(2, 47), mapped_point);
  mapped_point = MapAncestorToLocal(target, td, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);

  LayoutBox* tr = td->ParentBox();
  ASSERT_TRUE(tr->IsTableRow());
  mapped_point = MapLocalToAncestor(td, tr, FloatPoint(2, 47));
  EXPECT_EQ(FloatPoint(126, 47), mapped_point);
  mapped_point = MapAncestorToLocal(td, tr, mapped_point);
  EXPECT_EQ(FloatPoint(2, 47), mapped_point);

  LayoutBox* tbody = tr->ParentBox();
  ASSERT_TRUE(tbody->IsTableSection());
  mapped_point = MapLocalToAncestor(tr, tbody, FloatPoint(126, 47));
  EXPECT_EQ(FloatPoint(126, 161), mapped_point);
  mapped_point = MapAncestorToLocal(tr, tbody, mapped_point);
  EXPECT_EQ(FloatPoint(126, 47), mapped_point);

  LayoutBox* table = tbody->ParentBox();
  ASSERT_TRUE(table->IsTable());
  mapped_point = MapLocalToAncestor(tbody, table, FloatPoint(126, 161));
  EXPECT_EQ(FloatPoint(131, 290), mapped_point);
  mapped_point = MapAncestorToLocal(tbody, table, mapped_point);
  EXPECT_EQ(FloatPoint(126, 161), mapped_point);

  mapped_point = MapLocalToAncestor(table, container, FloatPoint(131, 290));
  EXPECT_EQ(FloatPoint(143, 302), mapped_point);
  mapped_point = MapAncestorToLocal(table, container, mapped_point);
  EXPECT_EQ(FloatPoint(131, 290), mapped_point);
}

static bool FloatValuesAlmostEqual(float expected, float actual) {
  return fabs(expected - actual) < 0.01;
}

static bool FloatQuadsAlmostEqual(const FloatQuad& expected,
                                  const FloatQuad& actual) {
  return FloatValuesAlmostEqual(expected.P1().X(), actual.P1().X()) &&
         FloatValuesAlmostEqual(expected.P1().Y(), actual.P1().Y()) &&
         FloatValuesAlmostEqual(expected.P2().X(), actual.P2().X()) &&
         FloatValuesAlmostEqual(expected.P2().Y(), actual.P2().Y()) &&
         FloatValuesAlmostEqual(expected.P3().X(), actual.P3().X()) &&
         FloatValuesAlmostEqual(expected.P3().Y(), actual.P3().Y()) &&
         FloatValuesAlmostEqual(expected.P4().X(), actual.P4().X()) &&
         FloatValuesAlmostEqual(expected.P4().Y(), actual.P4().Y());
}

// If comparison fails, pretty-print the error using EXPECT_EQ()
#define EXPECT_FLOAT_QUAD_EQ(expected, actual)      \
  do {                                              \
    if (!FloatQuadsAlmostEqual(expected, actual)) { \
      EXPECT_EQ(expected, actual);                  \
    }                                               \
  } while (false)

TEST_F(MapCoordinatesTest, Transforms) {
  SetBodyInnerHTML(R"HTML(
    <div id='container'>
        <div id='outerTransform' style='transform:rotate(45deg);
    width:200px; height:200px;'>
            <div id='innerTransform' style='transform:rotate(45deg);
    width:200px; height:200px;'>
                <div id='target' style='width:200px; height:200px;'></div>
            </div>
        </div>
    </div>
  )HTML");

  LayoutBox* target = ToLayoutBox(GetLayoutObjectByElementId("target"));
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatQuad initial_quad(FloatPoint(0, 0), FloatPoint(200, 0),
                         FloatPoint(200, 200), FloatPoint(0, 200));
  FloatQuad mapped_quad =
      MapLocalToAncestor(target, container, initial_quad, kUseTransforms);
  EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(200, 0), FloatPoint(200, 200),
                                 FloatPoint(0, 200), FloatPoint(0, 0)),
                       mapped_quad);
  mapped_quad =
      MapAncestorToLocal(target, container, mapped_quad, kUseTransforms);
  EXPECT_FLOAT_QUAD_EQ(initial_quad, mapped_quad);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* inner_transform =
      ToLayoutBox(GetLayoutObjectByElementId("innerTransform"));
  LayoutBox* outer_transform =
      ToLayoutBox(GetLayoutObjectByElementId("outerTransform"));

  mapped_quad =
      MapLocalToAncestor(target, inner_transform, initial_quad, kUseTransforms);
  EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(0, 0), FloatPoint(200, 0),
                                 FloatPoint(200, 200), FloatPoint(0, 200)),
                       mapped_quad);
  mapped_quad =
      MapAncestorToLocal(target, inner_transform, mapped_quad, kUseTransforms);
  EXPECT_FLOAT_QUAD_EQ(initial_quad, mapped_quad);

  initial_quad = FloatQuad(FloatPoint(0, 0), FloatPoint(200, 0),
                           FloatPoint(200, 200), FloatPoint(0, 200));
  mapped_quad = MapLocalToAncestor(inner_transform, outer_transform,
                                   initial_quad, kUseTransforms);
  // Clockwise rotation by 45 degrees.
  EXPECT_FLOAT_QUAD_EQ(
      FloatQuad(FloatPoint(100, -41.42), FloatPoint(241.42, 100),
                FloatPoint(100, 241.42), FloatPoint(-41.42, 100)),
      mapped_quad);
  mapped_quad = MapAncestorToLocal(inner_transform, outer_transform,
                                   mapped_quad, kUseTransforms);
  EXPECT_FLOAT_QUAD_EQ(initial_quad, mapped_quad);

  initial_quad = FloatQuad(FloatPoint(100, -41.42), FloatPoint(241.42, 100),
                           FloatPoint(100, 241.42), FloatPoint(-41.42, 100));
  mapped_quad = MapLocalToAncestor(outer_transform, container, initial_quad,
                                   kUseTransforms);
  // Another clockwise rotation by 45 degrees. So now 90 degrees in total.
  EXPECT_FLOAT_QUAD_EQ(FloatQuad(FloatPoint(200, 0), FloatPoint(200, 200),
                                 FloatPoint(0, 200), FloatPoint(0, 0)),
                       mapped_quad);
  mapped_quad = MapAncestorToLocal(outer_transform, container, mapped_quad,
                                   kUseTransforms);
  EXPECT_FLOAT_QUAD_EQ(initial_quad, mapped_quad);
}

TEST_F(MapCoordinatesTest, SVGShape) {
  SetBodyInnerHTML(R"HTML(
    <svg id='container'>
        <g transform='translate(100 200)'>
            <rect id='target' width='100' height='100'/>
        </g>
    </svg>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(100, 200), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, SVGShapeScale) {
  SetBodyInnerHTML(R"HTML(
    <svg id='container'>
        <g transform='scale(2) translate(50 40)'>
            <rect id='target' transform='translate(50 80)' x='66' y='77'
    width='100' height='100'/>
        </g>
    </svg>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(200, 240), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, SVGShapeWithViewBoxWithoutScale) {
  SetBodyInnerHTML(R"HTML(
    <svg id='container' viewBox='0 0 200 200' width='400' height='200'>
        <g transform='translate(100 50)'>
            <rect id='target' width='100' height='100'/>
        </g>
    </svg>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(200, 50), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, SVGShapeWithViewBoxWithScale) {
  SetBodyInnerHTML(R"HTML(
    <svg id='container' viewBox='0 0 100 100' width='400' height='200'>
        <g transform='translate(50 50)'>
            <rect id='target' width='100' height='100'/>
        </g>
    </svg>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(200, 100), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, SVGShapeWithViewBoxWithNonZeroOffset) {
  SetBodyInnerHTML(R"HTML(
    <svg id='container' viewBox='100 100 200 200' width='400' height='200'>
        <g transform='translate(100 50)'>
            <rect id='target' transform='translate(100 100)' width='100'
    height='100'/>
        </g>
    </svg>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(200, 50), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, SVGShapeWithViewBoxWithNonZeroOffsetAndScale) {
  SetBodyInnerHTML(R"HTML(
    <svg id='container' viewBox='100 100 100 100' width='400' height='200'>
        <g transform='translate(50 50)'>
            <rect id='target' transform='translate(100 100)' width='100'
    height='100'/>
        </g>
    </svg>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(200, 100), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, SVGForeignObject) {
  SetBodyInnerHTML(R"HTML(
    <svg id='container' viewBox='0 0 100 100' width='400' height='200'>
        <g transform='translate(50 50)'>
            <foreignObject transform='translate(-25 -25)'>
                <div xmlns='http://www.w3.org/1999/xhtml' id='target'
    style='margin-left: 50px; border: 42px; padding: 84px; width: 50px;
    height: 50px'>
                </div>
            </foreignObject>
        </g>
    </svg>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  LayoutBox* container = ToLayoutBox(GetLayoutObjectByElementId("container"));

  FloatPoint mapped_point = MapLocalToAncestor(target, container, FloatPoint());
  EXPECT_EQ(FloatPoint(250, 50), mapped_point);
  // <svg>
  mapped_point = MapAncestorToLocal(target->Parent()->Parent()->Parent(),
                                    container, FloatPoint(250, 50));
  EXPECT_EQ(FloatPoint(250, 50), mapped_point);
  // <g>
  mapped_point = MapAncestorToLocal(target->Parent()->Parent(), container,
                                    FloatPoint(250, 50));
  EXPECT_EQ(FloatPoint(25, -25), mapped_point);
  // <foreignObject>
  mapped_point =
      MapAncestorToLocal(target->Parent(), container, FloatPoint(250, 50));
  EXPECT_EQ(FloatPoint(50, 0), mapped_point);
  // <div>
  mapped_point = MapAncestorToLocal(target, container, FloatPoint(250, 50));
  EXPECT_EQ(FloatPoint(), mapped_point);
}

TEST_F(MapCoordinatesTest, LocalToAbsoluteTransform) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='position: absolute; left: 0; top: 0;'>
      <div id='scale' style='transform: scale(2.0); transform-origin: left
    top;'>
        <div id='child'></div>
      </div>
    </div>
  )HTML");
  LayoutBoxModelObject* container =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("container"));
  TransformationMatrix container_matrix = container->LocalToAbsoluteTransform();
  EXPECT_TRUE(container_matrix.IsIdentity());

  LayoutObject* child = GetLayoutObjectByElementId("child");
  TransformationMatrix child_matrix = child->LocalToAbsoluteTransform();
  EXPECT_FALSE(child_matrix.IsIdentityOrTranslation());
  EXPECT_TRUE(child_matrix.IsAffine());
  EXPECT_EQ(0.0, child_matrix.ProjectPoint(FloatPoint(0.0, 0.0)).X());
  EXPECT_EQ(0.0, child_matrix.ProjectPoint(FloatPoint(0.0, 0.0)).Y());
  EXPECT_EQ(20.0, child_matrix.ProjectPoint(FloatPoint(10.0, 20.0)).X());
  EXPECT_EQ(40.0, child_matrix.ProjectPoint(FloatPoint(10.0, 20.0)).Y());
}

TEST_F(MapCoordinatesTest, LocalToAncestorTransform) {
  SetBodyInnerHTML(R"HTML(
    <div id='container'>
      <div id='rotate1' style='transform: rotate(45deg); transform-origin:
    left top;'>
        <div id='rotate2' style='transform: rotate(90deg);
    transform-origin: left top;'>
          <div id='child'></div>
        </div>
      </div>
    </div>
  )HTML");
  LayoutBoxModelObject* container =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("container"));
  LayoutBoxModelObject* rotate1 =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("rotate1"));
  LayoutBoxModelObject* rotate2 =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("rotate2"));
  LayoutObject* child = GetLayoutObjectByElementId("child");
  TransformationMatrix matrix;

  matrix = child->LocalToAncestorTransform(rotate2);
  EXPECT_TRUE(matrix.IsIdentity());

  // Rotate (100, 0) 90 degrees to (0, 100)
  matrix = child->LocalToAncestorTransform(rotate1);
  EXPECT_FALSE(matrix.IsIdentity());
  EXPECT_TRUE(matrix.IsAffine());
  EXPECT_NEAR(0.0, matrix.ProjectPoint(FloatPoint(100.0, 0.0)).X(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(100.0, matrix.ProjectPoint(FloatPoint(100.0, 0.0)).Y(),
              LayoutUnit::Epsilon());

  // Rotate (100, 0) 135 degrees to (-70.7, 70.7)
  matrix = child->LocalToAncestorTransform(container);
  EXPECT_FALSE(matrix.IsIdentity());
  EXPECT_TRUE(matrix.IsAffine());
  EXPECT_NEAR(-100.0 * sqrt(2.0) / 2.0,
              matrix.ProjectPoint(FloatPoint(100.0, 0.0)).X(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(100.0 * sqrt(2.0) / 2.0,
              matrix.ProjectPoint(FloatPoint(100.0, 0.0)).Y(),
              LayoutUnit::Epsilon());
}

TEST_F(MapCoordinatesTest, LocalToAbsoluteTransformFlattens) {
  // This Page is not actually being shown by a compositor, but we act like it
  // will in order to test behaviour.
  GetPage().GetSettings().SetAcceleratedCompositingEnabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style='position: absolute; left: 0; top: 0;'>
      <div style='transform: rotateY(45deg);
    -webkit-transform-style:preserve-3d;'>
        <div style='transform: rotateY(-45deg);
    -webkit-transform-style:preserve-3d;'>
          <div id='child1'></div>
        </div>
      </div>
      <div style='transform: rotateY(45deg);'>
        <div style='transform: rotateY(-45deg);'>
          <div id='child2'></div>
        </div>
      </div>
    </div>
  )HTML");
  LayoutObject* child1 = GetLayoutObjectByElementId("child1");
  LayoutObject* child2 = GetLayoutObjectByElementId("child2");
  TransformationMatrix matrix;

  matrix = child1->LocalToAbsoluteTransform();

  // With child1, the rotations cancel and points should map basically back to
  // themselves.
  EXPECT_NEAR(100.0, matrix.ProjectPoint(FloatPoint(100.0, 50.0)).X(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(50.0, matrix.ProjectPoint(FloatPoint(100.0, 50.0)).Y(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(50.0, matrix.ProjectPoint(FloatPoint(50.0, 100.0)).X(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(100.0, matrix.ProjectPoint(FloatPoint(50.0, 100.0)).Y(),
              LayoutUnit::Epsilon());

  // With child2, each rotation gets flattened and the end result is
  // approximately a 90-degree rotation.
  matrix = child2->LocalToAbsoluteTransform();
  EXPECT_NEAR(50.0, matrix.ProjectPoint(FloatPoint(100.0, 50.0)).X(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(50.0, matrix.ProjectPoint(FloatPoint(100.0, 50.0)).Y(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(25.0, matrix.ProjectPoint(FloatPoint(50.0, 100.0)).X(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(100.0, matrix.ProjectPoint(FloatPoint(50.0, 100.0)).Y(),
              LayoutUnit::Epsilon());
}

// This test verifies that the mapped location of a div within a scroller
// remains the same after scroll when ignoring scroll offset.
TEST_F(MapCoordinatesTest, IgnoreScrollOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      .scroller { overflow: scroll; height: 100px; width: 100px;
        top: 100px; position: absolute; }
      .box { width: 10px; height: 10px; top: 10px; position: absolute; }
      .spacer { height: 2000px; }
    </style>
    <div class='scroller' id='scroller'>
      <div class='box' id='box'></div>
      <div class='spacer'></div>
    </div>
  )HTML");

  LayoutBox* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  LayoutBox* box = ToLayoutBox(GetLayoutObjectByElementId("box"));

  EXPECT_EQ(FloatPoint(0, 10), MapLocalToAncestor(box, scroller, FloatPoint()));
  EXPECT_EQ(FloatPoint(0, 10), MapLocalToAncestor(box, scroller, FloatPoint(),
                                                  kIgnoreScrollOffset));

  scroller->ScrollToPosition(FloatPoint(0, 50));

  EXPECT_EQ(FloatPoint(0, -40),
            MapLocalToAncestor(box, scroller, FloatPoint()));
  EXPECT_EQ(FloatPoint(0, 10), MapLocalToAncestor(box, scroller, FloatPoint(),
                                                  kIgnoreScrollOffset));
}

// This test verifies that the mapped location of an inline div within a
// scroller remains the same after scroll when ignoring scroll offset.
TEST_F(MapCoordinatesTest, IgnoreScrollOffsetForInline) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      .scroller { overflow: scroll; width: 100px; height: 100px; top: 100px;
        position: absolute; }
      .box { width: 10px; height: 10px; top: 10px; position: sticky; }
      .inline { display: inline; }
      .spacer { height: 2000px; }
    </style>
    <div class='scroller' id='scroller'>
      <div class='inline box' id='box'></div>
      <div class='spacer'></div>
    </div>
  )HTML");

  LayoutBox* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  LayoutInline* box = ToLayoutInline(GetLayoutObjectByElementId("box"));

  EXPECT_EQ(FloatPoint(0, 10), MapLocalToAncestor(box, scroller, FloatPoint()));
  EXPECT_EQ(FloatPoint(0, 10), MapLocalToAncestor(box, scroller, FloatPoint(),
                                                  kIgnoreScrollOffset));

  scroller->ScrollToPosition(FloatPoint(0, 50));

  EXPECT_EQ(FloatPoint(0, 10), MapLocalToAncestor(box, scroller, FloatPoint()));
  EXPECT_EQ(FloatPoint(0, 60), MapLocalToAncestor(box, scroller, FloatPoint(),
                                                  kIgnoreScrollOffset));
}

// This test verifies that ignoring scroll offset works with writing modes.
TEST_F(MapCoordinatesTest, IgnoreScrollOffsetWithWritingModes) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      .scroller { writing-mode: vertical-rl; overflow: scroll; height: 100px;
        width: 100px; top: 100px; position: absolute; }
      .box { width: 10px; height: 10px; top: 10px; position: absolute; }
      .spacer { width: 2000px; height: 2000px; }
    </style>
    <div class='scroller' id='scroller'>
      <div class='box' id='box'></div>
      <div class='spacer'></div>
    </div>
  )HTML");

  LayoutBox* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  LayoutBox* box = ToLayoutBox(GetLayoutObjectByElementId("box"));

  EXPECT_EQ(FloatPoint(90, 10),
            MapLocalToAncestor(box, scroller, FloatPoint()));
  EXPECT_EQ(
      FloatPoint(1990, 10),
      MapLocalToAncestor(box, scroller, FloatPoint(), kIgnoreScrollOffset));

  scroller->ScrollToPosition(FloatPoint(0, 50));

  EXPECT_EQ(FloatPoint(1990, -40),
            MapLocalToAncestor(box, scroller, FloatPoint()));
  EXPECT_EQ(
      FloatPoint(1990, 10),
      MapLocalToAncestor(box, scroller, FloatPoint(), kIgnoreScrollOffset));

  scroller->ScrollToPosition(FloatPoint(1900, 50));

  EXPECT_EQ(FloatPoint(90, -40),
            MapLocalToAncestor(box, scroller, FloatPoint()));
  EXPECT_EQ(
      FloatPoint(1990, 10),
      MapLocalToAncestor(box, scroller, FloatPoint(), kIgnoreScrollOffset));
}

// This test verifies that ignoring scroll offset works with writing modes and
// non-overlay scrollbar.
TEST_F(MapCoordinatesTest,
       IgnoreScrollOffsetWithWritingModesAndNonOverlayScrollbar) {
  ScopedOverlayScrollbarsForTest overlay_scrollbars(false);
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      .scroller { writing-mode: vertical-rl; overflow: scroll; height: 100px;
        width: 100px; top: 100px; position: absolute; }
      .box { width: 10px; height: 10px; top: 10px; position: absolute; }
      .spacer { width: 2000px; height: 2000px; }
    </style>
    <div class='scroller' id='scroller'>
      <div class='box' id='box'></div>
      <div class='spacer'></div>
    </div>
  )HTML");

  LayoutBox* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  LayoutBox* box = ToLayoutBox(GetLayoutObjectByElementId("box"));

  // The box is on the left of the scrollbar so the width of the scrollbar
  // affects the location of the box.
  EXPECT_EQ(FloatPoint(75, 10),
            MapLocalToAncestor(box, scroller, FloatPoint()));
  EXPECT_EQ(
      FloatPoint(1990, 10),
      MapLocalToAncestor(box, scroller, FloatPoint(), kIgnoreScrollOffset));

  scroller->ScrollToPosition(FloatPoint(0, 0));
  // The box is now on the right of the scrollbar therefore there is nothing
  // between the box and the right border of the content.
  EXPECT_EQ(FloatPoint(1990, 10),
            MapLocalToAncestor(box, scroller, FloatPoint()));
  EXPECT_EQ(
      FloatPoint(1990, 10),
      MapLocalToAncestor(box, scroller, FloatPoint(), kIgnoreScrollOffset));
}

}  // namespace blink
