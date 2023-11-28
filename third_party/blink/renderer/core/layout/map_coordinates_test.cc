// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class MapCoordinatesTest : public RenderingTest {
 public:
  MapCoordinatesTest()
      : RenderingTest(MakeGarbageCollected<SingleChildLocalFrameClient>()) {}

  void SetUp() override {
    // This is required to test 3d transforms.
    EnableCompositing();
    RenderingTest::SetUp();
  }

  PhysicalOffset MapLocalToAncestor(const LayoutObject*,
                                    const LayoutBoxModelObject* ancestor,
                                    PhysicalOffset,
                                    MapCoordinatesFlags = 0) const;
  gfx::QuadF MapLocalToAncestor(const LayoutObject*,
                                const LayoutBoxModelObject* ancestor,
                                gfx::QuadF,
                                MapCoordinatesFlags = 0) const;
  PhysicalOffset MapAncestorToLocal(const LayoutObject*,
                                    const LayoutBoxModelObject* ancestor,
                                    PhysicalOffset,
                                    MapCoordinatesFlags = 0) const;
  gfx::QuadF MapAncestorToLocal(const LayoutObject*,
                                const LayoutBoxModelObject* ancestor,
                                gfx::QuadF,
                                MapCoordinatesFlags = 0) const;

  // Adjust point by the scroll offset of the LayoutView.  This only has an
  // effect if root layer scrolling is enabled.  The only reason for doing
  // this here is so the test expected values can be the same whether or not
  // root layer scrolling is enabled.  This is analogous to what
  // LayoutGeometryMapTest does; for more context, see:
  // https://codereview.chromium.org/2417103002/#msg11
  PhysicalOffset AdjustForFrameScroll(const PhysicalOffset&) const;
};

// One note about tests here that operate on LayoutInline and LayoutText
// objects: mapLocalToAncestor() expects such objects to pass their static
// location and size (relatively to the border edge of their container) to
// mapLocalToAncestor() via the TransformState argument. mapLocalToAncestor() is
// then only expected to make adjustments for relative-positioning,
// container-specific characteristics (such as writing mode roots, multicol),
// and so on. This in contrast to LayoutBox objects, where the TransformState
// passed is relative to the box itself, not the container.

PhysicalOffset MapCoordinatesTest::AdjustForFrameScroll(
    const PhysicalOffset& point) const {
  PhysicalOffset result(point);
  LayoutView* layout_view = GetDocument().GetLayoutView();
  if (layout_view->IsScrollContainer())
    result -= PhysicalOffset(layout_view->ScrolledContentOffset());
  return result;
}

PhysicalOffset MapCoordinatesTest::MapLocalToAncestor(
    const LayoutObject* object,
    const LayoutBoxModelObject* ancestor,
    PhysicalOffset point,
    MapCoordinatesFlags mode) const {
  return object->LocalToAncestorPoint(point, ancestor, mode);
}

gfx::QuadF MapCoordinatesTest::MapLocalToAncestor(
    const LayoutObject* object,
    const LayoutBoxModelObject* ancestor,
    gfx::QuadF quad,
    MapCoordinatesFlags mode) const {
  return object->LocalToAncestorQuad(quad, ancestor, mode);
}

PhysicalOffset MapCoordinatesTest::MapAncestorToLocal(
    const LayoutObject* object,
    const LayoutBoxModelObject* ancestor,
    PhysicalOffset point,
    MapCoordinatesFlags mode) const {
  return object->AncestorToLocalPoint(ancestor, point, mode);
}

gfx::QuadF MapCoordinatesTest::MapAncestorToLocal(
    const LayoutObject* object,
    const LayoutBoxModelObject* ancestor,
    gfx::QuadF quad,
    MapCoordinatesFlags mode) const {
  return object->AncestorToLocalQuad(ancestor, quad, mode);
}

TEST_F(MapCoordinatesTest, SimpleText) {
  SetBodyInnerHTML("<div id='container'><br>text</div>");

  auto* container = GetLayoutBoxByElementId("container");
  LayoutObject* text = To<LayoutBlockFlow>(container)->LastChild();
  ASSERT_TRUE(text->IsText());
  PhysicalOffset mapped_point =
      MapLocalToAncestor(text, container, PhysicalOffset(10, 30));
  EXPECT_EQ(PhysicalOffset(10, 30), mapped_point);
  mapped_point = MapAncestorToLocal(text, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 30), mapped_point);
}

TEST_F(MapCoordinatesTest, SimpleInline) {
  SetBodyInnerHTML("<div><span id='target'>text</span></div>");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, To<LayoutBoxModelObject>(target->Parent()),
                         PhysicalOffset(10, 10));
  EXPECT_EQ(PhysicalOffset(10, 10), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, To<LayoutBoxModelObject>(target->Parent()), mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 10), mapped_point);
}

TEST_F(MapCoordinatesTest, SimpleBlock) {
  SetBodyInnerHTML(R"HTML(
    <div style='margin:666px; border:8px solid; padding:7px;'>
        <div id='target' style='margin:10px; border:666px;
    padding:666px;'></div>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, To<LayoutBoxModelObject>(target->Parent()),
                         PhysicalOffset(100, 100));
  EXPECT_EQ(PhysicalOffset(125, 125), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, To<LayoutBoxModelObject>(target->Parent()), mapped_point);
  EXPECT_EQ(PhysicalOffset(100, 100), mapped_point);
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
  To<Element>(overflow->GetNode())
      ->GetLayoutBoxForScrolling()
      ->GetScrollableArea()
      ->ScrollToAbsolutePosition(gfx::PointF(32, 54));

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, To<LayoutBoxModelObject>(target->Parent()),
                         PhysicalOffset(100, 100));
  EXPECT_EQ(PhysicalOffset(93, 271), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, To<LayoutBoxModelObject>(target->Parent()), mapped_point);
  EXPECT_EQ(PhysicalOffset(100, 100), mapped_point);
}

TEST_F(MapCoordinatesTest, TextInRelPosInline) {
  SetBodyInnerHTML(
      "<div><span style='position:relative; left:7px; top:4px;'><br "
      "id='sibling'>text</span></div>");

  LayoutObject* br = GetLayoutObjectByElementId("sibling");
  LayoutObject* text = br->NextSibling();
  ASSERT_TRUE(text->IsText());
  PhysicalOffset mapped_point =
      MapLocalToAncestor(text, text->ContainingBlock(), PhysicalOffset(10, 30));
  EXPECT_EQ(PhysicalOffset(10, 30), mapped_point);
  mapped_point =
      MapAncestorToLocal(text, text->ContainingBlock(), mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 30), mapped_point);
}

TEST_F(MapCoordinatesTest, RelposInline) {
  SetBodyInnerHTML(
      "<span id='target' style='position:relative; left:50px; "
      "top:100px;'>text</span>");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, To<LayoutBoxModelObject>(target->Parent()),
                         PhysicalOffset(10, 10));
  EXPECT_EQ(PhysicalOffset(10, 10), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, To<LayoutBoxModelObject>(target->Parent()), mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 10), mapped_point);
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
  auto* parent = To<LayoutInline>(target->Parent());
  auto* containing_block = To<LayoutBlockFlow>(parent->Parent());

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, containing_block, PhysicalOffset(20, 10));
  EXPECT_EQ(PhysicalOffset(20, 10), mapped_point);
  mapped_point = MapAncestorToLocal(target, containing_block, mapped_point);
  EXPECT_EQ(PhysicalOffset(20, 10), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  mapped_point = MapLocalToAncestor(target, parent, PhysicalOffset(20, 10));
  EXPECT_EQ(PhysicalOffset(20, 10), mapped_point);

  mapped_point = MapLocalToAncestor(parent, containing_block, mapped_point);
  EXPECT_EQ(PhysicalOffset(20, 10), mapped_point);

  mapped_point = MapAncestorToLocal(parent, containing_block, mapped_point);
  EXPECT_EQ(PhysicalOffset(20, 10), mapped_point);

  mapped_point = MapAncestorToLocal(target, parent, mapped_point);
  EXPECT_EQ(PhysicalOffset(20, 10), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(106, 106), mapped_point);
  mapped_point =
      MapAncestorToLocal(target, container, PhysicalOffset(110, 110));
  EXPECT_EQ(PhysicalOffset(4, 4), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  auto* middle = GetLayoutBoxByElementId("middle");

  mapped_point = MapLocalToAncestor(target, middle, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(61, 61), mapped_point);

  mapped_point = MapLocalToAncestor(middle, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(106, 106), mapped_point);

  mapped_point = MapAncestorToLocal(middle, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(61, 61), mapped_point);

  mapped_point = MapAncestorToLocal(target, middle, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(17, 17), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, PhysicalOffset(18, 18));
  EXPECT_EQ(PhysicalOffset(1, 1), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  auto* static_child = GetLayoutBoxByElementId("staticChild");

  mapped_point = MapLocalToAncestor(target, static_child, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(-28, -28), mapped_point);

  mapped_point = MapLocalToAncestor(static_child, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(17, 17), mapped_point);

  mapped_point = MapAncestorToLocal(static_child, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(-28, -28), mapped_point);

  mapped_point = MapAncestorToLocal(target, static_child, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(55, 80), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, PhysicalOffset(56, 82));
  EXPECT_EQ(PhysicalOffset(1, 2), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  auto* static_child = GetLayoutBoxByElementId("staticChild");

  mapped_point = MapLocalToAncestor(target, static_child, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(10, 35), mapped_point);

  mapped_point = MapLocalToAncestor(static_child, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(55, 80), mapped_point);

  mapped_point = MapAncestorToLocal(static_child, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 35), mapped_point);

  mapped_point = MapAncestorToLocal(target, static_child, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* static_child = GetLayoutBoxByElementId("staticChild");
  auto* container = GetLayoutBoxByElementId("container");
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(IsA<LayoutView>(view));

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, view, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(9, 9), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, PhysicalOffset(10, 11));
  EXPECT_EQ(PhysicalOffset(1, 2), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  mapped_point = MapLocalToAncestor(target, static_child, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(-15, -15), mapped_point);

  mapped_point = MapLocalToAncestor(static_child, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(-3, -3), mapped_point);

  mapped_point = MapLocalToAncestor(container, body, mapped_point);
  EXPECT_EQ(PhysicalOffset(1, 1), mapped_point);

  mapped_point = MapLocalToAncestor(body, html, mapped_point);
  EXPECT_EQ(PhysicalOffset(9, 9), mapped_point);

  mapped_point = MapLocalToAncestor(html, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(9, 9), mapped_point);

  mapped_point = MapAncestorToLocal(html, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(9, 9), mapped_point);

  mapped_point = MapAncestorToLocal(body, html, mapped_point);
  EXPECT_EQ(PhysicalOffset(1, 1), mapped_point);

  mapped_point = MapAncestorToLocal(container, body, mapped_point);
  EXPECT_EQ(PhysicalOffset(-3, -3), mapped_point);

  mapped_point = MapAncestorToLocal(static_child, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(-15, -15), mapped_point);

  mapped_point = MapAncestorToLocal(target, static_child, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* static_child = GetLayoutBoxByElementId("staticChild");
  auto* container = GetLayoutBoxByElementId("container");
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(IsA<LayoutView>(view));

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, target->ContainingBlock(), PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(36, 61), mapped_point);
  mapped_point = MapAncestorToLocal(target, target->ContainingBlock(),
                                    PhysicalOffset(36, 61));
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  mapped_point = MapLocalToAncestor(target, static_child, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(10, 35), mapped_point);

  mapped_point = MapLocalToAncestor(static_child, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(25, 50), mapped_point);

  mapped_point = MapLocalToAncestor(container, body, mapped_point);
  EXPECT_EQ(PhysicalOffset(28, 53), mapped_point);

  mapped_point = MapLocalToAncestor(body, html, mapped_point);
  EXPECT_EQ(PhysicalOffset(36, 61), mapped_point);

  mapped_point = MapLocalToAncestor(html, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(36, 61), mapped_point);

  mapped_point = MapAncestorToLocal(html, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(36, 61), mapped_point);

  mapped_point = MapAncestorToLocal(body, html, mapped_point);
  EXPECT_EQ(PhysicalOffset(28, 53), mapped_point);

  mapped_point = MapAncestorToLocal(container, body, mapped_point);
  EXPECT_EQ(PhysicalOffset(25, 50), mapped_point);

  mapped_point = MapAncestorToLocal(static_child, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 35), mapped_point);

  mapped_point = MapAncestorToLocal(target, static_child, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* outer_fixed = GetLayoutBoxByElementId("outerFixed");
  auto* static_child = GetLayoutBoxByElementId("staticChild");
  auto* container = GetLayoutBoxByElementId("container");
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(IsA<LayoutView>(view));

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, view, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(9, 9), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, PhysicalOffset(9, 9));
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  mapped_point = MapLocalToAncestor(target, outer_fixed, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(-101, -101), mapped_point);

  mapped_point = MapLocalToAncestor(outer_fixed, static_child, mapped_point);
  EXPECT_EQ(PhysicalOffset(-15, -15), mapped_point);

  mapped_point = MapLocalToAncestor(static_child, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(-3, -3), mapped_point);

  mapped_point = MapLocalToAncestor(container, body, mapped_point);
  EXPECT_EQ(PhysicalOffset(1, 1), mapped_point);

  mapped_point = MapLocalToAncestor(body, html, mapped_point);
  EXPECT_EQ(PhysicalOffset(9, 9), mapped_point);

  mapped_point = MapLocalToAncestor(html, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(9, 9), mapped_point);

  mapped_point = MapAncestorToLocal(html, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(9, 9), mapped_point);

  mapped_point = MapAncestorToLocal(body, html, mapped_point);
  EXPECT_EQ(PhysicalOffset(1, 1), mapped_point);

  mapped_point = MapAncestorToLocal(container, body, mapped_point);
  EXPECT_EQ(PhysicalOffset(-3, -3), mapped_point);

  mapped_point = MapAncestorToLocal(static_child, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(-15, -15), mapped_point);

  mapped_point = MapAncestorToLocal(outer_fixed, static_child, mapped_point);
  EXPECT_EQ(PhysicalOffset(-101, -101), mapped_point);

  mapped_point = MapAncestorToLocal(target, outer_fixed, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
}

TEST_F(MapCoordinatesTest, FixedPosInFixedPosScrollView) {
  SetBodyInnerHTML(R"HTML(
    <div style='height: 4000px'></div>
    <div id='container' style='position:fixed; top: 100px; left: 100px'>
      <div id='target' style='position:fixed; top: 200px; left: 200px'>
      </div>
    </div>
  )HTML");

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(IsA<LayoutView>(view));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 50), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(50, GetDocument().View()->LayoutViewport()->ScrollOffsetInt().y());

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, view, PhysicalOffset());
  EXPECT_EQ(AdjustForFrameScroll(PhysicalOffset(200, 250)), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point = MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(100, 100), mapped_point);
  mapped_point =
      MapAncestorToLocal(target, container, PhysicalOffset(100, 100));
  EXPECT_EQ(PhysicalOffset(), mapped_point);
}

TEST_F(MapCoordinatesTest, FixedPosInAbsolutePosScrollView) {
  SetBodyInnerHTML(R"HTML(
    <div style='height: 4000px'></div>
    <div id='container' style='position:absolute; top: 100px; left: 100px'>
      <div id='target' style='position:fixed; top: 200px; left: 200px'>
      </div>
    </div>
  )HTML");

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(IsA<LayoutView>(view));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 50), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(50, GetDocument().View()->LayoutViewport()->ScrollOffsetInt().y());

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, view, PhysicalOffset());
  EXPECT_EQ(AdjustForFrameScroll(PhysicalOffset(200, 250)), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point = MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(100, 150), mapped_point);
  mapped_point =
      MapAncestorToLocal(target, container, PhysicalOffset(100, 150));
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 50), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(50, GetDocument().View()->LayoutViewport()->ScrollOffsetInt().y());

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(IsA<LayoutView>(view));

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, view, PhysicalOffset());
  EXPECT_EQ(AdjustForFrameScroll(PhysicalOffset(0, 200)), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point = MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, PhysicalOffset(0, 0));
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point = MapLocalToAncestor(container, view, PhysicalOffset());
  EXPECT_EQ(AdjustForFrameScroll(PhysicalOffset(0, 200)), mapped_point);
  mapped_point = MapAncestorToLocal(container, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 50), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(50, GetDocument().View()->LayoutViewport()->ScrollOffsetInt().y());

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");
  LayoutBox* body = container->ParentBox();
  LayoutBox* html = body->ParentBox();
  LayoutBox* view = html->ParentBox();
  ASSERT_TRUE(IsA<LayoutView>(view));

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, view, PhysicalOffset());
  EXPECT_EQ(AdjustForFrameScroll(PhysicalOffset(0, 100)), mapped_point);
  mapped_point = MapAncestorToLocal(target, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point = MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(0, 0), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, PhysicalOffset(0, 0));
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point = MapLocalToAncestor(container, view, PhysicalOffset());
  EXPECT_EQ(AdjustForFrameScroll(PhysicalOffset(0, 100)), mapped_point);
  mapped_point = MapAncestorToLocal(container, view, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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
      ScrollOffset(0.0, 1000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  Element* target = ChildDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  PhysicalOffset mapped_point =
      MapAncestorToLocal(target->GetLayoutObject(), nullptr,
                         PhysicalOffset(10, 70), kTraverseDocumentBoundaries);

  // y = 70 - 8000, since the iframe is offset by 8000px from the main frame.
  // The scroll is not taken into account because the element is not fixed to
  // the root LayoutView, and the space of the root LayoutView does not include
  // scroll.
  EXPECT_EQ(PhysicalOffset(10, -7930), AdjustForFrameScroll(mapped_point));
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

  UpdateAllLifecyclePhasesForTest();

  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 1000), mojom::blink::ScrollType::kProgrammatic);
  ChildDocument().View()->UpdateAllLifecyclePhasesForTest();

  Element* target = ChildDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  PhysicalOffset mapped_point =
      MapAncestorToLocal(target->GetLayoutObject(), nullptr,
                         PhysicalOffset(200, 200), kTraverseDocumentBoundaries);

  // Derivation:
  // (200, 200) -> (-50, -50)  (Adjust for transform origin of scale, which is
  //                           at the center of the 500x500 iframe)
  // (-50, -50) -> (-25, -25)  (Divide by 2 to invert the scale)
  // (-25, -25) -> (225, 225)  (Add the origin back in)
  // (225, 225) -> (225, 1225) (Adjust by scroll offset of y=1000)
  EXPECT_EQ(PhysicalOffset(225, 1225), mapped_point);
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

  UpdateAllLifecyclePhasesForTest();
  ChildDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0.0, 1000), mojom::blink::ScrollType::kProgrammatic);
  UpdateAllLifecyclePhasesForTest();

  Element* target = ChildDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  PhysicalOffset mapped_point =
      MapAncestorToLocal(target->GetLayoutObject(), nullptr,
                         PhysicalOffset(0, 0), kTraverseDocumentBoundaries);

  EXPECT_EQ(PhysicalOffset(0, 0), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithText) {
  SetBodyInnerHTML(R"HTML(
    <div id='multicol' style='columns:2; column-gap:20px; width:400px;
    line-height:50px; padding:5px; orphans:1; widows:1;'>
        <br id='sibling'>
        text
    </div>
  )HTML");

  auto* const multicol =
      To<LayoutBlockFlow>(GetLayoutBoxByElementId("multicol"));
  LayoutObject* target = GetLayoutObjectByElementId("sibling")->NextSibling();
  ASSERT_TRUE(target->IsText());
  auto* const flow_thread = multicol->MultiColumnFlowThread();

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, flow_thread, PhysicalOffset(10, 70));
  EXPECT_EQ(PhysicalOffset(10, 70), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 70), mapped_point);

  mapped_point =
      MapLocalToAncestor(flow_thread, multicol, PhysicalOffset(10, 70));
  EXPECT_EQ(PhysicalOffset(225, 25), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 70), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithInline) {
  SetBodyInnerHTML(R"HTML(
    <div id='multicol' style='columns:2; column-gap:20px; width:400px;
    line-height:50px; padding:5px; orphans:1; widows:1;'>
        <span id='target'><br>text</span>
    </div>
  )HTML");

  auto* const multicol =
      To<LayoutBlockFlow>(GetLayoutBoxByElementId("multicol"));
  LayoutObject* target = GetLayoutObjectByElementId("target");
  auto* const flow_thread = multicol->MultiColumnFlowThread();

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, flow_thread, PhysicalOffset(10, 70));
  EXPECT_EQ(PhysicalOffset(10, 70), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 70), mapped_point);

  mapped_point =
      MapLocalToAncestor(flow_thread, multicol, PhysicalOffset(10, 70));
  EXPECT_EQ(PhysicalOffset(225, 25), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 70), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(125, 35), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* flow_thread = target->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, flow_thread, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(10, 120), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point =
      MapLocalToAncestor(flow_thread, container, PhysicalOffset(10, 120));
  EXPECT_EQ(PhysicalOffset(125, 35), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(10, 120), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithBlockAbove) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='columns:3; column-gap:0;
    column-fill:auto; width:300px; height:200px;'>
        <div id='target' style='margin-top:-50px; height:100px;'></div>
    </div>
  )HTML");

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(0, -50), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* flow_thread = target->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, flow_thread, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(0, -50), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point =
      MapLocalToAncestor(flow_thread, container, PhysicalOffset(0, -50));
  EXPECT_EQ(PhysicalOffset(0, -50), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(0, -50), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* outer_multicol = GetLayoutBoxByElementId("outerMulticol");
  auto* inner_multicol = GetLayoutBoxByElementId("innerMulticol");
  LayoutBox* inner_flow_thread = target->ParentBox();
  ASSERT_TRUE(inner_flow_thread->IsLayoutFlowThread());
  LayoutBox* outer_flow_thread = inner_multicol->ParentBox();
  ASSERT_TRUE(outer_flow_thread->IsLayoutFlowThread());

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, outer_multicol, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(435, 115), mapped_point);
  mapped_point = MapAncestorToLocal(target, outer_multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  mapped_point =
      MapLocalToAncestor(target, inner_flow_thread, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(0, 630), mapped_point);
  mapped_point = MapAncestorToLocal(target, inner_flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point = MapLocalToAncestor(inner_flow_thread, inner_multicol,
                                    PhysicalOffset(0, 630));
  EXPECT_EQ(PhysicalOffset(140, 305), mapped_point);
  mapped_point =
      MapAncestorToLocal(inner_flow_thread, inner_multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(0, 630), mapped_point);

  mapped_point = MapLocalToAncestor(inner_multicol, outer_flow_thread,
                                    PhysicalOffset(140, 305));
  EXPECT_EQ(PhysicalOffset(140, 315), mapped_point);
  mapped_point =
      MapAncestorToLocal(inner_multicol, outer_flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(140, 305), mapped_point);

  mapped_point = MapLocalToAncestor(outer_flow_thread, outer_multicol,
                                    PhysicalOffset(140, 315));
  EXPECT_EQ(PhysicalOffset(435, 115), mapped_point);
  mapped_point =
      MapAncestorToLocal(outer_flow_thread, outer_multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(140, 315), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* multicol = GetLayoutBoxByElementId("multicol");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, multicol, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(144, 54), mapped_point);
  mapped_point = MapAncestorToLocal(target, multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  auto* relpos = GetLayoutBoxByElementId("relpos");
  LayoutBox* flow_thread = relpos->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, relpos, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(25, 25), mapped_point);
  mapped_point = MapAncestorToLocal(target, relpos, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point =
      MapLocalToAncestor(relpos, flow_thread, PhysicalOffset(25, 25));
  EXPECT_EQ(PhysicalOffset(29, 139), mapped_point);
  mapped_point = MapAncestorToLocal(relpos, flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(25, 25), mapped_point);

  mapped_point =
      MapLocalToAncestor(flow_thread, multicol, PhysicalOffset(29, 139));
  EXPECT_EQ(PhysicalOffset(144, 54), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(29, 139), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolWithAbsPosInInlineRelPos) {
  SetBodyInnerHTML(R"HTML(
    <div id='multicol' style='columns:3; column-gap:0; column-fill:auto;
    width:300px; height:100px; border:8px solid; padding:7px;'>
        <div style='height:110px;'></div>
        <div id='container'>
          <span id='relpos' style='position:relative; left:4px; top:4px;'>
              <div id='target' style='position:absolute; left:15px; top:15px;
               margin:10px; border:13px; padding:13px;'></div>
          </span>
        </div>
    </div>
  )HTML");

  auto* target = GetLayoutBoxByElementId("target");
  auto* multicol = GetLayoutBoxByElementId("multicol");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, multicol, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(144, 54), mapped_point);
  mapped_point = MapAncestorToLocal(target, multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  auto* container = GetLayoutBoxByElementId("container");
  LayoutBox* flow_thread = container->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(29, 29), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point =
      MapLocalToAncestor(container, flow_thread, PhysicalOffset(25, 25));
  EXPECT_EQ(PhysicalOffset(25, 135), mapped_point);
  mapped_point = MapAncestorToLocal(container, flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(25, 25), mapped_point);

  mapped_point =
      MapLocalToAncestor(flow_thread, multicol, PhysicalOffset(29, 139));
  EXPECT_EQ(PhysicalOffset(144, 54), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(29, 139), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  // The multicol container isn't in the containing block chain of the abspos
  // #target.
  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(16, 16), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  auto* multicol = GetLayoutBoxByElementId("multicol");
  LayoutBox* flow_thread = target->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, flow_thread, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(-9, -9), mapped_point);

  mapped_point = MapLocalToAncestor(flow_thread, multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(6, 6), mapped_point);

  mapped_point = MapLocalToAncestor(multicol, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(16, 16), mapped_point);

  mapped_point = MapAncestorToLocal(multicol, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(6, 6), mapped_point);

  mapped_point = MapAncestorToLocal(flow_thread, multicol, mapped_point);
  EXPECT_EQ(PhysicalOffset(-9, -9), mapped_point);

  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
}

TEST_F(MapCoordinatesTest, MulticolRtl) {
  SetBodyInnerHTML(R"HTML(
    <div id='container' style='columns:3; column-gap:0; column-fill:auto;
    width:300px; height:200px; direction:rtl;'>
        <div style='height:200px;'></div>
        <div id='target' style='height:50px;'></div>
    </div>
  )HTML");

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(100, 0), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* flow_thread = target->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, flow_thread, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(0, 200), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point =
      MapLocalToAncestor(flow_thread, container, PhysicalOffset(0, 200));
  EXPECT_EQ(PhysicalOffset(100, 0), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(0, 200), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(300, 200), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* flow_thread = target->ParentBox();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());

  mapped_point = MapLocalToAncestor(target, flow_thread, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(0, 200), mapped_point);
  mapped_point = MapAncestorToLocal(target, flow_thread, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point =
      MapLocalToAncestor(flow_thread, container, PhysicalOffset(0, 200));
  EXPECT_EQ(PhysicalOffset(300, 200), mapped_point);
  mapped_point = MapAncestorToLocal(flow_thread, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(0, 200), mapped_point);
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

  // Map to the nearest container. Nothing special should happen because
  // everything is in physical coordinates.
  PhysicalOffset mapped_point =
      MapLocalToAncestor(text, text->ContainingBlock(), PhysicalOffset(75, 10));
  EXPECT_EQ(PhysicalOffset(75, 10), mapped_point);
  mapped_point =
      MapAncestorToLocal(text, text->ContainingBlock(), mapped_point);
  EXPECT_EQ(PhysicalOffset(75, 10), mapped_point);

  // Map to a container further up in the tree.
  mapped_point = MapLocalToAncestor(
      text, text->ContainingBlock()->ContainingBlock(), PhysicalOffset(75, 10));
  EXPECT_EQ(PhysicalOffset(80, 10), mapped_point);
  mapped_point = MapAncestorToLocal(
      text, text->ContainingBlock()->ContainingBlock(), mapped_point);
  EXPECT_EQ(PhysicalOffset(75, 10), mapped_point);
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

  // First map to the parent SPAN. Nothing special should happen.
  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, To<LayoutBoxModelObject>(target->Parent()),
                         PhysicalOffset(75, 10));
  EXPECT_EQ(PhysicalOffset(75, 10), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, To<LayoutBoxModelObject>(target->Parent()), mapped_point);
  EXPECT_EQ(PhysicalOffset(75, 10), mapped_point);

  // Continue to the nearest container. Nothing special should happen because
  // everything is in physical coordinates.
  mapped_point =
      MapLocalToAncestor(To<LayoutBoxModelObject>(target->Parent()),
                         target->ContainingBlock(), PhysicalOffset(75, 10));
  EXPECT_EQ(PhysicalOffset(75, 10), mapped_point);
  mapped_point = MapAncestorToLocal(To<LayoutBoxModelObject>(target->Parent()),
                                    target->ContainingBlock(), mapped_point);
  EXPECT_EQ(PhysicalOffset(75, 10), mapped_point);

  // Now map from the innermost inline to the nearest container in one go.
  mapped_point = MapLocalToAncestor(target, target->ContainingBlock(),
                                    PhysicalOffset(75, 10));
  EXPECT_EQ(PhysicalOffset(75, 10), mapped_point);
  mapped_point =
      MapAncestorToLocal(target, target->ContainingBlock(), mapped_point);
  EXPECT_EQ(PhysicalOffset(75, 10), mapped_point);

  // Map to a container further up in the tree.
  mapped_point =
      MapLocalToAncestor(target, target->ContainingBlock()->ContainingBlock(),
                         PhysicalOffset(75, 10));
  EXPECT_EQ(PhysicalOffset(82, 10), mapped_point);
  mapped_point = MapAncestorToLocal(
      target, target->ContainingBlock()->ContainingBlock(), mapped_point);
  EXPECT_EQ(PhysicalOffset(75, 10), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(153, 22), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  auto* middle = GetLayoutBoxByElementId("middle");

  mapped_point = MapLocalToAncestor(target, middle, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(7, 7), mapped_point);
  mapped_point = MapAncestorToLocal(target, middle, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  mapped_point = MapLocalToAncestor(middle, container, PhysicalOffset(7, 7));
  EXPECT_EQ(PhysicalOffset(153, 22), mapped_point);
  mapped_point = MapAncestorToLocal(middle, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(7, 7), mapped_point);
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(143, 302), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  LayoutBox* td = target->ParentBox();
  ASSERT_TRUE(td->IsTableCell());
  mapped_point = MapLocalToAncestor(target, td, PhysicalOffset());
  // Cells are middle-aligned by default.
  EXPECT_EQ(PhysicalOffset(2, 47), mapped_point);
  mapped_point = MapAncestorToLocal(target, td, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);

  LayoutBox* tr = td->ParentBox();
  ASSERT_TRUE(tr->IsTableRow());
  mapped_point = MapLocalToAncestor(td, tr, PhysicalOffset(2, 47));
  EXPECT_EQ(PhysicalOffset(116, 47), mapped_point);
  mapped_point = MapAncestorToLocal(td, tr, mapped_point);
  EXPECT_EQ(PhysicalOffset(2, 47), mapped_point);

  LayoutBox* tbody = tr->ParentBox();
  ASSERT_TRUE(tbody->IsTableSection());
  mapped_point = MapLocalToAncestor(tr, tbody, PhysicalOffset(126, 47));
  EXPECT_EQ(PhysicalOffset(126, 161), mapped_point);
  mapped_point = MapAncestorToLocal(tr, tbody, mapped_point);
  EXPECT_EQ(PhysicalOffset(126, 47), mapped_point);

  LayoutBox* table = tbody->ParentBox();
  ASSERT_TRUE(table->IsTable());
  mapped_point = MapLocalToAncestor(tbody, table, PhysicalOffset(126, 161));
  EXPECT_EQ(PhysicalOffset(141, 290), mapped_point);
  mapped_point = MapAncestorToLocal(tbody, table, mapped_point);
  EXPECT_EQ(PhysicalOffset(126, 161), mapped_point);

  mapped_point = MapLocalToAncestor(table, container, PhysicalOffset(131, 290));
  EXPECT_EQ(PhysicalOffset(143, 302), mapped_point);
  mapped_point = MapAncestorToLocal(table, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(131, 290), mapped_point);
}

static bool FloatValuesAlmostEqual(float expected, float actual) {
  return fabs(expected - actual) < 0.01;
}

static bool QuadsAlmostEqual(const gfx::QuadF& expected,
                             const gfx::QuadF& actual) {
  return FloatValuesAlmostEqual(expected.p1().x(), actual.p1().x()) &&
         FloatValuesAlmostEqual(expected.p1().y(), actual.p1().y()) &&
         FloatValuesAlmostEqual(expected.p2().x(), actual.p2().x()) &&
         FloatValuesAlmostEqual(expected.p2().y(), actual.p2().y()) &&
         FloatValuesAlmostEqual(expected.p3().x(), actual.p3().x()) &&
         FloatValuesAlmostEqual(expected.p3().y(), actual.p3().y()) &&
         FloatValuesAlmostEqual(expected.p4().x(), actual.p4().x()) &&
         FloatValuesAlmostEqual(expected.p4().y(), actual.p4().y());
}

// If comparison fails, pretty-print the error using EXPECT_EQ()
#define EXPECT_QUADF_EQ(expected, actual)      \
  do {                                         \
    if (!QuadsAlmostEqual(expected, actual)) { \
      EXPECT_EQ(expected, actual);             \
    }                                          \
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

  auto* target = GetLayoutBoxByElementId("target");
  auto* container = GetLayoutBoxByElementId("container");

  gfx::QuadF initial_quad(gfx::PointF(0, 0), gfx::PointF(200, 0),
                          gfx::PointF(200, 200), gfx::PointF(0, 200));
  gfx::QuadF mapped_quad = MapLocalToAncestor(target, container, initial_quad);
  EXPECT_QUADF_EQ(gfx::QuadF(gfx::PointF(200, 0), gfx::PointF(200, 200),
                             gfx::PointF(0, 200), gfx::PointF(0, 0)),
                  mapped_quad);
  mapped_quad = MapAncestorToLocal(target, container, mapped_quad);
  EXPECT_QUADF_EQ(initial_quad, mapped_quad);

  // Walk each ancestor in the chain separately, to verify each step on the way.
  auto* inner_transform = GetLayoutBoxByElementId("innerTransform");
  auto* outer_transform = GetLayoutBoxByElementId("outerTransform");

  mapped_quad = MapLocalToAncestor(target, inner_transform, initial_quad);
  EXPECT_QUADF_EQ(gfx::QuadF(gfx::PointF(0, 0), gfx::PointF(200, 0),
                             gfx::PointF(200, 200), gfx::PointF(0, 200)),
                  mapped_quad);
  mapped_quad = MapAncestorToLocal(target, inner_transform, mapped_quad);
  EXPECT_QUADF_EQ(initial_quad, mapped_quad);

  initial_quad = gfx::QuadF(gfx::PointF(0, 0), gfx::PointF(200, 0),
                            gfx::PointF(200, 200), gfx::PointF(0, 200));
  mapped_quad =
      MapLocalToAncestor(inner_transform, outer_transform, initial_quad);
  // Clockwise rotation by 45 degrees.
  EXPECT_QUADF_EQ(
      gfx::QuadF(gfx::PointF(100, -41.42), gfx::PointF(241.42, 100),
                 gfx::PointF(100, 241.42), gfx::PointF(-41.42, 100)),
      mapped_quad);
  mapped_quad =
      MapAncestorToLocal(inner_transform, outer_transform, mapped_quad);
  EXPECT_QUADF_EQ(initial_quad, mapped_quad);

  initial_quad = gfx::QuadF(gfx::PointF(100, -41.42), gfx::PointF(241.42, 100),
                            gfx::PointF(100, 241.42), gfx::PointF(-41.42, 100));
  mapped_quad = MapLocalToAncestor(outer_transform, container, initial_quad);
  // Another clockwise rotation by 45 degrees. So now 90 degrees in total.
  EXPECT_QUADF_EQ(gfx::QuadF(gfx::PointF(200, 0), gfx::PointF(200, 200),
                             gfx::PointF(0, 200), gfx::PointF(0, 0)),
                  mapped_quad);
  mapped_quad = MapAncestorToLocal(outer_transform, container, mapped_quad);
  EXPECT_QUADF_EQ(initial_quad, mapped_quad);
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
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(100, 200), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(200, 240), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(200, 50), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(200, 100), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(200, 50), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(200, 100), mapped_point);
  mapped_point = MapAncestorToLocal(target, container, mapped_point);
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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
  auto* container = GetLayoutBoxByElementId("container");

  PhysicalOffset mapped_point =
      MapLocalToAncestor(target, container, PhysicalOffset());
  EXPECT_EQ(PhysicalOffset(250, 50), mapped_point);
  // <svg>
  mapped_point = MapAncestorToLocal(target->Parent()->Parent()->Parent(),
                                    container, PhysicalOffset(250, 50));
  EXPECT_EQ(PhysicalOffset(250, 50), mapped_point);
  // <g>
  mapped_point = MapAncestorToLocal(target->Parent()->Parent(), container,
                                    PhysicalOffset(250, 50));
  EXPECT_EQ(PhysicalOffset(25, -25), mapped_point);
  // <foreignObject>
  mapped_point =
      MapAncestorToLocal(target->Parent(), container, PhysicalOffset(250, 50));
  EXPECT_EQ(PhysicalOffset(50, 0), mapped_point);
  // <div>
  mapped_point = MapAncestorToLocal(target, container, PhysicalOffset(250, 50));
  EXPECT_EQ(PhysicalOffset(), mapped_point);
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
  auto* container =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("container"));
  gfx::Transform container_matrix = container->LocalToAbsoluteTransform();
  EXPECT_TRUE(container_matrix.IsIdentity());

  LayoutObject* child = GetLayoutObjectByElementId("child");
  gfx::Transform child_matrix = child->LocalToAbsoluteTransform();
  EXPECT_FALSE(child_matrix.IsIdentityOrTranslation());
  EXPECT_TRUE(child_matrix.Is2dTransform());
  EXPECT_EQ(gfx::PointF(), child_matrix.ProjectPoint(gfx::PointF()));
  EXPECT_EQ(gfx::PointF(20.0f, 40.0f),
            child_matrix.ProjectPoint(gfx::PointF(10.0f, 20.0f)));
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
  auto* container =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("container"));
  auto* rotate1 =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("rotate1"));
  auto* rotate2 =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("rotate2"));
  LayoutObject* child = GetLayoutObjectByElementId("child");
  gfx::Transform matrix;

  matrix = child->LocalToAncestorTransform(rotate2);
  EXPECT_TRUE(matrix.IsIdentity());

  // Rotate (100, 0) 90 degrees to (0, 100)
  matrix = child->LocalToAncestorTransform(rotate1);
  EXPECT_FALSE(matrix.IsIdentity());
  EXPECT_TRUE(matrix.Is2dTransform());
  EXPECT_NEAR(0.0, matrix.ProjectPoint(gfx::PointF(100.0, 0.0)).x(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(100.0, matrix.ProjectPoint(gfx::PointF(100.0, 0.0)).y(),
              LayoutUnit::Epsilon());

  // Rotate (100, 0) 135 degrees to (-70.7, 70.7)
  matrix = child->LocalToAncestorTransform(container);
  EXPECT_FALSE(matrix.IsIdentity());
  EXPECT_TRUE(matrix.Is2dTransform());
  EXPECT_NEAR(-100.0 * sqrt(2.0) / 2.0,
              matrix.ProjectPoint(gfx::PointF(100.0, 0.0)).x(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(100.0 * sqrt(2.0) / 2.0,
              matrix.ProjectPoint(gfx::PointF(100.0, 0.0)).y(),
              LayoutUnit::Epsilon());
}

TEST_F(MapCoordinatesTest, LocalToAbsoluteTransformFlattens) {
  SetBodyInnerHTML(R"HTML(
    <div style='position: absolute; left: 0; top: 0;'>
      <div style='transform: rotateY(45deg); transform-style: preserve-3d;'>
        <div style='transform: rotateY(-45deg); transform-style: preserve-3d;'>
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
  gfx::Transform matrix;

  matrix = child1->LocalToAbsoluteTransform();

  // With child1, the rotations cancel and points should map basically back to
  // themselves.
  EXPECT_NEAR(100.0, matrix.MapPoint(gfx::PointF(100.0, 50.0)).x(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(50.0, matrix.MapPoint(gfx::PointF(100.0, 50.0)).y(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(50.0, matrix.MapPoint(gfx::PointF(50.0, 100.0)).x(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(100.0, matrix.MapPoint(gfx::PointF(50.0, 100.0)).y(),
              LayoutUnit::Epsilon());

  // With child2, each rotation gets flattened and the end result is
  // approximately a scale(1.0, 0.5).
  matrix = child2->LocalToAbsoluteTransform();
  EXPECT_NEAR(50.0, matrix.MapPoint(gfx::PointF(100.0, 50.0)).x(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(50.0, matrix.MapPoint(gfx::PointF(100.0, 50.0)).y(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(25.0, matrix.MapPoint(gfx::PointF(50.0, 100.0)).x(),
              LayoutUnit::Epsilon());
  EXPECT_NEAR(100.0, matrix.MapPoint(gfx::PointF(50.0, 100.0)).y(),
              LayoutUnit::Epsilon());
}

TEST_F(MapCoordinatesTest, Transform3DWithOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
    </style>
    <div style="perspective: 400px; width: 0; height: 0">
      <div>
        <div style="height: 100px"></div>
        <div style="transform-style: preserve-3d; transform: rotateY(0deg)">
          <div id="target" style="width: 100px; height: 100px;
                                  transform: translateZ(200px)">
          </div>
        </div>
      </div>
    </div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(gfx::QuadF(gfx::RectF(0, 100, 100, 100)),
            MapLocalToAncestor(target, nullptr,
                               gfx::QuadF(gfx::RectF(0, 0, 100, 100))));
}

TEST_F(MapCoordinatesTest, Transform3DWithOffset2) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
    </style>
    <div style="perspective: 400px; width: 0; height: 0">
      <div style="transform-style: preserve-3d">
        <div style="height: 100px"></div>
        <div style="transform-style: preserve-3d; transform: rotateY(0deg)">
          <div id="target" style="width: 100px; height: 100px;
                                  transform: translateZ(200px)">
          </div>
        </div>
      </div>
    </div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(gfx::QuadF(gfx::RectF(0, 200, 200, 200)),
            MapLocalToAncestor(target, nullptr,
                               gfx::QuadF(gfx::RectF(0, 0, 100, 100))));
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

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* box = GetLayoutBoxByElementId("box");

  EXPECT_EQ(PhysicalOffset(0, 10),
            MapLocalToAncestor(box, scroller, PhysicalOffset()));
  EXPECT_EQ(
      PhysicalOffset(0, 10),
      MapLocalToAncestor(box, scroller, PhysicalOffset(), kIgnoreScrollOffset));

  To<Element>(scroller->GetNode())
      ->GetLayoutBoxForScrolling()
      ->GetScrollableArea()
      ->ScrollToAbsolutePosition(gfx::PointF(0, 50));

  EXPECT_EQ(PhysicalOffset(0, -40),
            MapLocalToAncestor(box, scroller, PhysicalOffset()));
  EXPECT_EQ(
      PhysicalOffset(0, 10),
      MapLocalToAncestor(box, scroller, PhysicalOffset(), kIgnoreScrollOffset));
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

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* box = To<LayoutInline>(GetLayoutObjectByElementId("box"));

  EXPECT_EQ(PhysicalOffset(0, 10),
            MapLocalToAncestor(box, scroller, PhysicalOffset()));
  EXPECT_EQ(
      PhysicalOffset(0, 10),
      MapLocalToAncestor(box, scroller, PhysicalOffset(), kIgnoreScrollOffset));

  To<Element>(scroller->GetNode())
      ->GetLayoutBoxForScrolling()
      ->GetScrollableArea()
      ->ScrollToAbsolutePosition(gfx::PointF(0, 50));

  EXPECT_EQ(PhysicalOffset(0, 10),
            MapLocalToAncestor(box, scroller, PhysicalOffset()));
  EXPECT_EQ(
      PhysicalOffset(0, 60),
      MapLocalToAncestor(box, scroller, PhysicalOffset(), kIgnoreScrollOffset));
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

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* box = GetLayoutBoxByElementId("box");
  auto* scroll_element = To<Element>(scroller->GetNode());

  EXPECT_EQ(PhysicalOffset(90, 10),
            MapLocalToAncestor(box, scroller, PhysicalOffset()));
  EXPECT_EQ(
      PhysicalOffset(1990, 10),
      MapLocalToAncestor(box, scroller, PhysicalOffset(), kIgnoreScrollOffset));

  scroll_element->GetLayoutBoxForScrolling()
      ->GetScrollableArea()
      ->ScrollToAbsolutePosition(gfx::PointF(0, 50));

  EXPECT_EQ(PhysicalOffset(1990, -40),
            MapLocalToAncestor(box, scroller, PhysicalOffset()));
  EXPECT_EQ(
      PhysicalOffset(1990, 10),
      MapLocalToAncestor(box, scroller, PhysicalOffset(), kIgnoreScrollOffset));

  scroll_element->GetLayoutBoxForScrolling()
      ->GetScrollableArea()
      ->ScrollToAbsolutePosition(gfx::PointF(1900, 50));

  EXPECT_EQ(PhysicalOffset(90, -40),
            MapLocalToAncestor(box, scroller, PhysicalOffset()));
  EXPECT_EQ(
      PhysicalOffset(1990, 10),
      MapLocalToAncestor(box, scroller, PhysicalOffset(), kIgnoreScrollOffset));
}

TEST_F(MapCoordinatesTest, FixedPositionWithScrollOffset) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="position: fixed; top: 200px; left: 100px"></div>
    <div style="height: 10000px"></div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  PhysicalOffset expected(100, 200);
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset()));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset()));
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset(),
                                         kIgnoreScrollOffset));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset(),
                               kIgnoreScrollOffset));

  // Scroll offset doesn't affect MapLocalToAncestor(), regardless of
  // kIgnoreScrollOffset.
  GetLayoutView().GetScrollableArea()->ScrollToAbsolutePosition(
      gfx::PointF(0, 400));
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset()));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset()));
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset(),
                                         kIgnoreScrollOffset));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset(),
                               kIgnoreScrollOffset));
}

TEST_F(MapCoordinatesTest, FixedPositionWithScrollOffsetVerticalRL) {
  SetBodyInnerHTML(R"HTML(
    <style>body { writing-mode: vertical-rl; margin: 0; }</style>
    <div id="target" style="position: fixed; top: 200px; left: 100px"></div>
    <div style="width: 10000px"></div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  PhysicalOffset expected(100, 200);
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset()));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset()));
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset(),
                                         kIgnoreScrollOffset));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset(),
                               kIgnoreScrollOffset));

  // Scroll offset doesn't affect MapLocalToAncestor(), regardless of
  // kIgnoreScrollOffset.
  GetLayoutView().GetScrollableArea()->ScrollToAbsolutePosition(
      gfx::PointF(400, 0));
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset()));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset()));
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset(),
                                         kIgnoreScrollOffset));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset(),
                               kIgnoreScrollOffset));
}

TEST_F(MapCoordinatesTest, FixedPositionUnderTransformWithScrollOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div style="will-change: transform">
      <div id="target" style="position: fixed; top: 200px; left: 100px"></div>
    </div>
    <div style="height: 10000px"></div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  PhysicalOffset expected(100, 200);
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset()));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset()));
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset(),
                                         kIgnoreScrollOffset));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset(),
                               kIgnoreScrollOffset));

  // Fixed position under transform is treated like absolute position, so is
  // affected by scroll offset.
  GetLayoutView().GetScrollableArea()->ScrollToAbsolutePosition(
      gfx::PointF(0, 400));
  PhysicalOffset expected_scrolled(100, -200);
  EXPECT_EQ(expected_scrolled,
            MapLocalToAncestor(target, nullptr, PhysicalOffset()));
  EXPECT_EQ(expected_scrolled,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset()));
  EXPECT_EQ(expected, MapLocalToAncestor(target, nullptr, PhysicalOffset(),
                                         kIgnoreScrollOffset));
  EXPECT_EQ(expected,
            MapLocalToAncestor(target, &GetLayoutView(), PhysicalOffset(),
                               kIgnoreScrollOffset));
}

// This test verifies that ignoring scroll offset works with writing modes and
// non-overlay scrollbar.
TEST_F(MapCoordinatesTest,
       IgnoreScrollOffsetWithWritingModesAndNonOverlayScrollbar) {
  USE_NON_OVERLAY_SCROLLBARS_OR_QUIT();

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

  auto* scroller = GetLayoutBoxByElementId("scroller");
  auto* box = GetLayoutBoxByElementId("box");

  // The box is on the left of the scrollbar so the width of the scrollbar
  // affects the location of the box.
  EXPECT_EQ(PhysicalOffset(75, 10),
            MapLocalToAncestor(box, scroller, PhysicalOffset()));
  EXPECT_EQ(
      PhysicalOffset(1990, 10),
      MapLocalToAncestor(box, scroller, PhysicalOffset(), kIgnoreScrollOffset));

  To<Element>(scroller->GetNode())
      ->GetLayoutBoxForScrolling()
      ->GetScrollableArea()
      ->ScrollToAbsolutePosition(gfx::PointF(0, 0));

  // The box is now on the right of the scrollbar therefore there is nothing
  // between the box and the right border of the content.
  EXPECT_EQ(PhysicalOffset(1990, 10),
            MapLocalToAncestor(box, scroller, PhysicalOffset()));
  EXPECT_EQ(
      PhysicalOffset(1990, 10),
      MapLocalToAncestor(box, scroller, PhysicalOffset(), kIgnoreScrollOffset));
}

}  // namespace blink
