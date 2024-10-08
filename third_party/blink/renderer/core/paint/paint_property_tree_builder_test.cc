// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_property_tree_builder_test.h"

#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/paint/fragment_data_iterator.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/platform/graphics/compositing/paint_artifact_compositor.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"
#include "third_party/blink/renderer/platform/testing/layer_tree_host_embedder.h"
#include "third_party/blink/renderer/platform/testing/paint_property_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace blink {

void PaintPropertyTreeBuilderTest::LoadTestData(const char* file_name) {
  StringBuilder full_path;
  full_path.Append(test::BlinkRootDir());
  full_path.Append("/renderer/core/paint/test_data/");
  full_path.Append(file_name);
  const Vector<char> input_buffer = *test::ReadFromFile(full_path.ToString());
  SetBodyInnerHTML(String(input_buffer.data(), input_buffer.size()));
}

const TransformPaintPropertyNode*
PaintPropertyTreeBuilderTest::DocPreTranslation(const Document* document) {
  if (!document)
    document = &GetDocument();
  return document->GetLayoutView()
      ->FirstFragment()
      .PaintProperties()
      ->PaintOffsetTranslation();
}

const TransformPaintPropertyNode*
PaintPropertyTreeBuilderTest::DocScrollTranslation(const Document* document) {
  if (!document)
    document = &GetDocument();
  return document->GetLayoutView()
      ->FirstFragment()
      .PaintProperties()
      ->ScrollTranslation();
}

const ClipPaintPropertyNode* PaintPropertyTreeBuilderTest::DocContentClip(
    const Document* document) {
  if (!document)
    document = &GetDocument();
  return document->GetLayoutView()
      ->FirstFragment()
      .PaintProperties()
      ->OverflowClip();
}

const ScrollPaintPropertyNode* PaintPropertyTreeBuilderTest::DocScroll(
    const Document* document) {
  if (!document)
    document = &GetDocument();
  return document->GetLayoutView()->FirstFragment().PaintProperties()->Scroll();
}

const ObjectPaintProperties*
PaintPropertyTreeBuilderTest::PaintPropertiesForElement(const char* name) {
  return GetDocument()
      .getElementById(AtomicString(name))
      ->GetLayoutObject()
      ->FirstFragment()
      .PaintProperties();
}

const GeometryMapperTransformCache&
PaintPropertyTreeBuilderTest::GetTransformCache(
    const TransformPaintPropertyNode& transform) {
  return transform.GetTransformCache();
}

void PaintPropertyTreeBuilderTest::SetUp() {
  EnableCompositing();
  RenderingTest::SetUp();
}

#define CHECK_VISUAL_RECT(expected, source_object, ancestor, slop_factor)      \
  do {                                                                         \
    if ((source_object)->HasLayer() && (ancestor)->HasLayer()) {               \
      auto actual = LocalVisualRect(*(source_object));                         \
      (source_object)                                                          \
          ->MapToVisualRectInAncestorSpace(ancestor, actual,                   \
                                           kUseGeometryMapper);                \
      SCOPED_TRACE("GeometryMapper: ");                                        \
      EXPECT_EQ(expected, actual);                                             \
    }                                                                          \
                                                                               \
    if (slop_factor == LayoutUnit::Max())                                      \
      break;                                                                   \
    auto slow_path_rect = LocalVisualRect(*(source_object));                   \
    (source_object)->MapToVisualRectInAncestorSpace(ancestor, slow_path_rect); \
    if (slop_factor) {                                                         \
      auto inflated_expected = expected;                                       \
      inflated_expected.Inflate(LayoutUnit(slop_factor));                      \
      SCOPED_TRACE(String::Format(                                             \
          "Slow path rect: %s, Expected: %s, Inflated expected: %s",           \
          slow_path_rect.ToString().Ascii().c_str(),                           \
          expected.ToString().Ascii().c_str(),                                 \
          inflated_expected.ToString().Ascii().c_str()));                      \
      EXPECT_TRUE(                                                             \
          PhysicalRect(ToEnclosingRect(slow_path_rect)).Contains(expected));   \
      EXPECT_TRUE(inflated_expected.Contains(slow_path_rect));                 \
    } else {                                                                   \
      SCOPED_TRACE("Slow path: ");                                             \
      EXPECT_EQ(expected, slow_path_rect);                                     \
    }                                                                          \
  } while (0)

#define CHECK_EXACT_VISUAL_RECT(expected, source_object, ancestor) \
  CHECK_VISUAL_RECT(expected, source_object, ancestor, 0)

INSTANTIATE_TEST_SUITE_P(All,
                         PaintPropertyTreeBuilderTest,
                         ::testing::Values(0,
                                           kUnderInvalidationChecking,
                                           kElementCapture));

TEST_P(PaintPropertyTreeBuilderTest, FixedPosition) {
  LoadTestData("fixed-position.html");

  Element* positioned_scroll =
      GetDocument().getElementById(AtomicString("positionedScroll"));
  positioned_scroll->setScrollTop(3);
  Element* transformed_scroll =
      GetDocument().getElementById(AtomicString("transformedScroll"));
  transformed_scroll->setScrollTop(5);

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhasesForTest();

  // target1 is a fixed-position element inside an absolute-position scrolling
  // element.  It should be attached under the viewport to skip scrolling and
  // offset of the parent.
  Element* target1 = GetDocument().getElementById(AtomicString("target1"));
  const ObjectPaintProperties* target1_properties =
      target1->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_CLIP_RECT(FloatRoundedRect(0, 0, 100, 100),
                   target1_properties->OverflowClip());
  // Likewise, it inherits clip from the viewport, skipping overflow clip of the
  // scroller.
  EXPECT_EQ(DocContentClip(), target1_properties->OverflowClip()->Parent());
  // target1 should not have its own scroll node and instead should inherit
  // positionedScroll's.
  const ObjectPaintProperties* positioned_scroll_properties =
      positioned_scroll->GetLayoutObject()->FirstFragment().PaintProperties();
  auto* positioned_scroll_translation =
      positioned_scroll_properties->ScrollTranslation();
  auto* positioned_scroll_node = positioned_scroll_translation->ScrollNode();
  EXPECT_EQ(DocScroll(), positioned_scroll_node->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -3),
            positioned_scroll_translation->Get2dTranslation());
  EXPECT_EQ(nullptr, target1_properties->ScrollTranslation());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(200, 150, 100, 100),
                          target1->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // target2 is a fixed-position element inside a transformed scrolling element.
  // It should be attached under the scrolled box of the transformed element.
  Element* target2 = GetDocument().getElementById(AtomicString("target2"));
  const ObjectPaintProperties* target2_properties =
      target2->GetLayoutObject()->FirstFragment().PaintProperties();
  Element* scroller =
      GetDocument().getElementById(AtomicString("transformedScroll"));
  const ObjectPaintProperties* scroller_properties =
      scroller->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_CLIP_RECT(FloatRoundedRect(200, 150, 100, 100),
                   target2_properties->OverflowClip());
  EXPECT_EQ(scroller_properties->OverflowClip(),
            target2_properties->OverflowClip()->Parent());
  // target2 should not have it's own scroll node and instead should inherit
  // transformedScroll's.
  const ObjectPaintProperties* transformed_scroll_properties =
      transformed_scroll->GetLayoutObject()->FirstFragment().PaintProperties();
  auto* transformed_scroll_translation =
      transformed_scroll_properties->ScrollTranslation();
  auto* transformed_scroll_node = transformed_scroll_translation->ScrollNode();
  EXPECT_EQ(DocScroll(), transformed_scroll_node->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -5),
            transformed_scroll_translation->Get2dTranslation());
  EXPECT_EQ(nullptr, target2_properties->ScrollTranslation());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(208, 153, 200, 100),
                          target2->GetLayoutObject(),
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, PositionAndScroll) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  LoadTestData("position-and-scroll.html");

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  scroller->scrollTo(0, 100);
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhasesForTest();
  const ObjectPaintProperties* scroller_properties =
      scroller->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(0, -100),
            scroller_properties->ScrollTranslation()->Get2dTranslation());
  EXPECT_EQ(scroller_properties->PaintOffsetTranslation(),
            scroller_properties->ScrollTranslation()->Parent());
  EXPECT_EQ(DocScrollTranslation(),
            scroller_properties->PaintOffsetTranslation()->Parent());
  EXPECT_EQ(scroller_properties->PaintOffsetTranslation(),
            &scroller_properties->OverflowClip()->LocalTransformSpace());
  const auto* scroll = scroller_properties->ScrollTranslation()->ScrollNode();
  EXPECT_EQ(DocScroll(), scroll->Parent());
  EXPECT_EQ(gfx::Rect(0, 0, 413, 317), scroll->ContainerRect());
  EXPECT_EQ(gfx::Rect(0, 0, 660, 10200), scroll->ContentsRect());
  EXPECT_FALSE(scroll->UserScrollableHorizontal());
  EXPECT_TRUE(scroll->UserScrollableVertical());
  EXPECT_EQ(gfx::Vector2dF(120, 340),
            scroller_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_CLIP_RECT(FloatRoundedRect(0, 0, 413, 317),
                   scroller_properties->OverflowClip());
  EXPECT_EQ(DocContentClip(), scroller_properties->OverflowClip()->Parent());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(120, 340, 413, 317),
                          scroller->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // The relative-positioned element should have accumulated box offset (exclude
  // scrolling), and should be affected by ancestor scroll transforms.
  Element* rel_pos = GetDocument().getElementById(AtomicString("rel-pos"));
  const ObjectPaintProperties* rel_pos_properties =
      rel_pos->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(560, 780),
            rel_pos_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_EQ(scroller_properties->ScrollTranslation(),
            rel_pos_properties->PaintOffsetTranslation()->Parent());
  EXPECT_EQ(rel_pos_properties->Transform(),
            &rel_pos_properties->OverflowClip()->LocalTransformSpace());
  EXPECT_CLIP_RECT(FloatRoundedRect(0, 0, 100, 200),
                   rel_pos_properties->OverflowClip());
  EXPECT_EQ(scroller_properties->OverflowClip(),
            rel_pos_properties->OverflowClip()->Parent());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(), rel_pos->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // The absolute-positioned element should not be affected by non-positioned
  // scroller at all.
  Element* abs_pos = GetDocument().getElementById(AtomicString("abs-pos"));
  const ObjectPaintProperties* abs_pos_properties =
      abs_pos->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(123, 456),
            abs_pos_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            abs_pos_properties->PaintOffsetTranslation()->Parent());
  EXPECT_EQ(abs_pos_properties->Transform(),
            &abs_pos_properties->OverflowClip()->LocalTransformSpace());
  EXPECT_CLIP_RECT(FloatRoundedRect(0, 0, 300, 400),
                   abs_pos_properties->OverflowClip());
  EXPECT_EQ(DocContentClip(), abs_pos_properties->OverflowClip()->Parent());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(123, 456, 300, 400),
                          abs_pos->GetLayoutObject(),
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollExcludeScrollbars) {
  SetBodyInnerHTML(R"HTML(
    <div id='scroller'
         style='width: 100px; height: 100px; overflow: scroll;
                 border: 10px solid blue'>
      <div style='width: 400px; height: 400px'></div>
    </div>
  )HTML");
  CHECK(GetDocument().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars());

  const auto* properties = PaintPropertiesForElement("scroller");
  const auto* overflow_clip = properties->OverflowClip();

  EXPECT_EQ(DocContentClip(), overflow_clip->Parent());
  EXPECT_EQ(properties->PaintOffsetTranslation(),
            &overflow_clip->LocalTransformSpace());
  EXPECT_EQ(FloatClipRect(gfx::RectF(10, 10, 100, 100)),
            overflow_clip->LayoutClipRect());

  PaintLayer* paint_layer = GetPaintLayerByElementId("scroller");
  EXPECT_TRUE(paint_layer->GetScrollableArea()
                  ->VerticalScrollbar()
                  ->IsOverlayScrollbar());

  EXPECT_EQ(FloatClipRect(gfx::RectF(10, 10, 93, 93)),
            overflow_clip->LayoutClipRectExcludingOverlayScrollbars());
}

TEST_P(PaintPropertyTreeBuilderTest, OverlapNoPaintOffsetTranslation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div { width: 100px; height: 100px }
    </style>
    <div style='will-change: transform'></div>
    <div id=target style='margin-top: -50px; position: relative; opacity: 0.5'></div>
  )HTML");
  CHECK(GetDocument().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars());
  const auto* properties = PaintPropertiesForElement("target");
  EXPECT_EQ(nullptr, properties->PaintOffsetTranslation());
}

TEST_P(PaintPropertyTreeBuilderTest, AssumeOverlapNoPaintOffsetTranslation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div { width: 100px; height: 100px }
    </style>
    <div style='position: fixed'></div>
    <div id=target style='position: relative; opacity: 0.5'></div>
    <div style="height: 1000px"></div>
  )HTML");
  CHECK(GetDocument().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars());
  const auto* properties = PaintPropertiesForElement("target");
  EXPECT_EQ(nullptr, properties->PaintOffsetTranslation());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollExcludeScrollbarsSubpixel) {
  SetBodyInnerHTML(R"HTML(
    <div id='scroller'
         style='width: 100.5px; height: 100px; overflow: scroll;
                 border: 10px solid blue'>
      <div style='width: 400px; height: 400px'></div>
    </div>
  )HTML");
  CHECK(GetDocument().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars());

  const auto* scroller = GetLayoutObjectByElementId("scroller");
  const auto* properties = scroller->FirstFragment().PaintProperties();
  const auto* overflow_clip = properties->OverflowClip();

  EXPECT_EQ(DocContentClip(), overflow_clip->Parent());
  EXPECT_EQ(properties->PaintOffsetTranslation(),
            &overflow_clip->LocalTransformSpace());
  EXPECT_EQ(FloatClipRect(gfx::RectF(10, 10, 100.5, 100)),
            overflow_clip->LayoutClipRect());
  EXPECT_EQ(FloatRoundedRect(10, 10, 101, 100), overflow_clip->PaintClipRect());

  EXPECT_TRUE(To<LayoutBox>(scroller)
                  ->GetScrollableArea()
                  ->VerticalScrollbar()
                  ->IsOverlayScrollbar());

  EXPECT_EQ(FloatClipRect(gfx::RectF(10, 10, 93.5, 93)),
            overflow_clip->LayoutClipRectExcludingOverlayScrollbars());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollExcludeCssOverlayScrollbar) {
  SetBodyInnerHTML(R"HTML(
    <style>
    ::-webkit-scrollbar { background-color: transparent; }
    ::-webkit-scrollbar:vertical { width: 200px; }
    ::-webkit-scrollbar-thumb { background: transparent; }
    body {
      margin: 0 30px 0 0;
      background: lightgreen;
      overflow-y: overlay;
      overflow-x: hidden;
    }
    </style>
    <div style="height: 5000px; width: 100%; background: lightblue;"></div>
  )HTML");
  // The document content should not be clipped by the overlay scrollbar because
  // the scrollbar can be transparent and the content needs to paint below.
  EXPECT_CLIP_RECT(FloatRoundedRect(0, 0, 600, 600), DocContentClip());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollVerticalRL) {
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar {width: 15px; height: 15px}</style>
    <div id='scroller'
         style='width: 100px; height: 100px; overflow: scroll;
                writing-mode: vertical-rl; border: 10px solid blue'>
      <div id="content" style='width: 400px; height: 400px'></div>
    </div>
  )HTML");

  const auto* scroller = GetLayoutBoxByElementId("scroller");
  const auto* content = GetLayoutObjectByElementId("content");
  const auto* properties = scroller->FirstFragment().PaintProperties();
  const auto* overflow_clip = properties->OverflowClip();
  const auto* scroll_translation = properties->ScrollTranslation();
  const auto* scroll = properties->Scroll();

  // -315: container_width (100) - contents_width (400) - scrollber_width
  EXPECT_EQ(gfx::Vector2dF(-315, 0), scroll_translation->Get2dTranslation());
  EXPECT_EQ(scroll, scroll_translation->ScrollNode());
  // 10: border width. 85: container client size (== 100 - scrollbar width).
  EXPECT_EQ(gfx::Rect(10, 10, 85, 85), scroll->ContainerRect());
  EXPECT_EQ(gfx::Rect(10, 10, 400, 400), scroll->ContentsRect());
  EXPECT_EQ(PhysicalOffset(), scroller->FirstFragment().PaintOffset());
  EXPECT_EQ(gfx::Point(315, 0), scroller->ScrollOrigin());
  EXPECT_EQ(PhysicalOffset(10, 10), content->FirstFragment().PaintOffset());

  EXPECT_EQ(DocContentClip(), overflow_clip->Parent());
  EXPECT_EQ(properties->PaintOffsetTranslation(),
            &overflow_clip->LocalTransformSpace());
  EXPECT_CLIP_RECT(FloatRoundedRect(10, 10, 85, 85), overflow_clip);

  scroller->GetScrollableArea()->ScrollBy(ScrollOffset(-100, 0),
                                          mojom::blink::ScrollType::kUser);
  UpdateAllLifecyclePhasesForTest();

  // Only scroll_translation is affected by scrolling.
  EXPECT_EQ(gfx::Vector2dF(-215, 0), scroll_translation->Get2dTranslation());
  // Other properties are the same as before.
  EXPECT_EQ(scroll, scroll_translation->ScrollNode());
  EXPECT_EQ(gfx::Rect(10, 10, 85, 85), scroll->ContainerRect());
  EXPECT_EQ(gfx::Rect(10, 10, 400, 400), scroll->ContentsRect());
  EXPECT_EQ(PhysicalOffset(), scroller->FirstFragment().PaintOffset());
  EXPECT_EQ(gfx::Point(315, 0), scroller->ScrollOrigin());
  EXPECT_EQ(PhysicalOffset(10, 10), content->FirstFragment().PaintOffset());

  EXPECT_EQ(DocContentClip(), overflow_clip->Parent());
  EXPECT_EQ(properties->PaintOffsetTranslation(),
            &overflow_clip->LocalTransformSpace());
  EXPECT_CLIP_RECT(FloatRoundedRect(10, 10, 85, 85), overflow_clip);
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollRTL) {
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar {width: 15px; height: 15px}</style>
    <div id='scroller'
         style='width: 100px; height: 100px; overflow: scroll;
                direction: rtl; border: 10px solid blue'>
      <div id='content' style='width: 400px; height: 400px'></div>
    </div>
  )HTML");

  const auto* scroller = GetLayoutBoxByElementId("scroller");
  const auto* content = GetLayoutObjectByElementId("content");
  const auto* properties = scroller->FirstFragment().PaintProperties();
  const auto* overflow_clip = properties->OverflowClip();
  const auto* scroll_translation = properties->ScrollTranslation();
  const auto* scroll = properties->Scroll();

  // -315: container_width (100) - contents_width (400) - scrollbar width (15).
  EXPECT_EQ(gfx::Vector2dF(-315, 0), scroll_translation->Get2dTranslation());
  EXPECT_EQ(scroll, scroll_translation->ScrollNode());
  // 25: border width (10) + scrollbar (on the left) width (15).
  // 85: container client size (== 100 - scrollbar width).
  EXPECT_EQ(gfx::Rect(25, 10, 85, 85), scroll->ContainerRect());
  EXPECT_EQ(gfx::Rect(25, 10, 400, 400), scroll->ContentsRect());
  EXPECT_EQ(PhysicalOffset(), scroller->FirstFragment().PaintOffset());
  EXPECT_EQ(gfx::Point(315, 0), scroller->ScrollOrigin());
  EXPECT_EQ(PhysicalOffset(25, 10), content->FirstFragment().PaintOffset());

  EXPECT_EQ(DocContentClip(), overflow_clip->Parent());
  EXPECT_EQ(properties->PaintOffsetTranslation(),
            &overflow_clip->LocalTransformSpace());
  EXPECT_CLIP_RECT(FloatRoundedRect(25, 10, 85, 85), overflow_clip);

  scroller->GetScrollableArea()->ScrollBy(ScrollOffset(-100, 0),
                                          mojom::blink::ScrollType::kUser);
  UpdateAllLifecyclePhasesForTest();

  // Only scroll_translation is affected by scrolling.
  EXPECT_EQ(gfx::Vector2dF(-215, 0), scroll_translation->Get2dTranslation());
  // Other properties are the same as before.
  EXPECT_EQ(scroll, scroll_translation->ScrollNode());
  EXPECT_EQ(gfx::Rect(25, 10, 85, 85), scroll->ContainerRect());
  EXPECT_EQ(gfx::Rect(25, 10, 400, 400), scroll->ContentsRect());
  EXPECT_EQ(PhysicalOffset(), scroller->FirstFragment().PaintOffset());
  EXPECT_EQ(gfx::Point(315, 0), scroller->ScrollOrigin());
  EXPECT_EQ(PhysicalOffset(25, 10), content->FirstFragment().PaintOffset());

  EXPECT_EQ(DocContentClip(), overflow_clip->Parent());
  EXPECT_EQ(properties->PaintOffsetTranslation(),
            &overflow_clip->LocalTransformSpace());
  EXPECT_CLIP_RECT(FloatRoundedRect(25, 10, 85, 85), overflow_clip);
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollVerticalRLMulticol) {
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar {width: 15px; height: 15px}</style>
    <div id='scroller'
         style='width: 100px; height: 100px; overflow: scroll;
                writing-mode: vertical-rl; border: 10px solid blue'>
      <div id="multicol"
           style="width: 50px; height: 400px; columns: 2; column-gap: 0">
        <div id="child" style="width: 100px"></div>
      </div>
      <div style='width: 400px; height: 400px'></div>
    </div>
  )HTML");

  auto check_fragments = [this]() {
    const auto* child = GetLayoutObjectByElementId("child");
    ASSERT_EQ(2u, NumFragments(child));
    EXPECT_EQ(PhysicalOffset(410, 10), FragmentAt(child, 0).PaintOffset());
    EXPECT_EQ(PhysicalOffset(410, 210), FragmentAt(child, 1).PaintOffset());
  };
  check_fragments();

  // Fragment geometries are not affected by parent scrolling.
  GetLayoutBoxByElementId("scroller")
      ->GetScrollableArea()
      ->ScrollBy(ScrollOffset(-100, 200), mojom::blink::ScrollType::kUser);
  UpdateAllLifecyclePhasesForTest();
  check_fragments();
}

TEST_P(PaintPropertyTreeBuilderTest, DocScrollingTraditional) {
  SetBodyInnerHTML("<style> body { height: 10000px; } </style>");

  GetDocument().domWindow()->scrollTo(0, 100);

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(DocPreTranslation()->IsIdentity());
  EXPECT_EQ(
      GetDocument().GetPage()->GetVisualViewport().GetScrollTranslationNode(),
      DocPreTranslation()->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -100),
            DocScrollTranslation()->Get2dTranslation());
  EXPECT_EQ(DocPreTranslation(), DocScrollTranslation()->Parent());
  EXPECT_EQ(DocPreTranslation(), &DocContentClip()->LocalTransformSpace());
  EXPECT_CLIP_RECT(FloatRoundedRect(0, 0, 800, 600), DocContentClip());
  EXPECT_TRUE(DocContentClip()->Parent()->IsRoot());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 784, 10000),
                          GetDocument().body()->GetLayoutObject(),
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, Perspective) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #perspective {
        position: absolute;
        left: 50px;
        top: 100px;
        width: 400px;
        height: 300px;
        perspective: 100px;
      }
      #inner {
        transform: translateZ(0);
        width: 100px;
        height: 200px;
      }
    </style>
    <div id='perspective'>
      <div id='inner'></div>
    </div>
  )HTML");
  Element* perspective =
      GetDocument().getElementById(AtomicString("perspective"));
  const ObjectPaintProperties* perspective_properties =
      perspective->GetLayoutObject()->FirstFragment().PaintProperties();
  gfx::Transform matrix;
  matrix.ApplyPerspectiveDepth(100);
  EXPECT_EQ(matrix, perspective_properties->Perspective()->Matrix());
  // The perspective origin is the center of the border box plus accumulated
  // paint offset.
  EXPECT_EQ(gfx::Point3F(250, 250, 0),
            perspective_properties->Perspective()->Origin());
  EXPECT_EQ(DocScrollTranslation(),
            perspective_properties->Perspective()->Parent());

  // Adding perspective doesn't clear paint offset. The paint offset will be
  // passed down to children.
  Element* inner = GetDocument().getElementById(AtomicString("inner"));
  const ObjectPaintProperties* inner_properties =
      inner->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(50, 100),
            inner_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_EQ(perspective_properties->Perspective(),
            inner_properties->PaintOffsetTranslation()->Parent());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(50, 100, 100, 200),
                          inner->GetLayoutObject(),
                          GetDocument().View()->GetLayoutView());

  perspective->setAttribute(html_names::kStyleAttr,
                            AtomicString("perspective: 200px"));
  UpdateAllLifecyclePhasesForTest();
  gfx::Transform matrix1;
  matrix1.ApplyPerspectiveDepth(200);
  EXPECT_EQ(matrix1, perspective_properties->Perspective()->Matrix());
  EXPECT_EQ(gfx::Point3F(250, 250, 0),
            perspective_properties->Perspective()->Origin());
  EXPECT_EQ(DocScrollTranslation(),
            perspective_properties->Perspective()->Parent());

  perspective->setAttribute(html_names::kStyleAttr,
                            AtomicString("perspective-origin: 5% 20%"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(matrix, perspective_properties->Perspective()->Matrix());
  EXPECT_EQ(gfx::Point3F(70, 160, 0),
            perspective_properties->Perspective()->Origin());
  EXPECT_EQ(DocScrollTranslation(),
            perspective_properties->Perspective()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, Transform) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0 } </style>
    <div id='transform' style='margin-left: 50px; margin-top: 100px;
        width: 400px; height: 300px;
        transform: translate3d(123px, 456px, 789px)'>
    </div>
  )HTML");

  Element* transform = GetDocument().getElementById(AtomicString("transform"));
  const ObjectPaintProperties* transform_properties =
      transform->GetLayoutObject()->FirstFragment().PaintProperties();

  EXPECT_EQ(MakeTranslationMatrix(123, 456, 789),
            transform_properties->Transform()->Matrix());
  EXPECT_EQ(gfx::Point3F(200, 150, 0),
            transform_properties->Transform()->Origin());
  EXPECT_EQ(transform_properties->PaintOffsetTranslation(),
            transform_properties->Transform()->Parent());
  EXPECT_EQ(gfx::Vector2dF(50, 100),
            transform_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            transform_properties->PaintOffsetTranslation()->Parent());
  EXPECT_TRUE(transform_properties->Transform()->HasDirectCompositingReasons());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(173, 556, 400, 300),
                          transform->GetLayoutObject(),
                          GetDocument().View()->GetLayoutView());

  transform->setAttribute(html_names::kStyleAttr,
                          AtomicString("margin-left: 50px; margin-top: 100px; "
                                       "width: 400px; height: 300px;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(nullptr,
            transform->GetLayoutObject()->FirstFragment().PaintProperties());

  transform->setAttribute(
      html_names::kStyleAttr,
      AtomicString(
          "margin-left: 50px; margin-top: 100px; width: 400px; height: 300px; "
          "transform: translate3d(123px, 456px, 789px)"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(MakeTranslationMatrix(123, 456, 789), transform->GetLayoutObject()
                                                      ->FirstFragment()
                                                      .PaintProperties()
                                                      ->Transform()
                                                      ->Matrix());
}

TEST_P(PaintPropertyTreeBuilderTest, Preserve3D3DTransformedDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0 } </style>
    <div id='preserve' style='transform-style: preserve-3d'>
    <div id='transform' style='margin-left: 50px; margin-top: 100px;
        width: 400px; height: 300px;
        transform: translate3d(123px, 456px, 789px)'>
    </div>
    </div>
  )HTML");

  Element* preserve = GetDocument().getElementById(AtomicString("preserve"));
  const ObjectPaintProperties* preserve_properties =
      preserve->GetLayoutObject()->FirstFragment().PaintProperties();

  EXPECT_TRUE(preserve_properties->Transform());
  EXPECT_TRUE(preserve_properties->Transform()->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest, Perspective3DTransformedDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0 } </style>
    <div id='perspective' style='perspective: 800px;'>
    <div id='transform' style='margin-left: 50px; margin-top: 100px;
        width: 400px; height: 300px;
        transform: translate3d(123px, 456px, 789px)'>
    </div>
    </div>
  )HTML");

  Element* perspective =
      GetDocument().getElementById(AtomicString("perspective"));
  const ObjectPaintProperties* perspective_properties =
      perspective->GetLayoutObject()->FirstFragment().PaintProperties();

  EXPECT_TRUE(perspective_properties->Transform());
  EXPECT_TRUE(
      perspective_properties->Transform()->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest,
       TransformPerspective3DTransformedDescendant) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0 } </style>
    <div id='perspective' style='transform: perspective(800px);'>
      <div id='transform' style='margin-left: 50px; margin-top: 100px;
          width: 400px; height: 300px;
          transform: translate3d(123px, 456px, 789px)'>
      </div>
    </div>
  )HTML");

  Element* perspective =
      GetDocument().getElementById(AtomicString("perspective"));
  const ObjectPaintProperties* perspective_properties =
      perspective->GetLayoutObject()->FirstFragment().PaintProperties();

  EXPECT_TRUE(perspective_properties->Transform());
  EXPECT_TRUE(
      perspective_properties->Transform()->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest,
       TransformNodeWithActiveAnimationHasDirectCompositingReason) {
  LoadTestData("transform-animation.html");
  EXPECT_TRUE(PaintPropertiesForElement("target")
                  ->Transform()
                  ->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest,
       TransformAnimationDoesNotCreateEffectAndFilterNodes) {
  LoadTestData("transform-animation.html");
  EXPECT_NE(nullptr, PaintPropertiesForElement("target")->Transform());
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target")->Effect());
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target")->Filter());
}

TEST_P(PaintPropertyTreeBuilderTest,
       OpacityAnimationDoesNotCreateTransformAndFilterNodes) {
  LoadTestData("opacity-animation.html");
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target")->Transform());
  EXPECT_NE(nullptr, PaintPropertiesForElement("target")->Effect());
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target")->Filter());
}

TEST_P(PaintPropertyTreeBuilderTest,
       EffectNodeWithActiveAnimationHasDirectCompositingReason) {
  LoadTestData("opacity-animation.html");
  EXPECT_TRUE(PaintPropertiesForElement("target")
                  ->Effect()
                  ->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest, WillChangeTransform) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0 } </style>
    <div id='transform' style='margin-left: 50px; margin-top: 100px;
        width: 400px; height: 300px;
        will-change: transform'>
    </div>
  )HTML");

  Element* transform = GetDocument().getElementById(AtomicString("transform"));
  const ObjectPaintProperties* transform_properties =
      transform->GetLayoutObject()->FirstFragment().PaintProperties();

  EXPECT_TRUE(transform_properties->Transform()->IsIdentity());
  EXPECT_EQ(gfx::Vector2dF(),
            transform_properties->Transform()->Get2dTranslation());
  EXPECT_EQ(gfx::Point3F(), transform_properties->Transform()->Origin());
  EXPECT_EQ(gfx::Vector2dF(50, 100),
            transform_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_TRUE(transform_properties->Transform()->HasDirectCompositingReasons());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(50, 100, 400, 300),
                          transform->GetLayoutObject(),
                          GetDocument().View()->GetLayoutView());

  transform->setAttribute(html_names::kStyleAttr,
                          AtomicString("margin-left: 50px; margin-top: 100px; "
                                       "width: 400px; height: 300px;"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(nullptr,
            transform->GetLayoutObject()->FirstFragment().PaintProperties());

  transform->setAttribute(
      html_names::kStyleAttr,
      AtomicString(
          "margin-left: 50px; margin-top: 100px; width: 400px; height: 300px; "
          "will-change: transform"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(transform->GetLayoutObject()
                  ->FirstFragment()
                  .PaintProperties()
                  ->Transform()
                  ->IsIdentity());
}

TEST_P(PaintPropertyTreeBuilderTest, WillChangeContents) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0 } </style>
    <div id='transform' style='margin-left: 50px; margin-top: 100px;
        width: 400px; height: 300px;
        will-change: transform, contents'>
    </div>
  )HTML");

  Element* transform = GetDocument().getElementById(AtomicString("transform"));
  EXPECT_EQ(nullptr,
            transform->GetLayoutObject()->FirstFragment().PaintProperties());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(50, 100, 400, 300),
                          transform->GetLayoutObject(),
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest,
       BackfaceVisibilityWithPseudoStacking3DChildren) {
  ScopedBackfaceVisibilityInteropForTest bfi_enabled(true);
  SetBodyInnerHTML(R"HTML(
    <div style="backface-visibility: hidden; transform-style: preserve-3d">
      <div id=child style="isolation: isolate"></div>
    </div>
  )HTML");

  // The child needs a transform node to communicate that it is backface
  // visible to the compositor.
  EXPECT_NE(nullptr, PaintPropertiesForElement("child")->Transform());
  EXPECT_EQ(PaintPropertiesForElement("child")
                ->Transform()
                ->GetBackfaceVisibilityForTesting(),
            TransformPaintPropertyNode::BackfaceVisibility::kVisible);
}

TEST_P(PaintPropertyTreeBuilderTest, NoEffectAndFilterForNonStackingContext) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="will-change: right; backface-visibility: hidden">
    </div>
  )HTML");
  EXPECT_NE(nullptr, PaintPropertiesForElement("target")->Transform());
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target")->Effect());
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target")->Filter());
}

TEST_P(PaintPropertyTreeBuilderTest, RelativePositionInline) {
  LoadTestData("relative-position-inline.html");

  Element* inline_block =
      GetDocument().getElementById(AtomicString("inline-block"));
  const ObjectPaintProperties* inline_block_properties =
      inline_block->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(
      gfx::Vector2dF(135, 490),
      inline_block_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            inline_block_properties->PaintOffsetTranslation()->Parent());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(135, 490, 10, 20),
                          inline_block->GetLayoutObject(),
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, NestedOpacityEffect) {
  SetBodyInnerHTML(R"HTML(
    <div id='nodeWithoutOpacity' style='width: 100px; height: 200px'>
      <div id='childWithOpacity'
          style='opacity: 0.5; width: 50px; height: 60px;'>
        <div id='grandChildWithoutOpacity'
            style='width: 20px; height: 30px'>
          <div id='greatGrandChildWithOpacity'
              style='opacity: 0.2; width: 10px; height: 15px'></div>
        </div>
      </div>
    </div>
  )HTML");

  LayoutObject* node_without_opacity =
      GetLayoutObjectByElementId("nodeWithoutOpacity");
  const auto* data_without_opacity_properties =
      node_without_opacity->FirstFragment().PaintProperties();
  EXPECT_EQ(nullptr, data_without_opacity_properties);
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 100, 200), node_without_opacity,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* child_with_opacity =
      GetLayoutObjectByElementId("childWithOpacity");
  const ObjectPaintProperties* child_with_opacity_properties =
      child_with_opacity->FirstFragment().PaintProperties();
  EXPECT_EQ(0.5f, child_with_opacity_properties->Effect()->Opacity());
  // childWithOpacity is the root effect node.
  EXPECT_NE(nullptr, child_with_opacity_properties->Effect()->Parent());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 50, 60), child_with_opacity,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* grand_child_without_opacity =
      GetDocument()
          .getElementById(AtomicString("grandChildWithoutOpacity"))
          ->GetLayoutObject();
  EXPECT_EQ(nullptr,
            grand_child_without_opacity->FirstFragment().PaintProperties());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 20, 30),
                          grand_child_without_opacity,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* great_grand_child_with_opacity =
      GetDocument()
          .getElementById(AtomicString("greatGrandChildWithOpacity"))
          ->GetLayoutObject();
  const ObjectPaintProperties* great_grand_child_with_opacity_properties =
      great_grand_child_with_opacity->FirstFragment().PaintProperties();
  EXPECT_EQ(0.2f,
            great_grand_child_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(child_with_opacity_properties->Effect(),
            great_grand_child_with_opacity_properties->Effect()->Parent());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 10, 15),
                          great_grand_child_with_opacity,
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformNodeDoesNotAffectEffectNodes) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #nodeWithOpacity {
        opacity: 0.6;
        width: 100px;
        height: 200px;
      }
      #childWithTransform {
        transform: translate3d(10px, 10px, 0px);
        width: 50px;
        height: 60px;
      }
      #grandChildWithOpacity {
        opacity: 0.4;
        width: 20px;
        height: 30px;
      }
    </style>
    <div id='nodeWithOpacity'>
      <div id='childWithTransform'>
        <div id='grandChildWithOpacity'></div>
      </div>
    </div>
  )HTML");

  LayoutObject* node_with_opacity =
      GetLayoutObjectByElementId("nodeWithOpacity");
  const ObjectPaintProperties* node_with_opacity_properties =
      node_with_opacity->FirstFragment().PaintProperties();
  EXPECT_EQ(0.6f, node_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(DocContentClip(),
            node_with_opacity_properties->Effect()->OutputClip());
  EXPECT_NE(nullptr, node_with_opacity_properties->Effect()->Parent());
  EXPECT_EQ(nullptr, node_with_opacity_properties->Transform());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 100, 200), node_with_opacity,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* child_with_transform =
      GetLayoutObjectByElementId("childWithTransform");
  const ObjectPaintProperties* child_with_transform_properties =
      child_with_transform->FirstFragment().PaintProperties();
  EXPECT_EQ(nullptr, child_with_transform_properties->Effect());
  EXPECT_EQ(gfx::Vector2dF(10, 10),
            child_with_transform_properties->Transform()->Get2dTranslation());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(18, 18, 50, 60), child_with_transform,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* grand_child_with_opacity =
      GetLayoutObjectByElementId("grandChildWithOpacity");
  const ObjectPaintProperties* grand_child_with_opacity_properties =
      grand_child_with_opacity->FirstFragment().PaintProperties();
  EXPECT_EQ(0.4f, grand_child_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(DocContentClip(),
            grand_child_with_opacity_properties->Effect()->OutputClip());
  EXPECT_EQ(node_with_opacity_properties->Effect(),
            grand_child_with_opacity_properties->Effect()->Parent());
  EXPECT_EQ(nullptr, grand_child_with_opacity_properties->Transform());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(18, 18, 20, 30),
                          grand_child_with_opacity,
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectNodesAcrossStackingContext) {
  SetBodyInnerHTML(R"HTML(
    <div id='nodeWithOpacity'
        style='opacity: 0.6; width: 100px; height: 200px'>
      <div id='childWithStackingContext'
          style='position:absolute; width: 50px; height: 60px;'>
        <div id='grandChildWithOpacity'
            style='opacity: 0.4; width: 20px; height: 30px'></div>
      </div>
    </div>
  )HTML");

  LayoutObject* node_with_opacity =
      GetLayoutObjectByElementId("nodeWithOpacity");
  const ObjectPaintProperties* node_with_opacity_properties =
      node_with_opacity->FirstFragment().PaintProperties();
  EXPECT_EQ(0.6f, node_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(DocContentClip(),
            node_with_opacity_properties->Effect()->OutputClip());
  EXPECT_NE(nullptr, node_with_opacity_properties->Effect()->Parent());
  EXPECT_EQ(nullptr, node_with_opacity_properties->Transform());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 100, 200), node_with_opacity,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* child_with_stacking_context =
      GetDocument()
          .getElementById(AtomicString("childWithStackingContext"))
          ->GetLayoutObject();
  const ObjectPaintProperties* child_with_stacking_context_properties =
      child_with_stacking_context->FirstFragment().PaintProperties();
  EXPECT_EQ(nullptr, child_with_stacking_context_properties);
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 50, 60),
                          child_with_stacking_context,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* grand_child_with_opacity =
      GetLayoutObjectByElementId("grandChildWithOpacity");
  const ObjectPaintProperties* grand_child_with_opacity_properties =
      grand_child_with_opacity->FirstFragment().PaintProperties();
  EXPECT_EQ(0.4f, grand_child_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(DocContentClip(),
            grand_child_with_opacity_properties->Effect()->OutputClip());
  EXPECT_EQ(node_with_opacity_properties->Effect(),
            grand_child_with_opacity_properties->Effect()->Parent());
  EXPECT_EQ(nullptr, grand_child_with_opacity_properties->Transform());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 20, 30), grand_child_with_opacity,
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectNodesInSVG) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svgRoot'>
      <g id='groupWithOpacity' opacity='0.6'>
        <rect id='rectWithoutOpacity' />
        <rect id='rectWithOpacity' opacity='0.4' />
        <text id='textWithOpacity' opacity='0.2'>
          <tspan id='tspanWithOpacity' opacity='0.1' />
        </text>
      </g>
    </svg>
  )HTML");

  const auto* svg_clip = PaintPropertiesForElement("svgRoot")->OverflowClip();

  const auto* group_with_opacity_properties =
      PaintPropertiesForElement("groupWithOpacity");
  EXPECT_EQ(0.6f, group_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(svg_clip, group_with_opacity_properties->Effect()->OutputClip());
  EXPECT_EQ(&EffectPaintPropertyNode::Root(),
            group_with_opacity_properties->Effect()->Parent());

  EXPECT_EQ(nullptr, PaintPropertiesForElement("rectWithoutOpacity"));

  const auto* rect_with_opacity_properties =
      PaintPropertiesForElement("rectWithOpacity");
  EXPECT_EQ(0.4f, rect_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(svg_clip, rect_with_opacity_properties->Effect()->OutputClip());
  EXPECT_EQ(group_with_opacity_properties->Effect(),
            rect_with_opacity_properties->Effect()->Parent());

  // Ensure that opacity nodes are created for LayoutSVGText which inherits from
  // LayoutSVGBlock instead of LayoutSVGModelObject.
  const auto* text_with_opacity_properties =
      PaintPropertiesForElement("textWithOpacity");
  EXPECT_EQ(0.2f, text_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(svg_clip, text_with_opacity_properties->Effect()->OutputClip());
  EXPECT_EQ(group_with_opacity_properties->Effect(),
            text_with_opacity_properties->Effect()->Parent());

  // Ensure that opacity nodes are created for LayoutSVGTSpan which inherits
  // from LayoutSVGInline instead of LayoutSVGModelObject.
  const auto* tspan_with_opacity_properties =
      PaintPropertiesForElement("tspanWithOpacity");
  EXPECT_EQ(0.1f, tspan_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(svg_clip, tspan_with_opacity_properties->Effect()->OutputClip());
  EXPECT_EQ(text_with_opacity_properties->Effect(),
            tspan_with_opacity_properties->Effect()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectNodesAcrossHTMLSVGBoundary) {
  SetBodyInnerHTML(R"HTML(
    <div id='divWithOpacity' style='opacity: 0.2;'>
      <svg id='svgRootWithOpacity' style='opacity: 0.3;'>
        <rect id='rectWithOpacity' opacity='0.4' />
      </svg>
    </div>
  )HTML");

  const auto* div_with_opacity_properties =
      PaintPropertiesForElement("divWithOpacity");
  EXPECT_EQ(0.2f, div_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(DocContentClip(),
            div_with_opacity_properties->Effect()->OutputClip());
  EXPECT_NE(nullptr, div_with_opacity_properties->Effect()->Parent());

  const auto* svg_root_with_opacity_properties =
      PaintPropertiesForElement("svgRootWithOpacity");
  EXPECT_EQ(0.3f, svg_root_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(DocContentClip(),
            svg_root_with_opacity_properties->Effect()->OutputClip());
  EXPECT_EQ(div_with_opacity_properties->Effect(),
            svg_root_with_opacity_properties->Effect()->Parent());

  const auto* rect_with_opacity_properties =
      PaintPropertiesForElement("rectWithOpacity");
  EXPECT_EQ(0.4f, rect_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(svg_root_with_opacity_properties->OverflowClip(),
            rect_with_opacity_properties->Effect()->OutputClip());
  EXPECT_EQ(svg_root_with_opacity_properties->Effect(),
            rect_with_opacity_properties->Effect()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectNodesAcrossSVGHTMLBoundary) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svgRootWithOpacity' style='opacity: 0.3;'>
      <foreignObject id='foreignObjectWithOpacity' opacity='0.4' style='overflow: visible;'>
        <body>
          <span id='spanWithOpacity' style='opacity: 0.5'/>
        </body>
      </foreignObject>
    </svg>
  )HTML");

  const auto* svg_root_with_opacity_properties =
      PaintPropertiesForElement("svgRootWithOpacity");
  EXPECT_EQ(0.3f, svg_root_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(DocContentClip(),
            svg_root_with_opacity_properties->Effect()->OutputClip());
  EXPECT_NE(nullptr, svg_root_with_opacity_properties->Effect()->Parent());

  const auto* foreign_object_with_opacity_properties =
      PaintPropertiesForElement("foreignObjectWithOpacity");
  EXPECT_EQ(0.4f, foreign_object_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(svg_root_with_opacity_properties->OverflowClip(),
            foreign_object_with_opacity_properties->Effect()->OutputClip());
  EXPECT_EQ(svg_root_with_opacity_properties->Effect(),
            foreign_object_with_opacity_properties->Effect()->Parent());

  const auto* span_with_opacity_properties =
      PaintPropertiesForElement("spanWithOpacity");
  EXPECT_EQ(0.5f, span_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(svg_root_with_opacity_properties->OverflowClip(),
            span_with_opacity_properties->Effect()->OutputClip());
  EXPECT_EQ(foreign_object_with_opacity_properties->Effect(),
            span_with_opacity_properties->Effect()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformNodesInSVG) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0px;
      }
      svg {
        margin-left: 50px;
        transform: translate3d(1px, 2px, 3px);
        position: absolute;
        left: 20px;
        top: 25px;
      }
      rect {
        transform: translate(100px, 100px) rotate(45deg);
        transform-origin: 50px 25px;
      }
    </style>
    <svg id='svgRootWith3dTransform' width='100px' height='100px'>
      <rect id='rectWith2dTransform' width='100px' height='100px' />
    </svg>
  )HTML");

  LayoutObject& svg_root_with3d_transform =
      *GetDocument()
           .getElementById(AtomicString("svgRootWith3dTransform"))
           ->GetLayoutObject();
  const ObjectPaintProperties* svg_root_with3d_transform_properties =
      svg_root_with3d_transform.FirstFragment().PaintProperties();
  EXPECT_EQ(MakeTranslationMatrix(1, 2, 3),
            svg_root_with3d_transform_properties->Transform()->Matrix());
  EXPECT_EQ(gfx::Point3F(50, 50, 0),
            svg_root_with3d_transform_properties->Transform()->Origin());
  EXPECT_EQ(svg_root_with3d_transform_properties->PaintOffsetTranslation(),
            svg_root_with3d_transform_properties->Transform()->Parent());
  EXPECT_EQ(gfx::Vector2dF(70, 25),
            svg_root_with3d_transform_properties->PaintOffsetTranslation()
                ->Get2dTranslation());
  EXPECT_EQ(
      DocScrollTranslation(),
      svg_root_with3d_transform_properties->PaintOffsetTranslation()->Parent());

  LayoutObject& rect_with2d_transform =
      *GetLayoutObjectByElementId("rectWith2dTransform");
  const ObjectPaintProperties* rect_with2d_transform_properties =
      rect_with2d_transform.FirstFragment().PaintProperties();
  gfx::Transform matrix;
  matrix.Translate(100, 100);
  matrix.Rotate(45);
  // SVG's transform origin is baked into the transform.
  matrix.ApplyTransformOrigin(50, 25, 0);
  EXPECT_EQ(matrix, rect_with2d_transform_properties->Transform()->Matrix());
  EXPECT_EQ(gfx::Point3F(0, 0, 0),
            rect_with2d_transform_properties->Transform()->Origin());
  // SVG does not use paint offset.
  EXPECT_EQ(nullptr,
            rect_with2d_transform_properties->PaintOffsetTranslation());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGViewBoxTransform) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0px;
      }
      #svgWithViewBox {
        transform: translate3d(1px, 2px, 3px);
        position: absolute;
        width: 100px;
        height: 100px;
      }
      #rect {
        transform: translate(100px, 100px);
        width: 100px;
        height: 100px;
      }
    </style>
    <svg id='svgWithViewBox' viewBox='50 50 100 100'>
      <rect id='rect' />
    </svg>
  )HTML");

  LayoutObject& svg_with_view_box =
      *GetLayoutObjectByElementId("svgWithViewBox");
  const ObjectPaintProperties* svg_with_view_box_properties =
      svg_with_view_box.FirstFragment().PaintProperties();
  EXPECT_EQ(MakeTranslationMatrix(1, 2, 3),
            svg_with_view_box_properties->Transform()->Matrix());
  EXPECT_EQ(gfx::Vector2dF(-50, -50),
            svg_with_view_box_properties->ReplacedContentTransform()
                ->Get2dTranslation());
  EXPECT_EQ(svg_with_view_box_properties->ReplacedContentTransform()->Parent(),
            svg_with_view_box_properties->Transform());

  LayoutObject& rect = *GetLayoutObjectByElementId("rect");
  const ObjectPaintProperties* rect_properties =
      rect.FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(100, 100),
            rect_properties->Transform()->Get2dTranslation());
  EXPECT_EQ(svg_with_view_box_properties->ReplacedContentTransform(),
            rect_properties->Transform()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootPaintOffsetTransformNode) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0px; }
      #svg {
        margin-left: 50px;
        margin-top: 25px;
        width: 100px;
        height: 100px;
      }
    </style>
    <svg id='svg' />
  )HTML");

  LayoutObject& svg = *GetLayoutObjectByElementId("svg");
  const ObjectPaintProperties* svg_properties =
      svg.FirstFragment().PaintProperties();
  EXPECT_TRUE(svg_properties->PaintOffsetTranslation());
  EXPECT_EQ(gfx::Vector2dF(50, 25),
            svg_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_EQ(nullptr, svg_properties->ReplacedContentTransform());
  EXPECT_EQ(DocScrollTranslation(),
            svg_properties->PaintOffsetTranslation()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootLocalToBorderBoxTransformNode) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0px; }
      svg {
        margin-left: 2px;
        margin-top: 3px;
        transform: translate(5px, 7px);
        border: 11px solid green;
      }
    </style>
    <svg id='svg' width='100px' height='100px' viewBox='0 0 13 13'>
      <rect id='rect' transform='translate(17 19)' />
    </svg>
  )HTML");

  LayoutObject& svg = *GetLayoutObjectByElementId("svg");
  const ObjectPaintProperties* svg_properties =
      svg.FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(2, 3),
            svg_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_EQ(gfx::Vector2dF(5, 7),
            svg_properties->Transform()->Get2dTranslation());
  auto matrix = MakeTranslationMatrix(11, 11);
  matrix.Scale(100.0 / 13.0);
  EXPECT_TRANSFORM_EQ(matrix,
                      svg_properties->ReplacedContentTransform()->Matrix());
  EXPECT_EQ(svg_properties->PaintOffsetTranslation(),
            svg_properties->Transform()->Parent());
  EXPECT_EQ(svg_properties->Transform(),
            svg_properties->ReplacedContentTransform()->Parent());

  // Ensure the rect's transform is a child of the local to border box
  // transform.
  LayoutObject& rect = *GetLayoutObjectByElementId("rect");
  const ObjectPaintProperties* rect_properties =
      rect.FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(17, 19),
            rect_properties->Transform()->Get2dTranslation());
  EXPECT_EQ(svg_properties->ReplacedContentTransform(),
            rect_properties->Transform()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGNestedViewboxTransforms) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0px; } </style>
    <svg id='svg' width='100px' height='100px' viewBox='0 0 50 50'
        style='transform: translate(11px, 11px);'>
      <svg id='nestedSvg' width='50px' height='50px' viewBox='0 0 5 5'>
        <rect id='rect' transform='translate(13 13)' />
      </svg>
    </svg>
  )HTML");

  LayoutObject& svg = *GetLayoutObjectByElementId("svg");
  const ObjectPaintProperties* svg_properties =
      svg.FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(11, 11),
            svg_properties->Transform()->Get2dTranslation());
  EXPECT_EQ(MakeScaleMatrix(2),
            svg_properties->ReplacedContentTransform()->Matrix());

  LayoutObject& nested_svg = *GetLayoutObjectByElementId("nestedSvg");
  const ObjectPaintProperties* nested_svg_properties =
      nested_svg.FirstFragment().PaintProperties();
  EXPECT_EQ(MakeScaleMatrix(10), nested_svg_properties->Transform()->Matrix());
  EXPECT_EQ(nullptr, nested_svg_properties->ReplacedContentTransform());
  EXPECT_EQ(svg_properties->ReplacedContentTransform(),
            nested_svg_properties->Transform()->Parent());

  LayoutObject& rect = *GetLayoutObjectByElementId("rect");
  const ObjectPaintProperties* rect_properties =
      rect.FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(13, 13),
            rect_properties->Transform()->Get2dTranslation());
  EXPECT_EQ(nested_svg_properties->Transform(),
            rect_properties->Transform()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformNodesAcrossSVGHTMLBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0px; } </style>
    <svg id='svgWithTransform'
        style='transform: translate3d(1px, 2px, 3px);'>
      <foreignObject>
        <body>
          <div id='divWithTransform'
              style='transform: translate3d(3px, 4px, 5px);'></div>
        </body>
      </foreignObject>
    </svg>
  )HTML");

  LayoutObject& svg_with_transform =
      *GetLayoutObjectByElementId("svgWithTransform");
  const ObjectPaintProperties* svg_with_transform_properties =
      svg_with_transform.FirstFragment().PaintProperties();
  EXPECT_EQ(MakeTranslationMatrix(1, 2, 3),
            svg_with_transform_properties->Transform()->Matrix());

  LayoutObject& div_with_transform =
      *GetLayoutObjectByElementId("divWithTransform");
  const ObjectPaintProperties* div_with_transform_properties =
      div_with_transform.FirstFragment().PaintProperties();
  EXPECT_EQ(MakeTranslationMatrix(3, 4, 5),
            div_with_transform_properties->Transform()->Matrix());
  // Ensure the div's transform node is a child of the svg's transform node.
  EXPECT_EQ(svg_with_transform_properties->Transform(),
            div_with_transform_properties->Transform()->Parent()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, ForeignObjectWithTransformAndOffset) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0px; } </style>
    <svg id='svgWithTransform'>
      <foreignObject id="foreignObject"
          x="10" y="10" width="50" height="40" transform="scale(5)">
        <div id='div'></div>
      </foreignObject>
    </svg>
  )HTML");

  LayoutObject& foreign_object = *GetLayoutObjectByElementId("foreignObject");
  const ObjectPaintProperties* foreign_object_properties =
      foreign_object.FirstFragment().PaintProperties();
  EXPECT_EQ(MakeScaleMatrix(5),
            foreign_object_properties->Transform()->Matrix());
  EXPECT_EQ(PhysicalOffset(10, 10),
            foreign_object.FirstFragment().PaintOffset());
  EXPECT_EQ(nullptr, foreign_object_properties->PaintOffsetTranslation());

  LayoutObject& div = *GetLayoutObjectByElementId("div");
  EXPECT_EQ(PhysicalOffset(10, 10), div.FirstFragment().PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest, ForeignObjectWithMask) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0px; } </style>
    <svg id='svg' style='position; relative'>
      <foreignObject id="foreignObject"
          x="10" y="10" width="50" height="40"
          style="-webkit-mask:linear-gradient(red,red)">
        <div id='div'></div>
      </foreignObject>
    </svg>
  )HTML");

  LayoutObject& svg = *GetLayoutObjectByElementId("svg");
  LayoutObject& foreign_object = *GetLayoutObjectByElementId("foreignObject");
  const ObjectPaintProperties* foreign_object_properties =
      foreign_object.FirstFragment().PaintProperties();
  EXPECT_TRUE(foreign_object_properties->Mask());
  EXPECT_EQ(foreign_object_properties->MaskClip()->Parent(),
            foreign_object_properties->Mask()->OutputClip());
  EXPECT_EQ(&svg.FirstFragment().LocalBorderBoxProperties().Transform(),
            &foreign_object_properties->Mask()->LocalTransformSpace());
}

TEST_P(PaintPropertyTreeBuilderTest, PaintOffsetTranslationSVGHTMLBoundary) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg'
      <foreignObject>
        <body>
          <div id='divWithTransform'
              style='transform: translate3d(3px, 4px, 5px);'></div>
        </body>
      </foreignObject>
    </svg>
  )HTML");

  LayoutObject& svg = *GetLayoutObjectByElementId("svg");
  const ObjectPaintProperties* svg_properties =
      svg.FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(8, 8),
            svg_properties->PaintOffsetTranslation()->Get2dTranslation());

  LayoutObject& div_with_transform =
      *GetLayoutObjectByElementId("divWithTransform");
  const ObjectPaintProperties* div_with_transform_properties =
      div_with_transform.FirstFragment().PaintProperties();
  EXPECT_EQ(MakeTranslationMatrix(3, 4, 5),
            div_with_transform_properties->Transform()->Matrix());
  EXPECT_EQ(gfx::Vector2dF(8, 158),
            div_with_transform_properties->PaintOffsetTranslation()
                ->Get2dTranslation());
  EXPECT_EQ(div_with_transform_properties->PaintOffsetTranslation(),
            div_with_transform_properties->Transform()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGViewportContainer) {
  SetBodyInnerHTML(R"HTML(
    <!-- border radius of inner svg elemnents should be ignored. -->
    <style>svg { border-radius: 10px }</style>
    <svg id='svg'>
      <svg id='container1' width='30' height='30'></svg>
      <svg id='container2'
          width='30' height='30' x='40' y='50' viewBox='0 0 60 60'></svg>
      <svg id='container3' overflow='visible' width='30' height='30'></svg>
      <svg id='container4' overflow='visible'
          width='30' height='30' x='20' y='30'></svg>
    </svg>
  )HTML");

  const auto* svg_properties = PaintPropertiesForElement("svg");
  ASSERT_NE(nullptr, svg_properties);
  const auto* parent_transform = svg_properties->PaintOffsetTranslation();
  const auto* parent_clip = svg_properties->OverflowClip();

  // overflow: hidden and zero offset: OverflowClip only.
  const auto* properties1 = PaintPropertiesForElement("container1");
  ASSERT_NE(nullptr, properties1);
  const auto* clip = properties1->OverflowClip();
  const auto* transform = properties1->Transform();
  ASSERT_NE(nullptr, clip);
  EXPECT_EQ(nullptr, transform);
  EXPECT_EQ(parent_clip, clip->Parent());
  EXPECT_CLIP_RECT(gfx::RectF(0, 0, 30, 30), clip);
  EXPECT_EQ(parent_transform, &clip->LocalTransformSpace());

  // overflow: hidden and non-zero offset and viewport scale:
  // both Transform and OverflowClip.
  const auto* properties2 = PaintPropertiesForElement("container2");
  ASSERT_NE(nullptr, properties2);
  clip = properties2->OverflowClip();
  transform = properties2->Transform();
  ASSERT_NE(nullptr, clip);
  ASSERT_NE(nullptr, transform);
  EXPECT_EQ(parent_clip, clip->Parent());
  EXPECT_CLIP_RECT(gfx::RectF(0, 0, 60, 60), clip);
  EXPECT_EQ(transform, &clip->LocalTransformSpace());
  auto matrix = MakeTranslationMatrix(40, 50);
  matrix.Scale(0.5);
  EXPECT_EQ(matrix, transform->Matrix());
  EXPECT_EQ(parent_transform, transform->Parent());

  // overflow: visible and zero offset: no paint properties.
  const auto* properties3 = PaintPropertiesForElement("container3");
  EXPECT_EQ(nullptr, properties3);

  // overflow: visible and non-zero offset: Transform only.
  const auto* properties4 = PaintPropertiesForElement("container4");
  ASSERT_NE(nullptr, properties4);
  clip = properties4->OverflowClip();
  transform = properties4->Transform();
  EXPECT_EQ(nullptr, clip);
  ASSERT_NE(nullptr, transform);
  EXPECT_EQ(gfx::Vector2dF(20, 30), transform->Get2dTranslation());
  EXPECT_EQ(parent_transform, transform->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGForeignObjectOverflowClip) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg'>
      <foreignObject id='object1' x='10' y='20' width='30' height='40'
          overflow='hidden'>
      </foreignObject>
      <foreignObject id='object2' x='50' y='60' width='30' height='40'
          overflow='visible'>
      </foreignObject>
    </svg>
  )HTML");

  const auto* svg_properties = PaintPropertiesForElement("svg");
  ASSERT_NE(nullptr, svg_properties);
  const auto* parent_transform = svg_properties->PaintOffsetTranslation();
  const auto* parent_clip = svg_properties->OverflowClip();

  const auto* properties1 = PaintPropertiesForElement("object1");
  ASSERT_NE(nullptr, properties1);
  const auto* clip = properties1->OverflowClip();
  ASSERT_NE(nullptr, clip);
  EXPECT_EQ(parent_clip, clip->Parent());
  EXPECT_CLIP_RECT(gfx::RectF(10, 20, 30, 40), clip);
  EXPECT_EQ(parent_transform, &clip->LocalTransformSpace());

  const auto* properties2 = PaintPropertiesForElement("object2");
  EXPECT_EQ(nullptr, properties2);
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowClipWithEmptyVisualOverflow) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      ::-webkit-scrollbar {
        width: 10px;
        height: 10px;
      }
    </style>
    <div id='container' style='width: 100px; height: 100px;
        will-change: transform; overflow: scroll; background: lightblue;'>
      <div id='forcescroll' style='width: 0; height: 400px;'></div>
    </div>
  )HTML");

  const auto* clip = PaintPropertiesForElement("container")->OverflowClip();
  EXPECT_NE(nullptr, clip);
  EXPECT_CLIP_RECT(gfx::RectF(0, 0, 90, 90), clip);
}

TEST_P(PaintPropertyTreeBuilderTest,
       PaintOffsetTranslationSVGHTMLBoundaryMulticol) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg'>
      <foreignObject>
        <body>
          <div id='divWithColumns' style='columns: 2'>
            <div style='width: 5px; height: 5px; background: blue'>
          </div>
        </body>
      </foreignObject>
    </svg>
  )HTML");

  LayoutObject& svg = *GetLayoutObjectByElementId("svg");
  const ObjectPaintProperties* svg_properties =
      svg.FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(8, 8),
            svg_properties->PaintOffsetTranslation()->Get2dTranslation());
  LayoutObject& div_with_columns =
      *GetLayoutObjectByElementId("divWithColumns")->SlowFirstChild();
  EXPECT_EQ(PhysicalOffset(), div_with_columns.FirstFragment().PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest,
       FixedTransformAncestorAcrossSVGHTMLBoundary) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0px; } </style>
    <svg id='svg' style='transform: translate3d(1px, 2px, 3px);'>
      <g id='container' transform='translate(20 30)'>
        <foreignObject>
          <body>
            <div id='fixed'
                style='position: fixed; left: 200px; top: 150px;'></div>
          </body>
        </foreignObject>
      </g>
    </svg>
  )HTML");

  LayoutObject& svg = *GetLayoutObjectByElementId("svg");
  const ObjectPaintProperties* svg_properties =
      svg.FirstFragment().PaintProperties();
  EXPECT_EQ(MakeTranslationMatrix(1, 2, 3),
            svg_properties->Transform()->Matrix());

  LayoutObject& container = *GetLayoutObjectByElementId("container");
  const ObjectPaintProperties* container_properties =
      container.FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(20, 30),
            container_properties->Transform()->Get2dTranslation());
  EXPECT_EQ(svg_properties->Transform(),
            container_properties->Transform()->Parent());

  Element* fixed = GetDocument().getElementById(AtomicString("fixed"));
  // Ensure the fixed position element is rooted at the nearest transform
  // container.
  EXPECT_EQ(container_properties->Transform(), &fixed->GetLayoutObject()
                                                    ->FirstFragment()
                                                    .LocalBorderBoxProperties()
                                                    .Transform());
}

TEST_P(PaintPropertyTreeBuilderTest, ControlClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0;
      }
      input {
        border-radius: 0;
        border-width: 5px;
        padding: 0;
      }
    </style>
    <input id='button' type='button'
        style='width:345px; height:123px' value='some text'/>
  )HTML");

  LayoutObject& button = *GetLayoutObjectByElementId("button");
  const ObjectPaintProperties* button_properties =
      button.FirstFragment().PaintProperties();
  // Always create scroll translation for layout view even the document does
  // not scroll (not enough content).
  EXPECT_TRUE(DocScrollTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            &button_properties->OverflowClip()->LocalTransformSpace());

  EXPECT_CLIP_RECT(FloatRoundedRect(5, 5, 335, 113),
                   button_properties->OverflowClip());
  EXPECT_EQ(DocContentClip(), button_properties->OverflowClip()->Parent());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 345, 123), &button,
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, ControlClipInsideForeignObject) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  SetBodyInnerHTML(R"HTML(
    <div style='column-count:2;'>
      <div style='columns: 2'>
        <svg style='width: 500px; height: 500px;'>
          <foreignObject style='overflow: visible;'>
            <input id='button' style='width:345px; height:123px'
                 value='some text'/>
          </foreignObject>
        </svg>
      </div>
    </div>
  )HTML");

  LayoutObject& button = *GetLayoutObjectByElementId("button");
  const ObjectPaintProperties* button_properties =
      button.FirstFragment().PaintProperties();
  // Always create scroll translation for layout view even the document does
  // not scroll (not enough content).
  EXPECT_TRUE(DocScrollTranslation());
  EXPECT_CLIP_RECT(FloatRoundedRect(2, 2, 341, 119),
                   button_properties->OverflowClip());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 345, 123), &button,
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, BorderRadiusClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
     body {
       margin: 0px;
     }
     #div {
       border-radius: 12px 34px 56px 78px;
       border-top: 45px solid;
       border-right: 50px solid;
       border-bottom: 55px solid;
       border-left: 60px solid;
       width: 500px;
       height: 400px;
       overflow: scroll;
     }
    </style>
    <div id='div'></div>
  )HTML");

  LayoutObject& div = *GetLayoutObjectByElementId("div");
  const ObjectPaintProperties* div_properties =
      div.FirstFragment().PaintProperties();

  // Always create scroll translation for layout view even the document does
  // not scroll (not enough content).
  EXPECT_TRUE(DocScrollTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            &div_properties->OverflowClip()->LocalTransformSpace());

  // The overflow clip rect includes only the padding box.
  // padding box = border box(500+60+50, 400+45+55) - border outset(60+50,
  // 45+55) - scrollbars(15, 15)
  EXPECT_CLIP_RECT(FloatRoundedRect(60, 45, 500, 400),
                   div_properties->OverflowClip());
  auto& border_radius_clip =
      ToUnaliased(*div_properties->OverflowClip()->Parent());
  EXPECT_EQ(DocScrollTranslation(), &border_radius_clip.LocalTransformSpace());

  // The border radius clip is the area enclosed by inner border edge, including
  // the scrollbars.  As the border-radius is specified in outer radius, the
  // inner radius is calculated by:
  //     inner radius = max(outer radius - border width, 0)
  // In the case that two adjacent borders have different width, the inner
  // radius of the corner may transition from one value to the other. i.e. being
  // an ellipse.
  // The following is border box(610, 500) - border outset(110, 100).
  gfx::RectF border_box_minus_border_outset(60, 45, 500, 400);
  EXPECT_CLIP_RECT(
      FloatRoundedRect(
          border_box_minus_border_outset,
          gfx::SizeF(),        // (top left) = max((12, 12) - (60, 45), (0, 0))
          gfx::SizeF(),        // (top right) = max((34, 34) - (50, 45), (0, 0))
          gfx::SizeF(18, 23),  // (bot left) = max((78, 78) - (60, 55), (0, 0))
          gfx::SizeF(6, 1)),   // (bot right) = max((56, 56) - (50, 55), (0, 0))
      &border_radius_clip);
  EXPECT_EQ(DocContentClip(), border_radius_clip.Parent());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 610, 500), &div,
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, SubpixelBorderRadiusClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
     body {
       margin: 0px;
     }
     #div {
       margin-top: 0.5px;
       width: 100px;
       height: 100px;
       overflow: hidden;
       border-radius: 50%;
     }
    </style>
    <div id='div'></div>
  )HTML");

  LayoutObject& div = *GetLayoutObjectByElementId("div");
  const ObjectPaintProperties* div_properties =
      div.FirstFragment().PaintProperties();

  const ClipPaintPropertyNode* border_radius_clip =
      div_properties->InnerBorderRadiusClip();
  FloatClipRect expected_layout_clip_rect(gfx::RectF(0, 0.5, 100, 100));
  expected_layout_clip_rect.SetHasRadius();
  EXPECT_EQ(expected_layout_clip_rect, border_radius_clip->LayoutClipRect());
  EXPECT_EQ(
      FloatRoundedRect(gfx::RectF(0, 1, 100, 100), FloatRoundedRect::Radii(50)),
      border_radius_clip->PaintClipRect());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformNodesAcrossSubframes) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #divWithTransform {
        transform: translate3d(1px, 2px, 3px);
      }
    </style>
    <div id='divWithTransform'>
      <iframe id='iframe' style='border: 7px solid black'></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      body { margin: 0; }
      #innerDivWithTransform {
        transform: translate3d(4px, 5px, 6px);
        width: 100px;
        height: 200px;
      }
    </style>
    <div id='innerDivWithTransform'></div>
  )HTML");

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhasesForTest();

  LayoutObject* div_with_transform =
      GetLayoutObjectByElementId("divWithTransform");
  const ObjectPaintProperties* div_with_transform_properties =
      div_with_transform->FirstFragment().PaintProperties();
  EXPECT_EQ(MakeTranslationMatrix(1, 2, 3),
            div_with_transform_properties->Transform()->Matrix());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(1, 2, 800, 164), div_with_transform,
                          frame_view->GetLayoutView());

  LayoutObject* inner_div_with_transform =
      ChildDocument()
          .getElementById(AtomicString("innerDivWithTransform"))
          ->GetLayoutObject();
  const ObjectPaintProperties* inner_div_with_transform_properties =
      inner_div_with_transform->FirstFragment().PaintProperties();
  auto* inner_div_transform = inner_div_with_transform_properties->Transform();
  EXPECT_EQ(MakeTranslationMatrix(4, 5, 6), inner_div_transform->Matrix());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(12, 14, 100, 145),
                          inner_div_with_transform,
                          frame_view->GetLayoutView());

  // Ensure that the inner div's transform is correctly rooted in the root
  // frame's transform tree.
  // This asserts that we have the following tree structure:
  // Transform transform=translation=1.000000,2.000000,3.000000
  //   PreTranslation transform=translation=7.000000,7.000000,0.000000
  //     PaintOffsetTranslation transform=Identity
  //       ScrollTranslation transform=translation=0.000000,0.000000,0.000000
  //         Transform transform=translation=4.000000,5.000000,6.000000
  auto* inner_document_scroll_translation =
      inner_div_transform->UnaliasedParent();
  EXPECT_TRUE(inner_document_scroll_translation->IsIdentity());
  auto* paint_offset_translation =
      inner_document_scroll_translation->UnaliasedParent();
  auto* iframe_pre_translation = paint_offset_translation->UnaliasedParent();
  EXPECT_TRUE(paint_offset_translation->IsIdentity());
  EXPECT_EQ(gfx::Vector2dF(7, 7), iframe_pre_translation->Get2dTranslation());
  EXPECT_EQ(div_with_transform_properties->Transform(),
            iframe_pre_translation->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, FramesEstablishIsolation) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      .transformed {
        transform: translateX(1px);
      }
      #parent {
        width: 100px;
        height: 100px;
        overflow: hidden;
      }
    </style>
    <div id='parent'>
      <iframe id='iframe'></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      body { margin: 0; }
      #child {
        transform: translateX(50px);
        width: 50px;
        height: 50px;
        overflow: hidden;
      }
    </style>
    <div id='child'></div>
  )HTML");

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhasesForTest();

  LayoutObject* frame = ChildFrame().View()->GetLayoutView();
  const auto& frame_contents_properties =
      frame->FirstFragment().ContentsProperties();

  LayoutObject* child =
      ChildDocument().getElementById(AtomicString("child"))->GetLayoutObject();
  const auto& child_local_border_box_properties =
      child->FirstFragment().LocalBorderBoxProperties();
  auto* child_properties =
      child->GetMutableForPainting().FirstFragment().PaintProperties();

  // From the frame content's properties, we have:
  //  - transform isolation node
  //    - paint offset translation
  //      - transform
  EXPECT_EQ(gfx::Vector2dF(50, 0),
            ToUnaliased(child_local_border_box_properties.Transform())
                .Get2dTranslation());
  EXPECT_EQ(child_local_border_box_properties.Transform().Parent(),
            child_properties->PaintOffsetTranslation());
  EXPECT_EQ(child_local_border_box_properties.Transform().Parent()->Parent(),
            &frame_contents_properties.Transform());
  // Verify it's a true isolation node (i.e. it has a parent and it is a parent
  // alias).
  EXPECT_TRUE(frame_contents_properties.Transform().Parent());
  EXPECT_TRUE(frame_contents_properties.Transform().IsParentAlias());

  // Do similar checks for clip and effect, although the child local border box
  // properties directly reference the alias, since they do not have their own
  // clip and effect.
  EXPECT_EQ(&child_local_border_box_properties.Clip(),
            &frame_contents_properties.Clip());
  EXPECT_TRUE(frame_contents_properties.Clip().Parent());
  EXPECT_TRUE(frame_contents_properties.Clip().IsParentAlias());

  EXPECT_EQ(&child_local_border_box_properties.Effect(),
            &frame_contents_properties.Effect());
  EXPECT_TRUE(frame_contents_properties.Effect().Parent());
  EXPECT_TRUE(frame_contents_properties.Effect().IsParentAlias());

// The following part of the code would cause a DCHECK, but we want to see if
// the pre-paint iteration doesn't touch child's state, due to isolation. Hence,
// this only runs if we don't have DCHECKs enabled.
#if !DCHECK_IS_ON()
  // Now clobber the child transform to something identifiable.
  TransformPaintPropertyNode::State state{{MakeTranslationMatrix(123, 321)}};
  child_properties->UpdateTransform(
      *child_local_border_box_properties.Transform().Parent(),
      std::move(state));
  // Verify that we clobbered it correctly.
  EXPECT_EQ(gfx::Vector2dF(123, 321),
            ToUnaliased(child_local_border_box_properties.Transform())
                .Get2dTranslation());

  // This causes a tree topology change which forces the subtree to be updated.
  // However, isolation stops this recursion.
  GetDocument()
      .getElementById(AtomicString("parent"))
      ->setAttribute(html_names::kClassAttr, AtomicString("transformed"));
  frame_view->UpdateAllLifecyclePhasesForTest();

  // Verify that our clobbered state is still clobbered.
  EXPECT_EQ(gfx::Vector2dF(123, 321),
            ToUnaliased(child_local_border_box_properties.Transform())
                .Get2dTranslation());
#endif  // !DCHECK_IS_ON()
}

TEST_P(PaintPropertyTreeBuilderTest, TransformNodesInTransformedSubframes) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #divWithTransform {
        transform: translate3d(1px, 2px, 3px);
      }
      iframe {
        transform: translate3d(4px, 5px, 6px);
        border: 42px solid;
        margin: 7px;
      }
    </style>
    <div id='divWithTransform'>
      <iframe></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      body { margin: 31px; }
      #transform {
        transform: translate3d(7px, 8px, 9px);
        width: 100px;
        height: 200px;
      }
    </style>
    <div id='transform'></div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhasesForTest();

  // Assert that we have the following tree structure:
  // ...
  //   Transform transform=translation=1.000000,2.000000,3.000000
  //     PaintOffsetTranslation transform=translation=7.000000,7.000000,0.000000
  //       Transform transform=translation=4.000000,5.000000,6.000000
  //         PreTranslation transform=translation=42.000000,42.000000,0.000000
  //           ScrollTranslation transform=translation=0.000000,0.000000,0.00000
  //             PaintOffsetTranslation transform=translation=31.00,31.00,0.00
  //               Transform transform=translation=7.000000,8.000000,9.000000

  LayoutObject* inner_div_with_transform =
      ChildDocument()
          .getElementById(AtomicString("transform"))
          ->GetLayoutObject();
  auto* inner_div_transform =
      inner_div_with_transform->FirstFragment().PaintProperties()->Transform();
  EXPECT_EQ(MakeTranslationMatrix(7, 8, 9), inner_div_transform->Matrix());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(92, 95, 100, 111),
                          inner_div_with_transform,
                          frame_view->GetLayoutView());

  auto* inner_document_paint_offset_translation =
      inner_div_transform->UnaliasedParent();
  EXPECT_EQ(gfx::Vector2dF(31, 31),
            inner_document_paint_offset_translation->Get2dTranslation());
  auto* inner_document_scroll_translation =
      inner_document_paint_offset_translation->UnaliasedParent();
  EXPECT_TRUE(inner_document_scroll_translation->IsIdentity());
  auto* iframe_pre_translation =
      inner_document_scroll_translation->UnaliasedParent();
  EXPECT_EQ(gfx::Vector2dF(42, 42), iframe_pre_translation->Get2dTranslation());
  auto* iframe_transform = iframe_pre_translation->UnaliasedParent();
  EXPECT_EQ(MakeTranslationMatrix(4, 5, 6), iframe_transform->Matrix());
  auto* iframe_paint_offset_translation = iframe_transform->UnaliasedParent();
  EXPECT_EQ(gfx::Vector2dF(7, 7),
            iframe_paint_offset_translation->Get2dTranslation());
  auto* div_with_transform_transform =
      iframe_paint_offset_translation->UnaliasedParent();
  EXPECT_EQ(MakeTranslationMatrix(1, 2, 3),
            div_with_transform_transform->Matrix());

  LayoutObject* div_with_transform =
      GetLayoutObjectByElementId("divWithTransform");
  EXPECT_EQ(div_with_transform_transform,
            div_with_transform->FirstFragment().PaintProperties()->Transform());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(1, 2, 800, 248), div_with_transform,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, TreeContextClipByNonStackingContext) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that is scrolled by a
  // containing block that is not one of the painting ancestors.
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      /* to prevent the mock overlay scrollbar from affecting compositing. */
      #scroller::-webkit-scrollbar { display: none; }
    </style>
    <div id='scroller' style='overflow:scroll; width:400px; height:300px;'>
      <div id='child'
          style='position:relative; width:100px; height: 200px;'></div>
      <div style='height:10000px;'></div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* scroller = GetLayoutObjectByElementId("scroller");
  const ObjectPaintProperties* scroller_properties =
      scroller->FirstFragment().PaintProperties();
  LayoutObject* child = GetLayoutObjectByElementId("child");

  EXPECT_EQ(scroller_properties->OverflowClip(),
            &child->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(scroller_properties->ScrollTranslation(),
            &child->FirstFragment().LocalBorderBoxProperties().Transform());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 400, 300), scroller,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 100, 200), child,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest,
       TreeContextUnclipFromParentStackingContext) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that has a scrolling
  // painting ancestor that is not its containing block (thus should not be
  // scrolled by it).

  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #scroller {
        overflow:scroll;
        opacity:0.5;
      }
      #child {
        position:absolute;
        left:0;
        top:0;
        width: 100px;
        height: 200px;
      }
    </style>
    <div id='scroller'>
      <div id='child'></div>
      <div id='forceScroll' style='height:10000px;'></div>
    </div>
  )HTML");

  auto& scroller = *GetLayoutObjectByElementId("scroller");
  const ObjectPaintProperties* scroller_properties =
      scroller.FirstFragment().PaintProperties();
  LayoutObject& child = *GetLayoutObjectByElementId("child");

  EXPECT_EQ(DocContentClip(),
            &child.FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(DocScrollTranslation(),
            &child.FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(scroller_properties->Effect(),
            &child.FirstFragment().LocalBorderBoxProperties().Effect());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 800, 10000), &scroller,
                          GetDocument().View()->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 100, 200), &child,
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, TableCellLayoutLocation) {
  // This test verifies that the border box space of a table cell is being
  // correctly computed.  Table cells have weird location adjustment in our
  // layout/paint implementation.
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0;
      }
      table {
        border-spacing: 0;
        margin: 20px;
        padding: 40px;
        border: 10px solid black;
      }
      td {
        width: 100px;
        height: 100px;
        padding: 0;
      }
      #target {
        position: relative;
        width: 100px;
        height: 100px;
      }
    </style>
    <table>
      <tr><td></td><td></td></tr>
      <tr><td></td><td><div id='target'></div></td></tr>
    </table>
  )HTML");

  LayoutObject& target = *GetLayoutObjectByElementId("target");
  EXPECT_EQ(PhysicalOffset(170, 170), target.FirstFragment().PaintOffset());
  EXPECT_EQ(DocScrollTranslation(),
            &target.FirstFragment().LocalBorderBoxProperties().Transform());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(170, 170, 100, 100), &target,
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, CSSClipFixedPositionDescendant) {
  // This test verifies that clip tree hierarchy being generated correctly for
  // the hard case such that a fixed position element getting clipped by an
  // absolute position CSS clip.
  SetBodyInnerHTML(R"HTML(
    <style>
      #clip {
        position: absolute;
        left: 123px;
        top: 456px;
        clip: rect(10px, 80px, 70px, 40px);
        width: 100px;
        height: 100px;
      }
      #fixed {
        position: fixed;
        left: 654px;
        top: 321px;
        width: 10px;
        height: 20px
      }
    </style>
    <div id='clip'><div id='fixed'></div></div>
  )HTML");
  PhysicalRect local_clip_rect(40, 10, 40, 60);
  PhysicalRect absolute_clip_rect = local_clip_rect;
  absolute_clip_rect.offset += PhysicalOffset(123, 456);

  LayoutObject& clip = *GetLayoutObjectByElementId("clip");
  const ObjectPaintProperties* clip_properties =
      clip.FirstFragment().PaintProperties();
  EXPECT_EQ(DocContentClip(), clip_properties->CssClip()->Parent());
  EXPECT_EQ(DocScrollTranslation(),
            &clip_properties->CssClip()->LocalTransformSpace());
  EXPECT_CLIP_RECT(gfx::RectF(absolute_clip_rect), clip_properties->CssClip());
  CHECK_VISUAL_RECT(absolute_clip_rect, &clip,
                    GetDocument().View()->GetLayoutView(),
                    // TODO(crbug.com/599939): mapToVisualRectInAncestorSpace()
                    // doesn't apply css clip on the object itself.
                    LayoutUnit::Max());

  LayoutObject* fixed = GetLayoutObjectByElementId("fixed");
  EXPECT_EQ(clip_properties->CssClip(),
            &fixed->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(fixed->FirstFragment().PaintProperties()->PaintOffsetTranslation(),
            &fixed->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(PhysicalOffset(0, 0), fixed->FirstFragment().PaintOffset());
  CHECK_VISUAL_RECT(PhysicalRect(), fixed,
                    GetDocument().View()->GetLayoutView(),
                    // TODO(crbug.com/599939): CSS clip of fixed-position
                    // descendants is broken in
                    // mapToVisualRectInAncestorSpace().
                    LayoutUnit::Max());
}

TEST_P(PaintPropertyTreeBuilderTest, CSSClipAbsPositionDescendant) {
  // This test verifies that clip tree hierarchy being generated correctly for
  // the hard case such that a fixed position element getting clipped by an
  // absolute position CSS clip.
  SetBodyInnerHTML(R"HTML(
    <style>
      #clip {
        position: absolute;
        left: 123px;
        top: 456px;
        clip: rect(10px, 80px, 70px, 40px);
        width: 100px;
        height: 100px;
      }
      #absolute {
        position: absolute;
        left: 654px;
        top: 321px;
        width: 10px;
        heght: 20px
      }
    </style>
    <div id='clip'><div id='absolute'></div></div>
  )HTML");

  PhysicalRect local_clip_rect(40, 10, 40, 60);
  PhysicalRect absolute_clip_rect = local_clip_rect;
  absolute_clip_rect.offset += PhysicalOffset(123, 456);

  auto* clip = GetLayoutObjectByElementId("clip");
  const ObjectPaintProperties* clip_properties =
      clip->FirstFragment().PaintProperties();
  EXPECT_EQ(DocContentClip(), clip_properties->CssClip()->Parent());
  // Always create scroll translation for layout view even the document does
  // not scroll (not enough content).
  EXPECT_TRUE(DocScrollTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            &clip_properties->CssClip()->LocalTransformSpace());
  EXPECT_CLIP_RECT(gfx::RectF(absolute_clip_rect), clip_properties->CssClip());
  CHECK_VISUAL_RECT(absolute_clip_rect, clip,
                    GetDocument().View()->GetLayoutView(),
                    // TODO(crbug.com/599939): mapToVisualRectInAncestorSpace()
                    // doesn't apply css clip on the object itself.
                    LayoutUnit::Max());

  auto* absolute = GetLayoutObjectByElementId("absolute");
  EXPECT_EQ(clip_properties->CssClip(),
            &absolute->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_TRUE(DocScrollTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            &absolute->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(PhysicalOffset(777, 777), absolute->FirstFragment().PaintOffset());
  CHECK_VISUAL_RECT(PhysicalRect(), absolute,
                    GetDocument().View()->GetLayoutView(),
                    // TODO(crbug.com/599939): CSS clip of fixed-position
                    // descendants is broken in
                    // mapToVisualRectInAncestorSpace().
                    LayoutUnit::Max());
}

TEST_P(PaintPropertyTreeBuilderTest, CSSClipSubpixel) {
  // This test verifies that clip tree hierarchy being generated correctly for
  // a subpixel-positioned element with CSS clip.
  SetBodyInnerHTML(R"HTML(
    <style>
      #clip {
        position: absolute;
        left: 123.5px;
        top: 456px;
        clip: rect(10px, 80px, 70px, 40px);
        width: 100px;
        height: 100px;
      }
    </style>
    <div id='clip'></div>
  )HTML");

  PhysicalRect local_clip_rect(40, 10, 40, 60);
  PhysicalRect absolute_clip_rect = local_clip_rect;
  // Moved by 124 pixels due to pixel-snapping.
  absolute_clip_rect.offset +=
      PhysicalOffset(LayoutUnit(123.5), LayoutUnit(456));

  auto* clip = GetLayoutObjectByElementId("clip");
  const ObjectPaintProperties* clip_properties =
      clip->FirstFragment().PaintProperties();
  EXPECT_EQ(DocContentClip(), clip_properties->CssClip()->Parent());
  // Always create scroll translation for layout view even the document does
  // not scroll (not enough content).
  EXPECT_TRUE(DocScrollTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            &clip_properties->CssClip()->LocalTransformSpace());
  EXPECT_EQ(FloatClipRect(gfx::RectF(absolute_clip_rect)),
            clip_properties->CssClip()->LayoutClipRect());
  EXPECT_EQ(FloatRoundedRect(ToPixelSnappedRect((absolute_clip_rect))),
            clip_properties->CssClip()->PaintClipRect());
}

TEST_P(PaintPropertyTreeBuilderTest, CSSClipFixedPositionDescendantNonShared) {
  // This test is similar to CSSClipFixedPositionDescendant above, except that
  // now we have a parent overflow clip that should be escaped by the fixed
  // descendant.
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0;
      }
      /* to prevent the mock overlay scrollbar from affecting compositing. */
      #overflow::-webkit-scrollbar {
        display: none;
      }
      #overflow {
        position: relative;
        width: 50px;
        height: 50px;
        overflow: scroll;
      }
      #clip {
        position: absolute;
        left: 123px;
        top: 456px;
        clip: rect(10px, 80px, 70px, 40px);
        width: 100px;
        height: 100px;
      }
      #fixed {
        position: fixed;
        left: 654px;
        top: 321px;
      }
    </style>
    <div id='overflow'><div id='clip'><div id='fixed'></div></div></div>
  )HTML");
  PhysicalRect local_clip_rect(40, 10, 40, 60);
  PhysicalRect absolute_clip_rect = local_clip_rect;
  absolute_clip_rect.offset += PhysicalOffset(123, 456);

  LayoutObject& overflow = *GetLayoutObjectByElementId("overflow");
  const ObjectPaintProperties* overflow_properties =
      overflow.FirstFragment().PaintProperties();
  EXPECT_EQ(DocContentClip(), overflow_properties->OverflowClip()->Parent());
  // Always create scroll translation for layout view even the document does
  // not scroll (not enough content).
  EXPECT_TRUE(DocScrollTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            overflow_properties->ScrollTranslation()->Parent()->Parent());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 50, 50), &overflow,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* clip = GetLayoutObjectByElementId("clip");
  const ObjectPaintProperties* clip_properties =
      clip->FirstFragment().PaintProperties();
  EXPECT_EQ(overflow_properties->OverflowClip(),
            clip_properties->CssClip()->Parent());
  EXPECT_EQ(overflow_properties->ScrollTranslation(),
            &clip_properties->CssClip()->LocalTransformSpace());
  EXPECT_CLIP_RECT(FloatRoundedRect(gfx::RectF(absolute_clip_rect)),
                   clip_properties->CssClip());
  EXPECT_EQ(DocContentClip(),
            clip_properties->CssClipFixedPosition()->Parent());
  EXPECT_EQ(overflow_properties->ScrollTranslation(),
            &clip_properties->CssClipFixedPosition()->LocalTransformSpace());
  EXPECT_CLIP_RECT(FloatRoundedRect(gfx::RectF(absolute_clip_rect)),
                   clip_properties->CssClipFixedPosition());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(), clip,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* fixed = GetLayoutObjectByElementId("fixed");
  EXPECT_EQ(clip_properties->CssClipFixedPosition(),
            &fixed->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(fixed->FirstFragment().PaintProperties()->PaintOffsetTranslation(),
            &fixed->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(PhysicalOffset(0, 0), fixed->FirstFragment().PaintOffset());
  CHECK_VISUAL_RECT(PhysicalRect(), fixed,
                    GetDocument().View()->GetLayoutView(),
                    // TODO(crbug.com/599939): CSS clip of fixed-position
                    // descendants is broken in geometry mapping.
                    LayoutUnit::Max());
}

TEST_P(PaintPropertyTreeBuilderTest, ColumnSpannerUnderRelativePositioned) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #spanner {
        column-span: all;
        opacity: 0.5;
        width: 100px;
        height: 100px;
      }
    </style>
    <div style='columns: 3; position: absolute; top: 44px; left: 55px;'>
      <div style='position: relative; top: 100px; left: 100px'>
        <div id='spanner'></div>
      </div>
    </div>
  )HTML");

  LayoutObject* spanner = GetLayoutObjectByElementId("spanner");
  EXPECT_EQ(PhysicalOffset(55, 44), spanner->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(55, 44, 100, 100), spanner,
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, FractionalPaintOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      div { position: absolute; }
      #a {
        width: 70px;
        height: 70px;
        left: 0.1px;
        top: 0.3px;
      }
      #b {
        width: 40px;
        height: 40px;
        left: 0.5px;
        top: 11.1px;
      }
    </style>
    <div id='a'>
      <div id='b'></div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* a = GetLayoutObjectByElementId("a");
  PhysicalOffset a_paint_offset(LayoutUnit(0.1), LayoutUnit(0.3));
  EXPECT_EQ(a_paint_offset, a->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(LayoutUnit(0.1), LayoutUnit(0.3),
                                       LayoutUnit(70), LayoutUnit(70)),
                          a, frame_view->GetLayoutView());

  LayoutObject* b = GetLayoutObjectByElementId("b");
  PhysicalOffset b_paint_offset =
      a_paint_offset + PhysicalOffset(LayoutUnit(0.5), LayoutUnit(11.1));
  EXPECT_EQ(b_paint_offset, b->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(LayoutUnit(0.1), LayoutUnit(0.3),
                                       LayoutUnit(70), LayoutUnit(70)),
                          a, frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, PaintOffsetWithBasicPixelSnapping) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      div { position: relative; }
      #a {
        width: 70px;
        height: 70px;
        left: 0.3px;
        top: 0.3px;
      }
      #b {
        width: 40px;
        height: 40px;
        transform: translateZ(0);
      }
      #c {
        width: 40px;
        height: 40px;
       left: 0.1px;
       top: 0.1px;
      }
    </style>
    <div id='a'>
      <div id='b'>
        <div id='c'></div>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  EXPECT_TRUE(b_properties->Transform()->IsIdentity());
  // The paint offset transform should be snapped from (0.3,0.3) to (0,0).
  EXPECT_TRUE(ToUnaliased(*b_properties->Transform()->Parent()).IsIdentity());
  // The residual subpixel adjustment should be (0.3,0.3) - (0,0) = (0.3,0.3).
  PhysicalOffset subpixel_accumulation(LayoutUnit(0.3), LayoutUnit(0.3));
  EXPECT_EQ(subpixel_accumulation, b->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(LayoutUnit(0.3), LayoutUnit(0.3),
                                       LayoutUnit(40), LayoutUnit(40)),
                          b, frame_view->GetLayoutView());

  // c's painted should start at subpixelAccumulation + (0.1,0.1) = (0.4,0.4).
  LayoutObject* c = GetLayoutObjectByElementId("c");
  PhysicalOffset c_paint_offset =
      subpixel_accumulation + PhysicalOffset(LayoutUnit(0.1), LayoutUnit(0.1));
  EXPECT_EQ(c_paint_offset, c->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(PhysicalRect(LayoutUnit(0.4), LayoutUnit(0.4),
                                 LayoutUnit(40), LayoutUnit(40)),
                    c, frame_view->GetLayoutView(), 1);
}

TEST_P(PaintPropertyTreeBuilderTest,
       PaintOffsetWithPixelSnappingThroughTransform) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      div { position: relative; }
      #a {
        width: 70px;
        height: 70px;
        left: 0.7px;
        top: 0.7px;
      }
      #b {
        width: 40px;
        height: 40px;
        transform: translateZ(0);
      }
      #c {
        width: 40px;
        height: 40px;
        left: 0.7px;
        top: 0.7px;
      }
    </style>
    <div id='a'>
      <div id='b'>
        <div id='c'></div>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  EXPECT_TRUE(b_properties->Transform()->IsIdentity());
  // The paint offset transform should be snapped from (0.7,0.7) to (1,1).
  EXPECT_EQ(
      gfx::Vector2dF(1, 1),
      ToUnaliased(*b_properties->Transform()->Parent()).Get2dTranslation());
  // The residual subpixel adjustment should be (0.7,0.7) - (1,1) = (-0.3,-0.3).
  PhysicalOffset subpixel_accumulation =
      PhysicalOffset(LayoutUnit(0.7), LayoutUnit(0.7)) - PhysicalOffset(1, 1);
  EXPECT_EQ(subpixel_accumulation, b->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(LayoutUnit(0.7), LayoutUnit(0.7),
                                       LayoutUnit(40), LayoutUnit(40)),
                          b, frame_view->GetLayoutView());

  // c's painting should start at subpixelAccumulation + (0.7,0.7) = (0.4,0.4).
  LayoutObject* c = GetLayoutObjectByElementId("c");
  PhysicalOffset c_paint_offset =
      subpixel_accumulation + PhysicalOffset(LayoutUnit(0.7), LayoutUnit(0.7));
  EXPECT_EQ(c_paint_offset, c->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(PhysicalRect(LayoutUnit(0.7) + LayoutUnit(0.7),
                                 LayoutUnit(0.7) + LayoutUnit(0.7),
                                 LayoutUnit(40), LayoutUnit(40)),
                    c, frame_view->GetLayoutView(), 1);
}

TEST_P(PaintPropertyTreeBuilderTest,
       NonTranslationTransformShouldResetSubpixelPaintOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      div { position: relative; }
      #a {
        width: 70px;
        height: 70px;
        left: 0.9px;
        top: 0.9px;
      }
      #b {
        width: 40px;
        height: 40px;
        transform: scale(10);
        transform-origin: 0 0;
      }
      #c {
        width: 40px;
        height: 40px;
        left: 0.6px;
        top: 0.6px;
      }
    </style>
    <div id='a'>
      <div id='b'>
        <div id='c'></div>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  EXPECT_EQ(MakeScaleMatrix(10), b_properties->Transform()->Matrix());
  // The paint offset transform should not be snapped.
  EXPECT_EQ(
      gfx::Vector2dF(1, 1),
      ToUnaliased(*b_properties->Transform()->Parent()).Get2dTranslation());
  EXPECT_EQ(PhysicalOffset(), b->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(PhysicalRect(LayoutUnit(1), LayoutUnit(1), LayoutUnit(400),
                                 LayoutUnit(400)),
                    b, frame_view->GetLayoutView(), 1);

  // c's painting should start at c_offset.
  LayoutObject* c = GetLayoutObjectByElementId("c");
  LayoutUnit c_offset = LayoutUnit(0.6);
  EXPECT_EQ(PhysicalOffset(c_offset, c_offset),
            c->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px
  // in the transformed space (c_offset * 10 in view space) and 1px in the
  // view space.
  CHECK_VISUAL_RECT(PhysicalRect(c_offset * 10 + 1, c_offset * 10 + 1,
                                 LayoutUnit(400), LayoutUnit(400)),
                    c, frame_view->GetLayoutView(), c_offset * 10 + 1);
}

TEST_P(PaintPropertyTreeBuilderTest,
       WillChangeTransformShouldResetSubpixelPaintOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      div { position: relative; }
      #a {
        width: 70px;
        height: 70px;
        left: 0.9px;
        top: 0.9px;
      }
      #b {
        width: 40px;
        height: 40px;
        will-change: transform;
      }
      #c {
        width: 40px;
        height: 40px;
        left: 0.6px;
        top: 0.6px;
      }
    </style>
    <div id='a'>
      <div id='b'>
        <div id='c'></div>
      </div>
    </div>
  )HTML");

  auto* b = GetLayoutObjectByElementId("b");
  const auto* b_properties = b->FirstFragment().PaintProperties();
  EXPECT_TRUE(
      b_properties->Transform()->RequiresCompositingForWillChangeTransform());
  // The paint offset transform should not be snapped.
  EXPECT_EQ(
      gfx::Vector2dF(1, 1),
      ToUnaliased(*b_properties->Transform()->Parent()).Get2dTranslation());
  EXPECT_EQ(PhysicalOffset(), b->FirstFragment().PaintOffset());
  // c's painting should start at c_offset.
  auto* c = GetLayoutObjectByElementId("c");
  LayoutUnit c_offset = LayoutUnit(0.6);
  EXPECT_EQ(PhysicalOffset(c_offset, c_offset),
            c->FirstFragment().PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest,
       TransformAnimationShouldResetSubpixelPaintOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      div { position: relative; }
      #a {
        width: 70px;
        height: 70px;
        left: 0.9px;
        top: 0.9px;
      }
      #b {
        width: 40px;
        height: 40px;
        animation: spin 2s infinite;
      }
      @keyframes spin {
        from { transform: rotate(0deg) }
        to { transform: rotate(360deg) }
      }
      #c {
        width: 40px;
        height: 40px;
        left: 0.6px;
        top: 0.6px;
      }
    </style>
    <div id='a'>
      <div id='b'>
        <div id='c'></div>
      </div>
    </div>
  )HTML");

  auto* b = GetLayoutObjectByElementId("b");
  const auto* b_properties = b->FirstFragment().PaintProperties();
  EXPECT_TRUE(b_properties->Transform()->HasActiveTransformAnimation());
  // The paint offset transform should not be snapped.
  EXPECT_EQ(
      gfx::Vector2dF(1, 1),
      ToUnaliased(*b_properties->Transform()->Parent()).Get2dTranslation());
  EXPECT_EQ(PhysicalOffset(), b->FirstFragment().PaintOffset());
  // c's painting should start at c_offset.
  auto* c = GetLayoutObjectByElementId("c");
  LayoutUnit c_offset = LayoutUnit(0.6);
  EXPECT_EQ(PhysicalOffset(c_offset, c_offset),
            c->FirstFragment().PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest,
       PaintOffsetWithPixelSnappingThroughMultipleTransforms) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      div { position: relative; }
      #a {
        width: 70px;
        height: 70px;
        left: 0.7px;
        top: 0.7px;
      }
      #b {
        width: 40px;
        height: 40px;
        transform: translate3d(5px, 7px, 0);
      }
      #c {
        width: 40px;
        height: 40px;
        transform: translate3d(11px, 13px, 0);
      }
      #d {
        width: 40px;
        height: 40px;
        left: 0.7px;
        top: 0.7px;
      }
    </style>
    <div id='a'>
      <div id='b'>
        <div id='c'>
          <div id='d'></div>
        </div>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(5, 7),
            b_properties->Transform()->Get2dTranslation());
  // The paint offset transform should be snapped from (0.7,0.7) to (1,1).
  EXPECT_EQ(
      gfx::Vector2dF(1, 1),
      ToUnaliased(*b_properties->Transform()->Parent()).Get2dTranslation());
  // The residual subpixel adjustment should be (0.7,0.7) - (1,1) = (-0.3,-0.3).
  PhysicalOffset subpixel_accumulation =
      PhysicalOffset(LayoutUnit(0.7), LayoutUnit(0.7)) - PhysicalOffset(1, 1);
  EXPECT_EQ(subpixel_accumulation, b->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(LayoutUnit(5.7), LayoutUnit(7.7),
                                       LayoutUnit(40), LayoutUnit(40)),
                          b, frame_view->GetLayoutView());

  LayoutObject* c = GetLayoutObjectByElementId("c");
  const ObjectPaintProperties* c_properties =
      c->FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(11, 13),
            c_properties->Transform()->Get2dTranslation());
  // The paint offset should be (-0.3,-0.3) but the paint offset transform
  // should still be at (0,0) because it should be snapped.
  EXPECT_EQ(
      gfx::Vector2dF(),
      ToUnaliased(*c_properties->Transform()->Parent()).Get2dTranslation());
  // The residual subpixel adjustment should still be (-0.3,-0.3).
  EXPECT_EQ(subpixel_accumulation, c->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(LayoutUnit(16.7), LayoutUnit(20.7),
                                       LayoutUnit(40), LayoutUnit(40)),
                          c, frame_view->GetLayoutView());

  // d should be painted starting at subpixelAccumulation + (0.7,0.7) =
  // (0.4,0.4).
  LayoutObject* d = GetLayoutObjectByElementId("d");
  PhysicalOffset d_paint_offset =
      subpixel_accumulation + PhysicalOffset(LayoutUnit(0.7), LayoutUnit(0.7));
  EXPECT_EQ(d_paint_offset, d->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(PhysicalRect(LayoutUnit(16.7) + LayoutUnit(0.7),
                                 LayoutUnit(20.7) + LayoutUnit(0.7),
                                 LayoutUnit(40), LayoutUnit(40)),
                    d, frame_view->GetLayoutView(), 1);
}

TEST_P(PaintPropertyTreeBuilderTest, PaintOffsetWithPixelSnappingWithFixedPos) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      #a {
        width: 70px;
        height: 70px;
        left: 0.7px;
        position: relative;
      }
      #b {
        width: 40px;
        height: 40px;
        transform: translateZ(0);
        position: relative;
      }
      #fixed {
        width: 40px;
        height: 40px;
        position: fixed;
      }
      #d {
        width: 40px;
        height: 40px;
        left: 0.7px;
        position: relative;
      }
    </style>
    <div id='a'>
      <div id='b'>
        <div id='fixed'>
          <div id='d'></div>
        </div>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(), b_properties->Transform()->Get2dTranslation());
  // The paint offset transform should be snapped from (0.7,0) to (1,0).
  EXPECT_EQ(
      gfx::Vector2dF(1, 0),
      ToUnaliased(*b_properties->Transform()->Parent()).Get2dTranslation());
  // The residual subpixel adjustment should be (0.7,0) - (1,0) = (-0.3,0).
  PhysicalOffset subpixel_accumulation =
      PhysicalOffset(LayoutUnit(0.7), LayoutUnit()) - PhysicalOffset(1, 0);
  EXPECT_EQ(subpixel_accumulation, b->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(LayoutUnit(0.7), LayoutUnit(0),
                                       LayoutUnit(40), LayoutUnit(40)),
                          b, frame_view->GetLayoutView());

  LayoutObject* fixed = GetLayoutObjectByElementId("fixed");
  // The residual subpixel adjustment should still be (-0.3,0).
  EXPECT_EQ(subpixel_accumulation, fixed->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(LayoutUnit(0.7), LayoutUnit(0),
                                       LayoutUnit(40), LayoutUnit(40)),
                          fixed, frame_view->GetLayoutView());

  // d should be painted starting at subpixelAccumulation + (0.7,0) = (0.4,0).
  LayoutObject* d = GetLayoutObjectByElementId("d");
  PhysicalOffset d_paint_offset =
      subpixel_accumulation + PhysicalOffset(LayoutUnit(0.7), LayoutUnit());
  EXPECT_EQ(d_paint_offset, d->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(PhysicalRect(LayoutUnit(0.7) + LayoutUnit(0.7),
                                 LayoutUnit(), LayoutUnit(40), LayoutUnit(40)),
                    d, frame_view->GetLayoutView(), 1);
}

TEST_P(PaintPropertyTreeBuilderTest, SvgPixelSnappingShouldResetPaintOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #svg {
        position: relative;
        left: 0.1px;
        transform: matrix(1, 0, 0, 1, 0, 0);
      }
    </style>
    <svg id='svg'>
        <rect id='rect' transform='translate(1, 1)'/>
    </svg>
  )HTML");

  LayoutObject& svg_with_transform = *GetLayoutObjectByElementId("svg");
  const ObjectPaintProperties* svg_with_transform_properties =
      svg_with_transform.FirstFragment().PaintProperties();
  EXPECT_TRUE(svg_with_transform_properties->Transform()->IsIdentity());
  EXPECT_EQ(PhysicalOffset(LayoutUnit(0.1), LayoutUnit()),
            svg_with_transform.FirstFragment().PaintOffset());
  EXPECT_TRUE(svg_with_transform_properties->ReplacedContentTransform() ==
              nullptr);

  LayoutObject& rect_with_transform = *GetLayoutObjectByElementId("rect");
  const ObjectPaintProperties* rect_with_transform_properties =
      rect_with_transform.FirstFragment().PaintProperties();
  EXPECT_EQ(gfx::Vector2dF(1, 1),
            rect_with_transform_properties->Transform()->Get2dTranslation());

  // Ensure there is no PaintOffset transform between the rect and the svg's
  // transform.
  EXPECT_EQ(svg_with_transform_properties->Transform(),
            rect_with_transform_properties->Transform()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SvgRootAndForeignObjectPixelSnapping) {
  SetBodyInnerHTML(R"HTML(
    <svg id=svg style='position: relative; left: 0.6px; top: 0.3px'>
      <foreignObject id=foreign x='3.5' y='5.4' transform='translate(1, 1)'>
        <div id=div style='position: absolute; left: 5.6px; top: 7.3px'>
        </div>
      </foreignObject>
    </svg>
  )HTML");

  const auto* svg = GetLayoutObjectByElementId("svg");
  const auto* svg_properties = svg->FirstFragment().PaintProperties();
  // The paint offset of (8.6, 8.3) is rounded off here. The fractional part
  // remains PaintOffset.
  EXPECT_EQ(gfx::Vector2dF(9, 8),
            svg_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_EQ(PhysicalOffset(LayoutUnit(-0.40625), LayoutUnit(0.3)),
            svg->FirstFragment().PaintOffset());
  EXPECT_EQ(nullptr, svg_properties->ReplacedContentTransform());
  const auto* foreign_object = GetLayoutObjectByElementId("foreign");
  const auto* foreign_object_properties =
      foreign_object->FirstFragment().PaintProperties();
  EXPECT_EQ(nullptr, foreign_object_properties->PaintOffsetTranslation());

  PhysicalOffset fo_translation(LayoutUnit(3.5f), LayoutUnit(5.4f));
  EXPECT_EQ(fo_translation, foreign_object->FirstFragment().PaintOffset());

  const auto* div = GetLayoutObjectByElementId("div");
  // Paint offset of descendant of foreignObject accumulates on paint offset
  // of foreignObject.
  PhysicalOffset div_offset(LayoutUnit(5.6f), LayoutUnit(7.3f));
  EXPECT_EQ(fo_translation + div_offset, div->FirstFragment().PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest, NoRenderingContextByDefault) {
  SetBodyInnerHTML("<div style='transform: translateZ(0)'></div>");

  const ObjectPaintProperties* properties = GetDocument()
                                                .body()
                                                ->firstChild()
                                                ->GetLayoutObject()
                                                ->FirstFragment()
                                                .PaintProperties();
  ASSERT_TRUE(properties->Transform());
  EXPECT_FALSE(properties->Transform()->HasRenderingContext());
}

TEST_P(PaintPropertyTreeBuilderTest, Preserve3DCreatesSharedRenderingContext) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform-style: preserve-3d'>
      <div id='a' style='transform: translateZ(0); width: 30px; height: 40px'>
      </div>
      <div id='b' style='transform: translateZ(0); width: 20px; height: 10px'>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* a = GetLayoutObjectByElementId("a");
  const ObjectPaintProperties* a_properties =
      a->FirstFragment().PaintProperties();
  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  ASSERT_TRUE(a_properties->Transform() && b_properties->Transform());
  EXPECT_NE(a_properties->Transform(), b_properties->Transform());

  EXPECT_TRUE(a_properties->Transform()->HasRenderingContext());
  EXPECT_FALSE(a_properties->Transform()->FlattensInheritedTransform());
  EXPECT_TRUE(b_properties->Transform()->HasRenderingContext());
  EXPECT_FALSE(b_properties->Transform()->FlattensInheritedTransform());
  EXPECT_EQ(a_properties->Transform()->RenderingContextId(),
            b_properties->Transform()->RenderingContextId());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 48, 20, 10), b,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest,
       IntermediateElementPreventsSharedRenderingContext) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='transform-style: preserve-3d'>
      <div>
        <div id='a' style='transform: translateZ(0); width: 30px; height: 40px'>
        </div>
      </div>
      <div id='b' style='transform: translateZ(0); width: 20px; height: 10px'>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* a = GetLayoutObjectByElementId("a");
  const ObjectPaintProperties* a_properties =
      a->FirstFragment().PaintProperties();
  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  ASSERT_TRUE(a_properties->Transform() && b_properties->Transform());
  EXPECT_NE(a_properties->Transform(), b_properties->Transform());

  const ObjectPaintProperties* parent_properties =
      b->FirstFragment().PaintProperties();

  EXPECT_FALSE(a_properties->Transform()->HasRenderingContext());
  EXPECT_TRUE(a_properties->Transform()->FlattensInheritedTransform());
  EXPECT_TRUE(b_properties->Transform()->HasRenderingContext());
  EXPECT_FALSE(b_properties->Transform()->FlattensInheritedTransform());
  EXPECT_NE(a_properties->Transform()->RenderingContextId(),
            b_properties->Transform()->RenderingContextId());

  EXPECT_EQ(parent_properties->Transform()->RenderingContextId(),
            b_properties->Transform()->RenderingContextId());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 48, 20, 10), b,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest,
       IntermediateElementWithPropertiesPreventsSharedRenderingContext) {
  SetBodyInnerHTML(R"HTML(
    <div id='parent' style='transform-style: preserve-3d'>
      <div style="overflow: hidden">
        <div id='a' style='transform: translateZ(0); width: 30px; height: 40px'>
        </div>
      </div>
      <div id='b' style='transform: translateZ(0); width: 20px; height: 10px'>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* a = GetLayoutObjectByElementId("a");
  const ObjectPaintProperties* a_properties =
      a->FirstFragment().PaintProperties();
  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  ASSERT_TRUE(a_properties->Transform() && b_properties->Transform());
  EXPECT_NE(a_properties->Transform(), b_properties->Transform());

  const ObjectPaintProperties* parent_properties =
      b->FirstFragment().PaintProperties();

  EXPECT_FALSE(a_properties->Transform()->HasRenderingContext());
  EXPECT_TRUE(a_properties->Transform()->FlattensInheritedTransform());
  EXPECT_TRUE(b_properties->Transform()->HasRenderingContext());
  EXPECT_FALSE(b_properties->Transform()->FlattensInheritedTransform());
  EXPECT_NE(a_properties->Transform()->RenderingContextId(),
            b_properties->Transform()->RenderingContextId());

  EXPECT_EQ(parent_properties->Transform()->RenderingContextId(),
            b_properties->Transform()->RenderingContextId());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 48, 20, 10), b,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, FlatTransformStyleEndsRenderingContext) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #a {
        transform: translateZ(0);
        width: 30px;
        height: 40px;
      }
      #b {
        transform: translateZ(0);
        width: 10px;
        height: 20px;
      }
    </style>
    <div style='transform-style: preserve-3d'>
      <div id='a'>
        <div id='b'></div>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* a = GetLayoutObjectByElementId("a");
  const ObjectPaintProperties* a_properties =
      a->FirstFragment().PaintProperties();
  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  ASSERT_FALSE(a->StyleRef().Preserves3D());

  ASSERT_TRUE(a_properties->Transform() && b_properties->Transform());

  // #a should participate in a rendering context (due to its parent), but its
  // child #b should not.
  EXPECT_TRUE(a_properties->Transform()->HasRenderingContext());
  EXPECT_FALSE(b_properties->Transform()->HasRenderingContext());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 10, 20), b,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, NestedRenderingContexts) {
  SetBodyInnerHTML(R"HTML(
    <div style='transform-style: preserve-3d'>
      <div id='a' style='transform: translateZ(0); width: 50px; height: 60px'>
        <div style='transform-style: preserve-3d; width: 30px; height: 40px'>
          <div id='b'
              style='transform: translateZ(0); width: 10px; height: 20px'>
          </div>
        </div>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* a = GetLayoutObjectByElementId("a");
  const ObjectPaintProperties* a_properties =
      a->FirstFragment().PaintProperties();
  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  ASSERT_FALSE(a->StyleRef().Preserves3D());
  ASSERT_TRUE(a_properties->Transform() && b_properties->Transform());

  // #a should participate in a rendering context (due to its parent). Its child
  //  does preserve 3D, but since #a does not, #a's rendering context is not
  //  passed on to its children. Thus #b ends up in a separate rendering
  //  context rooted at its parent.
  EXPECT_TRUE(a_properties->Transform()->HasRenderingContext());
  EXPECT_TRUE(b_properties->Transform()->HasRenderingContext());
  EXPECT_NE(a_properties->Transform()->RenderingContextId(),
            b_properties->Transform()->RenderingContextId());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 50, 60), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 10, 20), b,
                          frame_view->GetLayoutView());
}

// Returns true if some node will flatten the transform due to |node| before it
// is inherited by |node| (including if node->flattensInheritedTransform()).
static bool SomeNodeFlattensTransform(
    const TransformPaintPropertyNode* node,
    const TransformPaintPropertyNode* ancestor) {
  while (node != ancestor) {
    if (node->FlattensInheritedTransform())
      return true;
    node = node->UnaliasedParent();
  }
  return false;
}

TEST_P(PaintPropertyTreeBuilderTest, FlatTransformStylePropagatesToChildren) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #a {
        transform: translateZ(0);
        transform-style: flat;
        width: 30px;
        height: 40px;
      }
      #b {
        transform: translateZ(0);
        width: 10px;
        height: 10px;
      }
    </style>
    <div id='a'>
      <div id='b'></div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* a = GetLayoutObjectByElementId("a");
  LayoutObject* b = GetLayoutObjectByElementId("b");
  const auto* a_transform = a->FirstFragment().PaintProperties()->Transform();
  ASSERT_TRUE(a_transform);
  const auto* b_transform = b->FirstFragment().PaintProperties()->Transform();
  ASSERT_TRUE(b_transform);
  ASSERT_TRUE(a_transform->IsAncestorOf(*b_transform));

  // Some node must flatten the inherited transform from #a before it reaches
  // #b's transform.
  EXPECT_TRUE(SomeNodeFlattensTransform(b_transform, a_transform));
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 10, 10), b,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest,
       Preserve3DTransformStylePropagatesToChildren) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #a {
        transform: translateZ(0);
        transform-style: preserve-3d;
        width: 30px;
        height: 40px;
      }
      #b {
        transform: translateZ(0);
        width: 10px;
        height: 10px;
      }
    </style>
    <div id='a'>
      <div id='b'></div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* a = GetLayoutObjectByElementId("a");
  LayoutObject* b = GetLayoutObjectByElementId("b");
  const auto* a_transform = a->FirstFragment().PaintProperties()->Transform();
  ASSERT_TRUE(a_transform);
  const auto* b_transform = b->FirstFragment().PaintProperties()->Transform();
  ASSERT_TRUE(b_transform);
  ASSERT_TRUE(a_transform->IsAncestorOf(*b_transform));

  // No node may flatten the inherited transform from #a before it reaches
  // #b's transform.
  EXPECT_FALSE(SomeNodeFlattensTransform(b_transform, a_transform));
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 10, 10), b,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, PerspectiveIsNotFlattened) {
  // It's necessary to make nodes from the one that applies perspective to
  // ones that combine with it preserve 3D. Otherwise, the perspective doesn't
  // do anything.
  SetBodyInnerHTML(R"HTML(
    <div id='a' style='perspective: 800px; width: 30px; height: 40px'>
      <div id='b'
          style='transform: translateZ(0); width: 10px; height: 20px'></div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* a = GetLayoutObjectByElementId("a");
  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* a_properties =
      a->FirstFragment().PaintProperties();
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  const TransformPaintPropertyNode* a_perspective = a_properties->Perspective();
  ASSERT_TRUE(a_perspective);
  const TransformPaintPropertyNode* b_transform = b_properties->Transform();
  ASSERT_TRUE(b_transform);
  ASSERT_TRUE(a_perspective->IsAncestorOf(*b_transform));
  EXPECT_FALSE(SomeNodeFlattensTransform(b_transform, a_perspective));
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 10, 20), b,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, FlatteningIn3DContext) {
  SetBodyInnerHTML(R"HTML(
    <div id="a" style="transform-style: preserve-3d">
      <div id="b" style="transform: translate3d(0, 0, 33px)">
        <div id="c" style="transform: translate3d(0, 0, -10px)">C</div>
      </div>
      <div id="d" style="transform: translate3d(0, -10px, 22px)">D</div>
    </div>
  )HTML");

  const auto* a_properties = PaintPropertiesForElement("a");
  ASSERT_NE(a_properties, nullptr);
  ASSERT_NE(a_properties->Transform(), nullptr);
  EXPECT_TRUE(a_properties->Transform()->IsIdentity());
  EXPECT_TRUE(a_properties->Transform()->HasRenderingContext());
  EXPECT_TRUE(a_properties->Transform()->FlattensInheritedTransform());
  EXPECT_EQ(a_properties->Effect(), nullptr);

  const auto* b_properties = PaintPropertiesForElement("b");
  ASSERT_NE(b_properties, nullptr);
  ASSERT_NE(b_properties->Transform(), nullptr);
  EXPECT_EQ(MakeTranslationMatrix(0, 0, 33),
            b_properties->Transform()->Matrix());
  EXPECT_EQ(a_properties->Transform()->RenderingContextId(),
            b_properties->Transform()->RenderingContextId());
  EXPECT_FALSE(b_properties->Transform()->FlattensInheritedTransform());
  // Force render surface with an effect node for |b| which is an 3D object in
  // its container while it flattens its contents.
  ASSERT_NE(b_properties->Effect(), nullptr);
  EXPECT_EQ(b_properties->Transform(),
            &b_properties->Effect()->LocalTransformSpace());

  const auto* c_properties = PaintPropertiesForElement("c");
  ASSERT_NE(c_properties, nullptr);
  ASSERT_NE(c_properties->Transform(), nullptr);
  EXPECT_EQ(MakeTranslationMatrix(0, 0, -10),
            c_properties->Transform()->Matrix());
  EXPECT_FALSE(c_properties->Transform()->HasRenderingContext());
  EXPECT_TRUE(c_properties->Transform()->FlattensInheritedTransform());
  EXPECT_EQ(c_properties->Filter(), nullptr);

  const auto* d_properties = PaintPropertiesForElement("d");
  ASSERT_NE(d_properties, nullptr);
  ASSERT_NE(d_properties->Transform(), nullptr);
  EXPECT_EQ(MakeTranslationMatrix(0, -10, 22),
            d_properties->Transform()->Matrix());
  EXPECT_EQ(a_properties->Transform()->RenderingContextId(),
            d_properties->Transform()->RenderingContextId());
  EXPECT_FALSE(d_properties->Transform()->FlattensInheritedTransform());
  EXPECT_NE(d_properties->Effect(), nullptr);
}

TEST_P(PaintPropertyTreeBuilderTest,
       PerspectiveDoesNotEstablishRenderingContext) {
  // It's necessary to make nodes from the one that applies perspective to
  // ones that combine with it preserve 3D. Otherwise, the perspective doesn't
  // do anything.
  SetBodyInnerHTML(R"HTML(
    <div id='a' style='perspective: 800px; width: 30px; height: 40px'>
      <div id='b'
          style='transform: translateZ(0); width: 10px; height: 20px'></div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  LayoutObject* a = GetLayoutObjectByElementId("a");
  LayoutObject* b = GetLayoutObjectByElementId("b");
  const ObjectPaintProperties* a_properties =
      a->FirstFragment().PaintProperties();
  const ObjectPaintProperties* b_properties =
      b->FirstFragment().PaintProperties();
  const TransformPaintPropertyNode* a_perspective = a_properties->Perspective();
  ASSERT_TRUE(a_perspective);
  EXPECT_FALSE(a_perspective->HasRenderingContext());
  const TransformPaintPropertyNode* b_transform = b_properties->Transform();
  ASSERT_TRUE(b_transform);
  EXPECT_FALSE(b_transform->HasRenderingContext());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(8, 8, 10, 20), b,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, CachedProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div id='a' style='transform: translate(33px, 44px); width: 50px;
        height: 60px'>
      <div id='b' style='transform: translate(55px, 66px); width: 30px;
          height: 40px'>
        <div id='c' style='transform: translate(77px, 88px); width: 10px;
            height: 20px'>C<div>
      </div>
    </div>
  )HTML");
  LocalFrameView* frame_view = GetDocument().View();

  Element* a = GetDocument().getElementById(AtomicString("a"));
  const ObjectPaintProperties* a_properties =
      a->GetLayoutObject()->FirstFragment().PaintProperties();
  const TransformPaintPropertyNode* a_transform_node =
      a_properties->Transform();
  EXPECT_EQ(gfx::Vector2dF(33, 44), a_transform_node->Get2dTranslation());

  Element* b = GetDocument().getElementById(AtomicString("b"));
  const ObjectPaintProperties* b_properties =
      b->GetLayoutObject()->FirstFragment().PaintProperties();
  const TransformPaintPropertyNode* b_transform_node =
      b_properties->Transform();
  EXPECT_EQ(gfx::Vector2dF(55, 66), b_transform_node->Get2dTranslation());

  Element* c = GetDocument().getElementById(AtomicString("c"));
  const ObjectPaintProperties* c_properties =
      c->GetLayoutObject()->FirstFragment().PaintProperties();
  const TransformPaintPropertyNode* c_transform_node =
      c_properties->Transform();
  EXPECT_EQ(gfx::Vector2dF(77, 88), c_transform_node->Get2dTranslation());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(33, 44, 50, 60), a->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(88, 110, 30, 40), b->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(165, 198, 10, 20), c->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // Change transform of b. B's transform node should be a new node with the new
  // value, and a and c's transform nodes should be unchanged (with c's parent
  // adjusted).
  b->setAttribute(html_names::kStyleAttr,
                  AtomicString("transform: translate(111px, 222px)"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(a_properties,
            a->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(a_transform_node, a_properties->Transform());

  EXPECT_EQ(b_properties,
            b->GetLayoutObject()->FirstFragment().PaintProperties());
  b_transform_node = b_properties->Transform();
  EXPECT_EQ(gfx::Vector2dF(111, 222), b_transform_node->Get2dTranslation());
  EXPECT_EQ(a_transform_node, b_transform_node->Parent()->Parent());

  EXPECT_EQ(c_properties,
            c->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(c_transform_node, c_properties->Transform());
  EXPECT_EQ(b_transform_node, c_transform_node->Parent()->Parent());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(33, 44, 50, 60), a->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(144, 266, 50, 20), b->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(221, 354, 10, 20), c->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // Remove transform from b. B's transform node should be removed from the
  // tree, and a and c's transform nodes should be unchanged (with c's parent
  // adjusted).
  b->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(a_properties,
            a->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(a_transform_node, a_properties->Transform());

  EXPECT_EQ(nullptr, b->GetLayoutObject()->FirstFragment().PaintProperties());

  EXPECT_EQ(c_properties,
            c->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(c_transform_node, c_properties->Transform());
  EXPECT_EQ(a_transform_node, c_transform_node->Parent()->Parent());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(33, 44, 50, 60), a->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(33, 44, 50, 20), b->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(110, 132, 10, 20), c->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // Re-add transform to b. B's transform node should be inserted into the tree,
  // and a and c's transform nodes should be unchanged (with c's parent
  // adjusted).
  b->setAttribute(html_names::kStyleAttr,
                  AtomicString("transform: translate(4px, 5px)"));
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(a_properties,
            a->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(a_transform_node, a_properties->Transform());

  b_properties = b->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(b_properties,
            b->GetLayoutObject()->FirstFragment().PaintProperties());
  b_transform_node = b_properties->Transform();
  EXPECT_EQ(gfx::Vector2dF(4, 5), b_transform_node->Get2dTranslation());
  EXPECT_EQ(a_transform_node, b_transform_node->Parent()->Parent());

  EXPECT_EQ(c_properties,
            c->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(c_transform_node, c_properties->Transform());
  EXPECT_EQ(b_transform_node, c_transform_node->Parent()->Parent());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(33, 44, 50, 60), a->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(37, 49, 50, 20), b->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(PhysicalRect(114, 137, 10, 20), c->GetLayoutObject(),
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowClipContentsTreeState) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that is scrolled by a
  // containing block that is not one of the painting ancestors.
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 20px 30px; }</style>
    <div id='clipper'
        style='overflow: hidden; width: 400px; height: 300px;'>
      <div id='child'
          style='position: relative; width: 500px; height: 600px;'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* clipper =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("clipper"));
  const ObjectPaintProperties* clip_properties =
      clipper->FirstFragment().PaintProperties();
  LayoutObject* child = GetLayoutObjectByElementId("child");

  // Always create scroll translation for layout view even the document does
  // not scroll (not enough content).
  EXPECT_TRUE(DocScrollTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            &clipper->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(DocContentClip(),
            &clipper->FirstFragment().LocalBorderBoxProperties().Clip());

  auto contents_properties = clipper->FirstFragment().ContentsProperties();
  EXPECT_EQ(PhysicalOffset(30, 20), clipper->FirstFragment().PaintOffset());

  EXPECT_EQ(DocScrollTranslation(), &contents_properties.Transform());
  EXPECT_EQ(clip_properties->OverflowClip(), &contents_properties.Clip());

  EXPECT_EQ(DocScrollTranslation(),
            &child->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(clip_properties->OverflowClip(),
            &child->FirstFragment().LocalBorderBoxProperties().Clip());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 500, 600), child, clipper);
}

TEST_P(PaintPropertyTreeBuilderTest, ReplacedSvgContentWithIsolation) {
  SetBodyInnerHTML(R"HTML(
    <style>
    body { margin 0px; }
    </style>
    <svg id='replacedsvg'
        style='contain:paint; will-change:transform;' width="100px" height="200px"
        viewBox='50 50 100 100'>
    </svg>
  )HTML");

  LayoutBoxModelObject* svg =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("replacedsvg"));
  const ObjectPaintProperties* svg_properties =
      svg->FirstFragment().PaintProperties();

  EXPECT_TRUE(svg_properties->TransformIsolationNode());
  EXPECT_TRUE(svg_properties->ReplacedContentTransform());
  EXPECT_EQ(svg_properties->TransformIsolationNode()->Parent(),
            svg_properties->ReplacedContentTransform());
}

TEST_P(PaintPropertyTreeBuilderTest, ReplacedContentTransformFlattening) {
  SetBodyInnerHTML(R"HTML(
    <svg id="svg"
        style="transform: perspective(100px) rotateY(0deg);"
        width="100px"
        height="200px"
        viewBox="50 50 100 100">
    </svg>
  )HTML");

  const auto* svg = To<LayoutBoxModelObject>(GetLayoutObjectByElementId("svg"));
  const auto* svg_properties = svg->FirstFragment().PaintProperties();

  const auto* replaced_transform = svg_properties->ReplacedContentTransform();
  EXPECT_TRUE(replaced_transform->FlattensInheritedTransform());
  EXPECT_TRUE(
      ToUnaliased(*replaced_transform->Parent()).FlattensInheritedTransform());
}

TEST_P(PaintPropertyTreeBuilderTest, ContainPaintOrStyleLayoutTreeState) {
  for (const char* containment : {"paint", "style layout"}) {
    SCOPED_TRACE(containment);
    SetBodyInnerHTML(String::Format(R"HTML(
      <style>body { margin: 20px 30px; }</style>
      <div id='clipper'
          style='contain: %s; width: 300px; height: 200px;'>
        <div id='child'
            style='position: relative; width: 400px; height: 500px;'></div>
      </div>
    )HTML",
                                    containment));

    auto* clipper =
        To<LayoutBoxModelObject>(GetLayoutObjectByElementId("clipper"));
    const ObjectPaintProperties* clip_properties =
        clipper->FirstFragment().PaintProperties();
    LayoutObject* child = GetLayoutObjectByElementId("child");
    const auto& clip_local_properties =
        clipper->FirstFragment().LocalBorderBoxProperties();

    // Verify that we created isolation nodes.
    EXPECT_TRUE(clip_properties->TransformIsolationNode());
    EXPECT_TRUE(clip_properties->HasTransformNode());
    EXPECT_TRUE(clip_properties->EffectIsolationNode());
    EXPECT_TRUE(clip_properties->HasEffectNode());
    EXPECT_TRUE(clip_properties->ClipIsolationNode());
    EXPECT_TRUE(clip_properties->HasClipNode());

    // Verify parenting:

    // Transform isolation node should be parented to the local border box
    // properties transform, which should be the paint offset translation.
    EXPECT_EQ(clip_properties->TransformIsolationNode()->Parent(),
              &clip_local_properties.Transform());
    EXPECT_EQ(clip_properties->TransformIsolationNode()->Parent(),
              clip_properties->PaintOffsetTranslation());
    // Similarly, effect isolation node is parented to the local border box
    // properties effect.
    EXPECT_EQ(clip_properties->EffectIsolationNode()->Parent(),
              &clip_local_properties.Effect());
    if (strcmp(containment, "paint") == 0) {
      // If we contain paint, then clip isolation node is parented to the
      // overflow clip, which is in turn parented to the local border box
      // properties clip.
      EXPECT_EQ(clip_properties->ClipIsolationNode()->Parent(),
                clip_properties->OverflowClip());
      EXPECT_EQ(clip_properties->OverflowClip()->Parent(),
                &clip_local_properties.Clip());
    } else {
      // Otherwise, the clip isolation node is parented to the local border box
      // properties clip directly.
      EXPECT_EQ(clip_properties->ClipIsolationNode()->Parent(),
                &clip_local_properties.Clip());
    }

    // Verify transform:

    EXPECT_TRUE(clip_properties->TransformIsolationNode()->IsParentAlias());

    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
    // Isolation induces paint offset translation, so the node should be
    // different from the doc node, but its parent is the same as the doc
    // node.
    EXPECT_EQ(DocScrollTranslation(), clipper->FirstFragment()
                                          .LocalBorderBoxProperties()
                                          .Transform()
                                          .Parent());

    // Verify clip:

    EXPECT_EQ(DocContentClip(),
              &clipper->FirstFragment().LocalBorderBoxProperties().Clip());
    EXPECT_TRUE(clip_properties->ClipIsolationNode()->IsParentAlias());

    // Verify contents properties and child properties:

    auto contents_properties = clipper->FirstFragment().ContentsProperties();
    // Since the clipper is isolated, its paint offset should be 0, 0.
    EXPECT_EQ(PhysicalOffset(), clipper->FirstFragment().PaintOffset());
    // Ensure that the contents properties match isolation nodes.
    EXPECT_EQ(clip_properties->TransformIsolationNode(),
              &contents_properties.Transform());
    EXPECT_EQ(clip_properties->ClipIsolationNode(),
              &contents_properties.Clip());
    EXPECT_EQ(clip_properties->EffectIsolationNode(),
              &contents_properties.Effect());

    // Child should be using isolation nodes as its local border box properties.
    EXPECT_EQ(&contents_properties.Transform(),
              &child->FirstFragment().LocalBorderBoxProperties().Transform());
    EXPECT_EQ(&contents_properties.Clip(),
              &child->FirstFragment().LocalBorderBoxProperties().Clip());
    EXPECT_EQ(&contents_properties.Effect(),
              &child->FirstFragment().LocalBorderBoxProperties().Effect());
    CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 400, 500), child, clipper);
  }
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollContentsTreeState) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that is scrolled by a
  // containing block that is not one of the painting ancestors.
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 20px 30px; }
      /* to prevent the mock overlay scrollbar from affecting compositing. */
      #clipper::-webkit-scrollbar { display: none; }
    </style>
    <div id='clipper' style='overflow:scroll; width:400px; height:300px;'>
      <div id='child'
          style='position:relative; width:500px; height: 600px;'></div>
      <div style='width: 200px; height: 10000px'></div>
    </div>
    <div id='forceScroll' style='height: 4000px;'></div>
  )HTML");

  Element* clipper_element =
      GetDocument().getElementById(AtomicString("clipper"));
  clipper_element->scrollTo(1, 2);

  auto* clipper = To<LayoutBoxModelObject>(clipper_element->GetLayoutObject());
  const ObjectPaintProperties* clip_properties =
      clipper->FirstFragment().PaintProperties();
  LayoutObject* child = GetLayoutObjectByElementId("child");

  EXPECT_EQ(
      DocScrollTranslation(),
      clipper->FirstFragment().LocalBorderBoxProperties().Transform().Parent());
  EXPECT_EQ(clip_properties->PaintOffsetTranslation(),
            &clipper->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(DocContentClip(),
            &clipper->FirstFragment().LocalBorderBoxProperties().Clip());

  auto contents_properties = clipper->FirstFragment().ContentsProperties();
  EXPECT_EQ(gfx::Vector2dF(30, 20),
            clip_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_EQ(PhysicalOffset(), clipper->FirstFragment().PaintOffset());
  EXPECT_EQ(clip_properties->ScrollTranslation(),
            &contents_properties.Transform());
  EXPECT_EQ(clip_properties->OverflowClip(), &contents_properties.Clip());

  EXPECT_EQ(clip_properties->ScrollTranslation(),
            &child->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(clip_properties->OverflowClip(),
            &child->FirstFragment().LocalBorderBoxProperties().Clip());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 500, 600), child, clipper);
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollWithRoundedRect) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      ::-webkit-scrollbar {
        width: 13px;
        height: 13px;
      }
      #roundedBox {
        width: 200px;
        height: 200px;
        border-radius: 100px;
        background-color: red;
        border: 50px solid green;
        overflow: scroll;
      }
      #roundedBoxChild {
        width: 200px;
        height: 200px;
        background-color: orange;
      }
    </style>
    <div id='roundedBox'>
      <div id='roundedBoxChild'></div>
    </div>
  )HTML");

  LayoutObject& rounded_box = *GetLayoutObjectByElementId("roundedBox");
  const ObjectPaintProperties* rounded_box_properties =
      rounded_box.FirstFragment().PaintProperties();
  EXPECT_CLIP_RECT(FloatRoundedRect(gfx::RectF(50, 50, 200, 200),
                                    FloatRoundedRect::Radii(50)),
                   rounded_box_properties->InnerBorderRadiusClip());

  // Unlike the inner border radius clip, the overflow clip is inset by the
  // scrollbars (13px).
  EXPECT_CLIP_RECT(FloatRoundedRect(50, 50, 187, 187),
                   rounded_box_properties->OverflowClip());
  EXPECT_EQ(DocContentClip(),
            rounded_box_properties->InnerBorderRadiusClip()->Parent());
  EXPECT_EQ(rounded_box_properties->InnerBorderRadiusClip(),
            rounded_box_properties->OverflowClip()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollWithSubpixelBorder) {
  SetBodyInnerHTML(R"HTML(
      <style>
        #scroller {
          width: 200px;
          height: 201.594px;
          border: 2.8px solid blue;
          overflow: scroll;
        }
        #content {
          width: 600px;
          height: 201.594px;
        }
      </style>
      <div id="scroller">
        <div id="content"></div>
      </div>
    )HTML");

  PaintLayer* paint_layer = GetPaintLayerByElementId("scroller");
  ASSERT_FALSE(paint_layer->GetScrollableArea()->HasVerticalOverflow());

  // When there is no vertical overflow, the contents height should not be
  // larger than the container height.
  const auto* properties = PaintPropertiesForElement("scroller");
  const auto* scroll = properties->Scroll();
  EXPECT_EQ(scroll->ContentsRect().height(), scroll->ContainerRect().height());
}

TEST_P(PaintPropertyTreeBuilderTest, CssClipContentsTreeState) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that is scrolled by a
  // containing block that is not one of the painting ancestors.
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 20px 30px; }</style>
    <div id='clipper' style='position: absolute;
        clip: rect(10px, 80px, 70px, 40px); width:300px; height:200px;'>
      <div id='child' style='position:relative; width:400px; height: 500px;'>
      </div>
    </div>
  )HTML");

  auto* clipper =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("clipper"));
  const ObjectPaintProperties* clip_properties =
      clipper->FirstFragment().PaintProperties();
  LayoutObject* child = GetLayoutObjectByElementId("child");

  // Always create scroll translation for layout view even the document does
  // not scroll (not enough content).
  EXPECT_TRUE(DocScrollTranslation());
  EXPECT_EQ(DocScrollTranslation(),
            &clipper->FirstFragment().LocalBorderBoxProperties().Transform());
  // CSS clip on an element causes it to clip itself, not just descendants.
  EXPECT_EQ(clip_properties->CssClip(),
            &clipper->FirstFragment().LocalBorderBoxProperties().Clip());

  auto contents_properties = clipper->FirstFragment().ContentsProperties();
  EXPECT_EQ(PhysicalOffset(30, 20), clipper->FirstFragment().PaintOffset());
  EXPECT_EQ(DocScrollTranslation(), &contents_properties.Transform());
  EXPECT_EQ(clip_properties->CssClip(), &contents_properties.Clip());

  CHECK_EXACT_VISUAL_RECT(PhysicalRect(0, 0, 400, 500), child, clipper);
}

TEST_P(PaintPropertyTreeBuilderTest,
       ReplacedContentTransformContentsTreeState) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 20px 30px;
      }
      svg {
        position: absolute;
      }
      rect {
        transform: translate(100px, 100px);
      }
    </style>
    <svg id='svgWithViewBox' width='100px' height='100px'
        viewBox='50 50 100 100'>
      <rect id='rect' width='100px' height='100px' />
    </svg>
  )HTML");

  LayoutObject& svg_with_view_box =
      *GetLayoutObjectByElementId("svgWithViewBox");
  const auto* paint_offset_translation = svg_with_view_box.FirstFragment()
                                             .PaintProperties()
                                             ->PaintOffsetTranslation();
  EXPECT_EQ(paint_offset_translation, &svg_with_view_box.FirstFragment()
                                           .LocalBorderBoxProperties()
                                           .Transform());
  EXPECT_EQ(DocScrollTranslation(), paint_offset_translation->Parent());
  EXPECT_EQ(gfx::Vector2dF(30, 20),
            paint_offset_translation->Get2dTranslation());
  EXPECT_EQ(PhysicalOffset(), svg_with_view_box.FirstFragment().PaintOffset());

  const auto* replaced_content_transform = svg_with_view_box.FirstFragment()
                                               .PaintProperties()
                                               ->ReplacedContentTransform();
  EXPECT_EQ(
      replaced_content_transform,
      &svg_with_view_box.FirstFragment().ContentsProperties().Transform());
  EXPECT_EQ(paint_offset_translation, replaced_content_transform->Parent());
  EXPECT_EQ(gfx::Vector2dF(-50, -50),
            replaced_content_transform->Get2dTranslation());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowHiddenScrollProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0px;
      }
      #overflowHidden {
        overflow: hidden;
        width: 5px;
        height: 3px;
      }
      .forceScroll {
        height: 79px;
      }
    </style>
    <div id='overflowHidden'>
      <div class='forceScroll'></div>
    </div>
  )HTML");

  Element* overflow_hidden =
      GetDocument().getElementById(AtomicString("overflowHidden"));

  const ObjectPaintProperties* overflow_hidden_scroll_properties =
      overflow_hidden->GetLayoutObject()->FirstFragment().PaintProperties();

  // No scroll translation when the scroll offset is zero.
  EXPECT_EQ(nullptr, overflow_hidden_scroll_properties->ScrollTranslation());
  EXPECT_EQ(nullptr, overflow_hidden_scroll_properties->Scroll());

  // Both scroll translation and scroll nodes when the scroll offset is not
  // zero.
  overflow_hidden->setScrollTop(37);
  UpdateAllLifecyclePhasesForTest();
  auto* scroll_translation =
      overflow_hidden_scroll_properties->ScrollTranslation();
  ASSERT_NE(nullptr, scroll_translation);
  EXPECT_EQ(gfx::Vector2dF(0, -37), scroll_translation->Get2dTranslation());
  auto* scroll = scroll_translation->ScrollNode();
  if (RuntimeEnabledFeatures::ScrollNodeForOverflowHiddenEnabled()) {
    ASSERT_NE(nullptr, scroll);
    EXPECT_EQ(scroll, overflow_hidden_scroll_properties->Scroll());
    EXPECT_FALSE(scroll->UserScrollableHorizontal());
    EXPECT_FALSE(scroll->UserScrollableVertical());
  } else {
    EXPECT_EQ(nullptr, scroll);
  }
}

TEST_P(PaintPropertyTreeBuilderTest, FrameOverflowHiddenScrollProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      html {
        margin: 0px;
        overflow: hidden;
        width: 300px;
        height: 300px;
      }
      .forceScroll {
        height: 5000px;
      }
    </style>
    <div class='forceScroll'></div>
  )HTML");

  GetDocument().domWindow()->scrollTo(0, 37);

  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(gfx::Vector2dF(0, -37), DocScrollTranslation()->Get2dTranslation());
  EXPECT_TRUE(DocScrollTranslation()->ScrollNode());
  EXPECT_TRUE(DocScroll());
}

TEST_P(PaintPropertyTreeBuilderTest, NestedScrollProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0px;
      }
      #overflowA {
        overflow: scroll;
        width: 5px;
        height: 3px;
      }
      #overflowB {
        overflow: scroll;
        width: 9px;
        height: 7px;
      }
      .forceScroll {
        height: 100px;
      }
    </style>
    <div id='overflowA'>
      <div id='overflowB'>
        <div class='forceScroll'></div>
      </div>
      <div class='forceScroll'></div>
    </div>
  )HTML");

  Element* overflow_a = GetDocument().getElementById(AtomicString("overflowA"));
  overflow_a->setScrollTop(37);
  Element* overflow_b = GetDocument().getElementById(AtomicString("overflowB"));
  overflow_b->setScrollTop(41);

  UpdateAllLifecyclePhasesForTest();

  const ObjectPaintProperties* overflow_a_scroll_properties =
      overflow_a->GetLayoutObject()->FirstFragment().PaintProperties();
  // Because the frameView is does not scroll, overflowA's scroll should be
  // under the root.
  auto* scroll_a_translation =
      overflow_a_scroll_properties->ScrollTranslation();
  auto* overflow_a_scroll_node = scroll_a_translation->ScrollNode();
  EXPECT_EQ(DocScroll(), overflow_a_scroll_node->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -37), scroll_a_translation->Get2dTranslation());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 3), overflow_a_scroll_node->ContainerRect());
  // 107 is the forceScroll element plus the height of the overflow scroll child
  // (overflowB).
  EXPECT_EQ(gfx::Rect(0, 0, 9, 107), overflow_a_scroll_node->ContentsRect());
  EXPECT_TRUE(overflow_a_scroll_node->UserScrollableHorizontal());
  EXPECT_TRUE(overflow_a_scroll_node->UserScrollableVertical());

  const ObjectPaintProperties* overflow_b_scroll_properties =
      overflow_b->GetLayoutObject()->FirstFragment().PaintProperties();
  // The overflow child's scroll node should be a child of the parent's
  // (overflowA) scroll node.
  auto* scroll_b_translation =
      overflow_b_scroll_properties->ScrollTranslation();
  auto* overflow_b_scroll_node = scroll_b_translation->ScrollNode();
  EXPECT_EQ(overflow_a_scroll_node, overflow_b_scroll_node->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -41), scroll_b_translation->Get2dTranslation());
  EXPECT_EQ(gfx::Rect(0, 0, 9, 7), overflow_b_scroll_node->ContainerRect());
  EXPECT_EQ(gfx::Rect(0, 0, 9, 100), overflow_b_scroll_node->ContentsRect());
  EXPECT_TRUE(overflow_b_scroll_node->UserScrollableHorizontal());
  EXPECT_TRUE(overflow_b_scroll_node->UserScrollableVertical());
}

TEST_P(PaintPropertyTreeBuilderTest, PositionedScrollerIsNotNested) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0px;
      }
      #overflow {
        overflow: scroll;
        width: 5px;
        height: 3px;
      }
      #absposOverflow {
        position: absolute;
        top: 0;
        left: 0;
        overflow: scroll;
        width: 9px;
        height: 7px;
      }
      #fixedOverflow {
        position: fixed;
        top: 0;
        left: 0;
        overflow: scroll;
        width: 13px;
        height: 11px;
      }
      .forceScroll {
        height: 4000px;
      }
    </style>
    <div id='overflow'>
      <div id='absposOverflow'>
        <div class='forceScroll'></div>
      </div>
      <div id='fixedOverflow'>
        <div class='forceScroll'></div>
      </div>
      <div class='forceScroll'></div>
    </div>
    <div class='forceScroll'></div>
  )HTML");

  Element* overflow = GetDocument().getElementById(AtomicString("overflow"));
  overflow->setScrollTop(37);
  Element* abspos_overflow =
      GetDocument().getElementById(AtomicString("absposOverflow"));
  abspos_overflow->setScrollTop(41);
  Element* fixed_overflow =
      GetDocument().getElementById(AtomicString("fixedOverflow"));
  fixed_overflow->setScrollTop(43);

  UpdateAllLifecyclePhasesForTest();

  // The frame should scroll due to the "forceScroll" element.
  EXPECT_NE(nullptr, DocScroll());

  const ObjectPaintProperties* overflow_scroll_properties =
      overflow->GetLayoutObject()->FirstFragment().PaintProperties();
  auto* scroll_translation = overflow_scroll_properties->ScrollTranslation();
  auto* overflow_scroll_node = scroll_translation->ScrollNode();
  EXPECT_EQ(
      DocScroll(),
      overflow_scroll_properties->ScrollTranslation()->ScrollNode()->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -37), scroll_translation->Get2dTranslation());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 3), overflow_scroll_node->ContainerRect());
  // The height should be 4000px because the (dom-order) overflow children are
  // positioned and do not contribute to the height. Only the 4000px
  // "forceScroll" height is present.
  EXPECT_EQ(gfx::Rect(0, 0, 5, 4000), overflow_scroll_node->ContentsRect());

  const ObjectPaintProperties* abspos_overflow_scroll_properties =
      abspos_overflow->GetLayoutObject()->FirstFragment().PaintProperties();
  auto* abspos_scroll_translation =
      abspos_overflow_scroll_properties->ScrollTranslation();
  auto* abspos_overflow_scroll_node = abspos_scroll_translation->ScrollNode();
  // The absolute position overflow scroll node is parented under the frame, not
  // the dom-order parent.
  EXPECT_EQ(DocScroll(), abspos_overflow_scroll_node->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -41),
            abspos_scroll_translation->Get2dTranslation());
  EXPECT_EQ(gfx::Rect(0, 0, 9, 7),
            abspos_overflow_scroll_node->ContainerRect());
  EXPECT_EQ(gfx::Rect(0, 0, 9, 4000),
            abspos_overflow_scroll_node->ContentsRect());

  const ObjectPaintProperties* fixed_overflow_scroll_properties =
      fixed_overflow->GetLayoutObject()->FirstFragment().PaintProperties();
  auto* fixed_scroll_translation =
      fixed_overflow_scroll_properties->ScrollTranslation();
  auto* fixed_overflow_scroll_node = fixed_scroll_translation->ScrollNode();
  // The fixed position overflow scroll node is parented under the frame, not
  // the dom-order parent.
  EXPECT_EQ(DocScroll(), fixed_overflow_scroll_node->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -43),
            fixed_scroll_translation->Get2dTranslation());
  EXPECT_EQ(gfx::Rect(0, 0, 13, 11),
            fixed_overflow_scroll_node->ContainerRect());
  EXPECT_EQ(gfx::Rect(0, 0, 13, 4000),
            fixed_overflow_scroll_node->ContentsRect());
}

TEST_P(PaintPropertyTreeBuilderTest, NestedPositionedScrollProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * {
        margin: 0px;
      }
      #overflowA {
        position: absolute;
        top: 7px;
        left: 11px;
        overflow: scroll;
        width: 20px;
        height: 20px;
      }
      #overflowB {
        position: absolute;
        top: 1px;
        left: 3px;
        overflow: scroll;
        width: 5px;
        height: 3px;
      }
      .forceScroll {
        height: 100px;
      }
    </style>
    <div id='overflowA'>
      <div id='overflowB'>
        <div class='forceScroll'></div>
      </div>
      <div class='forceScroll'></div>
    </div>
  )HTML");

  Element* overflow_a = GetDocument().getElementById(AtomicString("overflowA"));
  overflow_a->setScrollTop(37);
  Element* overflow_b = GetDocument().getElementById(AtomicString("overflowB"));
  overflow_b->setScrollTop(41);

  UpdateAllLifecyclePhasesForTest();

  const ObjectPaintProperties* overflow_a_scroll_properties =
      overflow_a->GetLayoutObject()->FirstFragment().PaintProperties();
  // Because the frameView is does not scroll, overflowA's scroll should be
  // under the root.
  auto* scroll_a_translation =
      overflow_a_scroll_properties->ScrollTranslation();
  auto* overflow_a_scroll_node = scroll_a_translation->ScrollNode();
  EXPECT_EQ(DocScroll(), overflow_a_scroll_node->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -37), scroll_a_translation->Get2dTranslation());
  EXPECT_EQ(gfx::Rect(0, 0, 20, 20), overflow_a_scroll_node->ContainerRect());
  // 100 is the forceScroll element's height because the overflow child does not
  // contribute to the height.
  EXPECT_EQ(gfx::Rect(0, 0, 20, 100), overflow_a_scroll_node->ContentsRect());
  EXPECT_TRUE(overflow_a_scroll_node->UserScrollableHorizontal());
  EXPECT_TRUE(overflow_a_scroll_node->UserScrollableVertical());

  const ObjectPaintProperties* overflow_b_scroll_properties =
      overflow_b->GetLayoutObject()->FirstFragment().PaintProperties();
  // The overflow child's scroll node should be a child of the parent's
  // (overflowA) scroll node.
  auto* scroll_b_translation =
      overflow_b_scroll_properties->ScrollTranslation();
  auto* overflow_b_scroll_node = scroll_b_translation->ScrollNode();
  EXPECT_EQ(overflow_a_scroll_node, overflow_b_scroll_node->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -41), scroll_b_translation->Get2dTranslation());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 3), overflow_b_scroll_node->ContainerRect());
  EXPECT_EQ(gfx::Rect(0, 0, 5, 100), overflow_b_scroll_node->ContentsRect());
  EXPECT_TRUE(overflow_b_scroll_node->UserScrollableHorizontal());
  EXPECT_TRUE(overflow_b_scroll_node->UserScrollableVertical());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootClip) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' style="width: 100px; height: 100.5px">
      <rect width='200' height='200' fill='red' />
    </svg>
  )HTML");

  const ClipPaintPropertyNode* clip = GetLayoutObjectByElementId("svg")
                                          ->FirstFragment()
                                          .PaintProperties()
                                          ->OverflowClip();
  EXPECT_EQ(DocContentClip(), clip->Parent());
  EXPECT_EQ(gfx::Vector2dF(8, 8), GetLayoutObjectByElementId("svg")
                                      ->FirstFragment()
                                      .PaintProperties()
                                      ->PaintOffsetTranslation()
                                      ->Get2dTranslation());
  // TODO(crbug.com/1248598): For now we pixel snap both layout clip rect and
  // paint clip rect for replaced elements.
  EXPECT_CLIP_RECT(FloatRoundedRect(0, 0, 100, 101), clip);
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootNoClip) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' xmlns='http://www.w3.org/2000/svg' width='100px'
        height='100px' style='overflow: visible'>
      <rect width='200' height='200' fill='red' />
    </svg>
  )HTML");

  EXPECT_FALSE(GetLayoutObjectByElementId("svg")
                   ->FirstFragment()
                   .PaintProperties()
                   ->OverflowClip());
}

TEST_P(PaintPropertyTreeBuilderTest, MainThreadScrollReasonsWithoutScrolling) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #overflow {
        overflow: scroll;
        width: 100px;
        height: 100px;
      }
      .backgroundAttachmentFixed {
        background-image: url('foo');
        background-attachment: fixed;
        width: 10px;
        height: 10px;
      }
      .forceScroll {
        height: 4000px;
      }
    </style>
    <div id='overflow'>
      <div class='backgroundAttachmentFixed'></div>
    </div>
    <div class='forceScroll'></div>
  )HTML");
  Element* overflow = GetDocument().getElementById(AtomicString("overflow"));
  EXPECT_TRUE(DocScroll()->HasBackgroundAttachmentFixedDescendants());
  // No scroll node is needed.
  EXPECT_EQ(overflow->GetLayoutObject()
                ->FirstFragment()
                .PaintProperties()
                ->ScrollTranslation(),
            nullptr);
}

TEST_P(PaintPropertyTreeBuilderTest, PaintOffsetsUnderMultiColumnScrolled) {
  SetBodyInnerHTML(R"HTML(
    <!doctype HTML>
    <div style='columns: 1;'>
       <div id=scroller style='height: 400px; width: 400px; overflow: auto;'>
         <div style='width: 50px; height: 1000px; background: lightgray'>
       </div>
     </div>
    </div>
  )HTML");

  LayoutBox* scroller = GetLayoutBoxByElementId("scroller");
  scroller->GetScrollableArea()->ScrollBy(ScrollOffset(0, 300),
                                          mojom::blink::ScrollType::kUser);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(gfx::Vector2dF(8, 8), scroller->FirstFragment()
                                      .PaintProperties()
                                      ->PaintOffsetTranslation()
                                      ->Get2dTranslation());
}

TEST_P(PaintPropertyTreeBuilderTest,
       PaintOffsetsUnderMultiColumnWithVisualOverflow) {
  SetBodyInnerHTML(R"HTML(
    <div style='columns: 2; width: 300px; column-gap: 0; height: 100px'>
      <div id=target1 style='outline: 2px solid black; width: 100px;
          height: 100px'></div>
      <div id=target2 style='outline: 2px solid black; width: 100px;
          height: 100px'></div>
    </div>
  )HTML");

  LayoutObject* target1 = GetLayoutObjectByElementId("target1");

  // Outline does not affect paint offset, since it is positioned to the
  // top-left of the border box.
  EXPECT_EQ(PhysicalOffset(8, 8), target1->FirstFragment().PaintOffset());
  // |target1| is only in the first column.
  EXPECT_FALSE(target1->IsFragmented());

  LayoutObject* target2 = GetLayoutObjectByElementId("target2");
  EXPECT_EQ(PhysicalOffset(158, 8), target2->FirstFragment().PaintOffset());
  // |target2| is only in the second column.
  EXPECT_FALSE(target2->IsFragmented());
}

TEST_P(PaintPropertyTreeBuilderTest,
       PaintOffsetsUnderMultiColumnWithScrollableOverflow) {
  SetBodyInnerHTML(R"HTML(
    <div style='columns: 2; width: 300px; column-gap: 0; height: 100px'>
      <div id='parent' style='outline: 2px solid black;
          width: 100px; height: 100px'>
        <div id='child' style='width: 100px; height: 200px'></div>
      </div>
    </div>
  )HTML");

  const LayoutBox* parent = GetLayoutBoxByElementId("parent");

  // The parent will need to generate 2 fragments, to hold child fragments
  // that contribute to scrollable overflow.
  ASSERT_EQ(2u, NumFragments(parent));
  EXPECT_EQ(PhysicalOffset(158, 8), FragmentAt(parent, 1).PaintOffset());
  // But since the #parent doesn't take up any space on its own in the second
  // fragment, the block-size should be 0.
  ASSERT_EQ(2u, parent->PhysicalFragmentCount());
  EXPECT_EQ(LayoutUnit(100), parent->GetPhysicalFragment(0)->Size().height);
  EXPECT_EQ(LayoutUnit(), parent->GetPhysicalFragment(1)->Size().height);
  EXPECT_EQ(PhysicalOffset(8, 8), FragmentAt(parent, 0).PaintOffset());

  LayoutObject* child = GetLayoutObjectByElementId("child");
  ASSERT_EQ(2u, NumFragments(child));
  EXPECT_EQ(PhysicalOffset(8, 8), FragmentAt(child, 0).PaintOffset());
  EXPECT_EQ(PhysicalOffset(158, 8), FragmentAt(child, 1).PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest, SpanFragmentsLimitedToSize) {
  SetBodyInnerHTML(R"HTML(
    <div style='columns: 10; height: 100px; width: 5000px'>
      <div style='width: 50px; height: 5000px'>
        <span id=target>Text</span>
      </div>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(1u, NumFragments(target));
}

TEST_P(PaintPropertyTreeBuilderTest,
       PaintOffsetUnderMulticolumnScrollFixedPos) {
  SetBodyInnerHTML(R"HTML(
    <div id=fixed style='position: fixed; columns: 2; column-gap: 20px; width: 120px;'>
      <div id="first" style='height: 20px; background: lightblue'></div>
      <div id="second" style='height: 20px; background: lightgray'></div>
    </div>
    <div style='height: 2000px'></div>
  )HTML");

  auto test = [&]() {
    LayoutObject* first = GetLayoutObjectByElementId("first");
    LayoutObject* second = GetLayoutObjectByElementId("second");
    EXPECT_EQ(PhysicalOffset(), first->FirstFragment().PaintOffset());
    EXPECT_FALSE(first->IsFragmented());
    EXPECT_EQ(PhysicalOffset(70, 0), second->FirstFragment().PaintOffset());
    EXPECT_FALSE(second->IsFragmented());
  };

  test();

  GetDocument().View()->LayoutViewport()->ScrollBy(
      ScrollOffset(0, 25), mojom::blink::ScrollType::kUser);
  UpdateAllLifecyclePhasesForTest();

  test();
}

TEST_P(PaintPropertyTreeBuilderTest, FragmentsUnderMultiColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      .space { height: 30px; }
      .abs { position: absolute; width: 20px; height: 20px; }
    </style>
    <div style='position:relative; width:400px; height:400px;'>
      <div style='columns:2; width: 200px; column-gap: 0'>
        <div id=relpos style='position: relative'>
          <div id=space1 class=space></div>
          <div id=space2 class=space></div>
          <div id=spanner style='column-span: all'>
            <div id=normal style='height: 50px'></div>
            <div id=top-left class=abs style='top: 0; left: 0'></div>
            <div id=bottom-right class=abs style='bottom: 0; right: 0'></div>
          </div>
          <div id=space3 class=space></div>
          <div id=space4 class=space></div>
        </div>
      </div>
    </div>
  )HTML");

  const auto* relpos = GetLayoutObjectByElementId("relpos");
  EXPECT_EQ(4u, NumFragments(relpos));

  EXPECT_EQ(PhysicalOffset(), FragmentAt(relpos, 0).PaintOffset());
  EXPECT_EQ(0u, FragmentAt(relpos, 0).FragmentID());
  EXPECT_EQ(nullptr, FragmentAt(relpos, 0).PaintProperties());

  EXPECT_EQ(PhysicalOffset(100, 0), FragmentAt(relpos, 1).PaintOffset());
  EXPECT_EQ(1u, FragmentAt(relpos, 1).FragmentID());
  EXPECT_EQ(nullptr, FragmentAt(relpos, 1).PaintProperties());

  EXPECT_EQ(PhysicalOffset(0, 80), FragmentAt(relpos, 2).PaintOffset());
  EXPECT_EQ(2u, FragmentAt(relpos, 2).FragmentID());
  EXPECT_EQ(nullptr, FragmentAt(relpos, 2).PaintProperties());

  EXPECT_EQ(PhysicalOffset(100, 80), FragmentAt(relpos, 3).PaintOffset());
  EXPECT_EQ(3u, FragmentAt(relpos, 3).FragmentID());
  EXPECT_EQ(nullptr, FragmentAt(relpos, 3).PaintProperties());

  // Above the spanner.
  // Column 1.
  const auto* space1 = GetLayoutObjectByElementId("space1");
  EXPECT_EQ(1u, NumFragments(space1));
  EXPECT_EQ(nullptr, space1->FirstFragment().PaintProperties());
  EXPECT_EQ(PhysicalOffset(), space1->FirstFragment().PaintOffset());
  const auto* space2 = GetLayoutObjectByElementId("space2");
  EXPECT_EQ(1u, NumFragments(space2));
  EXPECT_EQ(nullptr, space2->FirstFragment().PaintProperties());
  EXPECT_EQ(PhysicalOffset(100, 0), space2->FirstFragment().PaintOffset());

  // The spanner's normal flow.
  LayoutObject* spanner = GetLayoutObjectByElementId("spanner");
  EXPECT_EQ(1u, NumFragments(spanner));
  EXPECT_EQ(nullptr, spanner->FirstFragment().PaintProperties());
  EXPECT_EQ(PhysicalOffset(0, 30), spanner->FirstFragment().PaintOffset());
  LayoutObject* normal = GetLayoutObjectByElementId("normal");
  EXPECT_EQ(1u, NumFragments(normal));
  EXPECT_EQ(nullptr, normal->FirstFragment().PaintProperties());
  EXPECT_EQ(PhysicalOffset(0, 30), normal->FirstFragment().PaintOffset());

  // Below the spanner.
  const auto* space3 = GetLayoutObjectByElementId("space3");
  EXPECT_EQ(1u, NumFragments(space3));
  EXPECT_EQ(nullptr, space3->FirstFragment().PaintProperties());
  EXPECT_EQ(PhysicalOffset(0, 80), space3->FirstFragment().PaintOffset());
  const auto* space4 = GetLayoutObjectByElementId("space4");
  EXPECT_EQ(1u, NumFragments(space4));
  EXPECT_EQ(nullptr, space4->FirstFragment().PaintProperties());
  EXPECT_EQ(PhysicalOffset(100, 80), space4->FirstFragment().PaintOffset());

  // Out-of-flow positioned descendants of the spanner. They are laid out in
  // the relative-position container.

  // "top-left" should be aligned to the top-left corner of space1.
  const auto* top_left = GetLayoutObjectByElementId("top-left");
  EXPECT_EQ(1u, NumFragments(top_left));
  EXPECT_EQ(PhysicalOffset(), top_left->FirstFragment().PaintOffset());

  const auto* bottom_right = GetLayoutObjectByElementId("bottom-right");
  EXPECT_EQ(1u, NumFragments(bottom_right));
  // According to the spec the containing block of a spanner is the multicol
  // container. Therefore, any OOF descendants of a spanner will ignore any
  // containing blocks inside the multicol container.
  EXPECT_EQ(PhysicalOffset(380, 380),
            bottom_right->FirstFragment().PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest,
       FragmentsUnderMultiColumnVerticalRLWithOverflow) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='multicol' style='columns:2; column-fill:auto; column-gap: 0;
        width: 200px; height: 200px; writing-mode: vertical-rl'>
      <div id='container' style='width: 100px'>
        <div id='content' style='width: 400px'></div>
      </div>
    </div>
  )HTML");

  LayoutObject* thread =
      GetLayoutObjectByElementId("multicol")->SlowFirstChild();
  LayoutObject* container = GetLayoutObjectByElementId("container");
  EXPECT_TRUE(thread->IsLayoutFlowThread());
  ASSERT_EQ(2u, NumFragments(container));
  EXPECT_EQ(PhysicalOffset(100, 0), FragmentAt(container, 0).PaintOffset());
  EXPECT_EQ(0u, FragmentAt(container, 0).FragmentID());
  EXPECT_EQ(PhysicalOffset(200, 100), FragmentAt(container, 1).PaintOffset());
  EXPECT_EQ(1u, FragmentAt(container, 1).FragmentID());

  LayoutObject* content = GetLayoutObjectByElementId("content");
  EXPECT_EQ(2u, NumFragments(content));

    EXPECT_EQ(PhysicalOffset(), FragmentAt(content, 0).PaintOffset());
    EXPECT_EQ(0u, FragmentAt(content, 0).FragmentID());
    EXPECT_EQ(PhysicalOffset(0, 100), FragmentAt(content, 1).PaintOffset());
    EXPECT_EQ(1u, FragmentAt(content, 1).FragmentID());
}

TEST_P(PaintPropertyTreeBuilderTest, LayerUnderOverflowClipUnderMultiColumn) {
  SetBodyInnerHTML(R"HTML(
    <div id='multicol' style='columns:2'>
      <div id='wrapper'>
        <div id='clip' style='height: 200px; overflow: hidden'>
          <div id='layer' style='position: relative; height: 800px'></div>
        </div>
        <div style='height: 200px'></div>
      </div>
    </div>
  )HTML");

  const auto* wrapper = GetLayoutObjectByElementId("wrapper");
  EXPECT_EQ(2u, NumFragments(wrapper));
  EXPECT_EQ(1u, NumFragments(GetLayoutObjectByElementId("clip")));
  EXPECT_EQ(1u, NumFragments(GetLayoutObjectByElementId("layer")));
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowClipUnderMultiColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div style='columns: 4; height: 100px; column-fill: auto; column-gap: 0'>
      <div id='clip' style='height: 200px; overflow: clip'>
        <div id='child1' style='height: 400px'></div>
        <div id='child2' style='height: 400px'></div>
      </div>
    </div>
  )HTML");

  const auto* clip = GetLayoutObjectByElementId("clip");
  const auto* child1 = GetLayoutObjectByElementId("child1");
  const auto* child2 = GetLayoutObjectByElementId("child2");
  ASSERT_EQ(2u, NumFragments(clip));
  ASSERT_EQ(2u, NumFragments(child1));
  ASSERT_EQ(1u, NumFragments(child2));
  EXPECT_EQ(PhysicalOffset(), FragmentAt(clip, 0).PaintOffset());
  EXPECT_EQ(0u, FragmentAt(clip, 0).FragmentID());
  EXPECT_EQ(PhysicalOffset(200, 0), FragmentAt(clip, 1).PaintOffset());
  EXPECT_EQ(1u, FragmentAt(clip, 1).FragmentID());
  EXPECT_EQ(PhysicalOffset(), FragmentAt(child1, 0).PaintOffset());
  EXPECT_EQ(0u, FragmentAt(child1, 0).FragmentID());
  EXPECT_EQ(PhysicalOffset(200, 0), FragmentAt(child1, 1).PaintOffset());
  EXPECT_EQ(1u, FragmentAt(child1, 1).FragmentID());
  EXPECT_EQ(PhysicalOffset(200, 300), FragmentAt(child2, 0).PaintOffset());
  EXPECT_EQ(1u, FragmentAt(child2, 0).FragmentID());
}

TEST_P(PaintPropertyTreeBuilderTest, CompositedUnderMultiColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='multicol' style='columns:3; column-fill:auto; column-gap: 0;
        width: 300px; height: 200px'>
      <div id='wrapper'>
        <div style='height: 300px'></div>
        <div id='composited' style='will-change: transform; height: 300px'>
          <div id='non-composited-child' style='height: 150px'></div>
          <div id='composited-child'
               style='will-change: transform; height: 150px'></div>
        </div>
      </div>
    </div>
  )HTML");

  LayoutObject* wrapper = GetLayoutObjectByElementId("wrapper");
  ASSERT_EQ(3u, NumFragments(wrapper));
  EXPECT_EQ(PhysicalOffset(0, 0), FragmentAt(wrapper, 0).PaintOffset());
  EXPECT_EQ(0u, FragmentAt(wrapper, 0).FragmentID());
  EXPECT_EQ(PhysicalOffset(100, 0), FragmentAt(wrapper, 1).PaintOffset());
  EXPECT_EQ(1u, FragmentAt(wrapper, 1).FragmentID());
  EXPECT_EQ(PhysicalOffset(200, 0), FragmentAt(wrapper, 2).PaintOffset());
  EXPECT_EQ(2u, FragmentAt(wrapper, 2).FragmentID());

  LayoutObject* composited = GetLayoutObjectByElementId("composited");
  LayoutObject* non_composited_child =
      GetLayoutObjectByElementId("non-composited-child");
  LayoutObject* composited_child =
      GetLayoutObjectByElementId("composited-child");

  EXPECT_EQ(2u, NumFragments(composited));
  EXPECT_EQ(PhysicalOffset(0, 0), FragmentAt(composited, 0).PaintOffset());
  EXPECT_EQ(1u, FragmentAt(composited, 0).FragmentID());
  EXPECT_EQ(PhysicalOffset(0, 0), FragmentAt(composited, 1).PaintOffset());
  EXPECT_EQ(2u, FragmentAt(composited, 1).FragmentID());
  EXPECT_EQ(2u, NumFragments(non_composited_child));
  EXPECT_EQ(PhysicalOffset(0, 0),
            FragmentAt(non_composited_child, 0).PaintOffset());
  EXPECT_EQ(1u, FragmentAt(non_composited_child, 0).FragmentID());
  EXPECT_EQ(PhysicalOffset(0, 0),
            FragmentAt(non_composited_child, 1).PaintOffset());
  EXPECT_EQ(2u, FragmentAt(non_composited_child, 1).FragmentID());
  EXPECT_EQ(1u, NumFragments(composited_child));
  EXPECT_EQ(PhysicalOffset(0, 0),
            FragmentAt(composited_child, 0).PaintOffset());
  EXPECT_EQ(2u, FragmentAt(composited_child, 0).FragmentID());
}

// Ensures no crash with multi-column containing relative-position inline with
// spanner with absolute-position children.
TEST_P(PaintPropertyTreeBuilderTest,
       MultiColumnInlineRelativeAndSpannerAndAbsPos) {
  SetBodyInnerHTML(R"HTML(
    <div style='columns:2; width: 200px; column-gap: 0'>
      <span style='position: relative'>
        <span id=spanner style='column-span: all'>
          <div id=absolute style='position: absolute'>absolute</div>
        </span>
      </span>
    </div>
  )HTML");
  // The "spanner" isn't a real spanner because it's an inline.
  EXPECT_FALSE(GetLayoutObjectByElementId("spanner")->IsColumnSpanAll());

  SetBodyInnerHTML(R"HTML(
    <div style='columns:2; width: 200px; column-gap: 0'>
      <span style='position: relative'>
        <div id=spanner style='column-span: all'>
          <div id=absolute style='position: absolute'>absolute</div>
        </div>
      </span>
    </div>
  )HTML");
  // There should be anonymous block created containing the inline "relative",
  // serving as the container of "absolute".
  EXPECT_TRUE(
      GetLayoutObjectByElementId("absolute")->Container()->IsLayoutBlock());
}

TEST_P(PaintPropertyTreeBuilderTest, FrameUnderMulticol) {
  SetBodyInnerHTML(R"HTML(
    <div style='columns: 2; width: 200px; height: 100px; coloum-gap: 0'>
      <iframe style='width: 50px; height: 150px'></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>
      body { margin: 0; }
      div { height: 60px; }
    </style>
    <div id='div1' style='background: blue'></div>
    <div id='div2' style='background: green'></div>
  )HTML");

  // This should not crash on duplicated subsequences in the iframe.
  UpdateAllLifecyclePhasesForTest();

  // TODO(crbug.com/797779): Add code to verify fragments under the iframe.
}

TEST_P(PaintPropertyTreeBuilderTest, CompositedMulticolFrameUnderMulticol) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div style='columns: 3; column-gap: 0; column-fill: auto;
        width: 300px; height: 200px'>
      <div style='height: 300px'></div>
      <iframe id='iframe' style='will-change: transform;
          width: 90px; height: 300px; border: none; background: green'></iframe>
    </div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <style>body { margin: 0 }</style>
    <div style='columns: 2; column-gap: 0; column-fill: auto;
        width: 80px; height: 100px'>
      <div id="multicolContent" style='height: 200px; background: blue'></div>
    </div>
  )HTML");

  // This should not crash on duplicated subsequences in the iframe.
  UpdateAllLifecyclePhasesForTest();

  // TODO(crbug.com/797779): Add code to verify fragments under the iframe.
}

// Test that becoming unfragmented correctly updates FragmentData. This means
// clearing the fragment ID. Also check the paint offset, for good measure.
TEST_P(PaintPropertyTreeBuilderTest, BecomingUnfragmented) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
         width: 30px; height: 20px; position: relative;
      }
    </style>
    <div style='columns:2; column-fill:auto; column-gap:0; height:20px; width:400px;'>
       <div style='height: 20px'></div>
       <div id=target></div>
     </div>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(1u, target->FirstFragment().FragmentID());
  EXPECT_EQ(PhysicalOffset(LayoutUnit(208), LayoutUnit(8)),
            target->FirstFragment().PaintOffset());
  Element* target_element =
      GetDocument().getElementById(AtomicString("target"));

  target_element->setAttribute(html_names::kStyleAttr,
                               AtomicString("position: absolute"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(0u, target->FirstFragment().FragmentID());
  EXPECT_EQ(PhysicalOffset(LayoutUnit(8), LayoutUnit(28)),
            target->FirstFragment().PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest, Reflection) {
  SetBodyInnerHTML(
      "<div id='filter' style='-webkit-box-reflect: below; height:1000px;'>"
      "</div>");
  const ObjectPaintProperties* filter_properties =
      GetLayoutObjectByElementId("filter")->FirstFragment().PaintProperties();
  EXPECT_EQ(DocScrollTranslation(),
            filter_properties->PaintOffsetTranslation()->Parent());
  EXPECT_EQ(gfx::Vector2dF(8, 8),
            filter_properties->PaintOffsetTranslation()->Get2dTranslation());
  EXPECT_TRUE(filter_properties->Filter()->Parent()->IsRoot());
  EXPECT_EQ(filter_properties->PaintOffsetTranslation(),
            &filter_properties->Filter()->LocalTransformSpace());
  EXPECT_EQ(DocContentClip(), filter_properties->Filter()->OutputClip());
}

TEST_P(PaintPropertyTreeBuilderTest, SimpleFilter) {
  SetBodyInnerHTML(
      "<div id='filter' style='filter:opacity(0.5); height:1000px;'>"
      "</div>");
  const ObjectPaintProperties* filter_properties =
      GetLayoutObjectByElementId("filter")->FirstFragment().PaintProperties();
  EXPECT_FALSE(filter_properties->PaintOffsetTranslation());
  EXPECT_TRUE(filter_properties->Filter()->Parent()->IsRoot());
  EXPECT_FALSE(filter_properties->PixelMovingFilterClipExpander());
  EXPECT_EQ(DocScrollTranslation(),
            &filter_properties->Filter()->LocalTransformSpace());
  EXPECT_EQ(DocContentClip(), filter_properties->Filter()->OutputClip());
}

TEST_P(PaintPropertyTreeBuilderTest, PixelMovingFilter) {
  SetBodyInnerHTML(
      "<div id='filter' style='filter:blur(10px); height:1000px;'>"
      "</div>");
  const ObjectPaintProperties* filter_properties =
      GetLayoutObjectByElementId("filter")->FirstFragment().PaintProperties();
  EXPECT_FALSE(filter_properties->PaintOffsetTranslation());

  auto* filter = filter_properties->Filter();
  ASSERT_TRUE(filter);
  EXPECT_TRUE(filter->Parent()->IsRoot());
  EXPECT_TRUE(filter->HasFilterThatMovesPixels());
  EXPECT_EQ(DocScrollTranslation(), &filter->LocalTransformSpace());
  EXPECT_EQ(DocContentClip(), filter->OutputClip());

  auto* clip = filter_properties->PixelMovingFilterClipExpander();
  ASSERT_TRUE(clip);
  EXPECT_EQ(filter->OutputClip(), clip->Parent());
  EXPECT_EQ(&clip->LocalTransformSpace(), &filter->LocalTransformSpace());
  EXPECT_EQ(filter, clip->PixelMovingFilter());
  EXPECT_TRUE(clip->LayoutClipRect().IsInfinite());
  EXPECT_EQ(gfx::RectF(InfiniteIntRect()), clip->PaintClipRect().Rect());
}

TEST_P(PaintPropertyTreeBuilderTest, SimpleFilterWithWillChangeTransform) {
  SetBodyInnerHTML(R"HTML(
    <div id='filter' style='filter:opacity(0.5); height:1000px;
                            will-change: transform'>"
    </div>
  )HTML");

  auto* properties = PaintPropertiesForElement("filter");
  ASSERT_TRUE(properties);
  auto* filter = properties->Filter();
  ASSERT_TRUE(filter);
  EXPECT_TRUE(filter->HasDirectCompositingReasons());
  EXPECT_FALSE(properties->PixelMovingFilterClipExpander());
}

TEST_P(PaintPropertyTreeBuilderTest, WillChangeFilterCreatesClipExpander) {
  SetBodyInnerHTML(
      "<div id='filter' style='height:1000px; will-change: filter'>");

  auto* properties = PaintPropertiesForElement("filter");
  ASSERT_TRUE(properties);
  auto* filter = properties->Filter();
  ASSERT_TRUE(filter);
  EXPECT_TRUE(filter->HasDirectCompositingReasons());
  auto* clip_expander = properties->PixelMovingFilterClipExpander();
  ASSERT_TRUE(clip_expander);
  EXPECT_EQ(filter, clip_expander->PixelMovingFilter());
}

TEST_P(PaintPropertyTreeBuilderTest, FilterReparentClips) {
  SetBodyInnerHTML(R"HTML(
    <div id='clip' style='overflow:hidden;'>
      <div id='filter' style='filter:opacity(0.5); height:1000px;'>
        <div id='child' style='position:fixed;'></div>
      </div>
    </div>
  )HTML");
  const ObjectPaintProperties* clip_properties =
      GetLayoutObjectByElementId("clip")->FirstFragment().PaintProperties();
  const ObjectPaintProperties* filter_properties =
      GetLayoutObjectByElementId("filter")->FirstFragment().PaintProperties();
  EXPECT_TRUE(filter_properties->Filter()->Parent()->IsRoot());
  EXPECT_EQ(clip_properties->OverflowClip(),
            filter_properties->Filter()->OutputClip());
  EXPECT_EQ(DocScrollTranslation(),
            &filter_properties->Filter()->LocalTransformSpace());

  const auto& child_paint_state = GetLayoutObjectByElementId("child")
                                      ->FirstFragment()
                                      .LocalBorderBoxProperties();

  // This will change once we added clip expansion node.
  EXPECT_EQ(filter_properties->Filter()->OutputClip(),
            &child_paint_state.Clip());
  EXPECT_EQ(filter_properties->Filter(), &child_paint_state.Effect());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformOriginWithAndWithoutTransform) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      div {
        width: 400px;
        height: 100px;
      }
      #translation {
        transform: translate(100px, 200px);
        transform-origin: 75% 75% 0;
      }
      #scale {
        transform: scale(2);
        transform-origin: 75% 75% 0;
      }
      #willChange {
        will-change: transform;
        transform-origin: 75% 75% 0;
      }
    </style>
    <div id='translation'></div>
    <div id='scale'></div>
    <div id='willChange'></div>
  )HTML");

  auto* translation = PaintPropertiesForElement("translation")->Transform();
  EXPECT_EQ(gfx::Vector2dF(100, 200), translation->Get2dTranslation());
  EXPECT_EQ(gfx::Point3F(300, 75, 0), translation->Origin());

  auto* scale = PaintPropertiesForElement("scale")->Transform();
  EXPECT_EQ(MakeScaleMatrix(2), scale->Matrix());
  EXPECT_EQ(gfx::Point3F(300, 75, 0), scale->Origin());

  auto* will_change = PaintPropertiesForElement("willChange")->Transform();
  EXPECT_TRUE(will_change->IsIdentity());
  EXPECT_EQ(gfx::Point3F(), will_change->Origin());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformOriginWithAndWithoutMotionPath) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      div {
        width: 100px;
        height: 100px;
      }
      #motionPath {
        position: absolute;
        offset-path: path('M0 0 L 200 400');
        offset-distance: 50%;
        offset-rotate: 0deg;
        transform-origin: 50% 50% 0;
      }
      #willChange {
        will-change: transform;
        transform-origin: 50% 50% 0;
      }
    </style>
    <div id='motionPath'></div>
    <div id='willChange'></div>
  )HTML");

  auto* motion_path = GetLayoutObjectByElementId("motionPath");
  auto* motion_path_properties = motion_path->FirstFragment().PaintProperties();
  EXPECT_EQ(motion_path_properties->Transform(), nullptr);
  EXPECT_EQ(gfx::Vector2dF(50, 150),
            motion_path_properties->Offset()->Get2dTranslation());
  EXPECT_EQ(gfx::Point3F(50, 50, 0),
            motion_path_properties->Offset()->Origin());

  auto* will_change = GetLayoutObjectByElementId("willChange");
  auto* will_change_properties = will_change->FirstFragment().PaintProperties();
  EXPECT_EQ(will_change_properties->Offset(), nullptr);
  EXPECT_TRUE(will_change_properties->Transform()->IsIdentity());
  EXPECT_EQ(gfx::Point3F(), will_change_properties->Transform()->Origin());
}

TEST_P(PaintPropertyTreeBuilderTest, ChangePositionUpdateDescendantProperties) {
  SetBodyInnerHTML(R"HTML(
    <style>
      * { margin: 0; }
      #ancestor { position: absolute; overflow: hidden }
      #descendant { position: absolute }
    </style>
    <div id='ancestor'>
      <div id='descendant'></div>
    </div>
  )HTML");

  LayoutObject* ancestor = GetLayoutObjectByElementId("ancestor");
  LayoutObject* descendant = GetLayoutObjectByElementId("descendant");
  EXPECT_EQ(ancestor->FirstFragment().PaintProperties()->OverflowClip(),
            &descendant->FirstFragment().LocalBorderBoxProperties().Clip());

  To<Element>(ancestor->GetNode())
      ->setAttribute(html_names::kStyleAttr, AtomicString("position: static"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_NE(ancestor->FirstFragment().PaintProperties()->OverflowClip(),
            &descendant->FirstFragment().LocalBorderBoxProperties().Clip());
}

TEST_P(PaintPropertyTreeBuilderTest,
       TransformNodeNotAnimatedStillHasCompositorElementId) {
  SetBodyInnerHTML("<div id='target' style='transform: translateX(2em)'></div");
  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  EXPECT_TRUE(properties->Transform());
  EXPECT_NE(CompositorElementId(),
            properties->Transform()->GetCompositorElementId());
}

TEST_P(PaintPropertyTreeBuilderTest,
       EffectNodeNotAnimatedStillHasCompositorElementId) {
  SetBodyInnerHTML("<div id='target' style='opacity: 0.5'></div");
  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  EXPECT_TRUE(properties->Effect());
  // TODO(flackr): Revisit whether effect ElementId should still exist when
  // animations are no longer keyed off of the existence it:
  // https://crbug.com/900241
  EXPECT_NE(CompositorElementId(),
            properties->Effect()->GetCompositorElementId());
}

TEST_P(PaintPropertyTreeBuilderTest,
       TransformNodeAnimatedHasCompositorElementId) {
  LoadTestData("transform-animation.html");
  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  EXPECT_TRUE(properties->Transform());
  EXPECT_NE(CompositorElementId(),
            properties->Transform()->GetCompositorElementId());
  EXPECT_TRUE(properties->Transform()->HasActiveTransformAnimation());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectNodeAnimatedHasCompositorElementId) {
  LoadTestData("opacity-animation.html");
  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  EXPECT_TRUE(properties->Effect());
  EXPECT_NE(CompositorElementId(),
            properties->Effect()->GetCompositorElementId());
  EXPECT_TRUE(properties->Effect()->HasActiveOpacityAnimation());
}

TEST_P(PaintPropertyTreeBuilderTest, FloatUnderInline) {
  SetBodyInnerHTML(R"HTML(
    <div style='position: absolute; top: 55px; left: 66px'>
      <span id='span'
          style='position: relative; top: 100px; left: 200px; opacity: 0.5'>
        <div id='target'
             style='overflow: hidden; float: left; width: 3px; height: 4px'>
        </div>
      </span>
    </div>
  )HTML");

  LayoutObject* span = GetLayoutObjectByElementId("span");
  const auto* effect = span->FirstFragment().PaintProperties()->Effect();
  ASSERT_TRUE(effect);
  EXPECT_EQ(0.5f, effect->Opacity());

  LayoutObject* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(PhysicalOffset(266, 155), target->FirstFragment().PaintOffset());
  EXPECT_EQ(effect,
            &target->FirstFragment().LocalBorderBoxProperties().Effect());
}

TEST_P(PaintPropertyTreeBuilderTest, ScrollNodeHasCompositorElementId) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='overflow: auto; width: 100px; height: 100px'>
      <div style='width: 200px; height: 200px'></div>
    </div>
  )HTML");

  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  // The scroll translation node should not have the element id as it should be
  // stored directly on the ScrollNode.
  EXPECT_EQ(CompositorElementId(),
            properties->ScrollTranslation()->GetCompositorElementId());
  EXPECT_NE(CompositorElementId(),
            properties->Scroll()->GetCompositorElementId());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowClipSubpixelPosition) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 20px 30px; }</style>
    <div id='clipper'
        style='position: relative; overflow: hidden;
               width: 400px; height: 300px; left: 1.5px'>
      <div style='width: 1000px; height: 1000px'></div>
    </div>
  )HTML");

  auto* clipper =
      To<LayoutBoxModelObject>(GetLayoutObjectByElementId("clipper"));
  const ObjectPaintProperties* clip_properties =
      clipper->FirstFragment().PaintProperties();

  EXPECT_EQ(PhysicalOffset(LayoutUnit(31.5), LayoutUnit(20)),
            clipper->FirstFragment().PaintOffset());
  // Result is pixel-snapped.
  EXPECT_EQ(FloatClipRect(gfx::RectF(31.5, 20, 400, 300)),
            clip_properties->OverflowClip()->LayoutClipRect());
  EXPECT_EQ(FloatRoundedRect(32, 20, 400, 300),
            clip_properties->OverflowClip()->PaintClipRect());
}

TEST_P(PaintPropertyTreeBuilderTest, MaskSimple) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width:300px; height:200.5px;
        -webkit-mask:linear-gradient(red,red)'>
      Lorem ipsum
    </div>
  )HTML");

  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  const ClipPaintPropertyNode* mask_clip = properties->MaskClip();

  const auto* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(mask_clip,
            &target->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(DocContentClip(), mask_clip->Parent());
  EXPECT_EQ(FloatClipRect(gfx::RectF(8, 8, 300, 200.5)),
            mask_clip->LayoutClipRect());
  EXPECT_EQ(FloatRoundedRect(8, 8, 300, 201), mask_clip->PaintClipRect());

  EXPECT_EQ(properties->Effect(),
            &target->FirstFragment().LocalBorderBoxProperties().Effect());
  EXPECT_TRUE(properties->Effect()->Parent()->IsRoot());
  EXPECT_EQ(SkBlendMode::kSrcOver, properties->Effect()->BlendMode());
  EXPECT_EQ(mask_clip->Parent(), properties->Effect()->OutputClip());

  EXPECT_EQ(properties->Effect(), properties->Mask()->Parent());
  EXPECT_EQ(SkBlendMode::kDstIn, properties->Mask()->BlendMode());
  EXPECT_EQ(mask_clip->Parent(), properties->Mask()->OutputClip());
}

TEST_P(PaintPropertyTreeBuilderTest, MaskWithOutset) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width:300px; height:200px;
        -webkit-mask-box-image-source:linear-gradient(red,red);
        -webkit-mask-box-image-outset:10px 20px;'>
      Lorem ipsum
    </div>
  )HTML");

  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  const ClipPaintPropertyNode* mask_clip = properties->MaskClip();

  const auto* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(mask_clip,
            &target->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(DocContentClip(), mask_clip->Parent());
  EXPECT_CLIP_RECT(FloatRoundedRect(-12, -2, 340, 220), mask_clip);

  EXPECT_EQ(properties->Effect(),
            &target->FirstFragment().LocalBorderBoxProperties().Effect());
  EXPECT_TRUE(properties->Effect()->Parent()->IsRoot());
  EXPECT_EQ(SkBlendMode::kSrcOver, properties->Effect()->BlendMode());
  EXPECT_EQ(mask_clip->Parent(), properties->Effect()->OutputClip());

  EXPECT_EQ(properties->Effect(), properties->Mask()->Parent());
  EXPECT_EQ(SkBlendMode::kDstIn, properties->Mask()->BlendMode());
  EXPECT_EQ(mask_clip->Parent(), properties->Mask()->OutputClip());
}

TEST_P(PaintPropertyTreeBuilderTest, MaskEscapeClip) {
  // This test verifies an abs-pos element still escape the scroll of a
  // static-pos ancestor, but gets clipped due to the presence of a mask.
  SetBodyInnerHTML(R"HTML(
    <div id='scroll' style='width:300px; height:200px; overflow:scroll;'>
      <div id='target' style='width:200px; height:300px;
          -webkit-mask:linear-gradient(red,red); border:10px dashed black;
          overflow:hidden;'>
        <div id='absolute' style='position:absolute; left:0; top:0;'>
          Lorem ipsum
        </div>
      </div>
    </div>
  )HTML");

  const ObjectPaintProperties* target_properties =
      PaintPropertiesForElement("target");
  const auto* overflow_clip1 = target_properties->MaskClip()->Parent();
  const auto* mask_clip = target_properties->MaskClip();
  const auto* overflow_clip2 = target_properties->OverflowClip();
  const auto* target = GetLayoutObjectByElementId("target");
  const auto& scroll_translation =
      target->FirstFragment().LocalBorderBoxProperties().Transform();

  const ObjectPaintProperties* scroll_properties =
      PaintPropertiesForElement("scroll");

  EXPECT_EQ(DocContentClip(), overflow_clip1->Parent());
  EXPECT_CLIP_RECT(FloatRoundedRect(0, 0, 300, 200),
                   &ToUnaliased(*overflow_clip1));
  EXPECT_EQ(scroll_properties->PaintOffsetTranslation(),
            &ToUnaliased(*overflow_clip1).LocalTransformSpace());

  EXPECT_EQ(mask_clip,
            &target->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(overflow_clip1, mask_clip->Parent());
  EXPECT_CLIP_RECT(FloatRoundedRect(0, 0, 220, 320), mask_clip);
  EXPECT_EQ(&scroll_translation, &mask_clip->LocalTransformSpace());

  EXPECT_EQ(mask_clip, overflow_clip2->Parent());
  EXPECT_CLIP_RECT(FloatRoundedRect(10, 10, 200, 300), overflow_clip2);
  EXPECT_EQ(&scroll_translation, &overflow_clip2->LocalTransformSpace());

  EXPECT_EQ(target_properties->Effect(),
            &target->FirstFragment().LocalBorderBoxProperties().Effect());
  EXPECT_TRUE(target_properties->Effect()->Parent()->IsRoot());
  EXPECT_EQ(SkBlendMode::kSrcOver, target_properties->Effect()->BlendMode());
  EXPECT_EQ(nullptr, target_properties->Effect()->OutputClip());

  EXPECT_EQ(target_properties->Effect(), target_properties->Mask()->Parent());
  EXPECT_EQ(SkBlendMode::kDstIn, target_properties->Mask()->BlendMode());
  EXPECT_EQ(mask_clip->Parent(), target_properties->Mask()->OutputClip());

  const auto* absolute = GetLayoutObjectByElementId("absolute");
  EXPECT_EQ(DocScrollTranslation(),
            &absolute->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(mask_clip,
            &absolute->FirstFragment().LocalBorderBoxProperties().Clip());
}

TEST_P(PaintPropertyTreeBuilderTest, MaskInline) {
  LoadAhem();
  // This test verifies CSS mask applied on an inline element is clipped to
  // the line box of the said element. In this test the masked element has
  // only one box, and one of the child element overflows the box.
  SetBodyInnerHTML(R"HTML(
    <style>* { font-family:Ahem; font-size:16px; }</style>
    Lorem
    <span id='target' style='-webkit-mask:linear-gradient(red,red);'>
      ipsum
      <span id='overflowing' style='position:relative; font-size:32px;'>
        dolor
      </span>
      sit amet,
    </span>
  )HTML");

  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  const ClipPaintPropertyNode* mask_clip = properties->MaskClip();
  const auto* target = GetLayoutObjectByElementId("target");

  EXPECT_EQ(mask_clip,
            &target->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(DocContentClip(), mask_clip->Parent());
  EXPECT_CLIP_RECT(FloatRoundedRect(104, 21, 432, 16), mask_clip);

  EXPECT_EQ(properties->Effect(),
            &target->FirstFragment().LocalBorderBoxProperties().Effect());
  EXPECT_TRUE(properties->Effect()->Parent()->IsRoot());
  EXPECT_EQ(SkBlendMode::kSrcOver, properties->Effect()->BlendMode());
  EXPECT_EQ(mask_clip->Parent(), properties->Effect()->OutputClip());

  EXPECT_EQ(properties->Effect(), properties->Mask()->Parent());
  EXPECT_EQ(SkBlendMode::kDstIn, properties->Mask()->BlendMode());
  EXPECT_EQ(mask_clip->Parent(), properties->Mask()->OutputClip());

  const auto* overflowing = GetLayoutObjectByElementId("overflowing");
  EXPECT_EQ(mask_clip,
            &overflowing->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(properties->Effect(),
            &overflowing->FirstFragment().LocalBorderBoxProperties().Effect());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGResource) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' xmlns='http://www.w3.org/2000/svg' >
     <g transform='scale(1000)'>
       <marker id='markerMiddle'  markerWidth='2' markerHeight='2' refX='5'
           refY='5' markerUnits='strokeWidth'>
         <g id='transformInsideMarker' transform='scale(4)'>
           <circle cx='5' cy='5' r='7' fill='green'/>
         </g>
       </marker>
     </g>
     <g id='transformOutsidePath' transform='scale(2)'>
       <path d='M 130 135 L 180 135 L 180 185'
           marker-mid='url(#markerMiddle)' fill='none' stroke-width='8px'
           stroke='black'/>
     </g>
    </svg>
  )HTML");

  const ObjectPaintProperties* transform_inside_marker_properties =
      PaintPropertiesForElement("transformInsideMarker");
  const ObjectPaintProperties* transform_outside_path_properties =
      PaintPropertiesForElement("transformOutsidePath");
  const ObjectPaintProperties* svg_properties =
      PaintPropertiesForElement("svg");

  // The <marker> object resets to a new paint property tree, so the
  // transform within it should have the root as parent.
  EXPECT_EQ(&TransformPaintPropertyNode::Root(),
            transform_inside_marker_properties->Transform()->Parent());

  // Whereas this is not true of the transform above the path.
  EXPECT_EQ(svg_properties->PaintOffsetTranslation(),
            transform_outside_path_properties->Transform()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGHiddenResource) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' xmlns='http://www.w3.org/2000/svg' >
     <g transform='scale(1000)'>
       <symbol id='symbol'>
         <g id='transformInsideSymbol' transform='scale(4)'>
           <circle cx='5' cy='5' r='7' fill='green'/>
         </g>
       </symbol>
     </g>
     <g id='transformOutsideUse' transform='scale(2)'>
       <use x='25' y='25' width='400' height='400' xlink:href='#symbol'/>
     </g>
    </svg>
  )HTML");

  const ObjectPaintProperties* transform_inside_symbol_properties =
      PaintPropertiesForElement("transformInsideSymbol");
  const ObjectPaintProperties* transform_outside_use_properties =
      PaintPropertiesForElement("transformOutsideUse");
  const ObjectPaintProperties* svg_properties =
      PaintPropertiesForElement("svg");

  // The <marker> object resets to a new paint property tree, so the
  // transform within it should have the root as parent.
  EXPECT_EQ(&TransformPaintPropertyNode::Root(),
            transform_inside_symbol_properties->Transform()->Parent());

  // Whereas this is not true of the transform above the path.
  EXPECT_EQ(svg_properties->PaintOffsetTranslation(),
            transform_outside_use_properties->Transform()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGBlending) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svgroot' width='100' height='100'
        style='position: relative; z-index: 0'>
      <rect id='rect' width='100' height='100' fill='#00FF00'
          style='mix-blend-mode: difference'/>
    </svg>
  )HTML");

  const auto* rect_properties = PaintPropertiesForElement("rect");
  ASSERT_TRUE(rect_properties->Effect());
  EXPECT_EQ(SkBlendMode::kDifference, rect_properties->Effect()->BlendMode());

  const auto* svg_root_properties = PaintPropertiesForElement("svgroot");
  ASSERT_TRUE(svg_root_properties->Effect());
  EXPECT_EQ(SkBlendMode::kSrcOver, svg_root_properties->Effect()->BlendMode());

  EXPECT_EQ(&EffectPaintPropertyNode::Root(),
            svg_root_properties->Effect()->Parent());
  EXPECT_EQ(svg_root_properties->Effect(), rect_properties->Effect()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootBlending) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svgroot' 'width=100' height='100' style='mix-blend-mode: multiply'>
    </svg>
  )HTML");

  const auto* html_properties = GetDocument()
                                    .documentElement()
                                    ->GetLayoutObject()
                                    ->FirstFragment()
                                    .PaintProperties();
  ASSERT_TRUE(html_properties->Effect());
  EXPECT_EQ(SkBlendMode::kSrcOver, html_properties->Effect()->BlendMode());

  const auto* svg_root_properties = PaintPropertiesForElement("svgroot");
  ASSERT_TRUE(svg_root_properties->Effect());
  EXPECT_EQ(SkBlendMode::kMultiply, svg_root_properties->Effect()->BlendMode());

  EXPECT_EQ(&EffectPaintPropertyNode::Root(),
            html_properties->Effect()->Parent());
  EXPECT_EQ(html_properties->Effect(), svg_root_properties->Effect()->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest, ScrollBoundsOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        margin: 0px;
      }
      #scroller {
        overflow-y: scroll;
        width: 100px;
        height: 100px;
        margin-left: 7px;
        margin-top: 11px;
      }
      .forceScroll {
        height: 200px;
      }
    </style>
    <div id='scroller'>
      <div class='forceScroll'></div>
    </div>
  )HTML");

  Element* scroller = GetDocument().getElementById(AtomicString("scroller"));
  scroller->setScrollTop(42);

  UpdateAllLifecyclePhasesForTest();

  const ObjectPaintProperties* scroll_properties =
      scroller->GetLayoutObject()->FirstFragment().PaintProperties();
  // Because the frameView is does not scroll, overflowHidden's scroll should be
  // under the root.
  auto* scroll_translation = scroll_properties->ScrollTranslation();
  auto* paint_offset_translation = scroll_properties->PaintOffsetTranslation();
  auto* scroll_node = scroll_translation->ScrollNode();
  EXPECT_EQ(DocScroll(), scroll_node->Parent());
  EXPECT_EQ(gfx::Vector2dF(0, -42), scroll_translation->Get2dTranslation());
  // The paint offset node should be offset by the margin.
  EXPECT_EQ(gfx::Vector2dF(7, 11),
            paint_offset_translation->Get2dTranslation());
  // And the scroll node should not.
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), scroll_node->ContainerRect());

  scroller->setAttribute(html_names::kStyleAttr,
                         AtomicString("border: 20px solid black;"));
  UpdateAllLifecyclePhasesForTest();
  // The paint offset node should be offset by the margin.
  EXPECT_EQ(gfx::Vector2dF(7, 11),
            paint_offset_translation->Get2dTranslation());
  // The scroll node should be offset by the border.
  EXPECT_EQ(gfx::Rect(20, 20, 100, 100), scroll_node->ContainerRect());

  scroller->setAttribute(html_names::kStyleAttr,
                         AtomicString("border: 20px solid black;"
                                      "transform: translate(20px, 30px);"));
  UpdateAllLifecyclePhasesForTest();
  // The scroll node's offset should not include margin if it has already been
  // included in a paint offset node.
  EXPECT_EQ(gfx::Rect(20, 20, 100, 100), scroll_node->ContainerRect());
  EXPECT_EQ(gfx::Vector2dF(7, 11),
            scroll_properties->PaintOffsetTranslation()->Get2dTranslation());
}

TEST_P(PaintPropertyTreeBuilderTest, BackfaceHidden) {
  SetBodyInnerHTML(R"HTML(
    <style>#target { position: absolute; top: 50px; left: 60px }</style>
    <div id='target' style='backface-visibility: hidden'></div>
  )HTML");

  const auto* target = GetLayoutObjectByElementId("target");
  const auto* target_properties = target->FirstFragment().PaintProperties();
  ASSERT_NE(nullptr, target_properties);
  const auto* paint_offset_translation =
      target_properties->PaintOffsetTranslation();
  ASSERT_NE(nullptr, paint_offset_translation);
  EXPECT_EQ(gfx::Vector2dF(60, 50),
            paint_offset_translation->Get2dTranslation());
  EXPECT_EQ(TransformPaintPropertyNode::BackfaceVisibility::kInherited,
            paint_offset_translation->GetBackfaceVisibilityForTesting());

  const auto* transform = target_properties->Transform();
  ASSERT_NE(nullptr, transform);
  EXPECT_TRUE(transform->IsIdentity());
  EXPECT_EQ(TransformPaintPropertyNode::BackfaceVisibility::kHidden,
            transform->GetBackfaceVisibilityForTesting());

  To<Element>(target->GetNode())
      ->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ(PhysicalOffset(60, 50), target->FirstFragment().PaintOffset());
  EXPECT_EQ(nullptr, target->FirstFragment().PaintProperties());
}

TEST_P(PaintPropertyTreeBuilderTest, FrameBorderRadius) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #iframe {
        width: 200px;
        height: 200px;
        border: 10px solid blue;
        border-radius: 50px;
        padding: 10px;
        overflow: visible;
      }
    </style>
    <iframe id='iframe'></iframe>
  )HTML");

  const auto* properties = PaintPropertiesForElement("iframe");
  ASSERT_NE(nullptr, properties);
  const auto* border_radius_clip = properties->InnerBorderRadiusClip();
  ASSERT_NE(nullptr, border_radius_clip);
  EXPECT_CLIP_RECT(FloatRoundedRect(gfx::RectF(28, 28, 200, 200),
                                    FloatRoundedRect::Radii(30)),
                   border_radius_clip);
  auto* overflow_clip = properties->OverflowClip();
  EXPECT_CLIP_RECT(FloatRoundedRect(28, 28, 200, 200), overflow_clip);
  EXPECT_EQ(overflow_clip->Parent(), border_radius_clip);
  EXPECT_EQ(DocContentClip(), border_radius_clip->Parent());
  EXPECT_EQ(DocScrollTranslation(), &border_radius_clip->LocalTransformSpace());
}

TEST_P(PaintPropertyTreeBuilderTest, NoPropertyForSVGTextWithReflection) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <text id='target' style='-webkit-box-reflect: below 2px'>x</text>
    </svg>
  )HTML");
  EXPECT_FALSE(PaintPropertiesForElement("target"));
}

TEST_P(PaintPropertyTreeBuilderTest, ImageBorderRadius) {
  SetBodyInnerHTML(R"HTML(
    <img id='img'
        style='width: 50px; height: 50px; border-radius: 30px; padding: 10px'>
  )HTML");

  const auto* properties = PaintPropertiesForElement("img");
  const auto* overflow_clip = properties->OverflowClip();
  ASSERT_NE(nullptr, overflow_clip);
  EXPECT_CLIP_RECT(
      FloatRoundedRect(gfx::RectF(18, 18, 50, 50), FloatRoundedRect::Radii(0)),
      overflow_clip);
  EXPECT_EQ(properties->InnerBorderRadiusClip(), overflow_clip->Parent());
  EXPECT_EQ(DocScrollTranslation(), &overflow_clip->LocalTransformSpace());

  const auto* border_radius_clip = properties->InnerBorderRadiusClip();
  ASSERT_NE(nullptr, border_radius_clip);
  EXPECT_EQ(DocContentClip(), border_radius_clip->Parent());
  EXPECT_CLIP_RECT(
      FloatRoundedRect(gfx::RectF(18, 18, 50, 50), FloatRoundedRect::Radii(20)),
      border_radius_clip);
}

TEST_P(PaintPropertyTreeBuilderTest, FrameClipWhenPrinting) {
  SetBodyInnerHTML("<iframe></iframe>");
  SetChildFrameHTML("");
  UpdateAllLifecyclePhasesForTest();

  // When not printing, both main and child frame views have content clip.
  auto* const main_frame_doc = &GetDocument();
  auto* const child_frame_doc = &ChildDocument();
  EXPECT_CLIP_RECT(gfx::RectF(0, 0, 800, 600), DocContentClip(main_frame_doc));
  EXPECT_CLIP_RECT(gfx::RectF(0, 0, 300, 150), DocContentClip(child_frame_doc));

  // When the main frame is printing, it should not have content clip.
  gfx::SizeF page_size(100, 100);
  GetFrame().StartPrinting(WebPrintParams(page_size));
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();
  EXPECT_EQ(nullptr, DocContentClip(main_frame_doc));
  EXPECT_CLIP_RECT(gfx::RectF(0, 0, 300, 150), DocContentClip(child_frame_doc));

  GetFrame().EndPrinting();
  UpdateAllLifecyclePhasesForTest();

  // When only the child frame is printing, it should not have content clip but
  // the main frame still have (which doesn't matter though).
  ChildFrame().StartPrinting(WebPrintParams(page_size));
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();
  ASSERT_NE(nullptr, DocContentClip(main_frame_doc));
  EXPECT_CLIP_RECT(gfx::RectF(0, 0, 800, 600), DocContentClip(main_frame_doc));
  EXPECT_EQ(nullptr, DocContentClip(child_frame_doc));
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowControlsClip) {
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar { width: 20px }</style>
    <div id='div1' style='overflow: scroll; width: 5px; height: 50px'></div>
    <div id='div2' style='overflow: scroll; width: 50px; height: 50px'></div>
  )HTML");

  const auto* properties1 = PaintPropertiesForElement("div1");
  ASSERT_NE(nullptr, properties1);
  const auto* overflow_controls_clip = properties1->OverflowControlsClip();
  EXPECT_CLIP_RECT(gfx::RectF(0, 0, 5, 50), overflow_controls_clip);

  const auto* properties2 = PaintPropertiesForElement("div2");
  ASSERT_NE(nullptr, properties2);
  EXPECT_EQ(nullptr, properties2->OverflowControlsClip());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowControlsClipSubpixel) {
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar { width: 20px }</style>
    <div id='div1' style='overflow: scroll; width: 5.5px; height: 50px'></div>
    <div id='div2' style='overflow: scroll; width: 50.5px; height: 50px'></div>
  )HTML");

  const auto* properties1 = PaintPropertiesForElement("div1");
  ASSERT_NE(nullptr, properties1);
  const auto* overflow_controls_clip = properties1->OverflowControlsClip();
  EXPECT_EQ(FloatClipRect(gfx::RectF(0, 0, 5.5, 50)),
            overflow_controls_clip->LayoutClipRect());
  EXPECT_EQ(FloatRoundedRect(0, 0, 6, 50),
            overflow_controls_clip->PaintClipRect());

  const auto* properties2 = PaintPropertiesForElement("div2");
  ASSERT_NE(nullptr, properties2);
  EXPECT_EQ(nullptr, properties2->OverflowControlsClip());
}

TEST_P(PaintPropertyTreeBuilderTest, FragmentPaintOffsetUnderOverflowScroll) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      ::-webkit-scrollbar { width: 20px }
    </style>
    <div id='container' style='margin-top: 50px; overflow-y: scroll'>
      <div style='columns: 2; height: 40px; column-gap: 0'>
        <div id='content' style='width: 20px; height: 20px'>TEST</div>
      </div>
    </div>
  )HTML");

  // container establishes paint_offset_root because it has scrollbar.
  EXPECT_NE(nullptr,
            PaintPropertiesForElement("container")->PaintOffsetTranslation());

  const auto* content = GetLayoutObjectByElementId("content");
  FragmentDataIterator iterator(*content);
  const auto* first_fragment = iterator.GetFragmentData();
  ASSERT_TRUE(iterator.Advance());
  const auto* second_fragment = iterator.GetFragmentData();
  ASSERT_NE(nullptr, second_fragment);

  EXPECT_EQ(PhysicalOffset(), first_fragment->PaintOffset());
  EXPECT_EQ(PhysicalOffset(390, 0), second_fragment->PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootWithMask) {
  SetBodyInnerHTML(R"HTML(
    <svg id="svg" width="16" height="16" mask="url(#test)">
      <rect width="100%" height="16" fill="#fff"></rect>
      <defs>
        <mask id="test">
          <g>
            <rect width="100%" height="100%" fill="#ffffff" style=""></rect>
          </g>
        </mask>
      </defs>
    </svg>
  )HTML");

  const auto& root = *To<LayoutSVGRoot>(GetLayoutObjectByElementId("svg"));
  EXPECT_TRUE(root.FirstFragment().PaintProperties()->Mask());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootWithCSSMask) {
  SetBodyInnerHTML(R"HTML(
    <svg id="svg" width="16" height="16" style="-webkit-mask-image: url(fake);">
    </svg>
  )HTML");

  const auto& root = *To<LayoutSVGRoot>(GetLayoutObjectByElementId("svg"));
  EXPECT_TRUE(root.FirstFragment().PaintProperties()->Mask());
}

TEST_P(PaintPropertyTreeBuilderTest, ElementCaptureEffectNode) {
  ScopedElementCaptureForTest scoped_element_capture(true);

  // This test makes sure that an ElementCaptureEffect node is properly added
  // when an element has a restriction ID.
  SetBodyInnerHTML(R"HTML(
     <style>
      .stacking {
        opacity: 0.9;
      }
    </style>
    <body id="body1">
      <div id="div1" width="640" height="480"/>
    </body>
  )HTML");

  Element* element = GetDocument().getElementById(AtomicString("div1"));
  ASSERT_TRUE(element);

  // As a plain div, the element shouldn't have a separate stacking context.
  EXPECT_FALSE(element->GetLayoutObject()->HasLayer());
  EXPECT_FALSE(element->GetLayoutObject()->IsStackingContext());
  element->SetRestrictionTargetId(
      std::make_unique<RestrictionTargetId>(base::Token::CreateRandom()));
  UpdateAllLifecyclePhasesForTest();

  // The element should still not have a proper stacking context.
  EXPECT_FALSE(element->GetLayoutObject()->HasLayer());
  EXPECT_FALSE(element->GetLayoutObject()->IsStackingContext());

  // Now that the div has a restriction ID and a stacking context, it should
  // have an element capture effect node.
  element->setAttribute(html_names::kClassAttr, AtomicString("stacking"));
  UpdateAllLifecyclePhasesForTest();
  const ObjectPaintProperties* paint_properties =
      element->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_TRUE(paint_properties && paint_properties->ElementCaptureEffect());
  EXPECT_TRUE(element->GetLayoutObject()->HasLayer());
  EXPECT_TRUE(element->GetLayoutObject()->IsStackingContext());

  // NOTE: we don't currently have a teardown path for element capture. Once an
  // element is marked for capture it is marked for the rest of its lifetime.
  // TODO(https://crbug.com/1472139): add a teardown path for element capture.
}

TEST_P(PaintPropertyTreeBuilderTest, ClearClipPathEffectNode) {
  // This test makes sure ClipPath effect node is cleared properly upon
  // removal of a clip-path.
  SetBodyInnerHTML(R"HTML(
    <svg>
      <clipPath clip-path="circle()" id="clip"></clipPath>
      <rect id="rect" width="800" clip-path="url(#clip)" height="800"/>
    </svg>
  )HTML");

  {
    const auto* rect = GetLayoutObjectByElementId("rect");
    ASSERT_TRUE(rect);
    EXPECT_TRUE(rect->FirstFragment().PaintProperties()->MaskClip());
    EXPECT_TRUE(rect->FirstFragment().PaintProperties()->ClipPathMask());
  }

  Element* clip = GetDocument().getElementById(AtomicString("clip"));
  ASSERT_TRUE(clip);
  clip->remove();
  UpdateAllLifecyclePhasesExceptPaint();

  {
    const auto* rect = GetLayoutObjectByElementId("rect");
    ASSERT_TRUE(rect);
    EXPECT_FALSE(rect->FirstFragment().PaintProperties()->MaskClip());
    EXPECT_FALSE(rect->FirstFragment().PaintProperties()->ClipPathMask());
  }
}

TEST_P(PaintPropertyTreeBuilderTest, EmptyClipPathSubpixelOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id="target"
         style="clip-path: polygon(0 0, 100% 0, 100% 100%, 0 100%, 0 0);
                position: relative; top: 0.75px; left: 0.25px; width: 0">
    </div>
  )HTML");

  const auto* target = GetLayoutObjectByElementId("target");
  ASSERT_TRUE(target->FirstFragment().PaintProperties());
  const auto* clip_path_clip =
      target->FirstFragment().PaintProperties()->ClipPathClip();
  ASSERT_TRUE(clip_path_clip);
  EXPECT_EQ(gfx::RectF(0.25, 0.75, 0, 0),
            clip_path_clip->LayoutClipRect().Rect());
  EXPECT_EQ(FloatRoundedRect(), clip_path_clip->PaintClipRect());
}

TEST_P(PaintPropertyTreeBuilderTest, EmptyMaskSubpixelOffset) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id="target"
         style="-webkit-mask: linear-gradient(blue, white);
                position: relative; top: 0.75px; left: 0.25px; width: 0">
    </div>
  )HTML");

  const auto* target = GetLayoutObjectByElementId("target");
  ASSERT_TRUE(target->FirstFragment().PaintProperties());
  const auto* mask_clip = target->FirstFragment().PaintProperties()->MaskClip();
  ASSERT_TRUE(mask_clip);
  EXPECT_EQ(gfx::RectF(0.25, 0.75, 0, 0), mask_clip->LayoutClipRect().Rect());
  EXPECT_EQ(FloatRoundedRect(), mask_clip->PaintClipRect());
}

TEST_P(PaintPropertyTreeBuilderTest, RootHasCompositedScrolling) {
  SetBodyInnerHTML(R"HTML(
    <div id='forceScroll' style='height: 2000px'></div>
  )HTML");

  // When the root scrolls, there should be direct compositing reasons.
  EXPECT_TRUE(DocScrollTranslation()->HasDirectCompositingReasons());

  // Remove scrolling from the root.
  Element* force_scroll_element =
      GetDocument().getElementById(AtomicString("forceScroll"));
  force_scroll_element->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesExceptPaint();
  // Always create scroll translation for layout view even the document does
  // not scroll (not enough content).
  EXPECT_TRUE(DocScrollTranslation());
}

TEST_P(PaintPropertyTreeBuilderTest, IframeDoesNotRequireCompositedScrolling) {
  SetBodyInnerHTML(R"HTML(
    <iframe style='width: 200px; height: 200px;'></iframe>
    <div id='forceScroll' style='height: 2000px'></div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id='forceInnerScroll' style='height: 2000px'></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(DocScrollTranslation()->HasDirectCompositingReasons());

  // When the child iframe scrolls, there should not be direct compositing
  // reasons because only the root frame needs scrolling compositing reasons.
  EXPECT_FALSE(
      DocScrollTranslation(&ChildDocument())->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest, ClipHitTestChangeDoesNotCauseFullRepaint) {
  SetBodyInnerHTML(R"HTML(
    <html>
      <body>
        <style>
          .noscrollbars::-webkit-scrollbar { display: none; }
        </style>
        <div id="child" style="width: 10px; height: 10px; position: absolute;">
        </div>
        <div id="forcescroll" style="height: 1000px;"></div>
      </body>
    </html>
  )HTML");
  CHECK(GetDocument().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars());
  UpdateAllLifecyclePhasesForTest();

  auto* child_layer = GetPaintLayerByElementId("child");
  EXPECT_FALSE(child_layer->SelfNeedsRepaint());

  GetDocument().body()->setAttribute(html_names::kClassAttr,
                                     AtomicString("noscrollbars"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(child_layer->SelfNeedsRepaint());
}

TEST_P(PaintPropertyTreeBuilderTest, ClipPathInheritanceWithoutMutation) {
  // This test verifies we properly included the path-based clip-path in
  // context when the clipping element didn't need paint property update.
  SetBodyInnerHTML(R"HTML(
    <div style="clip-path:circle();">
      <div id="child" style="position:relative; width:100px; height:100px;
          background:green;"></div>
    </div>
  )HTML");

  auto* child = GetLayoutBoxByElementId("child");
  const auto& old_clip_state =
      child->FirstFragment().LocalBorderBoxProperties().Clip();

  child->SetNeedsPaintPropertyUpdate();
  UpdateAllLifecyclePhasesForTest();

  const auto& new_clip_state =
      child->FirstFragment().LocalBorderBoxProperties().Clip();
  EXPECT_EQ(&old_clip_state, &new_clip_state);
}

TEST_P(PaintPropertyTreeBuilderTest, CompositedLayerSkipsFragmentClip) {
  SetBodyInnerHTML(R"HTML(
    <div id="columns" style="columns: 2">
      <div id="composited-with-clip"
           style="height: 100px; will-change: transform; overflow: hidden">
        <div id="child-clipped" style="height: 120px; position: relative"></div>
      </div>
      <div id="composited-without-clip"
           style="height: 100px; will-change: transform">
        <div id="child-unclipped" style="height: 100%; position: relative">
        </div>
      </div>
    </div>
  )HTML");

  const auto* composited_with_clip_properties =
      PaintPropertiesForElement("composited-with-clip");
  EXPECT_EQ(DocContentClip(),
            composited_with_clip_properties->OverflowClip()->Parent());
  EXPECT_EQ(composited_with_clip_properties->OverflowClip(),
            &GetLayoutObjectByElementId("child-clipped")
                 ->FirstFragment()
                 .LocalBorderBoxProperties()
                 .Clip());

  EXPECT_EQ(DocContentClip(),
            &GetLayoutObjectByElementId("composited-without-clip")
                 ->FirstFragment()
                 .LocalBorderBoxProperties()
                 .Clip());
  EXPECT_EQ(DocContentClip(), &GetLayoutObjectByElementId("child-unclipped")
                                   ->FirstFragment()
                                   .LocalBorderBoxProperties()
                                   .Clip());
}

TEST_P(PaintPropertyTreeBuilderTest, CompositedLayerUnderClipUnderMulticol) {
  SetBodyInnerHTML(R"HTML(
    <div id="multicol" style="columns: 2">
      <div id="clip" style="height: 100px; overflow: hidden">
        <div id="composited"
             style="width: 200px; height: 200px; will-change: transform">
        </div>
      </div>
    </div>
  )HTML");

  const auto* clip_properties = PaintPropertiesForElement("clip");
  const auto* composited = GetLayoutObjectByElementId("composited");
  EXPECT_EQ(clip_properties->OverflowClip(),
            &composited->FirstFragment().LocalBorderBoxProperties().Clip());
}

TEST_P(PaintPropertyTreeBuilderTest, RepeatingFixedPositionInPagedMedia) {
  SetBodyInnerHTML(R"HTML(
    <div id="fixed" style="position: fixed; top: 20px; left: 20px">
      <div id="fixed-child" style="position: relative; top: 10px"></div>
    </div>
    <div id="normal" style="height: 1000px"></div>
  )HTML");
  GetDocument().domWindow()->scrollTo(0, 200);
  UpdateAllLifecyclePhasesForTest();

  const auto* fixed = GetLayoutObjectByElementId("fixed");
  EXPECT_EQ(1u, NumFragments(fixed));

  const auto* fixed_child = GetLayoutObjectByElementId("fixed-child");
  EXPECT_EQ(1u, NumFragments(fixed_child));

  const auto* normal = GetLayoutObjectByElementId("normal");
  EXPECT_EQ(1u, NumFragments(normal));

  gfx::SizeF page_size(300, 400);
  GetFrame().StartPrinting(WebPrintParams(page_size));
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();
  fixed = GetLayoutObjectByElementId("fixed");
  fixed_child = GetLayoutObjectByElementId("fixed-child");
  normal = GetLayoutObjectByElementId("normal");

  // "fixed" should create fragments to repeat in each printed page.
  EXPECT_EQ(3u, NumFragments(fixed));
  for (int i = 0; i < 3; i++) {
    const auto& fragment = FragmentAt(fixed, i);
    auto* properties = fragment.PaintProperties();
    ASSERT_TRUE(properties);
    ASSERT_TRUE(properties->PaintOffsetTranslation());
    EXPECT_EQ(gfx::Vector2dF(20, 20 + 400 * i),
              properties->PaintOffsetTranslation()->Get2dTranslation());
    EXPECT_EQ(PhysicalOffset(), fragment.PaintOffset());
  }

  EXPECT_EQ(3u, NumFragments(fixed_child));
  for (int i = 0; i < 3; i++) {
    const auto& fragment = FragmentAt(fixed_child, i);
    EXPECT_EQ(FragmentAt(fixed, i).PaintOffset() + PhysicalOffset(0, 10),
              fragment.PaintOffset());
  }

  EXPECT_EQ(3u, NumFragments(normal));

  GetFrame().EndPrinting();
  UpdateAllLifecyclePhasesForTest();
  fixed = GetLayoutObjectByElementId("fixed");
  fixed_child = GetLayoutObjectByElementId("fixed-child");
  normal = GetLayoutObjectByElementId("normal");
  EXPECT_EQ(1u, NumFragments(fixed));
  EXPECT_EQ(1u, NumFragments(fixed_child));
  EXPECT_EQ(1u, NumFragments(normal));
}

TEST_P(PaintPropertyTreeBuilderTest,
       RepeatingFixedPositionWithTransformInPagedMedia) {
  SetBodyInnerHTML(R"HTML(
    <div id="fixed" style="position: fixed; top: 20px; left: 20px;
        transform: translateX(10px)">
      <div id="fixed-child" style="position: relative; top: 10px"></div>
    </div>
    <div id="normal" style="height: 1000px"></div>
  )HTML");
  GetDocument().domWindow()->scrollTo(0, 200);
  UpdateAllLifecyclePhasesForTest();

  const auto* fixed = GetLayoutObjectByElementId("fixed");
  EXPECT_EQ(1u, NumFragments(fixed));

  const auto* fixed_child = GetLayoutObjectByElementId("fixed-child");
  EXPECT_EQ(1u, NumFragments(fixed_child));

  gfx::SizeF page_size(300, 400);
  GetFrame().StartPrinting(WebPrintParams(page_size));
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();
  fixed = GetLayoutObjectByElementId("fixed");
  fixed_child = GetLayoutObjectByElementId("fixed-child");

  // "fixed" should create fragments to repeat in each printed page.
  EXPECT_EQ(3u, NumFragments(fixed));
  for (wtf_size_t i = 0; i < 3; i++) {
    const FragmentData& fragment = FragmentAt(fixed, i);
    EXPECT_EQ(PhysicalOffset(), fragment.PaintOffset());
    const auto* properties = fragment.PaintProperties();
    EXPECT_EQ(gfx::Vector2dF(20, 20 + i * 400),
              properties->PaintOffsetTranslation()->Get2dTranslation());
    EXPECT_EQ(gfx::Vector2dF(10, 0),
              properties->Transform()->Get2dTranslation());
    EXPECT_EQ(properties->PaintOffsetTranslation(),
              properties->Transform()->Parent());
    EXPECT_EQ(fragment.FragmentID(), i);
  }

  for (wtf_size_t i = 0; i < 3; i++) {
    const FragmentData& fragment = FragmentAt(fixed_child, i);
    EXPECT_EQ(PhysicalOffset(0, 10), fragment.PaintOffset());
    EXPECT_EQ(FragmentAt(fixed, i).PaintProperties()->Transform(),
              &fragment.LocalBorderBoxProperties().Transform());
    EXPECT_EQ(fragment.FragmentID(), i);
  }

  GetFrame().EndPrinting();
  UpdateAllLifecyclePhasesForTest();
  fixed = GetLayoutObjectByElementId("fixed");
  fixed_child = GetLayoutObjectByElementId("fixed-child");
  EXPECT_EQ(1u, NumFragments(fixed));
  EXPECT_EQ(1u, NumFragments(fixed_child));
}

TEST_P(PaintPropertyTreeBuilderTest,
       FloatPaintOffsetInContainerWithScrollbars) {
  SetBodyInnerHTML(R"HTML(
    <style>
      ::-webkit-scrollbar {width: 15px; height: 15px}
      .container {
        position: absolute; width: 200px; height: 200px; overflow: scroll;
      }
      .float-left {float: left; width: 100px; height: 100px;}
      .float-right {float: right; width: 100px; height: 100px;}
    </style>
    <div class="container">
      <div id="float-left" class="float-left"></div>
      <div id="float-right" class="float-right"></div>
    </div>
    <div class="container" style="direction: rtl">
      <div id="float-left-rtl" class="float-left"></div>
      <div id="float-right-rtl" class="float-right"></div>
    </div>
    <div class="container" style="writing-mode: vertical-rl">
      <div id="float-left-vrl" class="float-left"></div>
      <div id="float-right-vrl" class="float-right"></div>
    </div>
    <div class="container" style="writing-mode: vertical-rl; direction: rtl">
      <div id="float-left-rtl-vrl" class="float-left"></div>
      <div id="float-right-rtl-vrl" class="float-right"></div>
    </div>
    <div class="container" style="writing-mode: vertical-lr">
      <div id="float-left-vlr" class="float-left"></div>
      <div id="float-right-vlr" class="float-right"></div>
    </div>
    <div class="container" style="writing-mode: vertical-lr; direction: rtl">
      <div id="float-left-rtl-vlr" class="float-left"></div>
      <div id="float-right-rtl-vlr" class="float-right"></div>
    </div>
  )HTML");

  auto paint_offset = [this](const char* id) {
    return GetLayoutObjectByElementId(id)->FirstFragment().PaintOffset();
  };
  EXPECT_EQ(PhysicalOffset(), paint_offset("float-left"));
  EXPECT_EQ(PhysicalOffset(85, 100), paint_offset("float-right"));
  EXPECT_EQ(PhysicalOffset(15, 0), paint_offset("float-left-rtl"));
  EXPECT_EQ(PhysicalOffset(100, 100), paint_offset("float-right-rtl"));
  EXPECT_EQ(PhysicalOffset(100, 0), paint_offset("float-left-vrl"));
  EXPECT_EQ(PhysicalOffset(0, 85), paint_offset("float-right-vrl"));
  EXPECT_EQ(PhysicalOffset(100, 0), paint_offset("float-left-rtl-vrl"));
  EXPECT_EQ(PhysicalOffset(0, 85), paint_offset("float-right-rtl-vrl"));
  EXPECT_EQ(PhysicalOffset(), paint_offset("float-left-vlr"));
  EXPECT_EQ(PhysicalOffset(100, 85), paint_offset("float-right-vlr"));
  EXPECT_EQ(PhysicalOffset(), paint_offset("float-left-rtl-vlr"));
  EXPECT_EQ(PhysicalOffset(100, 85), paint_offset("float-right-rtl-vlr"));
}

TEST_P(PaintPropertyTreeBuilderTest, PaintOffsetForTextareaWithResizer) {
  GetPage().GetSettings().SetTextAreasAreResizable(true);
  SetBodyInnerHTML(R"HTML(
    <!doctype HTML>
    <style>
      div {
        width: 100%;
        height: 100px;
      }
      textarea {
        width: 200px;
        height: 100px;
      }
      ::-webkit-resizer {
        background-color: red;
      }
    </style>
    <div></div>
    <textarea id="target"></textarea>
  )HTML");

  const auto* box = GetLayoutBoxByElementId("target");
  const auto& fragment = box->FirstFragment();
  ASSERT_TRUE(fragment.PaintProperties());
  EXPECT_NE(fragment.PaintProperties()->PaintOffsetTranslation(), nullptr);
  EXPECT_EQ(PhysicalOffset(), fragment.PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest, SubpixelPositionedScrollNode) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      #scroller {
        position: relative;
        top: 0.5625px;
        width: 200px;
        height: 200.8125px;
        overflow: auto;
      }
      #space {
        width: 1000px;
        height: 200.8125px;
      }
    </style>
    <div id="scroller">
      <div id="space"></div>
    </div>
  )HTML");

  const auto* properties = PaintPropertiesForElement("scroller");
  const auto* scroll_node = properties->ScrollTranslation()->ScrollNode();
  EXPECT_EQ(gfx::Rect(0, 0, 200, 200), scroll_node->ContainerRect());
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 200), scroll_node->ContentsRect());
}

TEST_P(PaintPropertyTreeBuilderTest,
       LayoutMenuListHasOverlowAndLocalBorderBoxProperties) {
  SetBodyInnerHTML(R"HTML(
    <!doctype HTML>
    <select id="selection" style="width: 80px;">
      <option>lorem ipsum dolor</option>
    </select>
  )HTML");

  const auto& fragment = GetDocument()
                             .getElementById(AtomicString("selection"))
                             ->GetLayoutObject()
                             ->FirstFragment();

  EXPECT_TRUE(fragment.PaintProperties());
  EXPECT_TRUE(fragment.PaintProperties()->OverflowClip());
  ASSERT_TRUE(fragment.HasLocalBorderBoxProperties());
  EXPECT_EQ(&fragment.ContentsProperties().Clip(),
            fragment.PaintProperties()->OverflowClip());
}

TEST_P(PaintPropertyTreeBuilderTest, EmptyClipFragments) {
  SetBodyInnerHTML(R"HTML(
    <!doctype HTML>
    <style>h4 { column-span: all; }</style>
    <div id="container" style="columns:1;">
      <div id="wrapper">
        lorem
        <h4>hi</h4>
        <div><h4>hello</h4></div>
        ipsum
      </div>
    </div>
  )HTML");

  const auto* wrapper =
      GetDocument().getElementById(AtomicString("wrapper"))->GetLayoutObject();

  ASSERT_EQ(3u, NumFragments(wrapper));
  ASSERT_EQ(0u, FragmentAt(wrapper, 0).FragmentID());
  ASSERT_EQ(1u, FragmentAt(wrapper, 1).FragmentID());
  ASSERT_EQ(2u, FragmentAt(wrapper, 2).FragmentID());
}

TEST_P(PaintPropertyTreeBuilderTest, StickyConstraintChain) {
  // This test verifies the property tree builder set up sticky constraint
  // chain properly in case of nested sticky positioned elements.
  SetBodyInnerHTML(R"HTML(
    <div id="scroller" style="overflow:scroll; width:300px; height:200px;">
      <div id="outer" style="position:sticky; top:10px;">
        <div style="height:300px;">
          <span id="middle" style="position:sticky; top:25px;">
            <span id="inner" style="position:sticky; top:45px;"></span>
          </span>
        </div>
      </div>
      <div style="height:1000px;"></div>
    </div>
  )HTML");
  GetDocument().getElementById(AtomicString("scroller"))->setScrollTop(50);
  UpdateAllLifecyclePhasesForTest();

  const auto* outer_properties = PaintPropertiesForElement("outer");
  ASSERT_TRUE(outer_properties && outer_properties->StickyTranslation());
  EXPECT_TRUE(outer_properties->StickyTranslation()
                  ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 60),
            outer_properties->StickyTranslation()->Get2dTranslation());
  ASSERT_NE(nullptr,
            outer_properties->StickyTranslation()->GetStickyConstraint());
  EXPECT_EQ(CompositorElementId(), outer_properties->StickyTranslation()
                                       ->GetStickyConstraint()
                                       ->nearest_element_shifting_sticky_box);
  EXPECT_EQ(CompositorElementId(),
            outer_properties->StickyTranslation()
                ->GetStickyConstraint()
                ->nearest_element_shifting_containing_block);

  const auto* middle_properties = PaintPropertiesForElement("middle");
  ASSERT_TRUE(middle_properties && middle_properties->StickyTranslation());
  EXPECT_TRUE(middle_properties->StickyTranslation()
                  ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 15),
            middle_properties->StickyTranslation()->Get2dTranslation());
  ASSERT_NE(nullptr,
            middle_properties->StickyTranslation()->GetStickyConstraint());
  EXPECT_EQ(CompositorElementId(), middle_properties->StickyTranslation()
                                       ->GetStickyConstraint()
                                       ->nearest_element_shifting_sticky_box);
  EXPECT_EQ(outer_properties->StickyTranslation()->GetCompositorElementId(),
            middle_properties->StickyTranslation()
                ->GetStickyConstraint()
                ->nearest_element_shifting_containing_block);

  const auto* inner_properties = PaintPropertiesForElement("inner");
  ASSERT_TRUE(inner_properties && inner_properties->StickyTranslation());
  EXPECT_TRUE(inner_properties->StickyTranslation()
                  ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 20),
            inner_properties->StickyTranslation()->Get2dTranslation());
  ASSERT_NE(nullptr,
            inner_properties->StickyTranslation()->GetStickyConstraint());
  EXPECT_EQ(middle_properties->StickyTranslation()->GetCompositorElementId(),
            inner_properties->StickyTranslation()
                ->GetStickyConstraint()
                ->nearest_element_shifting_sticky_box);
  EXPECT_EQ(outer_properties->StickyTranslation()->GetCompositorElementId(),
            inner_properties->StickyTranslation()
                ->GetStickyConstraint()
                ->nearest_element_shifting_containing_block);
}

TEST_P(PaintPropertyTreeBuilderTest, StickyUnderOverflowHidden) {
  // This test verifies the property tree builder applies sticky offset
  // correctly when the scroll container doesn't have a scroll node, and
  // does not emit sticky constraints.
  SetBodyInnerHTML(R"HTML(
    <div id="scroller" style="overflow:hidden; width:300px; height:200px;">
      <div id="outer" style="position:sticky; top:10px;">
        <div style="height:300px;">
          <span id="middle" style="position:sticky; top:25px;">
            <span id="inner" style="position:sticky; top:45px;"></span>
          </span>
        </div>
      </div>
      <div style="height:1000px;"></div>
    </div>
  )HTML");

  const auto* outer_properties = PaintPropertiesForElement("outer");
  ASSERT_TRUE(outer_properties && outer_properties->StickyTranslation());
  // We still composite the element for better performance programmatic scroll
  // offset animation.
  EXPECT_TRUE(outer_properties->StickyTranslation()
                  ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 10),
            outer_properties->StickyTranslation()->Get2dTranslation());
  EXPECT_EQ(nullptr,
            outer_properties->StickyTranslation()->GetStickyConstraint());

  const auto* middle_properties = PaintPropertiesForElement("middle");
  ASSERT_TRUE(middle_properties && middle_properties->StickyTranslation());
  EXPECT_TRUE(middle_properties->StickyTranslation()
                  ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 15),
            middle_properties->StickyTranslation()->Get2dTranslation());
  EXPECT_EQ(nullptr,
            middle_properties->StickyTranslation()->GetStickyConstraint());

  const auto* inner_properties = PaintPropertiesForElement("inner");
  ASSERT_TRUE(inner_properties && inner_properties->StickyTranslation());
  EXPECT_TRUE(inner_properties->StickyTranslation()
                  ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 20),
            inner_properties->StickyTranslation()->Get2dTranslation());
  EXPECT_EQ(nullptr,
            inner_properties->StickyTranslation()->GetStickyConstraint());

  // The overflow:hidden scroller will create a scroll node when the scroll
  // offset is not zero.
  GetDocument().getElementById(AtomicString("scroller"))->setScrollTop(50);
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(outer_properties->StickyTranslation()
                  ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 60),
            outer_properties->StickyTranslation()->Get2dTranslation());
  if (RuntimeEnabledFeatures::ScrollNodeForOverflowHiddenEnabled()) {
    EXPECT_NE(nullptr,
              outer_properties->StickyTranslation()->GetStickyConstraint());
  } else {
    EXPECT_EQ(nullptr,
              outer_properties->StickyTranslation()->GetStickyConstraint());
  }

  ASSERT_TRUE(middle_properties && middle_properties->StickyTranslation());
  EXPECT_TRUE(middle_properties->StickyTranslation()
                  ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 15),
            middle_properties->StickyTranslation()->Get2dTranslation());
  if (RuntimeEnabledFeatures::ScrollNodeForOverflowHiddenEnabled()) {
    EXPECT_NE(nullptr,
              middle_properties->StickyTranslation()->GetStickyConstraint());
  } else {
    EXPECT_EQ(nullptr,
              middle_properties->StickyTranslation()->GetStickyConstraint());
  }

  ASSERT_TRUE(inner_properties && inner_properties->StickyTranslation());
  EXPECT_TRUE(inner_properties->StickyTranslation()
                  ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 20),
            inner_properties->StickyTranslation()->Get2dTranslation());
  if (RuntimeEnabledFeatures::ScrollNodeForOverflowHiddenEnabled()) {
    EXPECT_NE(nullptr,
              inner_properties->StickyTranslation()->GetStickyConstraint());
  } else {
    EXPECT_EQ(nullptr,
              inner_properties->StickyTranslation()->GetStickyConstraint());
  }
}

TEST_P(PaintPropertyTreeBuilderTest, StickyUnderScrollerWithoutOverflow) {
  // This test verifies the property tree builder applies sticky offset
  // correctly when the scroll container doesn't have overflow, and does not
  // emit compositing reasons or sticky constraints.
  SetBodyInnerHTML(R"HTML(
    <div id="scroller" style="overflow:scroll; width:300px; height:400px;">
      <div id="outer" style="position:sticky; top:10px;">
        <div style="height:300px;">
          <span id="middle" style="position:sticky; top:25px;">
            <span id="inner" style="position:sticky; top:45px;"></span>
          </span>
        </div>
      </div>
    </div>
  )HTML");

  const auto* outer_properties = PaintPropertiesForElement("outer");
  ASSERT_TRUE(outer_properties && outer_properties->StickyTranslation());
  EXPECT_FALSE(outer_properties->StickyTranslation()
                   ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 10),
            outer_properties->StickyTranslation()->Get2dTranslation());
  EXPECT_EQ(nullptr,
            outer_properties->StickyTranslation()->GetStickyConstraint());

  const auto* middle_properties = PaintPropertiesForElement("middle");
  ASSERT_TRUE(middle_properties && middle_properties->StickyTranslation());
  EXPECT_FALSE(middle_properties->StickyTranslation()
                   ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 15),
            middle_properties->StickyTranslation()->Get2dTranslation());
  EXPECT_EQ(nullptr,
            middle_properties->StickyTranslation()->GetStickyConstraint());

  const auto* inner_properties = PaintPropertiesForElement("inner");
  ASSERT_TRUE(inner_properties && inner_properties->StickyTranslation());
  EXPECT_FALSE(inner_properties->StickyTranslation()
                   ->RequiresCompositingForStickyPosition());
  EXPECT_EQ(gfx::Vector2dF(0, 20),
            inner_properties->StickyTranslation()->Get2dTranslation());
  EXPECT_EQ(nullptr,
            inner_properties->StickyTranslation()->GetStickyConstraint());
}

TEST_P(PaintPropertyTreeBuilderTest, WillChangeOpacityInducesAnEffectNode) {
  SetBodyInnerHTML(R"HTML(
    <style>.transluscent { opacity: 0.5; }</style>
    <div id="div" style="width:10px; height:10px; will-change: opacity;"></div>
  )HTML");

  const auto* properties = PaintPropertiesForElement("div");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->Effect());
  EXPECT_FLOAT_EQ(properties->Effect()->Opacity(), 1.f);

  auto* div = GetDocument().getElementById(AtomicString("div"));
  div->setAttribute(html_names::kClassAttr, AtomicString("transluscent"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(
      To<LayoutBox>(div->GetLayoutObject())->Layer()->SelfNeedsRepaint());

  ASSERT_TRUE(properties->Effect());
  EXPECT_FLOAT_EQ(properties->Effect()->Opacity(), 0.5f);
}

TEST_P(PaintPropertyTreeBuilderTest, EffectOutputClipWithFixedDescendant) {
  SetBodyInnerHTML(R"HTML(
    <!-- Case 1: No clip. -->
    <div id="target1" style="opacity: 0.1">
      <div style="position: fixed"></div>
    </div>
    <!-- Case 2: Clip under the container of fixed-position (the LayoutView) -->
    <div style="overflow: hidden">
      <div id="target2" style="opacity: 0.1">
        <div style="position: fixed"></div>
      </div>
    </div>
    <!-- Case 3: Clip above the container of fixed-position. -->
    <div id="clip3" style="overflow: hidden">
      <div style="transform: translateY(0)">
        <div id="target3" style="opacity: 0.1">
          <div style="position: fixed"></div>
        </div>
      </div>
    </div>
    <!-- Case 4: Clip on the container of fixed-position. -->
    <div id="clip4" style="overflow: hidden; transform: translateY(0)">
      <div id="target4" style="opacity: 0.1">
        <div style="position: fixed"></div>
      </div>
    </div>
    <!-- Case 5: The container of fixed-position is not a LayoutBlock. -->
    <table>
      <tr style="transform: translateY(0)">
        <td id="target5" style="opacity: 0.1">
          <div style="position: fixed"></div>
        </td>
      </tr>
    </table>
  )HTML");

  EXPECT_EQ(DocContentClip(),
            PaintPropertiesForElement("target1")->Effect()->OutputClip());
  // OutputClip is null because the fixed descendant escapes the effect's
  // current clip.
  EXPECT_EQ(nullptr,
            PaintPropertiesForElement("target2")->Effect()->OutputClip());
  EXPECT_EQ(PaintPropertiesForElement("clip3")->OverflowClip(),
            PaintPropertiesForElement("target3")->Effect()->OutputClip());
  EXPECT_EQ(PaintPropertiesForElement("clip4")->OverflowClip(),
            PaintPropertiesForElement("target4")->Effect()->OutputClip());
  EXPECT_EQ(DocContentClip(),
            PaintPropertiesForElement("target5")->Effect()->OutputClip());
}

TEST_P(PaintPropertyTreeBuilderTest, TableColOpacity) {
  SetBodyInnerHTML(R"HTML(
    <table>
      <col id="col" style="opacity: 0.5">
    </table>
  )HTML");

  // TODO(crbug.com/892734): For now table col doesn't support effects.
  EXPECT_EQ(nullptr, PaintPropertiesForElement("col"));
}

// Test the WebView API that allows rendering the whole page. In this case, we
// shouldn't create a clip node for the main frame.
TEST_P(PaintPropertyTreeBuilderTest, MainFrameDoesntClipContent) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      body,html {
        margin: 0;
        width: 100%;
        height: 100%;
      }
    </style>
  )HTML");

  EXPECT_TRUE(
      GetLayoutView().FirstFragment().PaintProperties()->OverflowClip());

  GetPage().GetSettings().SetMainFrameClipsContent(false);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(
      GetLayoutView().FirstFragment().PaintProperties()->OverflowClip());

  GetPage().GetSettings().SetMainFrameClipsContent(true);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(
      GetLayoutView().FirstFragment().PaintProperties()->OverflowClip());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootCompositedClipPathSimple) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' style='clip-path: circle(); will-change: opacity'></svg>
  )HTML");

  const auto* properties = PaintPropertiesForElement("svg");

  const auto* transform = properties->PaintOffsetTranslation();
  ASSERT_NE(nullptr, transform);
  EXPECT_EQ(nullptr, properties->MaskClip());

  const auto* clip_path_clip = properties->ClipPathClip();
  ASSERT_NE(nullptr, clip_path_clip);
  EXPECT_EQ(DocContentClip(), clip_path_clip->Parent());
  EXPECT_CLIP_RECT(FloatRoundedRect(gfx::RectF(75, 0, 150, 150), 75),
                   clip_path_clip);
  EXPECT_EQ(transform, &clip_path_clip->LocalTransformSpace());
  EXPECT_FALSE(clip_path_clip->ClipPath());

  const auto* overflow_clip = properties->OverflowClip();
  ASSERT_NE(nullptr, overflow_clip);
  EXPECT_EQ(clip_path_clip, overflow_clip->Parent());
  EXPECT_CLIP_RECT(gfx::RectF(0, 0, 300, 150), overflow_clip);
  EXPECT_EQ(transform, &overflow_clip->LocalTransformSpace());

  const auto* effect = properties->Effect();
  ASSERT_NE(nullptr, effect);
  EXPECT_EQ(&EffectPaintPropertyNode::Root(), effect->Parent());
  EXPECT_EQ(transform, &effect->LocalTransformSpace());
  EXPECT_EQ(clip_path_clip, effect->OutputClip());
  EXPECT_EQ(SkBlendMode::kSrcOver, effect->BlendMode());

  EXPECT_EQ(nullptr, properties->Mask());
  EXPECT_EQ(nullptr, properties->ClipPathMask());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootCompositedClipPathComplex) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg'
         style='clip-path: polygon(75px 0, 225px 150px, 75px 150px, 75px 0);
                will-change: opacity'></svg>
  )HTML");

  const auto* properties = PaintPropertiesForElement("svg");

  const auto* transform = properties->PaintOffsetTranslation();
  ASSERT_NE(nullptr, transform);
  EXPECT_EQ(nullptr, properties->MaskClip());

  const auto* clip_path_clip = properties->ClipPathClip();
  ASSERT_NE(nullptr, clip_path_clip);
  EXPECT_EQ(DocContentClip(), clip_path_clip->Parent());
  EXPECT_CLIP_RECT(gfx::RectF(75, 0, 150, 150), clip_path_clip);
  EXPECT_EQ(transform, &clip_path_clip->LocalTransformSpace());
  EXPECT_TRUE(clip_path_clip->ClipPath());

  const auto* overflow_clip = properties->OverflowClip();
  ASSERT_NE(nullptr, overflow_clip);
  EXPECT_EQ(clip_path_clip, overflow_clip->Parent());
  EXPECT_CLIP_RECT(gfx::RectF(0, 0, 300, 150), overflow_clip);
  EXPECT_EQ(transform, &overflow_clip->LocalTransformSpace());

  const auto* effect = properties->Effect();
  ASSERT_NE(nullptr, effect);
  EXPECT_TRUE(effect->HasDirectCompositingReasons());
  EXPECT_EQ(&EffectPaintPropertyNode::Root(), effect->Parent());
  EXPECT_EQ(transform, &effect->LocalTransformSpace());
  EXPECT_EQ(clip_path_clip, effect->OutputClip());
  EXPECT_EQ(SkBlendMode::kSrcOver, effect->BlendMode());

  EXPECT_EQ(nullptr, properties->Mask());
  EXPECT_EQ(nullptr, properties->ClipPathMask());
}

TEST_P(PaintPropertyTreeBuilderTest, SimpleOpacityChangeDoesNotCausePacUpdate) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      div {
        width: 100px;
        height: 100px;
        opacity: 0.5;
        will-change: opacity;
      }
    </style>
    <div id="element"></div>
  )HTML");

  auto* pac = GetDocument().View()->GetPaintArtifactCompositor();
  ASSERT_TRUE(pac);

  const auto* properties = PaintPropertiesForElement("element");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->Effect());
  EXPECT_FLOAT_EQ(properties->Effect()->Opacity(), 0.5f);
  EXPECT_FALSE(pac->NeedsUpdate());

  cc::EffectNode* cc_effect =
      GetChromeClient()
          .layer_tree_host()
          ->property_trees()
          ->effect_tree_mutable()
          .FindNodeFromElementId(
              properties->Effect()->GetCompositorElementId());
  ASSERT_TRUE(cc_effect);
  EXPECT_FLOAT_EQ(cc_effect->opacity, 0.5f);
  EXPECT_TRUE(cc_effect->effect_changed);
  EXPECT_FALSE(GetChromeClient()
                   .layer_tree_host()
                   ->property_trees()
                   ->effect_tree()
                   .needs_update());

  Element* element = GetDocument().getElementById(AtomicString("element"));
  element->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.9"));

  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FLOAT_EQ(properties->Effect()->Opacity(), 0.9f);
  EXPECT_FLOAT_EQ(cc_effect->opacity, 0.9f);
  EXPECT_TRUE(cc_effect->effect_changed);
  EXPECT_FALSE(pac->NeedsUpdate());
  EXPECT_TRUE(GetChromeClient()
                  .layer_tree_host()
                  ->property_trees()
                  ->effect_tree()
                  .needs_update());
}

TEST_P(PaintPropertyTreeBuilderTest, SimpleScrollChangeDoesNotCausePacUpdate) {
  SetHtmlInnerHTML(R"HTML(
    <style>
      #element {
        width: 100px;
        height: 100px;
        overflow: scroll;
        will-change: transform;
      }
      #spacer {
        width: 100px;
        height: 1000px;
      }
    </style>
    <div id="element"><div id="spacer"></div></div>
  )HTML");

  auto* pac = GetDocument().View()->GetPaintArtifactCompositor();
  ASSERT_TRUE(pac);

  const auto* properties = PaintPropertiesForElement("element");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->ScrollTranslation());
  ASSERT_TRUE(properties->ScrollTranslation()->ScrollNode());
  EXPECT_EQ(gfx::Vector2dF(0, 0),
            properties->ScrollTranslation()->Get2dTranslation());
  EXPECT_FALSE(pac->NeedsUpdate());

  auto* property_trees = GetChromeClient().layer_tree_host()->property_trees();
  const auto* cc_scroll_node =
      property_trees->scroll_tree().FindNodeFromElementId(
          properties->ScrollTranslation()
              ->ScrollNode()
              ->GetCompositorElementId());
  ASSERT_TRUE(cc_scroll_node);

  const auto* cc_transform_node =
      property_trees->transform_tree().Node(cc_scroll_node->transform_id);
  ASSERT_TRUE(cc_transform_node);

  EXPECT_TRUE(cc_transform_node->local.IsIdentity());
  EXPECT_FLOAT_EQ(cc_transform_node->scroll_offset.x(), 0);
  EXPECT_FLOAT_EQ(cc_transform_node->scroll_offset.y(), 0);
  auto current_scroll_offset =
      property_trees->scroll_tree().current_scroll_offset(
          properties->ScrollTranslation()
              ->ScrollNode()
              ->GetCompositorElementId());
  EXPECT_FLOAT_EQ(current_scroll_offset.x(), 0);
  EXPECT_FLOAT_EQ(current_scroll_offset.y(), 0);

  GetDocument().getElementById(AtomicString("element"))->setScrollTop(10.);
  UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_EQ(gfx::Vector2dF(0, -10),
            properties->ScrollTranslation()->Get2dTranslation());
  EXPECT_FALSE(pac->NeedsUpdate());
  EXPECT_TRUE(cc_transform_node->local.IsIdentity());
  EXPECT_FLOAT_EQ(cc_transform_node->scroll_offset.x(), 0);
  EXPECT_FLOAT_EQ(cc_transform_node->scroll_offset.y(), 10);
  current_scroll_offset = property_trees->scroll_tree().current_scroll_offset(
      properties->ScrollTranslation()->ScrollNode()->GetCompositorElementId());
  EXPECT_FLOAT_EQ(current_scroll_offset.x(), 0);
  EXPECT_FLOAT_EQ(current_scroll_offset.y(), 10);
  EXPECT_TRUE(property_trees->transform_tree().needs_update());
  EXPECT_TRUE(cc_transform_node->transform_changed);

  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintPropertyTreeBuilderTest,
       SimpleStickyTranslationChangeDoesNotCausePacUpdate) {
  SetBodyInnerHTML(R"HTML(
    <style>::webkit-scrollbar { width: 0; height: 0 }</style>
    <!-- position: relative and z-index: 1 are needed to make the scroller a
     stacking context (otherwise scroll of a non-stacking-context containing
     stacked descendant would cause PAC update).
     TODO(wangxianzhu): Remove them when fixing crbug.com/1310586. -->
    <div id="scroller" style="width: 200px; height: 200px; overflow: scroll;
                              background: blue; position: relative; z-index: 1">
      <div style="height: 300px"></div>
      <div id="target" style="position: sticky; bottom: 0; height: 20px"></div>
    </div>
  )HTML");

  auto* pac = GetDocument().View()->GetPaintArtifactCompositor();
  ASSERT_TRUE(pac);

  auto* properties = PaintPropertiesForElement("target");
  ASSERT_TRUE(properties);
  auto* sticky_translation = properties->StickyTranslation();
  ASSERT_TRUE(sticky_translation);
  EXPECT_EQ(gfx::Vector2dF(0, -120), sticky_translation->Get2dTranslation());

  auto* property_trees = GetChromeClient().layer_tree_host()->property_trees();
  const auto* cc_transform_node =
      property_trees->transform_tree().FindNodeFromElementId(
          sticky_translation->GetCompositorElementId());
  ASSERT_TRUE(cc_transform_node);
  // We don't push the sticky offset to cc.
  EXPECT_EQ(gfx::Vector2dF(), cc_transform_node->local.To2dTranslation());

  GetDocument().getElementById(AtomicString("scroller"))->setScrollTop(200);
  UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_EQ(gfx::Vector2dF(), sticky_translation->Get2dTranslation());
  EXPECT_FALSE(pac->NeedsUpdate());
  EXPECT_EQ(gfx::Vector2dF(), cc_transform_node->local.To2dTranslation());
  EXPECT_TRUE(property_trees->transform_tree().needs_update());
  EXPECT_TRUE(cc_transform_node->transform_changed);

  UpdateAllLifecyclePhasesForTest();
}

TEST_P(PaintPropertyTreeBuilderTest,
       NonCompositedTransformChangeCausesPacUpdate) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #outer {
        width: 100px;
        height: 100px;
        transform: translateY(0);
      }
      #inner {
        width: 10px;
        height: 10px;
        will-change: transform;
      }
    </style>
    <div id="outer">
      <div id="inner"></div>
    </div>
  )HTML");

  EXPECT_FALSE(
      GetDocument().View()->GetPaintArtifactCompositor()->NeedsUpdate());

  Element* outer = GetDocument().getElementById(AtomicString("outer"));
  outer->setAttribute(html_names::kStyleAttr,
                      AtomicString("transform: translateY(10px)"));
  UpdateAllLifecyclePhasesExceptPaint();

  EXPECT_TRUE(
      GetDocument().View()->GetPaintArtifactCompositor()->NeedsUpdate());
}

TEST_P(PaintPropertyTreeBuilderTest, VideoClipRect) {
  SetBodyInnerHTML(R"HTML(
    <video id="video" style="position:absolute;top:0;left:0;" controls
       src="missing_file.webm" width=320.2 height=240>
    </video>
  )HTML");

  Element* video_element = GetDocument().getElementById(AtomicString("video"));
  ASSERT_NE(nullptr, video_element);
  video_element->SetInlineStyleProperty(CSSPropertyID::kWidth, "320.2px");
  video_element->SetInlineStyleProperty(CSSPropertyID::kTop, "0.1px");
  video_element->SetInlineStyleProperty(CSSPropertyID::kLeft, "0.1px");
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhasesForTest();
  const ObjectPaintProperties* video_element_properties =
      video_element->GetLayoutObject()->FirstFragment().PaintProperties();
  // |video_element| is now sub-pixel positioned, at 0.1,0.1 320.2x240. With or
  // without pixel-snapped clipping, this will get clipped at 0,0 320x240.
  EXPECT_CLIP_RECT(FloatRoundedRect(0, 0, 320, 240),
                   video_element_properties->OverflowClip());

  // Now, move |video_element| to 10.4,10.4. At this point, without pixel
  // snapping that doesn't depend on paint offset, it will be clipped at 10,10
  // 321x240. With proper pixel snapping, the clip will be at 10,10,320,240.
  video_element->SetInlineStyleProperty(CSSPropertyID::kTop, "10.4px");
  video_element->SetInlineStyleProperty(CSSPropertyID::kLeft, "10.4px");
  frame_view->UpdateAllLifecyclePhasesForTest();
  EXPECT_CLIP_RECT(FloatRoundedRect(10, 10, 320, 240),
                   video_element_properties->OverflowClip());
}

// For NoPaintPropertyForXXXText cases. The styles trigger almost all paint
// properties on the container. The contained text should not create paint
// properties in any case.
#define ALL_PROPERTY_STYLES                                                  \
  "backface-visibility: hidden; transform: rotateY(1deg); perspective: 1px;" \
  "opacity: 0.5; filter: blur(5px); clip-path: circle(100%); "               \
  "clip: rect(0px, 2px, 2px, 0px); overflow: scroll; border-radius: 2px; "   \
  "width: 10px; height: 10px; top: 0; left: 0; position: sticky; columns: 2"

TEST_P(PaintPropertyTreeBuilderTest, NoPaintPropertyForBlockText) {
  SetBodyInnerHTML("<div id='container' style='" ALL_PROPERTY_STYLES
                   "'>T</div>");
  EXPECT_TRUE(PaintPropertiesForElement("container"));
  auto* text = GetDocument()
                   .getElementById(AtomicString("container"))
                   ->firstChild()
                   ->GetLayoutObject();
  ASSERT_TRUE(text->IsText());
  EXPECT_FALSE(text->FirstFragment().PaintProperties());
}

TEST_P(PaintPropertyTreeBuilderTest, NoPaintPropertyForInlineText) {
  SetBodyInnerHTML("<span id='container' style='" ALL_PROPERTY_STYLES
                   "'>T</span>");
  EXPECT_TRUE(PaintPropertiesForElement("container"));
  auto* text = GetDocument()
                   .getElementById(AtomicString("container"))
                   ->firstChild()
                   ->GetLayoutObject();
  ASSERT_TRUE(text->IsText());
  EXPECT_FALSE(text->FirstFragment().PaintProperties());
}

TEST_P(PaintPropertyTreeBuilderTest, NoPaintPropertyForSVGText) {
  SetBodyInnerHTML("<svg><text id='container' style='" ALL_PROPERTY_STYLES
                   "'>T</text>");
  EXPECT_TRUE(PaintPropertiesForElement("container"));
  auto* text = GetDocument()
                   .getElementById(AtomicString("container"))
                   ->firstChild()
                   ->GetLayoutObject();
  ASSERT_TRUE(text->IsText());
  EXPECT_FALSE(text->FirstFragment().PaintProperties());
}

TEST_P(PaintPropertyTreeBuilderTest, IsAffectedByOuterViewportBoundsDelta) {
  SetBodyInnerHTML(R"HTML(
    <style>div { will-change: transform; position: fixed; }</style>
    <div id="fixed1"></div>
    <div id="fixed2" style="right: 0"></div>
    <div id="fixed3" style="bottom: 0"></div>
    <div id="fixed4" style="bottom: 20px"></div>
    <div style="transform: translateX(100px)">
      <div id="fixed5" style="bottom: 0"></div>
    </div>
    <iframe></iframe>
  )HTML");
  SetChildFrameHTML(R"HTML(
     <div id="fixed"
          style="will-change: transform; position: fixed; bottom: 0"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  auto check_result = [&](const ObjectPaintProperties* properties,
                          bool expected) {
    ASSERT_TRUE(properties);
    ASSERT_TRUE(properties->PaintOffsetTranslation());
    EXPECT_EQ(expected, properties->PaintOffsetTranslation()
                            ->IsAffectedByOuterViewportBoundsDelta());
  };

  check_result(PaintPropertiesForElement("fixed1"), false);
  check_result(PaintPropertiesForElement("fixed2"), false);
  check_result(PaintPropertiesForElement("fixed3"), true);
  check_result(PaintPropertiesForElement("fixed4"), true);
  check_result(PaintPropertiesForElement("fixed5"), false);

  // Fixed elements in subframes are not affected by viewport.
  check_result(ChildDocument()
                   .getElementById(AtomicString("fixed"))
                   ->GetLayoutObject()
                   ->FirstFragment()
                   .PaintProperties(),
               false);
}

TEST_P(PaintPropertyTreeBuilderTest, TransformAnimationAxisAlignment) {
  SetBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        @keyframes transform_translation {
          0% { transform: translate(10px, 11px); }
          100% { transform: translate(20px, 21px); }
        }
        #translation_animation {
          animation-name: transform_translation;
          animation-duration: 1s;
          width: 100px;
          height: 100px;
          will-change: transform;
        }
        @keyframes transform_rotation {
          0% { transform: rotateZ(10deg); }
          100% { transform: rotateZ(20deg); }
        }
        #rotation_animation {
          animation-name: transform_rotation;
          animation-duration: 1s;
          width: 100px;
          height: 100px;
          will-change: transform;
        }
      </style>
      <div id="translation_animation"></div>
      <div id="rotation_animation"></div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  const auto* translation =
      PaintPropertiesForElement("translation_animation")->Transform();
  EXPECT_TRUE(translation->HasActiveTransformAnimation());
  EXPECT_TRUE(translation->TransformAnimationIsAxisAligned());

  const auto* rotation =
      PaintPropertiesForElement("rotation_animation")->Transform();
  EXPECT_TRUE(rotation->HasActiveTransformAnimation());
  EXPECT_FALSE(rotation->TransformAnimationIsAxisAligned());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollPropertyHierarchy) {
  SetBodyInnerHTML(R"HTML(
    <div id="top-scroller"
        style="position: relative; width: 50px; height: 50px; overflow: scroll">
      <div id="middle-scroller"
           style="width: 100px; height: 100px; overflow: scroll; opacity: 0.9">
        <div id="fixed" style="position: fixed"></div>
        <div id="absolute" style="position: absolute"></div>
        <div id="relative" style="position: relative; height: 1000px"></div>
      </div>
    </div>
  )HTML");

  auto* top_properties = PaintPropertiesForElement("top-scroller");
  ASSERT_TRUE(top_properties->OverflowClip());
  EXPECT_EQ(top_properties->ScrollTranslation()->ScrollNode(),
            top_properties->Scroll());

  auto* middle_properties = PaintPropertiesForElement("middle-scroller");
  EXPECT_EQ(middle_properties->PaintOffsetTranslation(),
            &middle_properties->OverflowClip()->LocalTransformSpace());
  EXPECT_EQ(top_properties->OverflowClip(),
            middle_properties->OverflowClip()->Parent());
  EXPECT_EQ(top_properties->Scroll(), middle_properties->Scroll()->Parent());
  EXPECT_EQ(middle_properties->ScrollTranslation()->ScrollNode(),
            middle_properties->Scroll());
  EXPECT_EQ(top_properties->ScrollTranslation(),
            middle_properties->ScrollTranslation()->Parent()->Parent());
  EXPECT_EQ(middle_properties->PaintOffsetTranslation(),
            &middle_properties->Effect()->LocalTransformSpace());

  // |fixed| escapes both top and middle scrollers.
  auto& fixed_fragment = GetLayoutObjectByElementId("fixed")->FirstFragment();
  EXPECT_EQ(fixed_fragment.PaintProperties()->PaintOffsetTranslation(),
            &fixed_fragment.PreTransform());
  EXPECT_EQ(top_properties->OverflowClip()->Parent(),
            &fixed_fragment.PreClip());

  // |absolute| escapes |middle-scroller| (position: static), but is contained
  // by |top-scroller| (position: relative)
  auto& absolute_fragment =
      GetLayoutObjectByElementId("absolute")->FirstFragment();
  EXPECT_EQ(top_properties->ScrollTranslation(),
            &absolute_fragment.PreTransform());
  EXPECT_EQ(top_properties->OverflowClip(), &absolute_fragment.PreClip());

  // |relative| is contained by |middle-scroller|.
  auto& relative_fragment =
      GetLayoutObjectByElementId("relative")->FirstFragment();
  EXPECT_EQ(middle_properties->ScrollTranslation(),
            &relative_fragment.PreTransform());
  EXPECT_EQ(middle_properties->OverflowClip(), &relative_fragment.PreClip());

  // The opacity on |middle-scroller| applies to all children.
  EXPECT_EQ(middle_properties->Effect(),
            &fixed_fragment.LocalBorderBoxProperties().Effect());
  EXPECT_EQ(middle_properties->Effect(),
            &absolute_fragment.LocalBorderBoxProperties().Effect());
  EXPECT_EQ(middle_properties->Effect(),
            &relative_fragment.LocalBorderBoxProperties().Effect());
}

TEST_P(PaintPropertyTreeBuilderTest, CompositedInline) {
  SetBodyInnerHTML(R"HTML(
    <span id="span" style="will-change: transform; position: relative">
      SPAN
    </span>
  )HTML");

  auto* properties = PaintPropertiesForElement("span");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->Transform());
  EXPECT_TRUE(properties->Transform()->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest, OutOfFlowContainedInMulticol) {
  SetBodyInnerHTML(R"HTML(
    <div id="columns" style="columns: 2; height: 100px">
      <div id="relative"
           style="position: relative; height: 200px; transform: translateX(0)">
        <div style="overflow: clip; height: 150px">
          <div id="absolute"
               style="position: absolute; width: 100%; height: 200px"></div>
          <div id="fixed"
               style="position: fixed; width: 100%; height: 200px"></div>
        </div>
      </div>
    </div>
  )HTML");

  const auto* relative = GetLayoutObjectByElementId("relative");
  ASSERT_EQ(2u, NumFragments(relative));
  const auto* absolute = GetLayoutObjectByElementId("absolute");
  ASSERT_EQ(2u, NumFragments(absolute));
  const auto* fixed = GetLayoutObjectByElementId("fixed");
  ASSERT_EQ(2u, NumFragments(fixed));

  for (unsigned i = 0; i < NumFragments(relative); i++) {
    SCOPED_TRACE(testing::Message() << "Fragment " << i);
    const auto* relative_transform =
        FragmentAt(relative, i).PaintProperties()->Transform();
    const auto& absolute_properties =
        FragmentAt(absolute, i).LocalBorderBoxProperties();
    const auto& fixed_properties =
        FragmentAt(fixed, i).LocalBorderBoxProperties();
    EXPECT_EQ(relative_transform, &absolute_properties.Transform());
    EXPECT_EQ(relative_transform, &fixed_properties.Transform());
  }
}

TEST_P(PaintPropertyTreeBuilderTest, SVGChildBackdropFilter) {
  SetBodyInnerHTML(R"HTML(
    <svg id="svg">
      <text id="text" style="backdrop-filter: blur(5px)">Text</text>
    </svg>
  )HTML");

  auto* svg_properties = PaintPropertiesForElement("svg");
  ASSERT_TRUE(svg_properties);
  ASSERT_TRUE(svg_properties->PaintOffsetTranslation());
  EXPECT_FALSE(
      svg_properties->PaintOffsetTranslation()->HasDirectCompositingReasons());

  auto* svg_text_properties = PaintPropertiesForElement("text");
  ASSERT_TRUE(svg_text_properties);
  ASSERT_TRUE(svg_text_properties->Effect());
  EXPECT_TRUE(svg_text_properties->Effect()->HasDirectCompositingReasons());
  // TODO(crbug.com/1131987): Backdrop-filter doesn't work in SVG yet.
  EXPECT_FALSE(svg_text_properties->Effect()->BackdropFilter());
  EXPECT_FALSE(svg_text_properties->Transform());
  EXPECT_FALSE(GetLayoutObjectByElementId("text")
                   ->SlowFirstChild()
                   ->FirstFragment()
                   .PaintProperties());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGTransformAnimationAndOrigin) {
  SetBodyInnerHTML(R"HTML(
    <svg width="200" height="200">
      <rect id="rect"
            style="animation: 2s infinite spin; transform-origin: 50% 50%">
    </svg>
    <style>
      @keyframes spin {
        0% { transform: rotate(0); }
        100% { transform: rotate(360deg); }
      }
    </style>
  )HTML");

  auto* properties = PaintPropertiesForElement("rect");
  ASSERT_TRUE(properties);
  auto* transform_node = properties->Transform();
  ASSERT_TRUE(transform_node);
  EXPECT_TRUE(transform_node->HasActiveTransformAnimation());
  EXPECT_EQ(gfx::Transform(), transform_node->Matrix());
  EXPECT_EQ(gfx::Point3F(100, 100, 0), transform_node->Origin());
}

TEST_P(PaintPropertyTreeBuilderTest, WillChangeBackdropFilter) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="will-change: backdrop-filter"></div>
  )HTML");

  auto* properties = PaintPropertiesForElement("target");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->Effect());
  EXPECT_FALSE(properties->Effect()->BackdropFilter());
  EXPECT_TRUE(
      properties->Effect()->RequiresCompositingForWillChangeBackdropFilter());

  // will-change:backdrop-filter should not cause transform or filter node.
  EXPECT_FALSE(properties->Transform());
  EXPECT_FALSE(properties->Filter());
}

TEST_P(PaintPropertyTreeBuilderTest,
       WillChangeBackdropFilterWithTransformAndFilter) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="will-change: backdrop-filter;
        transform: translateX(10px); filter: blur(5px)"></div>
  )HTML");

  auto* properties = PaintPropertiesForElement("target");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->Effect());
  EXPECT_FALSE(properties->Effect()->BackdropFilter());
  EXPECT_TRUE(
      properties->Effect()->RequiresCompositingForWillChangeBackdropFilter());

  // will-change:backdrop-filter should not add compositing reason for the
  // transform or the filter node.
  ASSERT_TRUE(properties->Transform());
  EXPECT_FALSE(properties->Transform()->HasDirectCompositingReasons());
  ASSERT_TRUE(properties->Filter());
  EXPECT_FALSE(properties->Filter()->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest, WillChangeFilter) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="will-change: filter"></div>
  )HTML");

  auto* properties = PaintPropertiesForElement("target");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->Filter());
  EXPECT_TRUE(properties->Filter()->Filter().IsEmpty());
  EXPECT_TRUE(properties->Filter()->RequiresCompositingForWillChangeFilter());

  // will-change:filter should not cause transform or effect node.
  EXPECT_FALSE(properties->Transform());
  EXPECT_FALSE(properties->Effect());
}

TEST_P(PaintPropertyTreeBuilderTest, WillChangeFilterWithTransformAndOpacity) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="will-change: filter;
        transform: translateX(10px); opacity: 0.5"></div>
  )HTML");

  auto* properties = PaintPropertiesForElement("target");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->Filter());
  EXPECT_TRUE(properties->Filter()->Filter().IsEmpty());
  EXPECT_TRUE(properties->Filter()->RequiresCompositingForWillChangeFilter());

  // will-change:filter should not add compositing reason for the transform or
  // the filter node.
  ASSERT_TRUE(properties->Transform());
  EXPECT_FALSE(properties->Transform()->HasDirectCompositingReasons());
  ASSERT_TRUE(properties->Effect());
  EXPECT_FALSE(properties->Effect()->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectCanUseCurrentClipAsOutputClipCrash) {
  SetBodyInnerHTML(R"HTML(
      <style type="text/css">
      .c1 { transform: rotate(180deg); }
      .c9 { position: relative; opacity: 0.1; }
      .c9 > .c18 { position: fixed; }
      </style>
      <fieldset id="f" class="c1"><samp class="c9"><footer
       class="c18"></footer></samp></fiedlset>
  )HTML");

  EXPECT_TRUE(GetLayoutObjectByElementId("f")
                  ->SlowFirstChild()
                  ->FirstFragment()
                  .HasLocalBorderBoxProperties());
}

// Test case for crbug.com/1381173.
TEST_P(PaintPropertyTreeBuilderTest, EffectOutputClipOfMissedOutOfFlow) {
  SetBodyInnerHTML(R"HTML(
    <div style="columns:2; column-fill:auto; height:100px;">
      <div style="height:150px;"></div>
      <div style="will-change:transform; width:50px; height:50px;">
        <div id="oof" style="position:absolute; top:-100px; opacity:0;">
          <div style="position:fixed;"></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* properties = PaintPropertiesForElement("oof");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->Effect());
  EXPECT_FALSE(properties->Effect()->OutputClip());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformChangesInvalidateGeometryMapper) {
  SetBodyInnerHTML(R"HTML(
    <style>#div { width:10px; height:10px; transform:translateX(9px); }</style>
    <div id="div" style="transform: translateX(5px);"></div>
  )HTML");

  const auto* properties = PaintPropertiesForElement("div");
  const auto& transform_cache = GetTransformCache(*properties->Transform());
  EXPECT_TRUE(transform_cache.IsValid());

  // Change the transform and ensure the geometry mapper cache is invalidated.
  auto* div = GetDocument().getElementById(AtomicString("div"));
  div->removeAttribute(html_names::kStyleAttr);
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(transform_cache.IsValid());

  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(transform_cache.IsValid());

  // Make a color change and ensure the geometry mapper cache is not
  // invalidated.
  div->setAttribute(html_names::kStyleAttr, AtomicString("background: green;"));
  UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_TRUE(transform_cache.IsValid());
}

TEST_P(PaintPropertyTreeBuilderTest,
       GeometryMapperCacheInvalidationAcrossIsolationNodes) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #composited { transform: translate3d(1px, 2px, 3px); }
      #container { contain: layout paint; width: 100px; height: 100px; }
      #target { transform: translateX(1px); }
    </style>
    <div id='composited'>
      <div id='container' style='transform: translate3d(1px, 2px, 3px);'>
        <div id='target'></div>
      </div>
    </div>
  )HTML");

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhasesForTest();

  auto* container = GetLayoutObjectByElementId("container");
  auto* container_properties = container->FirstFragment().PaintProperties();
  auto* target = GetLayoutObjectByElementId("target");
  auto* target_properties = target->FirstFragment().PaintProperties();
  EXPECT_EQ(target_properties->Transform()->NearestDirectlyCompositedAncestor(),
            container_properties->Transform());

  // Remove the direct compositing reason from #container.
  auto* container_element =
      GetDocument().getElementById(AtomicString("container"));
  container_element->setAttribute(html_names::kStyleAttr, g_empty_atom);
  frame_view->UpdateAllLifecyclePhasesForTest();

  auto* composited = GetLayoutObjectByElementId("composited");
  auto* composited_properties = composited->FirstFragment().PaintProperties();
  EXPECT_EQ(target_properties->Transform()->NearestDirectlyCompositedAncestor(),
            composited_properties->Transform());
}

TEST_P(PaintPropertyTreeBuilderTest, PromoteTrivial3DWithHighEndDevice) {
  SetBodyInnerHTML(R"HTML(
    <style>div {width: 100px; height: 100px; transform: translateZ(0)}</style>
    <div id='non-scroll'></div>
    <div id='scroll' style='overflow: scroll'>
      <div style='height: 2000px'></div>
    </div>
    <div id='effect' style="opacity: 0.5"></div>
  )HTML");

  const auto* non_scroll_properties = PaintPropertiesForElement("non-scroll");
  EXPECT_TRUE(
      non_scroll_properties->Transform()->HasDirectCompositingReasons());
  EXPECT_FALSE(non_scroll_properties->Effect());

  const auto* scroll_properties = PaintPropertiesForElement("scroll");
  EXPECT_TRUE(scroll_properties->Transform()->HasDirectCompositingReasons());
  EXPECT_EQ(CompositedScrollingPreference::kPreferred,
            scroll_properties->Scroll()->GetCompositedScrollingPreference());
  EXPECT_FALSE(scroll_properties->Effect());

  // Trivial 3d transform also triggers composited effect if effect exist.
  const auto* effect_properties = PaintPropertiesForElement("effect");
  EXPECT_TRUE(effect_properties->Transform()->HasDirectCompositingReasons());
  EXPECT_TRUE(effect_properties->Effect()->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest, DontPromoteTrivial3DWithLowEndDevice) {
  class LowEndPlatform : public TestingPlatformSupport {
    bool IsLowEndDevice() override { return true; }
  };

  ScopedTestingPlatformSupport<LowEndPlatform> platform;
  SetBodyInnerHTML(R"HTML(
    <style>div {width: 100px; height: 100px; transform: translateZ(0)}</style>
    <div id='non-scroll'></div>
    <div id='scroll' style='overflow: scroll'>
      <div style='height: 2000px'></div>
    </div>
    <div id='effect' style="opacity: 0.5"></div>
  )HTML");

  const auto* non_scroll_properties = PaintPropertiesForElement("non-scroll");
  EXPECT_FALSE(
      non_scroll_properties->Transform()->HasDirectCompositingReasons());
  EXPECT_FALSE(non_scroll_properties->Effect());

  const auto* scroll_properties = PaintPropertiesForElement("scroll");
  EXPECT_FALSE(scroll_properties->Transform()->HasDirectCompositingReasons());
  // We still prefer composited scrolling with Trivial 3d transform.
  EXPECT_EQ(CompositedScrollingPreference::kPreferred,
            scroll_properties->Scroll()->GetCompositedScrollingPreference());
  EXPECT_FALSE(scroll_properties->Effect());

  const auto* effect_properties = PaintPropertiesForElement("effect");
  EXPECT_FALSE(effect_properties->Transform()->HasDirectCompositingReasons());
  EXPECT_FALSE(effect_properties->Effect()->HasDirectCompositingReasons());
}

#define EXPECT_BACKGROUND_CLIP(properties, rect)                            \
  do {                                                                      \
    ASSERT_TRUE(properties);                                                \
    ASSERT_TRUE(properties->BackgroundClip());                              \
    EXPECT_EQ(rect, properties->BackgroundClip()->PaintClipRect().Rect());  \
    EXPECT_EQ(rect, properties->BackgroundClip()->LayoutClipRect().Rect()); \
  } while (false)

TEST_P(PaintPropertyTreeBuilderTest, BackgroundClip) {
  SetPreferCompositingToLCDText(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #target {
        width: 300px;
        height: 200px;
        border: 20px solid black;
        padding: 10px;
        box-sizing: border-box;
        background-image: linear-gradient(blue, red);
        background-attachment: fixed;
      }
    </style>
    <div id="target"></div>
  )HTML");

  auto* target = GetDocument().getElementById(AtomicString("target"));
  EXPECT_BACKGROUND_CLIP(PaintPropertiesForElement("target"),
                         gfx::RectF(0, 0, 300, 200));

  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("background-clip: padding-box"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_BACKGROUND_CLIP(PaintPropertiesForElement("target"),
                         gfx::RectF(20, 20, 260, 160));

  target->setAttribute(
      html_names::kStyleAttr,
      AtomicString("background-clip: padding-box; border-top-width: 15px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_BACKGROUND_CLIP(PaintPropertiesForElement("target"),
                         gfx::RectF(20, 15, 260, 165));

  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("background-clip: content-box"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_BACKGROUND_CLIP(PaintPropertiesForElement("target"),
                         gfx::RectF(30, 30, 240, 140));

  target->setAttribute(
      html_names::kStyleAttr,
      AtomicString("background-clip: content-box; padding-left: 25px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_BACKGROUND_CLIP(PaintPropertiesForElement("target"),
                         gfx::RectF(45, 30, 225, 140));

  target->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_BACKGROUND_CLIP(PaintPropertiesForElement("target"),
                         gfx::RectF(0, 0, 300, 200));

  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("border-radius: 20px"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(PaintPropertiesForElement("target"));

  target->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_BACKGROUND_CLIP(PaintPropertiesForElement("target"),
                         gfx::RectF(0, 0, 300, 200));

  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("box-shadow: 10px 20px blue"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(PaintPropertiesForElement("target"));

  target->setAttribute(html_names::kStyleAttr, g_empty_atom);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_BACKGROUND_CLIP(PaintPropertiesForElement("target"),
                         gfx::RectF(0, 0, 300, 200));

  target->setAttribute(html_names::kStyleAttr,
                       AtomicString("background-attachment: local"));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(PaintPropertiesForElement("target"));
}

TEST_P(PaintPropertyTreeBuilderTest, BackgroundClipFragmented) {
  SetPreferCompositingToLCDText(true);

  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #target {
        height: 600px;
        border: 20px solid black;
        padding: 10px;
        box-sizing: border-box;
        background-image: linear-gradient(blue, red);
        background-attachment: fixed;
        background-clip: content-box;
      }
    </style>
    <div style="width: 300px; height: 200px; columns: 3; column-gap: 0">
      <div id="target"></div>
     </div>
  )HTML");

  auto* target = GetLayoutObjectByElementId("target");
  ASSERT_EQ(3u, NumFragments(target));
  EXPECT_BACKGROUND_CLIP(FragmentAt(target, 0).PaintProperties(),
                         gfx::RectF(30, 30, 40, 170));
  EXPECT_BACKGROUND_CLIP(FragmentAt(target, 1).PaintProperties(),
                         gfx::RectF(130, 0, 40, 200));
  EXPECT_BACKGROUND_CLIP(FragmentAt(target, 2).PaintProperties(),
                         gfx::RectF(230, 0, 40, 170));

  GetDocument()
      .getElementById(AtomicString("target"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("background-clip: padding-box"));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(3u, NumFragments(target));
  EXPECT_BACKGROUND_CLIP(FragmentAt(target, 0).PaintProperties(),
                         gfx::RectF(20, 20, 60, 180));
  EXPECT_BACKGROUND_CLIP(FragmentAt(target, 1).PaintProperties(),
                         gfx::RectF(120, 0, 60, 200));
  EXPECT_BACKGROUND_CLIP(FragmentAt(target, 2).PaintProperties(),
                         gfx::RectF(220, 0, 60, 180));

  GetDocument()
      .getElementById(AtomicString("target"))
      ->setAttribute(html_names::kStyleAttr,
                     AtomicString("background-clip: border-box"));
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(3u, NumFragments(target));
  EXPECT_BACKGROUND_CLIP(FragmentAt(target, 0).PaintProperties(),
                         gfx::RectF(0, 0, 100, 200));
  EXPECT_BACKGROUND_CLIP(FragmentAt(target, 1).PaintProperties(),
                         gfx::RectF(100, 0, 100, 200));
  EXPECT_BACKGROUND_CLIP(FragmentAt(target, 2).PaintProperties(),
                         gfx::RectF(200, 0, 100, 200));
}

TEST_P(PaintPropertyTreeBuilderTest, OverlayScrollbarEffects) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 100px; height: 100px; overflow: scroll">
      <div style="height: 300px"></div>
    </div>
  )HTML");
  CHECK(GetDocument().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars());

  auto* properties = PaintPropertiesForElement("target");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->OverflowClip());
  EXPECT_FALSE(properties->HorizontalScrollbarEffect());
  ASSERT_TRUE(properties->VerticalScrollbarEffect());
  EXPECT_EQ(properties->OverflowClip()->Parent(),
            properties->VerticalScrollbarEffect()->OutputClip());
}

TEST_P(PaintPropertyTreeBuilderTest, OverlayScrollbarEffectsWithRadius) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="width: 100px; height: 100px; border-radius: 10px;
                            overflow: scroll">
      <div style="height: 300px"></div>
    </div>
  )HTML");
  CHECK(GetDocument().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars());

  auto* properties = PaintPropertiesForElement("target");
  ASSERT_TRUE(properties);
  ASSERT_TRUE(properties->OverflowClip());
  ASSERT_TRUE(properties->InnerBorderRadiusClip());
  EXPECT_FALSE(properties->HorizontalScrollbarEffect());
  ASSERT_TRUE(properties->VerticalScrollbarEffect());
  EXPECT_EQ(properties->InnerBorderRadiusClip()->Parent(),
            properties->VerticalScrollbarEffect()->OutputClip());
}

}  // namespace blink
