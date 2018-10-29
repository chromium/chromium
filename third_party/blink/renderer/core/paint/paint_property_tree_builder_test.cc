// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_property_tree_builder_test.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/paint/object_paint_properties.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_printer.h"
#include "third_party/blink/renderer/platform/graphics/paint/geometry_mapper.h"

namespace blink {

void PaintPropertyTreeBuilderTest::LoadTestData(const char* file_name) {
  String full_path = test::BlinkRootDir();
  full_path.append("/renderer/core/paint/test_data/");
  full_path.append(file_name);
  const Vector<char> input_buffer =
      test::ReadFromFile(full_path)->CopyAs<Vector<char>>();
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
      .getElementById(name)
      ->GetLayoutObject()
      ->FirstFragment()
      .PaintProperties();
}

void PaintPropertyTreeBuilderTest::SetUp() {
  RenderingTest::SetUp();
  EnableCompositing();
}

#define CHECK_VISUAL_RECT(expected, source_object, ancestor, slop_factor)      \
  do {                                                                         \
    if ((source_object)->HasLayer() && (ancestor)->HasLayer()) {               \
      LayoutRect actual((source_object)->LocalVisualRect());                   \
      (source_object)                                                          \
          ->MapToVisualRectInAncestorSpace(ancestor, actual,                   \
                                           kUseGeometryMapper);                \
      SCOPED_TRACE("GeometryMapper: ");                                        \
      EXPECT_EQ(expected, actual);                                             \
    }                                                                          \
                                                                               \
    if (slop_factor == LayoutUnit::Max())                                      \
      break;                                                                   \
    LayoutRect slow_path_rect = (source_object)->LocalVisualRect();            \
    (source_object)->MapToVisualRectInAncestorSpace(ancestor, slow_path_rect); \
    if (slop_factor) {                                                         \
      LayoutRect inflated_expected = LayoutRect(expected);                     \
      inflated_expected.Inflate(slop_factor);                                  \
      SCOPED_TRACE(String::Format(                                             \
          "Slow path rect: %s, Expected: %s, Inflated expected: %s",           \
          slow_path_rect.ToString().Ascii().data(),                            \
          expected.ToString().Ascii().data(),                                  \
          inflated_expected.ToString().Ascii().data()));                       \
      EXPECT_TRUE(LayoutRect(EnclosingIntRect(slow_path_rect))                 \
                      .Contains(LayoutRect(expected)));                        \
      EXPECT_TRUE(inflated_expected.Contains(slow_path_rect));                 \
    } else {                                                                   \
      SCOPED_TRACE("Slow path: ");                                             \
      EXPECT_EQ(expected, slow_path_rect);                                     \
    }                                                                          \
  } while (0)

#define CHECK_EXACT_VISUAL_RECT(expected, source_object, ancestor) \
  CHECK_VISUAL_RECT(expected, source_object, ancestor, 0)

INSTANTIATE_PAINT_TEST_CASE_P(PaintPropertyTreeBuilderTest);

TEST_P(PaintPropertyTreeBuilderTest, FixedPosition) {
  LoadTestData("fixed-position.html");

  Element* positioned_scroll = GetDocument().getElementById("positionedScroll");
  positioned_scroll->setScrollTop(3);
  Element* transformed_scroll =
      GetDocument().getElementById("transformedScroll");
  transformed_scroll->setScrollTop(5);

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhases();

  // target1 is a fixed-position element inside an absolute-position scrolling
  // element.  It should be attached under the viewport to skip scrolling and
  // offset of the parent.
  Element* target1 = GetDocument().getElementById("target1");
  const ObjectPaintProperties* target1_properties =
      target1->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(FloatRoundedRect(200, 150, 100, 100),
            target1_properties->OverflowClip()->ClipRect());
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
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetDocument()
                  .GetPage()
                  ->GetVisualViewport()
                  .GetScrollTranslationNode()
                  ->ScrollNode(),
              positioned_scroll_node->Parent());
  } else {
    EXPECT_EQ(DocScroll(), positioned_scroll_node->Parent());
  }
  EXPECT_EQ(TransformationMatrix().Translate(0, -3),
            positioned_scroll_translation->Matrix());
  EXPECT_EQ(nullptr, target1_properties->ScrollTranslation());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(200, 150, 100, 100),
                          target1->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // target2 is a fixed-position element inside a transformed scrolling element.
  // It should be attached under the scrolled box of the transformed element.
  Element* target2 = GetDocument().getElementById("target2");
  const ObjectPaintProperties* target2_properties =
      target2->GetLayoutObject()->FirstFragment().PaintProperties();
  Element* scroller = GetDocument().getElementById("transformedScroll");
  const ObjectPaintProperties* scroller_properties =
      scroller->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(FloatRoundedRect(200, 150, 100, 100),
            target2_properties->OverflowClip()->ClipRect());
  EXPECT_EQ(scroller_properties->OverflowClip(),
            target2_properties->OverflowClip()->Parent());
  // target2 should not have it's own scroll node and instead should inherit
  // transformedScroll's.
  const ObjectPaintProperties* transformed_scroll_properties =
      transformed_scroll->GetLayoutObject()->FirstFragment().PaintProperties();
  auto* transformed_scroll_translation =
      transformed_scroll_properties->ScrollTranslation();
  auto* transformed_scroll_node = transformed_scroll_translation->ScrollNode();
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetDocument()
                  .GetPage()
                  ->GetVisualViewport()
                  .GetScrollTranslationNode()
                  ->ScrollNode(),
              positioned_scroll_node->Parent());
  } else {
    EXPECT_EQ(DocScroll(), transformed_scroll_node->Parent());
  }
  EXPECT_EQ(TransformationMatrix().Translate(0, -5),
            transformed_scroll_translation->Matrix());
  EXPECT_EQ(nullptr, target2_properties->ScrollTranslation());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(208, 153, 200, 100),
                          target2->GetLayoutObject(),
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, PositionAndScroll) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  LoadTestData("position-and-scroll.html");

  Element* scroller = GetDocument().getElementById("scroller");
  scroller->scrollTo(0, 100);
  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhases();
  const ObjectPaintProperties* scroller_properties =
      scroller->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(0, -100),
            scroller_properties->ScrollTranslation()->Matrix());
  EXPECT_EQ(scroller_properties->PaintOffsetTranslation(),
            scroller_properties->ScrollTranslation()->Parent());
  EXPECT_EQ(DocScrollTranslation(),
            scroller_properties->PaintOffsetTranslation()->Parent());
  EXPECT_EQ(scroller_properties->PaintOffsetTranslation(),
            scroller_properties->OverflowClip()->LocalTransformSpace());
  const auto* scroll = scroller_properties->ScrollTranslation()->ScrollNode();
  EXPECT_EQ(DocScroll(), scroll->Parent());
  EXPECT_EQ(IntRect(0, 0, 413, 317), scroll->ContainerRect());
  EXPECT_EQ(IntSize(660, 10200), scroll->ContentsSize());
  EXPECT_FALSE(scroll->UserScrollableHorizontal());
  EXPECT_TRUE(scroll->UserScrollableVertical());
  EXPECT_EQ(FloatSize(120, 340), scroller_properties->PaintOffsetTranslation()
                                     ->Matrix()
                                     .To2DTranslation());
  EXPECT_EQ(FloatRoundedRect(0, 0, 413, 317),
            scroller_properties->OverflowClip()->ClipRect());
  EXPECT_EQ(DocContentClip(), scroller_properties->OverflowClip()->Parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(120, 340, 413, 317),
                          scroller->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // The relative-positioned element should have accumulated box offset (exclude
  // scrolling), and should be affected by ancestor scroll transforms.
  Element* rel_pos = GetDocument().getElementById("rel-pos");
  const ObjectPaintProperties* rel_pos_properties =
      rel_pos->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(560, 780),
            rel_pos_properties->PaintOffsetTranslation()->Matrix());
  EXPECT_EQ(scroller_properties->ScrollTranslation(),
            rel_pos_properties->PaintOffsetTranslation()->Parent());
  EXPECT_EQ(rel_pos_properties->Transform(),
            rel_pos_properties->OverflowClip()->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(0, 0, 100, 200),
            rel_pos_properties->OverflowClip()->ClipRect());
  EXPECT_EQ(scroller_properties->OverflowClip(),
            rel_pos_properties->OverflowClip()->Parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(), rel_pos->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // The absolute-positioned element should not be affected by non-positioned
  // scroller at all.
  Element* abs_pos = GetDocument().getElementById("abs-pos");
  const ObjectPaintProperties* abs_pos_properties =
      abs_pos->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(123, 456),
            abs_pos_properties->PaintOffsetTranslation()->Matrix());
  EXPECT_EQ(DocScrollTranslation(),
            abs_pos_properties->PaintOffsetTranslation()->Parent());
  EXPECT_EQ(abs_pos_properties->Transform(),
            abs_pos_properties->OverflowClip()->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(0, 0, 300, 400),
            abs_pos_properties->OverflowClip()->ClipRect());
  EXPECT_EQ(DocContentClip(), abs_pos_properties->OverflowClip()->Parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(123, 456, 300, 400),
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
            overflow_clip->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(10, 10, 100, 100), overflow_clip->ClipRect());

  PaintLayer* paint_layer =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("scroller"))->Layer();
  EXPECT_TRUE(paint_layer->GetScrollableArea()
                  ->VerticalScrollbar()
                  ->IsOverlayScrollbar());

  EXPECT_EQ(FloatRoundedRect(10, 10, 93, 93),
            overflow_clip->ClipRectExcludingOverlayScrollbars());
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
            overflow_clip->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(10, 10, 101, 100), overflow_clip->ClipRect());

  EXPECT_TRUE(ToLayoutBox(scroller)
                  ->GetScrollableArea()
                  ->VerticalScrollbar()
                  ->IsOverlayScrollbar());

  EXPECT_EQ(FloatRoundedRect(10, 10, 94, 93),
            overflow_clip->ClipRectExcludingOverlayScrollbars());
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

  const auto* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  const auto* content = GetLayoutObjectByElementId("content");
  const auto* properties = scroller->FirstFragment().PaintProperties();
  const auto* overflow_clip = properties->OverflowClip();
  const auto* scroll_translation = properties->ScrollTranslation();
  const auto* scroll = properties->Scroll();

  // -315: container_width (100) - contents_width (400) - scrollber_width
  EXPECT_EQ(TransformationMatrix().Translate(-315, 0),
            scroll_translation->Matrix());
  EXPECT_EQ(scroll, scroll_translation->ScrollNode());
  // 10: border width. 85: container client size (== 100 - scrollbar width).
  EXPECT_EQ(IntRect(10, 10, 85, 85), scroll->ContainerRect());
  EXPECT_EQ(IntSize(400, 400), scroll->ContentsSize());
  EXPECT_EQ(LayoutPoint(), scroller->FirstFragment().PaintOffset());
  EXPECT_EQ(IntPoint(315, 0), scroller->ScrollOrigin());
  EXPECT_EQ(LayoutPoint(10, 10), content->FirstFragment().PaintOffset());

  EXPECT_EQ(DocContentClip(), overflow_clip->Parent());
  EXPECT_EQ(properties->PaintOffsetTranslation(),
            overflow_clip->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(10, 10, 85, 85), overflow_clip->ClipRect());

  scroller->GetScrollableArea()->ScrollBy(ScrollOffset(-100, 0), kUserScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Only scroll_translation is affected by scrolling.
  EXPECT_EQ(TransformationMatrix().Translate(-215, 0),
            scroll_translation->Matrix());
  // Other properties are the same as before.
  EXPECT_EQ(scroll, scroll_translation->ScrollNode());
  EXPECT_EQ(IntRect(10, 10, 85, 85), scroll->ContainerRect());
  EXPECT_EQ(IntSize(400, 400), scroll->ContentsSize());
  EXPECT_EQ(LayoutPoint(), scroller->FirstFragment().PaintOffset());
  EXPECT_EQ(IntPoint(315, 0), scroller->ScrollOrigin());
  EXPECT_EQ(LayoutPoint(10, 10), content->FirstFragment().PaintOffset());

  EXPECT_EQ(DocContentClip(), overflow_clip->Parent());
  EXPECT_EQ(properties->PaintOffsetTranslation(),
            overflow_clip->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(10, 10, 85, 85), overflow_clip->ClipRect());
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

  const auto* scroller = ToLayoutBox(GetLayoutObjectByElementId("scroller"));
  const auto* content = GetLayoutObjectByElementId("content");
  const auto* properties = scroller->FirstFragment().PaintProperties();
  const auto* overflow_clip = properties->OverflowClip();
  const auto* scroll_translation = properties->ScrollTranslation();
  const auto* scroll = properties->Scroll();

  // -315: container_width (100) - contents_width (400) - scrollbar width (15).
  EXPECT_EQ(TransformationMatrix().Translate(-315, 0),
            scroll_translation->Matrix());
  EXPECT_EQ(scroll, scroll_translation->ScrollNode());
  // 25: border width (10) + scrollbar (on the left) width (15).
  // 85: container client size (== 100 - scrollbar width).
  EXPECT_EQ(IntRect(25, 10, 85, 85), scroll->ContainerRect());
  EXPECT_EQ(IntSize(400, 400), scroll->ContentsSize());
  EXPECT_EQ(LayoutPoint(), scroller->FirstFragment().PaintOffset());
  EXPECT_EQ(IntPoint(315, 0), scroller->ScrollOrigin());
  EXPECT_EQ(LayoutPoint(25, 10), content->FirstFragment().PaintOffset());

  EXPECT_EQ(DocContentClip(), overflow_clip->Parent());
  EXPECT_EQ(properties->PaintOffsetTranslation(),
            overflow_clip->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(25, 10, 85, 85), overflow_clip->ClipRect());

  scroller->GetScrollableArea()->ScrollBy(ScrollOffset(-100, 0), kUserScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  // Only scroll_translation is affected by scrolling.
  EXPECT_EQ(TransformationMatrix().Translate(-215, 0),
            scroll_translation->Matrix());
  // Other properties are the same as before.
  EXPECT_EQ(scroll, scroll_translation->ScrollNode());
  EXPECT_EQ(IntRect(25, 10, 85, 85), scroll->ContainerRect());
  EXPECT_EQ(IntSize(400, 400), scroll->ContentsSize());
  EXPECT_EQ(LayoutPoint(), scroller->FirstFragment().PaintOffset());
  EXPECT_EQ(IntPoint(315, 0), scroller->ScrollOrigin());
  EXPECT_EQ(LayoutPoint(25, 10), content->FirstFragment().PaintOffset());

  EXPECT_EQ(DocContentClip(), overflow_clip->Parent());
  EXPECT_EQ(properties->PaintOffsetTranslation(),
            overflow_clip->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(25, 10, 85, 85), overflow_clip->ClipRect());
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollVerticalRLMulticol) {
  SetBodyInnerHTML(R"HTML(
    <style>::-webkit-scrollbar {width: 15px; height: 15px}</style>
    <div id='scroller'
         style='width: 100px; height: 100px; overflow: scroll;
                writing-mode: vertical-rl; border: 10px solid blue'>
      <div id="multicol"
           style="width: 50px; height: 400px; columns: 2; column-gap: 0">
        <div style="width: 100px"></div>
      </div>
      <div style='width: 400px; height: 400px'></div>
    </div>
  )HTML");

  const auto* flow_thread =
      GetLayoutObjectByElementId("multicol")->SlowFirstChild();
  auto check_fragments = [flow_thread]() {
    ASSERT_EQ(2u, NumFragments(flow_thread));
    EXPECT_EQ(410, FragmentAt(flow_thread, 0)
                       .PaintProperties()
                       ->FragmentClip()
                       ->ClipRect()
                       .Rect()
                       .X());
    EXPECT_EQ(LayoutPoint(360, 10), FragmentAt(flow_thread, 0).PaintOffset());
    EXPECT_EQ(460, FragmentAt(flow_thread, 1)
                       .PaintProperties()
                       ->FragmentClip()
                       ->ClipRect()
                       .Rect()
                       .MaxX());
    EXPECT_EQ(LayoutPoint(410, 210), FragmentAt(flow_thread, 1).PaintOffset());
  };
  check_fragments();

  // Fragment geometries are not affected by parent scrolling.
  ToLayoutBox(GetLayoutObjectByElementId("scroller"))
      ->GetScrollableArea()
      ->ScrollBy(ScrollOffset(-100, 200), kUserScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();
  check_fragments();
}

TEST_P(PaintPropertyTreeBuilderTest, DocScrollingTraditional) {
  SetBodyInnerHTML("<style> body { height: 10000px; } </style>");

  GetDocument().domWindow()->scrollTo(0, 100);

  LocalFrameView* frame_view = GetDocument().View();
  frame_view->UpdateAllLifecyclePhases();
  EXPECT_EQ(TransformationMatrix(), DocPreTranslation()->Matrix());
  EXPECT_EQ(
      GetDocument().GetPage()->GetVisualViewport().GetScrollTranslationNode(),
      DocPreTranslation()->Parent());
  EXPECT_EQ(TransformationMatrix().Translate(0, -100),
            DocScrollTranslation()->Matrix());
  EXPECT_EQ(DocPreTranslation(), DocScrollTranslation()->Parent());
  EXPECT_EQ(DocPreTranslation(), DocContentClip()->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(0, 0, 800, 600), DocContentClip()->ClipRect());
  EXPECT_TRUE(DocContentClip()->Parent()->IsRoot());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 784, 10000),
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
  Element* perspective = GetDocument().getElementById("perspective");
  const ObjectPaintProperties* perspective_properties =
      perspective->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().ApplyPerspective(100),
            perspective_properties->Perspective()->Matrix());
  // The perspective origin is the center of the border box plus accumulated
  // paint offset.
  EXPECT_EQ(FloatPoint3D(250, 250, 0),
            perspective_properties->Perspective()->Origin());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(),
              perspective_properties->Perspective()->Parent());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              perspective_properties->Perspective()->Parent());
  }

  // Adding perspective doesn't clear paint offset. The paint offset will be
  // passed down to children.
  Element* inner = GetDocument().getElementById("inner");
  const ObjectPaintProperties* inner_properties =
      inner->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(50, 100),
            inner_properties->PaintOffsetTranslation()->Matrix());
  EXPECT_EQ(perspective_properties->Perspective(),
            inner_properties->PaintOffsetTranslation()->Parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(50, 100, 100, 200),
                          inner->GetLayoutObject(),
                          GetDocument().View()->GetLayoutView());

  perspective->setAttribute(HTMLNames::styleAttr, "perspective: 200px");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(TransformationMatrix().ApplyPerspective(200),
            perspective_properties->Perspective()->Matrix());
  EXPECT_EQ(FloatPoint3D(250, 250, 0),
            perspective_properties->Perspective()->Origin());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(),
              perspective_properties->Perspective()->Parent());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              perspective_properties->Perspective()->Parent());
  }

  perspective->setAttribute(HTMLNames::styleAttr, "perspective-origin: 5% 20%");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(TransformationMatrix().ApplyPerspective(100),
            perspective_properties->Perspective()->Matrix());
  EXPECT_EQ(FloatPoint3D(70, 160, 0),
            perspective_properties->Perspective()->Origin());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(),
              perspective_properties->Perspective()->Parent());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              perspective_properties->Perspective()->Parent());
  }
}

TEST_P(PaintPropertyTreeBuilderTest, Transform) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0 } </style>
    <div id='transform' style='margin-left: 50px; margin-top: 100px;
        width: 400px; height: 300px;
        transform: translate3d(123px, 456px, 789px)'>
    </div>
  )HTML");

  Element* transform = GetDocument().getElementById("transform");
  const ObjectPaintProperties* transform_properties =
      transform->GetLayoutObject()->FirstFragment().PaintProperties();

  EXPECT_EQ(TransformationMatrix().Translate3d(123, 456, 789),
            transform_properties->Transform()->Matrix());
  EXPECT_EQ(FloatPoint3D(200, 150, 0),
            transform_properties->Transform()->Origin());
  EXPECT_EQ(transform_properties->PaintOffsetTranslation(),
            transform_properties->Transform()->Parent());
  EXPECT_EQ(TransformationMatrix().Translate(50, 100),
            transform_properties->PaintOffsetTranslation()->Matrix());
  EXPECT_EQ(DocScrollTranslation(),
            transform_properties->PaintOffsetTranslation()->Parent());

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(
        transform_properties->Transform()->HasDirectCompositingReasons());
  }

  CHECK_EXACT_VISUAL_RECT(LayoutRect(173, 556, 400, 300),
                          transform->GetLayoutObject(),
                          GetDocument().View()->GetLayoutView());

  transform->setAttribute(
      HTMLNames::styleAttr,
      "margin-left: 50px; margin-top: 100px; width: 400px; height: 300px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(nullptr,
            transform->GetLayoutObject()->FirstFragment().PaintProperties());

  transform->setAttribute(
      HTMLNames::styleAttr,
      "margin-left: 50px; margin-top: 100px; width: 400px; height: 300px; "
      "transform: translate3d(123px, 456px, 789px)");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(TransformationMatrix().Translate3d(123, 456, 789),
            transform->GetLayoutObject()
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

  Element* preserve = GetDocument().getElementById("preserve");
  const ObjectPaintProperties* preserve_properties =
      preserve->GetLayoutObject()->FirstFragment().PaintProperties();

  EXPECT_TRUE(preserve_properties->Transform());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(
        preserve_properties->Transform()->HasDirectCompositingReasons());
  }
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

  Element* perspective = GetDocument().getElementById("perspective");
  const ObjectPaintProperties* perspective_properties =
      perspective->GetLayoutObject()->FirstFragment().PaintProperties();

  EXPECT_TRUE(perspective_properties->Transform());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(
        perspective_properties->Transform()->HasDirectCompositingReasons());
  }
}

TEST_P(PaintPropertyTreeBuilderTest,
       TransformNodeWithActiveAnimationHasDirectCompositingReason) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  LoadTestData("transform-animation.html");
  EXPECT_TRUE(PaintPropertiesForElement("target")
                  ->Transform()
                  ->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest,
       OpacityAnimationDoesNotCreateTransformNode) {
  LoadTestData("opacity-animation.html");
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target")->Transform());
}

TEST_P(PaintPropertyTreeBuilderTest,
       EffectNodeWithActiveAnimationHasDirectCompositingReason) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

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

  Element* transform = GetDocument().getElementById("transform");
  const ObjectPaintProperties* transform_properties =
      transform->GetLayoutObject()->FirstFragment().PaintProperties();

  EXPECT_EQ(TransformationMatrix(),
            transform_properties->Transform()->Matrix());
  EXPECT_EQ(TransformationMatrix().Translate(0, 0),
            transform_properties->Transform()->Matrix());
  // The value is zero without a transform property that needs transform-offset.
  EXPECT_EQ(FloatPoint3D(0, 0, 0), transform_properties->Transform()->Origin());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(nullptr, transform_properties->PaintOffsetTranslation());
  } else {
    // SPv1 creates PaintOffsetTranslation for composited layers.
    EXPECT_EQ(TransformationMatrix().Translate(50, 100),
              transform_properties->PaintOffsetTranslation()->Matrix());
  }
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(
        transform_properties->Transform()->HasDirectCompositingReasons());
  }

  CHECK_EXACT_VISUAL_RECT(LayoutRect(50, 100, 400, 300),
                          transform->GetLayoutObject(),
                          GetDocument().View()->GetLayoutView());

  transform->setAttribute(
      HTMLNames::styleAttr,
      "margin-left: 50px; margin-top: 100px; width: 400px; height: 300px;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(nullptr,
            transform->GetLayoutObject()->FirstFragment().PaintProperties());

  transform->setAttribute(
      HTMLNames::styleAttr,
      "margin-left: 50px; margin-top: 100px; width: 400px; height: 300px; "
      "will-change: transform");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(TransformationMatrix(), transform->GetLayoutObject()
                                        ->FirstFragment()
                                        .PaintProperties()
                                        ->Transform()
                                        ->Matrix());
}

TEST_P(PaintPropertyTreeBuilderTest, WillChangeContents) {
  SetBodyInnerHTML(R"HTML(
    <style> body { margin: 0 } </style>
    <div id='transform' style='margin-left: 50px; margin-top: 100px;
        width: 400px; height: 300px;
        will-change: transform, contents'>
    </div>
  )HTML");

  Element* transform = GetDocument().getElementById("transform");
  EXPECT_EQ(nullptr,
            transform->GetLayoutObject()->FirstFragment().PaintProperties());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(50, 100, 400, 300),
                          transform->GetLayoutObject(),
                          GetDocument().View()->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, RelativePositionInline) {
  LoadTestData("relative-position-inline.html");

  Element* inline_block = GetDocument().getElementById("inline-block");
  const ObjectPaintProperties* inline_block_properties =
      inline_block->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(135, 490),
            inline_block_properties->PaintOffsetTranslation()->Matrix());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(),
              inline_block_properties->PaintOffsetTranslation()->Parent());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              inline_block_properties->PaintOffsetTranslation()->Parent());
  }
  CHECK_EXACT_VISUAL_RECT(LayoutRect(135, 490, 10, 20),
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
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 100, 200), node_without_opacity,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* child_with_opacity =
      GetLayoutObjectByElementId("childWithOpacity");
  const ObjectPaintProperties* child_with_opacity_properties =
      child_with_opacity->FirstFragment().PaintProperties();
  EXPECT_EQ(0.5f, child_with_opacity_properties->Effect()->Opacity());
  // childWithOpacity is the root effect node.
  EXPECT_NE(nullptr, child_with_opacity_properties->Effect()->Parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 50, 60), child_with_opacity,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* grand_child_without_opacity =
      GetDocument()
          .getElementById("grandChildWithoutOpacity")
          ->GetLayoutObject();
  EXPECT_EQ(nullptr,
            grand_child_without_opacity->FirstFragment().PaintProperties());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 20, 30), grand_child_without_opacity,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* great_grand_child_with_opacity =
      GetDocument()
          .getElementById("greatGrandChildWithOpacity")
          ->GetLayoutObject();
  const ObjectPaintProperties* great_grand_child_with_opacity_properties =
      great_grand_child_with_opacity->FirstFragment().PaintProperties();
  EXPECT_EQ(0.2f,
            great_grand_child_with_opacity_properties->Effect()->Opacity());
  EXPECT_EQ(child_with_opacity_properties->Effect(),
            great_grand_child_with_opacity_properties->Effect()->Parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 15),
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
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 100, 200), node_with_opacity,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* child_with_transform =
      GetLayoutObjectByElementId("childWithTransform");
  const ObjectPaintProperties* child_with_transform_properties =
      child_with_transform->FirstFragment().PaintProperties();
  EXPECT_EQ(nullptr, child_with_transform_properties->Effect());
  EXPECT_EQ(TransformationMatrix().Translate(10, 10),
            child_with_transform_properties->Transform()->Matrix());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(18, 18, 50, 60), child_with_transform,
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
  CHECK_EXACT_VISUAL_RECT(LayoutRect(18, 18, 20, 30), grand_child_with_opacity,
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
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 100, 200), node_with_opacity,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* child_with_stacking_context =
      GetDocument()
          .getElementById("childWithStackingContext")
          ->GetLayoutObject();
  const ObjectPaintProperties* child_with_stacking_context_properties =
      child_with_stacking_context->FirstFragment().PaintProperties();
  EXPECT_EQ(nullptr, child_with_stacking_context_properties);
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 50, 60), child_with_stacking_context,
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
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 20, 30), grand_child_with_opacity,
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
           .getElementById("svgRootWith3dTransform")
           ->GetLayoutObject();
  const ObjectPaintProperties* svg_root_with3d_transform_properties =
      svg_root_with3d_transform.FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate3d(1, 2, 3),
            svg_root_with3d_transform_properties->Transform()->Matrix());
  EXPECT_EQ(FloatPoint3D(50, 50, 0),
            svg_root_with3d_transform_properties->Transform()->Origin());
  EXPECT_EQ(svg_root_with3d_transform_properties->PaintOffsetTranslation(),
            svg_root_with3d_transform_properties->Transform()->Parent());
  EXPECT_EQ(
      TransformationMatrix().Translate(70, 25),
      svg_root_with3d_transform_properties->PaintOffsetTranslation()->Matrix());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(),
              svg_root_with3d_transform_properties->PaintOffsetTranslation()
                  ->Parent());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              svg_root_with3d_transform_properties->PaintOffsetTranslation()
                  ->Parent());
  }
  LayoutObject& rect_with2d_transform =
      *GetLayoutObjectByElementId("rectWith2dTransform");
  const ObjectPaintProperties* rect_with2d_transform_properties =
      rect_with2d_transform.FirstFragment().PaintProperties();
  TransformationMatrix matrix;
  matrix.Translate(100, 100);
  matrix.Rotate(45);
  // SVG's transform origin is baked into the transform.
  matrix.ApplyTransformOrigin(50, 25, 0);
  EXPECT_EQ(matrix, rect_with2d_transform_properties->Transform()->Matrix());
  EXPECT_EQ(FloatPoint3D(0, 0, 0),
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
  EXPECT_EQ(TransformationMatrix().Translate3d(1, 2, 3),
            svg_with_view_box_properties->Transform()->Matrix());
  EXPECT_EQ(TransformationMatrix().Translate(-50, -50),
            svg_with_view_box_properties->ReplacedContentTransform()->Matrix());
  EXPECT_EQ(svg_with_view_box_properties->ReplacedContentTransform()->Parent(),
            svg_with_view_box_properties->Transform());

  LayoutObject& rect = *GetLayoutObjectByElementId("rect");
  const ObjectPaintProperties* rect_properties =
      rect.FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(100, 100),
            rect_properties->Transform()->Matrix());
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
  EXPECT_EQ(
      FloatSize(50, 25),
      svg_properties->PaintOffsetTranslation()->Matrix().To2DTranslation());
  EXPECT_EQ(nullptr, svg_properties->ReplacedContentTransform());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(),
              svg_properties->PaintOffsetTranslation()->Parent());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              svg_properties->PaintOffsetTranslation()->Parent());
  }
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
  EXPECT_EQ(TransformationMatrix().Translate(2, 3),
            svg_properties->PaintOffsetTranslation()->Matrix());
  EXPECT_EQ(TransformationMatrix().Translate(5, 7),
            svg_properties->Transform()->Matrix());
  EXPECT_EQ(TransformationMatrix().Translate(11, 11).Scale(100.0 / 13.0),
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
  EXPECT_EQ(TransformationMatrix().Translate(17, 19),
            rect_properties->Transform()->Matrix());
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
  EXPECT_EQ(TransformationMatrix().Translate(11, 11),
            svg_properties->Transform()->Matrix());
  EXPECT_EQ(TransformationMatrix().Scale(2),
            svg_properties->ReplacedContentTransform()->Matrix());

  LayoutObject& nested_svg = *GetLayoutObjectByElementId("nestedSvg");
  const ObjectPaintProperties* nested_svg_properties =
      nested_svg.FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Scale(10),
            nested_svg_properties->Transform()->Matrix());
  EXPECT_EQ(nullptr, nested_svg_properties->ReplacedContentTransform());
  EXPECT_EQ(svg_properties->ReplacedContentTransform(),
            nested_svg_properties->Transform()->Parent());

  LayoutObject& rect = *GetLayoutObjectByElementId("rect");
  const ObjectPaintProperties* rect_properties =
      rect.FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(13, 13),
            rect_properties->Transform()->Matrix());
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
  EXPECT_EQ(TransformationMatrix().Translate3d(1, 2, 3),
            svg_with_transform_properties->Transform()->Matrix());

  LayoutObject& div_with_transform =
      *GetLayoutObjectByElementId("divWithTransform");
  const ObjectPaintProperties* div_with_transform_properties =
      div_with_transform.FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate3d(3, 4, 5),
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
  EXPECT_EQ(TransformationMatrix().Scale(5),
            foreign_object_properties->Transform()->Matrix());
  EXPECT_EQ(LayoutPoint(10, 10), foreign_object.FirstFragment().PaintOffset());
  EXPECT_EQ(nullptr, foreign_object_properties->PaintOffsetTranslation());

  LayoutObject& div = *GetLayoutObjectByElementId("div");
  EXPECT_EQ(LayoutPoint(10, 10), div.FirstFragment().PaintOffset());
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
  EXPECT_EQ(foreign_object_properties->MaskClip(),
            foreign_object_properties->Mask()->OutputClip());
  EXPECT_EQ(svg.FirstFragment().LocalBorderBoxProperties().Transform(),
            foreign_object_properties->Mask()->LocalTransformSpace());
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
  EXPECT_EQ(
      FloatSize(8, 8),
      svg_properties->PaintOffsetTranslation()->Matrix().To2DTranslation());

  LayoutObject& div_with_transform =
      *GetLayoutObjectByElementId("divWithTransform");
  const ObjectPaintProperties* div_with_transform_properties =
      div_with_transform.FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate3d(3, 4, 5),
            div_with_transform_properties->Transform()->Matrix());
  EXPECT_EQ(FloatSize(8, 158),
            div_with_transform_properties->PaintOffsetTranslation()
                ->Matrix()
                .To2DTranslation());
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
  EXPECT_EQ(FloatRect(0, 0, 30, 30), clip->ClipRect().Rect());
  EXPECT_EQ(parent_transform, clip->LocalTransformSpace());

  // overflow: hidden and non-zero offset and viewport scale:
  // both Transform and OverflowClip.
  const auto* properties2 = PaintPropertiesForElement("container2");
  ASSERT_NE(nullptr, properties2);
  clip = properties2->OverflowClip();
  transform = properties2->Transform();
  ASSERT_NE(nullptr, clip);
  ASSERT_NE(nullptr, transform);
  EXPECT_EQ(parent_clip, clip->Parent());
  EXPECT_EQ(FloatRect(0, 0, 60, 60), clip->ClipRect().Rect());
  EXPECT_EQ(transform, clip->LocalTransformSpace());
  EXPECT_EQ(TransformationMatrix().Translate(40, 50).Scale(0.5),
            transform->Matrix());
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
  EXPECT_EQ(TransformationMatrix().Translate(20, 30), transform->Matrix());
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
  EXPECT_EQ(FloatRect(10, 20, 30, 40), clip->ClipRect().Rect());
  EXPECT_EQ(parent_transform, clip->LocalTransformSpace());

  const auto* properties2 = PaintPropertiesForElement("object2");
  EXPECT_EQ(nullptr, properties2);
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
  EXPECT_EQ(
      FloatSize(8, 8),
      svg_properties->PaintOffsetTranslation()->Matrix().To2DTranslation());
  LayoutObject& div_with_columns =
      *GetLayoutObjectByElementId("divWithColumns")->SlowFirstChild();
  EXPECT_EQ(LayoutPoint(0, 0), div_with_columns.FirstFragment().PaintOffset());
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
  EXPECT_EQ(TransformationMatrix().Translate3d(1, 2, 3),
            svg_properties->Transform()->Matrix());

  LayoutObject& container = *GetLayoutObjectByElementId("container");
  const ObjectPaintProperties* container_properties =
      container.FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(20, 30),
            container_properties->Transform()->Matrix());
  EXPECT_EQ(svg_properties->Transform(),
            container_properties->Transform()->Parent());

  Element* fixed = GetDocument().getElementById("fixed");
  // Ensure the fixed position element is rooted at the nearest transform
  // container.
  EXPECT_EQ(container_properties->Transform(), fixed->GetLayoutObject()
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
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(DocPreTranslation());
    EXPECT_FALSE(DocScrollTranslation());
    EXPECT_EQ(DocPreTranslation(),
              button_properties->OverflowClip()->LocalTransformSpace());
  } else {
    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
    EXPECT_EQ(DocScrollTranslation(),
              button_properties->OverflowClip()->LocalTransformSpace());
  }

  EXPECT_EQ(FloatRoundedRect(5, 5, 335, 113),
            button_properties->OverflowClip()->ClipRect());
  EXPECT_EQ(DocContentClip(), button_properties->OverflowClip()->Parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 345, 123), &button,
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
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_FALSE(DocScrollTranslation());
  } else {
    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
  }

  EXPECT_EQ(FloatRoundedRect(2, 2, 341, 119),
            button_properties->OverflowClip()->ClipRect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 345, 123), &button,
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

  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_FALSE(DocScrollTranslation());
    EXPECT_EQ(DocPreTranslation(),
              div_properties->OverflowClip()->LocalTransformSpace());
  } else {
    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
    EXPECT_EQ(DocScrollTranslation(),
              div_properties->OverflowClip()->LocalTransformSpace());
  }
  // The overflow clip rect includes only the padding box.
  // padding box = border box(500+60+50, 400+45+55) - border outset(60+50,
  // 45+55) - scrollbars(15, 15)
  EXPECT_EQ(FloatRoundedRect(60, 45, 500, 400),
            div_properties->OverflowClip()->ClipRect());
  const ClipPaintPropertyNode* border_radius_clip =
      div_properties->OverflowClip()->Parent();
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(), border_radius_clip->LocalTransformSpace());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              border_radius_clip->LocalTransformSpace());
  }
  // The border radius clip is the area enclosed by inner border edge, including
  // the scrollbars.  As the border-radius is specified in outer radius, the
  // inner radius is calculated by:
  //     inner radius = max(outer radius - border width, 0)
  // In the case that two adjacent borders have different width, the inner
  // radius of the corner may transition from one value to the other. i.e. being
  // an ellipse.
  // The following is border box(610, 500) - border outset(110, 100).
  FloatRect border_box_minus_border_outset(60, 45, 500, 400);
  EXPECT_EQ(
      FloatRoundedRect(
          border_box_minus_border_outset,
          FloatSize(0, 0),    // (top left) = max((12, 12) - (60, 45), (0, 0))
          FloatSize(0, 0),    // (top right) = max((34, 34) - (50, 45), (0, 0))
          FloatSize(18, 23),  // (bot left) = max((78, 78) - (60, 55), (0, 0))
          FloatSize(6, 1)),   // (bot right) = max((56, 56) - (50, 55), (0, 0))
      border_radius_clip->ClipRect());
  EXPECT_EQ(DocContentClip(), border_radius_clip->Parent());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 610, 500), &div,
                          GetDocument().View()->GetLayoutView());
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
  frame_view->UpdateAllLifecyclePhases();

  LayoutObject* div_with_transform =
      GetLayoutObjectByElementId("divWithTransform");
  const ObjectPaintProperties* div_with_transform_properties =
      div_with_transform->FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate3d(1, 2, 3),
            div_with_transform_properties->Transform()->Matrix());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(1, 2, 800, 164), div_with_transform,
                          frame_view->GetLayoutView());

  LayoutObject* inner_div_with_transform =
      ChildDocument()
          .getElementById("innerDivWithTransform")
          ->GetLayoutObject();
  const ObjectPaintProperties* inner_div_with_transform_properties =
      inner_div_with_transform->FirstFragment().PaintProperties();
  auto* inner_div_transform = inner_div_with_transform_properties->Transform();
  EXPECT_EQ(TransformationMatrix().Translate3d(4, 5, 6),
            inner_div_transform->Matrix());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(12, 14, 100, 145),
                          inner_div_with_transform,
                          frame_view->GetLayoutView());

  // Ensure that the inner div's transform is correctly rooted in the root
  // frame's transform tree.
  // This asserts that we have the following tree structure:
  // Transform transform=translation=1.000000,2.000000,3.000000
  //   PaintOffsetTranslation transform=Identity
  //     PreTranslation transform=translation=7.000000,7.000000,0.000000
  //       PaintOffsetTranslation transform=Identity
  //         ScrollTranslation transform=translation=0.000000,0.000000,0.000000
  //           Transform transform=translation=4.000000,5.000000,6.000000
  auto* inner_document_scroll_translation = inner_div_transform->Parent();
  EXPECT_EQ(TransformationMatrix().Translate3d(0, 0, 0),
            inner_document_scroll_translation->Matrix());
  auto* paint_offset_translation = inner_document_scroll_translation->Parent();
  auto* iframe_pre_translation =
      inner_document_scroll_translation->Parent()->Unalias()->Parent();
  EXPECT_EQ(FloatSize(), paint_offset_translation->Matrix().To2DTranslation());
  EXPECT_EQ(TransformationMatrix().Translate3d(7, 7, 0),
            iframe_pre_translation->Matrix());
  // SPv1 composited elements always create paint offset translation,
  // where in SPv2 they don't.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(div_with_transform_properties->Transform(),
              iframe_pre_translation->Parent());
  } else {
    LayoutObject* iframe_element = GetLayoutObjectByElementId("iframe");
    const ObjectPaintProperties* iframe_element_properties =
        iframe_element->FirstFragment().PaintProperties();
    EXPECT_EQ(iframe_element_properties->PaintOffsetTranslation(),
              iframe_pre_translation->Parent());
    EXPECT_EQ(TransformationMatrix(),
              iframe_element_properties->PaintOffsetTranslation()->Matrix());
    EXPECT_EQ(div_with_transform_properties->Transform(),
              iframe_element_properties->PaintOffsetTranslation()->Parent());
  }
}

TEST_P(PaintPropertyTreeBuilderTest, FramesEstablishIsolation) {
  if (!RuntimeEnabledFeatures::LayoutViewIsolationNodesEnabled())
    return;

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
  frame_view->UpdateAllLifecyclePhases();

  LayoutObject* frame = ChildFrame().View()->GetLayoutView();
  const auto& frame_contents_properties =
      frame->FirstFragment().ContentsProperties();

  LayoutObject* child =
      ChildDocument().getElementById("child")->GetLayoutObject();
  const auto& child_local_border_box_properties =
      child->FirstFragment().LocalBorderBoxProperties();
  auto* child_properties =
      child->GetMutableForPainting().FirstFragment().PaintProperties();

  // From the frame content's properties, we have:
  //  - transform isolation node
  //    - paint offset translation
  //      - transform
  EXPECT_EQ(TransformationMatrix().Translate(50, 0),
            child_local_border_box_properties.Transform()->Matrix());
  EXPECT_EQ(child_local_border_box_properties.Transform()->Parent(),
            child_properties->PaintOffsetTranslation());
  EXPECT_EQ(child_local_border_box_properties.Transform()->Parent()->Parent(),
            frame_contents_properties.Transform());
  // Verify it's a true isolation node (i.e. it has a parent and it is a parent
  // alias).
  EXPECT_TRUE(frame_contents_properties.Transform()->Parent());
  EXPECT_TRUE(frame_contents_properties.Transform()->IsParentAlias());

  // Do similar checks for clip and effect, although the child local border box
  // properties directly reference the alias, since they do not have their own
  // clip and effect.
  EXPECT_EQ(child_local_border_box_properties.Clip(),
            frame_contents_properties.Clip());
  EXPECT_TRUE(frame_contents_properties.Clip()->Parent());
  EXPECT_TRUE(frame_contents_properties.Clip()->IsParentAlias());

  EXPECT_EQ(child_local_border_box_properties.Effect(),
            frame_contents_properties.Effect());
  EXPECT_TRUE(frame_contents_properties.Effect()->Parent());
  EXPECT_TRUE(frame_contents_properties.Effect()->IsParentAlias());

// The following part of the code would cause a DCHECK, but we want to see if
// the pre-paint iteration doesn't touch child's state, due to isolation. Hence,
// this only runs if we don't have DCHECKs enabled.
#if !DCHECK_IS_ON()
  // Now clobber the child transform to something identifiable.
  TransformPaintPropertyNode::State state{
      TransformationMatrix().Translate(123, 321)};
  child_properties->UpdateTransform(
      *child_local_border_box_properties.Transform()->Parent(),
      std::move(state));
  // Verify that we clobbered it correctly.
  EXPECT_EQ(TransformationMatrix().Translate(123, 321),
            child_local_border_box_properties.Transform()->Matrix());

  // This causes a tree topology change which forces the subtree to be updated.
  // However, isolation stops this recursion.
  GetDocument().getElementById("parent")->setAttribute(HTMLNames::classAttr,
                                                       "transformed");
  frame_view->UpdateAllLifecyclePhases();

  // Verify that our clobbered state is still clobbered.
  EXPECT_EQ(TransformationMatrix().Translate(123, 321),
            child_local_border_box_properties.Transform()->Matrix());
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
  frame_view->UpdateAllLifecyclePhases();

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
      ChildDocument().getElementById("transform")->GetLayoutObject();
  auto* inner_div_transform =
      inner_div_with_transform->FirstFragment().PaintProperties()->Transform();
  EXPECT_EQ(TransformationMatrix().Translate3d(7, 8, 9),
            inner_div_transform->Matrix());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(92, 95, 100, 111),
                          inner_div_with_transform,
                          frame_view->GetLayoutView());

  auto* inner_document_paint_offset_translation = inner_div_transform->Parent();
  EXPECT_EQ(TransformationMatrix().Translate3d(31, 31, 0),
            inner_document_paint_offset_translation->Matrix());
  auto* inner_document_scroll_translation =
      inner_document_paint_offset_translation->Parent()->Unalias();
  EXPECT_EQ(TransformationMatrix().Translate3d(0, 0, 0),
            inner_document_scroll_translation->Matrix());
  auto* iframe_pre_translation = inner_document_scroll_translation->Parent();
  EXPECT_EQ(TransformationMatrix().Translate3d(42, 42, 0),
            iframe_pre_translation->Matrix());
  auto* iframe_transform = iframe_pre_translation->Parent();
  EXPECT_EQ(TransformationMatrix().Translate3d(4, 5, 6),
            iframe_transform->Matrix());
  auto* iframe_paint_offset_translation = iframe_transform->Parent();
  EXPECT_EQ(TransformationMatrix().Translate3d(7, 7, 0),
            iframe_paint_offset_translation->Matrix());
  auto* div_with_transform_transform =
      iframe_paint_offset_translation->Parent();
  EXPECT_EQ(TransformationMatrix().Translate3d(1, 2, 3),
            div_with_transform_transform->Matrix());

  LayoutObject* div_with_transform =
      GetLayoutObjectByElementId("divWithTransform");
  EXPECT_EQ(div_with_transform_transform,
            div_with_transform->FirstFragment().PaintProperties()->Transform());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(1, 2, 800, 248), div_with_transform,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, TreeContextClipByNonStackingContext) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that is scrolled by a
  // containing block that is not one of the painting ancestors.
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
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
            child->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(scroller_properties->ScrollTranslation(),
            child->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_NE(nullptr,
            child->FirstFragment().LocalBorderBoxProperties().Effect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 400, 300), scroller,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 100, 200), child,
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
            child.FirstFragment().LocalBorderBoxProperties().Clip());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocScrollTranslation(),
              child.FirstFragment().LocalBorderBoxProperties().Transform());
  } else {
    // For SPv1, |child| is composited so we created PaintOffsetTranslation.
    EXPECT_EQ(child.FirstFragment().PaintProperties()->PaintOffsetTranslation(),
              child.FirstFragment().LocalBorderBoxProperties().Transform());
  }
  EXPECT_EQ(scroller_properties->Effect(),
            child.FirstFragment().LocalBorderBoxProperties().Effect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 800, 10000), &scroller,
                          GetDocument().View()->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 100, 200), &child,
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
  EXPECT_EQ(LayoutPoint(170, 170), target.FirstFragment().PaintOffset());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(),
              target.FirstFragment().LocalBorderBoxProperties().Transform());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              target.FirstFragment().LocalBorderBoxProperties().Transform());
  }
  CHECK_EXACT_VISUAL_RECT(LayoutRect(170, 170, 100, 100), &target,
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
  LayoutRect local_clip_rect(40, 10, 40, 60);
  LayoutRect absolute_clip_rect = local_clip_rect;
  absolute_clip_rect.Move(123, 456);

  LayoutObject& clip = *GetLayoutObjectByElementId("clip");
  const ObjectPaintProperties* clip_properties =
      clip.FirstFragment().PaintProperties();
  EXPECT_EQ(DocContentClip(), clip_properties->CssClip()->Parent());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(),
              clip_properties->CssClip()->LocalTransformSpace());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              clip_properties->CssClip()->LocalTransformSpace());
  }
  EXPECT_EQ(FloatRoundedRect(FloatRect(absolute_clip_rect)),
            clip_properties->CssClip()->ClipRect());
  CHECK_VISUAL_RECT(absolute_clip_rect, &clip,
                    GetDocument().View()->GetLayoutView(),
                    // TODO(crbug.com/599939): mapToVisualRectInAncestorSpace()
                    // doesn't apply css clip on the object itself.
                    LayoutUnit::Max());

  LayoutObject* fixed = GetLayoutObjectByElementId("fixed");
  EXPECT_EQ(clip_properties->CssClip(),
            fixed->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(DocPreTranslation(),
            fixed->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(LayoutPoint(654, 321), fixed->FirstFragment().PaintOffset());
  CHECK_VISUAL_RECT(LayoutRect(), fixed, GetDocument().View()->GetLayoutView(),
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

  LayoutRect local_clip_rect(40, 10, 40, 60);
  LayoutRect absolute_clip_rect = local_clip_rect;
  absolute_clip_rect.Move(123, 456);

  auto* clip = GetLayoutObjectByElementId("clip");
  const ObjectPaintProperties* clip_properties =
      clip->FirstFragment().PaintProperties();
  EXPECT_EQ(DocContentClip(), clip_properties->CssClip()->Parent());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_FALSE(DocScrollTranslation());
    EXPECT_EQ(DocPreTranslation(),
              clip_properties->CssClip()->LocalTransformSpace());
  } else {
    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
    EXPECT_EQ(DocScrollTranslation(),
              clip_properties->CssClip()->LocalTransformSpace());
  }
  EXPECT_EQ(FloatRoundedRect(FloatRect(absolute_clip_rect)),
            clip_properties->CssClip()->ClipRect());
  CHECK_VISUAL_RECT(absolute_clip_rect, clip,
                    GetDocument().View()->GetLayoutView(),
                    // TODO(crbug.com/599939): mapToVisualRectInAncestorSpace()
                    // doesn't apply css clip on the object itself.
                    LayoutUnit::Max());

  auto* absolute = GetLayoutObjectByElementId("absolute");
  EXPECT_EQ(clip_properties->CssClip(),
            absolute->FirstFragment().LocalBorderBoxProperties().Clip());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_FALSE(DocScrollTranslation());
    EXPECT_EQ(DocPreTranslation(),
              absolute->FirstFragment().LocalBorderBoxProperties().Transform());
  } else {
    EXPECT_TRUE(DocScrollTranslation());
    EXPECT_EQ(DocScrollTranslation(),
              absolute->FirstFragment().LocalBorderBoxProperties().Transform());
  }
  EXPECT_EQ(LayoutPoint(777, 777), absolute->FirstFragment().PaintOffset());
  CHECK_VISUAL_RECT(LayoutRect(), absolute,
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

  LayoutRect local_clip_rect(40, 10, 40, 60);
  LayoutRect absolute_clip_rect = local_clip_rect;
  // Moved by 124 pixels due to pixel-snapping.
  absolute_clip_rect.Move(124, 456);

  auto* clip = GetLayoutObjectByElementId("clip");
  const ObjectPaintProperties* clip_properties =
      clip->FirstFragment().PaintProperties();
  EXPECT_EQ(DocContentClip(), clip_properties->CssClip()->Parent());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(DocPreTranslation());
    EXPECT_EQ(DocPreTranslation(),
              clip_properties->CssClip()->LocalTransformSpace());
  } else {
    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
    EXPECT_EQ(DocScrollTranslation(),
              clip_properties->CssClip()->LocalTransformSpace());
  }
  EXPECT_EQ(FloatRoundedRect(FloatRect(absolute_clip_rect)),
            clip_properties->CssClip()->ClipRect());
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
  LayoutRect local_clip_rect(40, 10, 40, 60);
  LayoutRect absolute_clip_rect = local_clip_rect;
  absolute_clip_rect.Move(123, 456);

  LayoutObject& overflow = *GetLayoutObjectByElementId("overflow");
  const ObjectPaintProperties* overflow_properties =
      overflow.FirstFragment().PaintProperties();
  EXPECT_EQ(DocContentClip(), overflow_properties->OverflowClip()->Parent());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(DocPreTranslation());
    EXPECT_EQ(DocPreTranslation(),
              overflow_properties->ScrollTranslation()->Parent()->Parent());
  } else {
    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
    EXPECT_EQ(DocScrollTranslation(),
              overflow_properties->ScrollTranslation()->Parent()->Parent());
  }
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 50, 50), &overflow,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* clip = GetLayoutObjectByElementId("clip");
  const ObjectPaintProperties* clip_properties =
      clip->FirstFragment().PaintProperties();
  EXPECT_EQ(overflow_properties->OverflowClip(),
            clip_properties->CssClip()->Parent());
  EXPECT_EQ(overflow_properties->ScrollTranslation(),
            clip_properties->CssClip()->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(FloatRect(absolute_clip_rect)),
            clip_properties->CssClip()->ClipRect());
  EXPECT_EQ(DocContentClip(),
            clip_properties->CssClipFixedPosition()->Parent());
  EXPECT_EQ(overflow_properties->ScrollTranslation(),
            clip_properties->CssClipFixedPosition()->LocalTransformSpace());
  EXPECT_EQ(FloatRoundedRect(FloatRect(absolute_clip_rect)),
            clip_properties->CssClipFixedPosition()->ClipRect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(), clip,
                          GetDocument().View()->GetLayoutView());

  LayoutObject* fixed = GetLayoutObjectByElementId("fixed");
  EXPECT_EQ(clip_properties->CssClipFixedPosition(),
            fixed->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(DocPreTranslation(),
            fixed->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(LayoutPoint(654, 321), fixed->FirstFragment().PaintOffset());
  CHECK_VISUAL_RECT(LayoutRect(), fixed, GetDocument().View()->GetLayoutView(),
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
  EXPECT_EQ(LayoutPoint(55, 44), spanner->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(55, 44, 100, 100), spanner,
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
  LayoutPoint a_paint_offset = LayoutPoint(FloatPoint(0.1, 0.3));
  EXPECT_EQ(a_paint_offset, a->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(0.1), LayoutUnit(0.3),
                                     LayoutUnit(70), LayoutUnit(70)),
                          a, frame_view->GetLayoutView());

  LayoutObject* b = GetLayoutObjectByElementId("b");
  LayoutPoint b_paint_offset =
      a_paint_offset + LayoutPoint(FloatPoint(0.5, 11.1));
  EXPECT_EQ(b_paint_offset, b->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(0.1), LayoutUnit(0.3),
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
  EXPECT_EQ(TransformationMatrix().Translate3d(0, 0, 0),
            b_properties->Transform()->Matrix());
  // The paint offset transform should be snapped from (0.3,0.3) to (0,0).
  EXPECT_EQ(TransformationMatrix().Translate(0, 0),
            b_properties->Transform()->Parent()->Matrix());
  // The residual subpixel adjustment should be (0.3,0.3) - (0,0) = (0.3,0.3).
  LayoutPoint subpixel_accumulation = LayoutPoint(FloatPoint(0.3, 0.3));
  EXPECT_EQ(subpixel_accumulation, b->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(FloatRect(0.3, 0.3, 40, 40)), b,
                          frame_view->GetLayoutView());

  // c's painted should start at subpixelAccumulation + (0.1,0.1) = (0.4,0.4).
  LayoutObject* c = GetLayoutObjectByElementId("c");
  LayoutPoint c_paint_offset =
      subpixel_accumulation + LayoutPoint(FloatPoint(0.1, 0.1));
  EXPECT_EQ(c_paint_offset, c->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(LayoutRect(FloatRect(0.4, 0.4, 40, 40)), c,
                    frame_view->GetLayoutView(), 1);
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
  EXPECT_EQ(TransformationMatrix().Translate3d(0, 0, 0),
            b_properties->Transform()->Matrix());
  // The paint offset transform should be snapped from (0.7,0.7) to (1,1).
  EXPECT_EQ(TransformationMatrix().Translate(1, 1),
            b_properties->Transform()->Parent()->Matrix());
  // The residual subpixel adjustment should be (0.7,0.7) - (1,1) = (-0.3,-0.3).
  LayoutPoint subpixel_accumulation =
      LayoutPoint(LayoutPoint(FloatPoint(0.7, 0.7)) - LayoutPoint(1, 1));
  EXPECT_EQ(subpixel_accumulation, b->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(0.7), LayoutUnit(0.7),
                                     LayoutUnit(40), LayoutUnit(40)),
                          b, frame_view->GetLayoutView());

  // c's painting should start at subpixelAccumulation + (0.7,0.7) = (0.4,0.4).
  LayoutObject* c = GetLayoutObjectByElementId("c");
  LayoutPoint c_paint_offset =
      subpixel_accumulation + LayoutPoint(FloatPoint(0.7, 0.7));
  EXPECT_EQ(c_paint_offset, c->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(LayoutRect(LayoutUnit(0.7) + LayoutUnit(0.7),
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
  EXPECT_EQ(TransformationMatrix().Scale(10),
            b_properties->Transform()->Matrix());
  // The paint offset transform should not be snapped.
  EXPECT_EQ(TransformationMatrix().Translate(1, 1),
            b_properties->Transform()->Parent()->Matrix());
  EXPECT_EQ(LayoutPoint(), b->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(LayoutRect(LayoutUnit(1), LayoutUnit(1), LayoutUnit(400),
                               LayoutUnit(400)),
                    b, frame_view->GetLayoutView(), 1);

  // c's painting should start at c_offset.
  LayoutObject* c = GetLayoutObjectByElementId("c");
  LayoutUnit c_offset = LayoutUnit(0.6);
  EXPECT_EQ(LayoutPoint(c_offset, c_offset), c->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px
  // in the transformed space (c_offset * 10 in view space) and 1px in the
  // view space.
  CHECK_VISUAL_RECT(LayoutRect(c_offset * 10 + 1, c_offset * 10 + 1,
                               LayoutUnit(400), LayoutUnit(400)),
                    c, frame_view->GetLayoutView(), c_offset * 10 + 1);
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
  EXPECT_EQ(TransformationMatrix().Translate3d(5, 7, 0),
            b_properties->Transform()->Matrix());
  // The paint offset transform should be snapped from (0.7,0.7) to (1,1).
  EXPECT_EQ(TransformationMatrix().Translate(1, 1),
            b_properties->Transform()->Parent()->Matrix());
  // The residual subpixel adjustment should be (0.7,0.7) - (1,1) = (-0.3,-0.3).
  LayoutPoint subpixel_accumulation =
      LayoutPoint(LayoutPoint(FloatPoint(0.7, 0.7)) - LayoutPoint(1, 1));
  EXPECT_EQ(subpixel_accumulation, b->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(5.7), LayoutUnit(7.7),
                                     LayoutUnit(40), LayoutUnit(40)),
                          b, frame_view->GetLayoutView());

  LayoutObject* c = GetLayoutObjectByElementId("c");
  const ObjectPaintProperties* c_properties =
      c->FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate3d(11, 13, 0),
            c_properties->Transform()->Matrix());
  // The paint offset should be (-0.3,-0.3) but the paint offset transform
  // should still be at (0,0) because it should be snapped.
  EXPECT_EQ(TransformationMatrix().Translate(0, 0),
            c_properties->Transform()->Parent()->Matrix());
  // The residual subpixel adjustment should still be (-0.3,-0.3).
  EXPECT_EQ(subpixel_accumulation, c->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(16.7), LayoutUnit(20.7),
                                     LayoutUnit(40), LayoutUnit(40)),
                          c, frame_view->GetLayoutView());

  // d should be painted starting at subpixelAccumulation + (0.7,0.7) =
  // (0.4,0.4).
  LayoutObject* d = GetLayoutObjectByElementId("d");
  LayoutPoint d_paint_offset =
      subpixel_accumulation + LayoutPoint(FloatPoint(0.7, 0.7));
  EXPECT_EQ(d_paint_offset, d->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(LayoutRect(LayoutUnit(16.7) + LayoutUnit(0.7),
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
  EXPECT_EQ(TransformationMatrix().Translate(0, 0),
            b_properties->Transform()->Matrix());
  // The paint offset transform should be snapped from (0.7,0) to (1,0).
  EXPECT_EQ(TransformationMatrix().Translate(1, 0),
            b_properties->Transform()->Parent()->Matrix());
  // The residual subpixel adjustment should be (0.7,0) - (1,0) = (-0.3,0).
  LayoutPoint subpixel_accumulation =
      LayoutPoint(LayoutPoint(FloatPoint(0.7, 0)) - LayoutPoint(1, 0));
  EXPECT_EQ(subpixel_accumulation, b->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(0.7), LayoutUnit(0),
                                     LayoutUnit(40), LayoutUnit(40)),
                          b, frame_view->GetLayoutView());

  LayoutObject* fixed = GetLayoutObjectByElementId("fixed");
  // The residual subpixel adjustment should still be (-0.3,0).
  EXPECT_EQ(subpixel_accumulation, fixed->FirstFragment().PaintOffset());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(LayoutUnit(0.7), LayoutUnit(0),
                                     LayoutUnit(40), LayoutUnit(40)),
                          fixed, frame_view->GetLayoutView());

  // d should be painted starting at subpixelAccumulation + (0.7,0) = (0.4,0).
  LayoutObject* d = GetLayoutObjectByElementId("d");
  LayoutPoint d_paint_offset =
      subpixel_accumulation + LayoutPoint(FloatPoint(0.7, 0));
  EXPECT_EQ(d_paint_offset, d->FirstFragment().PaintOffset());
  // Visual rects via the non-paint properties system use enclosingIntRect
  // before applying transforms, because they are computed bottom-up and
  // therefore can't apply pixel snapping. Therefore apply a slop of 1px.
  CHECK_VISUAL_RECT(LayoutRect(LayoutUnit(0.7) + LayoutUnit(0.7), LayoutUnit(),
                               LayoutUnit(40), LayoutUnit(40)),
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
  EXPECT_EQ(TransformationMatrix(),
            svg_with_transform_properties->Transform()->Matrix());
  EXPECT_EQ(LayoutPoint(FloatPoint(0.1, 0)),
            svg_with_transform.FirstFragment().PaintOffset());
  EXPECT_TRUE(svg_with_transform_properties->ReplacedContentTransform() ==
              nullptr);

  LayoutObject& rect_with_transform = *GetLayoutObjectByElementId("rect");
  const ObjectPaintProperties* rect_with_transform_properties =
      rect_with_transform.FirstFragment().PaintProperties();
  EXPECT_EQ(TransformationMatrix().Translate(1, 1),
            rect_with_transform_properties->Transform()->Matrix());

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
  EXPECT_EQ(
      FloatSize(9, 8),
      svg_properties->PaintOffsetTranslation()->Matrix().To2DTranslation());
  EXPECT_EQ(LayoutPoint(LayoutUnit(-0.40625), LayoutUnit(0.3)),
            svg->FirstFragment().PaintOffset());
  EXPECT_EQ(nullptr, svg_properties->ReplacedContentTransform());
  const auto* foreign_object = GetLayoutObjectByElementId("foreign");
  const auto* foreign_object_properties =
      foreign_object->FirstFragment().PaintProperties();
  EXPECT_EQ(nullptr, foreign_object_properties->PaintOffsetTranslation());

  EXPECT_EQ(LayoutPoint(4, 5), foreign_object->FirstFragment().PaintOffset());

  const auto* div = GetLayoutObjectByElementId("div");
  // Paint offset of descendant of foreignObject accumulates on paint offset
  // of foreignObject.
  EXPECT_EQ(LayoutPoint(LayoutUnit(4 + 5.6), LayoutUnit(5 + 7.3)),
            div->FirstFragment().PaintOffset());
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
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(a_properties->Transform()->HasRenderingContext());
    EXPECT_TRUE(b_properties->Transform()->HasRenderingContext());
    EXPECT_EQ(a_properties->Transform()->RenderingContextId(),
              b_properties->Transform()->RenderingContextId());
  }
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 48, 20, 10), b,
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
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(a_properties->Transform()->HasRenderingContext());
    EXPECT_FALSE(b_properties->Transform()->HasRenderingContext());
  }
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 20), b,
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

  // #a should participate in a rendering context (due to its parent). Its
  // child does preserve 3D, but since #a does not, #a's rendering context is
  // not passed on to its children. Thus #b ends up in a separate rendering
  // context rooted at its parent.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(a_properties->Transform()->HasRenderingContext());
    EXPECT_TRUE(b_properties->Transform()->HasRenderingContext());
    EXPECT_NE(a_properties->Transform()->RenderingContextId(),
              b_properties->Transform()->RenderingContextId());
  }
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 50, 60), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 20), b,
                          frame_view->GetLayoutView());
}

// Returns true if the first node has the second as an ancestor.
static bool NodeHasAncestor(const TransformPaintPropertyNode* node,
                            const TransformPaintPropertyNode* ancestor) {
  while (node) {
    if (node == ancestor)
      return true;
    node = node->Parent();
  }
  return false;
}

// Returns true if some node will flatten the transform due to |node| before it
// is inherited by |node| (including if node->flattensInheritedTransform()).
static bool SomeNodeFlattensTransform(
    const TransformPaintPropertyNode* node,
    const TransformPaintPropertyNode* ancestor) {
  while (node != ancestor) {
    if (node->FlattensInheritedTransform())
      return true;
    node = node->Parent();
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
  ASSERT_TRUE(NodeHasAncestor(b_transform, a_transform));

  // Some node must flatten the inherited transform from #a before it reaches
  // #b's transform.
  EXPECT_TRUE(SomeNodeFlattensTransform(b_transform, a_transform));
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 10), b,
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
  ASSERT_TRUE(NodeHasAncestor(b_transform, a_transform));

  // No node may flatten the inherited transform from #a before it reaches
  // #b's transform.
  EXPECT_FALSE(SomeNodeFlattensTransform(b_transform, a_transform));
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 10), b,
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
  ASSERT_TRUE(NodeHasAncestor(b_transform, a_perspective));
  EXPECT_FALSE(SomeNodeFlattensTransform(b_transform, a_perspective));
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 20), b,
                          frame_view->GetLayoutView());
}

TEST_P(PaintPropertyTreeBuilderTest, FlatteningIn3DContext) {
  if (!RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    return;

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
  EXPECT_EQ(TransformationMatrix(), a_properties->Transform()->Matrix());
  EXPECT_TRUE(a_properties->Transform()->HasRenderingContext());
  EXPECT_TRUE(a_properties->Transform()->FlattensInheritedTransform());
  EXPECT_EQ(a_properties->Effect(), nullptr);

  const auto* b_properties = PaintPropertiesForElement("b");
  ASSERT_NE(b_properties, nullptr);
  ASSERT_NE(b_properties->Transform(), nullptr);
  EXPECT_EQ(TransformationMatrix().Translate3d(0, 0, 33),
            b_properties->Transform()->Matrix());
  EXPECT_EQ(a_properties->Transform()->RenderingContextId(),
            b_properties->Transform()->RenderingContextId());
  EXPECT_FALSE(b_properties->Transform()->FlattensInheritedTransform());
  // Force render surface with an effect node for |b| which is an 3D object in
  // its container while it flattens its contents.
  ASSERT_NE(b_properties->Effect(), nullptr);
  EXPECT_EQ(b_properties->Transform(),
            b_properties->Effect()->LocalTransformSpace());

  const auto* c_properties = PaintPropertiesForElement("c");
  ASSERT_NE(c_properties, nullptr);
  ASSERT_NE(c_properties->Transform(), nullptr);
  EXPECT_EQ(TransformationMatrix().Translate3d(0, 0, -10),
            c_properties->Transform()->Matrix());
  EXPECT_FALSE(c_properties->Transform()->HasRenderingContext());
  EXPECT_TRUE(c_properties->Transform()->FlattensInheritedTransform());
  EXPECT_EQ(c_properties->Effect(), nullptr);

  const auto* d_properties = PaintPropertiesForElement("d");
  ASSERT_NE(d_properties, nullptr);
  ASSERT_NE(d_properties->Transform(), nullptr);
  EXPECT_EQ(TransformationMatrix().Translate3d(0, -10, 22),
            d_properties->Transform()->Matrix());
  EXPECT_EQ(a_properties->Transform()->RenderingContextId(),
            d_properties->Transform()->RenderingContextId());
  EXPECT_FALSE(d_properties->Transform()->FlattensInheritedTransform());
  EXPECT_EQ(d_properties->Effect(), nullptr);
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
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 30, 40), a,
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(8, 8, 10, 20), b,
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

  Element* a = GetDocument().getElementById("a");
  const ObjectPaintProperties* a_properties =
      a->GetLayoutObject()->FirstFragment().PaintProperties();
  const TransformPaintPropertyNode* a_transform_node =
      a_properties->Transform();
  EXPECT_EQ(TransformationMatrix().Translate(33, 44),
            a_transform_node->Matrix());

  Element* b = GetDocument().getElementById("b");
  const ObjectPaintProperties* b_properties =
      b->GetLayoutObject()->FirstFragment().PaintProperties();
  const TransformPaintPropertyNode* b_transform_node =
      b_properties->Transform();
  EXPECT_EQ(TransformationMatrix().Translate(55, 66),
            b_transform_node->Matrix());

  Element* c = GetDocument().getElementById("c");
  const ObjectPaintProperties* c_properties =
      c->GetLayoutObject()->FirstFragment().PaintProperties();
  const TransformPaintPropertyNode* c_transform_node =
      c_properties->Transform();
  EXPECT_EQ(TransformationMatrix().Translate(77, 88),
            c_transform_node->Matrix());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(33, 44, 50, 60), a->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(88, 110, 30, 40), b->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(165, 198, 10, 20), c->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // Change transform of b. B's transform node should be a new node with the new
  // value, and a and c's transform nodes should be unchanged (with c's parent
  // adjusted).
  b->setAttribute(HTMLNames::styleAttr, "transform: translate(111px, 222px)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(a_properties,
            a->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(a_transform_node, a_properties->Transform());

  EXPECT_EQ(b_properties,
            b->GetLayoutObject()->FirstFragment().PaintProperties());
  b_transform_node = b_properties->Transform();
  EXPECT_EQ(TransformationMatrix().Translate(111, 222),
            b_transform_node->Matrix());
  EXPECT_EQ(a_transform_node, b_transform_node->Parent()->Parent());

  EXPECT_EQ(c_properties,
            c->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(c_transform_node, c_properties->Transform());
  EXPECT_EQ(b_transform_node, c_transform_node->Parent()->Parent());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(33, 44, 50, 60), a->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(144, 266, 50, 20), b->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(221, 354, 10, 20), c->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // Remove transform from b. B's transform node should be removed from the
  // tree, and a and c's transform nodes should be unchanged (with c's parent
  // adjusted).
  b->setAttribute(HTMLNames::styleAttr, "");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(a_properties,
            a->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(a_transform_node, a_properties->Transform());

  EXPECT_EQ(nullptr, b->GetLayoutObject()->FirstFragment().PaintProperties());

  EXPECT_EQ(c_properties,
            c->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(c_transform_node, c_properties->Transform());
  EXPECT_EQ(a_transform_node, c_transform_node->Parent()->Parent());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(33, 44, 50, 60), a->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(33, 44, 50, 20), b->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(110, 132, 10, 20), c->GetLayoutObject(),
                          frame_view->GetLayoutView());

  // Re-add transform to b. B's transform node should be inserted into the tree,
  // and a and c's transform nodes should be unchanged (with c's parent
  // adjusted).
  b->setAttribute(HTMLNames::styleAttr, "transform: translate(4px, 5px)");
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(a_properties,
            a->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(a_transform_node, a_properties->Transform());

  b_properties = b->GetLayoutObject()->FirstFragment().PaintProperties();
  EXPECT_EQ(b_properties,
            b->GetLayoutObject()->FirstFragment().PaintProperties());
  b_transform_node = b_properties->Transform();
  EXPECT_EQ(TransformationMatrix().Translate(4, 5), b_transform_node->Matrix());
  EXPECT_EQ(a_transform_node, b_transform_node->Parent()->Parent());

  EXPECT_EQ(c_properties,
            c->GetLayoutObject()->FirstFragment().PaintProperties());
  EXPECT_EQ(c_transform_node, c_properties->Transform());
  EXPECT_EQ(b_transform_node, c_transform_node->Parent()->Parent());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(33, 44, 50, 60), a->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(37, 49, 50, 20), b->GetLayoutObject(),
                          frame_view->GetLayoutView());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(114, 137, 10, 20), c->GetLayoutObject(),
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
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("clipper"));
  const ObjectPaintProperties* clip_properties =
      clipper->FirstFragment().PaintProperties();
  LayoutObject* child = GetLayoutObjectByElementId("child");

  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_FALSE(DocScrollTranslation());
    EXPECT_TRUE(DocPreTranslation());
    EXPECT_EQ(DocPreTranslation(),
              clipper->FirstFragment().LocalBorderBoxProperties().Transform());
  } else {
    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
    EXPECT_EQ(DocScrollTranslation(),
              clipper->FirstFragment().LocalBorderBoxProperties().Transform());
  }
  EXPECT_EQ(DocContentClip(),
            clipper->FirstFragment().LocalBorderBoxProperties().Clip());

  auto contents_properties = clipper->FirstFragment().ContentsProperties();
  EXPECT_EQ(LayoutPoint(30, 20), clipper->FirstFragment().PaintOffset());

  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(), contents_properties.Transform());
  } else {
    EXPECT_EQ(DocScrollTranslation(), contents_properties.Transform());
  }
  EXPECT_EQ(clip_properties->OverflowClip(), contents_properties.Clip());

  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(),
              child->FirstFragment().LocalBorderBoxProperties().Transform());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              child->FirstFragment().LocalBorderBoxProperties().Transform());
  }
  EXPECT_EQ(clip_properties->OverflowClip(),
            child->FirstFragment().LocalBorderBoxProperties().Clip());

  EXPECT_NE(nullptr,
            child->FirstFragment().LocalBorderBoxProperties().Effect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 500, 600), child, clipper);
}

TEST_P(PaintPropertyTreeBuilderTest, ContainsPaintContentsTreeState) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 20px 30px; }</style>
    <div id='clipper'
        style='contain: paint; width: 300px; height: 200px;'>
      <div id='child'
          style='position: relative; width: 400px; height: 500px;'></div>
    </div>
  )HTML");

  LayoutBoxModelObject* clipper =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("clipper"));
  const ObjectPaintProperties* clip_properties =
      clipper->FirstFragment().PaintProperties();
  LayoutObject* child = GetLayoutObjectByElementId("child");
  const auto& clip_local_properties =
      clipper->FirstFragment().LocalBorderBoxProperties();

  // Verify that we created isolation nodes.
  EXPECT_TRUE(clip_properties->TransformIsolationNode());
  EXPECT_TRUE(clip_properties->EffectIsolationNode());
  EXPECT_TRUE(clip_properties->ClipIsolationNode());

  // Verify parenting:

  // Transform isolation node should be parented to the local border box
  // properties transform, which should be the paint offset translation.
  EXPECT_EQ(clip_properties->TransformIsolationNode()->Parent(),
            clip_local_properties.Transform());
  EXPECT_EQ(clip_properties->TransformIsolationNode()->Parent(),
            clip_properties->PaintOffsetTranslation());
  // Similarly, effect isolation node is parented to the local border box
  // properties effect.
  EXPECT_EQ(clip_properties->EffectIsolationNode()->Parent(),
            clip_local_properties.Effect());
  // Clip isolation node, however, is parented to the overflow clip, which is in
  // turn parented to the local border box properties clip.
  EXPECT_EQ(clip_properties->ClipIsolationNode()->Parent(),
            clip_properties->OverflowClip());
  EXPECT_EQ(clip_properties->OverflowClip()->Parent(),
            clip_local_properties.Clip());

  // Verify transform:

  // Isolation transform node should be identity.
  EXPECT_EQ(clip_properties->TransformIsolationNode()->Matrix(),
            TransformationMatrix());

  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_FALSE(DocScrollTranslation());
    EXPECT_TRUE(DocPreTranslation());
    // Isolation induces paint offset translation, so the node should be
    // different from the doc node, but its parent is the same as the doc node.
    EXPECT_EQ(DocPreTranslation(), clipper->FirstFragment()
                                       .LocalBorderBoxProperties()
                                       .Transform()
                                       ->Parent());
  } else {
    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
    // Isolation induces paint offset translation, so the node should be
    // different from the doc node, but its parent is the same as the doc node.
    EXPECT_EQ(DocScrollTranslation(), clipper->FirstFragment()
                                          .LocalBorderBoxProperties()
                                          .Transform()
                                          ->Parent());
  }

  // Verify clip:

  EXPECT_EQ(DocContentClip(),
            clipper->FirstFragment().LocalBorderBoxProperties().Clip());
  // Clip isolation node should be big enough to encompass all other clips,
  // including DocContentClip.
  EXPECT_TRUE(clip_properties->ClipIsolationNode()->ClipRect().Rect().Contains(
      DocContentClip()->ClipRect().Rect()));

  // Verify contents properties and child properties:

  auto contents_properties = clipper->FirstFragment().ContentsProperties();
  // Since the clipper is isolated, its paint offset should be 0, 0.
  EXPECT_EQ(LayoutPoint(0, 0), clipper->FirstFragment().PaintOffset());
  // Ensure that the contents properties match isolation nodes.
  EXPECT_EQ(clip_properties->TransformIsolationNode(),
            contents_properties.Transform());
  EXPECT_EQ(clip_properties->ClipIsolationNode(), contents_properties.Clip());
  EXPECT_EQ(clip_properties->EffectIsolationNode(),
            contents_properties.Effect());

  // Child should be using isolation nodes as its local border box properties.
  EXPECT_EQ(contents_properties.Transform(),
            child->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(contents_properties.Clip(),
            child->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(contents_properties.Effect(),
            child->FirstFragment().LocalBorderBoxProperties().Effect());
  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 400, 500), child, clipper);
}

TEST_P(PaintPropertyTreeBuilderTest, OverflowScrollContentsTreeState) {
  // This test verifies the tree builder correctly computes and records the
  // property tree context for a (pseudo) stacking context that is scrolled by a
  // containing block that is not one of the painting ancestors.
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 20px 30px; }</style>
    <div id='clipper' style='overflow:scroll; width:400px; height:300px;'>
      <div id='child'
          style='position:relative; width:500px; height: 600px;'></div>
      <div style='width: 200px; height: 10000px'></div>
    </div>
    <div id='forceScroll' style='height: 4000px;'></div>
  )HTML");

  Element* clipper_element = GetDocument().getElementById("clipper");
  clipper_element->scrollTo(1, 2);

  LayoutBoxModelObject* clipper =
      ToLayoutBoxModelObject(clipper_element->GetLayoutObject());
  const ObjectPaintProperties* clip_properties =
      clipper->FirstFragment().PaintProperties();
  LayoutObject* child = GetLayoutObjectByElementId("child");

  EXPECT_EQ(DocScrollTranslation(), clipper->FirstFragment()
                                        .LocalBorderBoxProperties()
                                        .Transform()
                                        ->Parent());
  EXPECT_EQ(clip_properties->PaintOffsetTranslation(),
            clipper->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(DocContentClip(),
            clipper->FirstFragment().LocalBorderBoxProperties().Clip());

  auto contents_properties = clipper->FirstFragment().ContentsProperties();
  EXPECT_EQ(
      FloatSize(30, 20),
      clip_properties->PaintOffsetTranslation()->Matrix().To2DTranslation());
  EXPECT_EQ(LayoutPoint(0, 0), clipper->FirstFragment().PaintOffset());
  EXPECT_EQ(clip_properties->ScrollTranslation(),
            contents_properties.Transform());
  EXPECT_EQ(clip_properties->OverflowClip(), contents_properties.Clip());

  EXPECT_EQ(clip_properties->ScrollTranslation(),
            child->FirstFragment().LocalBorderBoxProperties().Transform());
  EXPECT_EQ(clip_properties->OverflowClip(),
            child->FirstFragment().LocalBorderBoxProperties().Clip());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 500, 600), child, clipper);
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
  EXPECT_EQ(
      FloatRoundedRect(FloatRect(50, 50, 200, 200), FloatSize(50, 50),
                       FloatSize(50, 50), FloatSize(50, 50), FloatSize(50, 50)),
      rounded_box_properties->InnerBorderRadiusClip()->ClipRect());

  // Unlike the inner border radius clip, the overflow clip is inset by the
  // scrollbars (13px).
  EXPECT_EQ(FloatRoundedRect(50, 50, 187, 187),
            rounded_box_properties->OverflowClip()->ClipRect());
  EXPECT_EQ(DocContentClip(),
            rounded_box_properties->InnerBorderRadiusClip()->Parent());
  EXPECT_EQ(rounded_box_properties->InnerBorderRadiusClip(),
            rounded_box_properties->OverflowClip()->Parent());
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

  LayoutBoxModelObject* clipper =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("clipper"));
  const ObjectPaintProperties* clip_properties =
      clipper->FirstFragment().PaintProperties();
  LayoutObject* child = GetLayoutObjectByElementId("child");

  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_TRUE(DocPreTranslation());
    EXPECT_EQ(DocPreTranslation(),
              clipper->FirstFragment().LocalBorderBoxProperties().Transform());
  } else {
    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
    EXPECT_EQ(DocScrollTranslation(),
              clipper->FirstFragment().LocalBorderBoxProperties().Transform());
  }
  // CSS clip on an element causes it to clip itself, not just descendants.
  EXPECT_EQ(clip_properties->CssClip(),
            clipper->FirstFragment().LocalBorderBoxProperties().Clip());

  auto contents_properties = clipper->FirstFragment().ContentsProperties();
  EXPECT_EQ(LayoutPoint(30, 20), clipper->FirstFragment().PaintOffset());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(), contents_properties.Transform());
  } else {
    EXPECT_EQ(DocScrollTranslation(), contents_properties.Transform());
  }
  EXPECT_EQ(clip_properties->CssClip(), contents_properties.Clip());

  CHECK_EXACT_VISUAL_RECT(LayoutRect(0, 0, 400, 500), child, clipper);
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
  EXPECT_EQ(
      paint_offset_translation,
      svg_with_view_box.FirstFragment().LocalBorderBoxProperties().Transform());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(), paint_offset_translation->Parent());
  } else {
    EXPECT_EQ(DocScrollTranslation(), paint_offset_translation->Parent());
  }
  EXPECT_EQ(FloatSize(30, 20),
            paint_offset_translation->Matrix().To2DTranslation());
  EXPECT_EQ(LayoutPoint(0, 0), svg_with_view_box.FirstFragment().PaintOffset());

  const auto* replaced_content_transform = svg_with_view_box.FirstFragment()
                                               .PaintProperties()
                                               ->ReplacedContentTransform();
  EXPECT_EQ(replaced_content_transform,
            svg_with_view_box.FirstFragment().ContentsProperties().Transform());
  EXPECT_EQ(paint_offset_translation, replaced_content_transform->Parent());
  EXPECT_EQ(FloatSize(-50, -50),
            replaced_content_transform->Matrix().To2DTranslation());
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

  Element* overflow_hidden = GetDocument().getElementById("overflowHidden");
  overflow_hidden->setScrollTop(37);

  GetDocument().View()->UpdateAllLifecyclePhases();

  const ObjectPaintProperties* overflow_hidden_scroll_properties =
      overflow_hidden->GetLayoutObject()->FirstFragment().PaintProperties();

  // Because the overflow hidden does not scroll and only has a static scroll
  // offset, there should be a scroll translation node but no scroll node.
  auto* scroll_translation =
      overflow_hidden_scroll_properties->ScrollTranslation();
  EXPECT_EQ(TransformationMatrix().Translate(0, -37),
            scroll_translation->Matrix());
  EXPECT_EQ(nullptr, scroll_translation->ScrollNode());
  EXPECT_EQ(nullptr, overflow_hidden_scroll_properties->Scroll());
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

  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(TransformationMatrix().Translate(0, -37),
            DocScrollTranslation()->Matrix());

  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(nullptr, DocScrollTranslation()->ScrollNode());
    EXPECT_EQ(nullptr, DocScroll());
  } else {
    EXPECT_TRUE(DocScrollTranslation()->ScrollNode());
    EXPECT_TRUE(DocScroll());
  }
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

  Element* overflow_a = GetDocument().getElementById("overflowA");
  overflow_a->setScrollTop(37);
  Element* overflow_b = GetDocument().getElementById("overflowB");
  overflow_b->setScrollTop(41);

  GetDocument().View()->UpdateAllLifecyclePhases();

  const ObjectPaintProperties* overflow_a_scroll_properties =
      overflow_a->GetLayoutObject()->FirstFragment().PaintProperties();
  // Because the frameView is does not scroll, overflowA's scroll should be
  // under the root.
  auto* scroll_a_translation =
      overflow_a_scroll_properties->ScrollTranslation();
  auto* overflow_a_scroll_node = scroll_a_translation->ScrollNode();
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetDocument()
                  .GetPage()
                  ->GetVisualViewport()
                  .GetScrollTranslationNode()
                  ->ScrollNode(),
              overflow_a_scroll_node->Parent());
  } else {
    EXPECT_EQ(DocScroll(), overflow_a_scroll_node->Parent());
  }
  EXPECT_EQ(TransformationMatrix().Translate(0, -37),
            scroll_a_translation->Matrix());
  EXPECT_EQ(IntRect(0, 0, 5, 3), overflow_a_scroll_node->ContainerRect());
  // 107 is the forceScroll element plus the height of the overflow scroll child
  // (overflowB).
  EXPECT_EQ(IntSize(9, 107), overflow_a_scroll_node->ContentsSize());
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
  EXPECT_EQ(TransformationMatrix().Translate(0, -41),
            scroll_b_translation->Matrix());
  EXPECT_EQ(IntRect(0, 0, 9, 7), overflow_b_scroll_node->ContainerRect());
  EXPECT_EQ(IntSize(9, 100), overflow_b_scroll_node->ContentsSize());
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

  Element* overflow = GetDocument().getElementById("overflow");
  overflow->setScrollTop(37);
  Element* abspos_overflow = GetDocument().getElementById("absposOverflow");
  abspos_overflow->setScrollTop(41);
  Element* fixed_overflow = GetDocument().getElementById("fixedOverflow");
  fixed_overflow->setScrollTop(43);

  GetDocument().View()->UpdateAllLifecyclePhases();

  // The frame should scroll due to the "forceScroll" element.
  EXPECT_NE(nullptr, DocScroll());

  const ObjectPaintProperties* overflow_scroll_properties =
      overflow->GetLayoutObject()->FirstFragment().PaintProperties();
  auto* scroll_translation = overflow_scroll_properties->ScrollTranslation();
  auto* overflow_scroll_node = scroll_translation->ScrollNode();
  EXPECT_EQ(
      DocScroll(),
      overflow_scroll_properties->ScrollTranslation()->ScrollNode()->Parent());
  EXPECT_EQ(TransformationMatrix().Translate(0, -37),
            scroll_translation->Matrix());
  EXPECT_EQ(IntRect(0, 0, 5, 3), overflow_scroll_node->ContainerRect());
  // The height should be 4000px because the (dom-order) overflow children are
  // positioned and do not contribute to the height. Only the 4000px
  // "forceScroll" height is present.
  EXPECT_EQ(IntSize(5, 4000), overflow_scroll_node->ContentsSize());

  const ObjectPaintProperties* abspos_overflow_scroll_properties =
      abspos_overflow->GetLayoutObject()->FirstFragment().PaintProperties();
  auto* abspos_scroll_translation =
      abspos_overflow_scroll_properties->ScrollTranslation();
  auto* abspos_overflow_scroll_node = abspos_scroll_translation->ScrollNode();
  // The absolute position overflow scroll node is parented under the frame, not
  // the dom-order parent.
  EXPECT_EQ(DocScroll(), abspos_overflow_scroll_node->Parent());
  EXPECT_EQ(TransformationMatrix().Translate(0, -41),
            abspos_scroll_translation->Matrix());
  EXPECT_EQ(IntRect(0, 0, 9, 7), abspos_overflow_scroll_node->ContainerRect());
  EXPECT_EQ(IntSize(9, 4000), abspos_overflow_scroll_node->ContentsSize());

  const ObjectPaintProperties* fixed_overflow_scroll_properties =
      fixed_overflow->GetLayoutObject()->FirstFragment().PaintProperties();
  auto* fixed_scroll_translation =
      fixed_overflow_scroll_properties->ScrollTranslation();
  auto* fixed_overflow_scroll_node = fixed_scroll_translation->ScrollNode();
  // The fixed position overflow scroll node is parented under the root, not the
  // dom-order parent or frame's scroll.
  EXPECT_EQ(GetDocument()
                .GetPage()
                ->GetVisualViewport()
                .GetScrollTranslationNode()
                ->ScrollNode(),
            fixed_overflow_scroll_node->Parent());
  EXPECT_EQ(TransformationMatrix().Translate(0, -43),
            fixed_scroll_translation->Matrix());
  EXPECT_EQ(IntRect(0, 0, 13, 11), fixed_overflow_scroll_node->ContainerRect());
  EXPECT_EQ(IntSize(13, 4000), fixed_overflow_scroll_node->ContentsSize());
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

  Element* overflow_a = GetDocument().getElementById("overflowA");
  overflow_a->setScrollTop(37);
  Element* overflow_b = GetDocument().getElementById("overflowB");
  overflow_b->setScrollTop(41);

  GetDocument().View()->UpdateAllLifecyclePhases();

  const ObjectPaintProperties* overflow_a_scroll_properties =
      overflow_a->GetLayoutObject()->FirstFragment().PaintProperties();
  // Because the frameView is does not scroll, overflowA's scroll should be
  // under the root.
  auto* scroll_a_translation =
      overflow_a_scroll_properties->ScrollTranslation();
  auto* overflow_a_scroll_node = scroll_a_translation->ScrollNode();
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetDocument()
                  .GetPage()
                  ->GetVisualViewport()
                  .GetScrollTranslationNode()
                  ->ScrollNode(),
              overflow_a_scroll_node->Parent());
  } else {
    EXPECT_EQ(DocScroll(), overflow_a_scroll_node->Parent());
  }
  EXPECT_EQ(TransformationMatrix().Translate(0, -37),
            scroll_a_translation->Matrix());
  EXPECT_EQ(IntRect(0, 0, 20, 20), overflow_a_scroll_node->ContainerRect());
  // 100 is the forceScroll element's height because the overflow child does not
  // contribute to the height.
  EXPECT_EQ(IntSize(20, 100), overflow_a_scroll_node->ContentsSize());
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
  EXPECT_EQ(TransformationMatrix().Translate(0, -41),
            scroll_b_translation->Matrix());
  EXPECT_EQ(IntRect(0, 0, 5, 3), overflow_b_scroll_node->ContainerRect());
  EXPECT_EQ(IntSize(5, 100), overflow_b_scroll_node->ContentsSize());
  EXPECT_TRUE(overflow_b_scroll_node->UserScrollableHorizontal());
  EXPECT_TRUE(overflow_b_scroll_node->UserScrollableVertical());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootClip) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' width='100px' height='100px'>
      <rect width='200' height='200' fill='red' />
    </svg>
  )HTML");

  const ClipPaintPropertyNode* clip = GetLayoutObjectByElementId("svg")
                                          ->FirstFragment()
                                          .PaintProperties()
                                          ->OverflowClip();
  EXPECT_EQ(DocContentClip(), clip->Parent());
  EXPECT_EQ(FloatSize(8, 8), GetLayoutObjectByElementId("svg")
                                 ->FirstFragment()
                                 .PaintProperties()
                                 ->PaintOffsetTranslation()
                                 ->Matrix()
                                 .To2DTranslation());
  EXPECT_EQ(FloatRoundedRect(0, 0, 100, 100), clip->ClipRect());
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
  Element* overflow = GetDocument().getElementById("overflow");
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

  LayoutObject* scroller = GetLayoutObjectByElementId("scroller");
  ToLayoutBox(scroller)->GetScrollableArea()->ScrollBy(ScrollOffset(0, 300),
                                                       kUserScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  EXPECT_EQ(FloatSize(8, 8), scroller->FirstFragment()
                                 .PaintProperties()
                                 ->PaintOffsetTranslation()
                                 ->Matrix()
                                 .To2DTranslation());
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
  EXPECT_EQ(LayoutPoint(8, 8), target1->FirstFragment().PaintOffset());
  // |target1| is only in the first column.
  EXPECT_FALSE(target1->FirstFragment().NextFragment());

  LayoutObject* target2 = GetLayoutObjectByElementId("target2");
  EXPECT_EQ(LayoutPoint(158, 8), target2->FirstFragment().PaintOffset());
  // |target2| is only in the second column.
  EXPECT_FALSE(target2->FirstFragment().NextFragment());
}

TEST_P(PaintPropertyTreeBuilderTest,
       PaintOffsetsUnderMultiColumnWithLayoutOverflow) {
  SetBodyInnerHTML(R"HTML(
    <div style='columns: 2; width: 300px; column-gap: 0; height: 100px'>
      <div id='parent' style='outline: 2px solid black;
          width: 100px; height: 100px'>
        <div id='child' style='width: 100px; height: 200px'></div>
      </div>
    </div>
  )HTML");

  LayoutObject* parent = GetLayoutObjectByElementId("parent");
  // Parent has 1 fragment regardless of the overflowing child.
  ASSERT_EQ(1u, NumFragments(parent));
  EXPECT_EQ(LayoutPoint(8, 8), FragmentAt(parent, 0).PaintOffset());

  LayoutObject* child = GetLayoutObjectByElementId("child");
  ASSERT_EQ(2u, NumFragments(child));
  EXPECT_EQ(LayoutPoint(8, 8), FragmentAt(child, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(158, -92), FragmentAt(child, 1).PaintOffset());
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
    <div id=fixed style='position: fixed; columns: 2'>
      <div style='width: 50px; height: 20px; background: lightblue'></div>
      <div style='width: 50px; height: 20px; background: lightgray'></div>
    </div>
    <div style='height: 2000px'></div>
  )HTML");
  LayoutObject* fixed = GetLayoutObjectByElementId("fixed");
  LayoutObject* multicol_container = fixed->SlowFirstChild();

  ASSERT_TRUE(multicol_container->FirstFragment().NextFragment());
  ASSERT_FALSE(
      multicol_container->FirstFragment().NextFragment()->NextFragment());
  EXPECT_EQ(LayoutPoint(8, 8),
            multicol_container->FirstFragment().PaintOffset());
  EXPECT_EQ(LayoutPoint(59, -12),
            multicol_container->FirstFragment().NextFragment()->PaintOffset());

  GetDocument().View()->LayoutViewport()->ScrollBy(ScrollOffset(0, 25),
                                                   kUserScroll);
  GetDocument().View()->UpdateAllLifecyclePhases();

  ASSERT_TRUE(multicol_container->FirstFragment().NextFragment());
  ASSERT_FALSE(
      multicol_container->FirstFragment().NextFragment()->NextFragment());

  EXPECT_EQ(LayoutPoint(8, 8),
            multicol_container->FirstFragment().PaintOffset());
  EXPECT_EQ(LayoutPoint(59, -12),
            multicol_container->FirstFragment().NextFragment()->PaintOffset());
}

TEST_P(PaintPropertyTreeBuilderTest, FragmentsUnderMultiColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      .space { height: 30px; }
      .abs { position: absolute; width: 20px; height: 20px; }
    </style>
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
  )HTML");

  const auto* relpos = GetLayoutObjectByElementId("relpos");
  const auto* flowthread = relpos->Parent();
  EXPECT_EQ(4u, NumFragments(relpos));
  EXPECT_EQ(4u, NumFragments(flowthread));

  EXPECT_EQ(LayoutPoint(0, 0), FragmentAt(relpos, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 0), FragmentAt(relpos, 0).PaginationOffset());
  EXPECT_EQ(LayoutUnit(), FragmentAt(relpos, 0).LogicalTopInFlowThread());
  EXPECT_EQ(nullptr, FragmentAt(relpos, 0).PaintProperties());
  EXPECT_EQ(LayoutPoint(0, 0), FragmentAt(flowthread, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 0), FragmentAt(flowthread, 0).PaginationOffset());
  EXPECT_EQ(LayoutUnit(), FragmentAt(flowthread, 0).LogicalTopInFlowThread());
  const auto* fragment_clip =
      FragmentAt(flowthread, 0).PaintProperties()->FragmentClip();
  ASSERT_NE(nullptr, fragment_clip);
  EXPECT_EQ(FloatRect(-1000000, -1000000, 2000000, 1000030),
            fragment_clip->ClipRect().Rect());
  EXPECT_EQ(fragment_clip,
            FragmentAt(relpos, 0).LocalBorderBoxProperties().Clip());

  EXPECT_EQ(LayoutPoint(100, -30), FragmentAt(relpos, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(100, -30), FragmentAt(relpos, 1).PaginationOffset());
  EXPECT_EQ(LayoutUnit(30), FragmentAt(relpos, 1).LogicalTopInFlowThread());
  EXPECT_EQ(nullptr, FragmentAt(relpos, 1).PaintProperties());
  EXPECT_EQ(LayoutPoint(100, -30), FragmentAt(flowthread, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(100, -30),
            FragmentAt(flowthread, 1).PaginationOffset());
  EXPECT_EQ(LayoutUnit(30), FragmentAt(flowthread, 1).LogicalTopInFlowThread());
  fragment_clip = FragmentAt(flowthread, 1).PaintProperties()->FragmentClip();
  ASSERT_NE(nullptr, fragment_clip);
  EXPECT_EQ(FloatRect(-999900, 0, 2000000, 30),
            fragment_clip->ClipRect().Rect());
  EXPECT_EQ(fragment_clip,
            FragmentAt(relpos, 1).LocalBorderBoxProperties().Clip());

  EXPECT_EQ(LayoutPoint(0, 20), FragmentAt(relpos, 2).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 20), FragmentAt(relpos, 2).PaginationOffset());
  EXPECT_EQ(LayoutUnit(60), FragmentAt(relpos, 2).LogicalTopInFlowThread());
  EXPECT_EQ(nullptr, FragmentAt(relpos, 2).PaintProperties());
  EXPECT_EQ(LayoutPoint(0, 20), FragmentAt(flowthread, 2).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 20), FragmentAt(flowthread, 2).PaginationOffset());
  EXPECT_EQ(LayoutUnit(60), FragmentAt(flowthread, 2).LogicalTopInFlowThread());
  fragment_clip = FragmentAt(flowthread, 2).PaintProperties()->FragmentClip();
  ASSERT_NE(nullptr, fragment_clip);
  EXPECT_EQ(FloatRect(-1000000, 80, 2000000, 30),
            fragment_clip->ClipRect().Rect());
  EXPECT_EQ(fragment_clip,
            FragmentAt(relpos, 2).LocalBorderBoxProperties().Clip());

  EXPECT_EQ(LayoutPoint(100, -10), FragmentAt(relpos, 3).PaintOffset());
  EXPECT_EQ(LayoutPoint(100, -10), FragmentAt(relpos, 3).PaginationOffset());
  EXPECT_EQ(LayoutUnit(90), FragmentAt(relpos, 3).LogicalTopInFlowThread());
  EXPECT_EQ(nullptr, FragmentAt(relpos, 3).PaintProperties());
  EXPECT_EQ(LayoutPoint(100, -10), FragmentAt(flowthread, 3).PaintOffset());
  EXPECT_EQ(LayoutPoint(100, -10),
            FragmentAt(flowthread, 3).PaginationOffset());
  EXPECT_EQ(LayoutUnit(90), FragmentAt(flowthread, 3).LogicalTopInFlowThread());
  fragment_clip = FragmentAt(flowthread, 3).PaintProperties()->FragmentClip();
  ASSERT_NE(nullptr, fragment_clip);
  EXPECT_EQ(FloatRect(-999900, 80, 2000000, 999910),
            fragment_clip->ClipRect().Rect());
  EXPECT_EQ(fragment_clip,
            FragmentAt(relpos, 3).LocalBorderBoxProperties().Clip());

  // Above the spanner.
  // Column 1.
  const auto* space1 = GetLayoutObjectByElementId("space1");
  EXPECT_EQ(1u, NumFragments(space1));
  EXPECT_EQ(nullptr, space1->FirstFragment().PaintProperties());
  EXPECT_EQ(LayoutPoint(), space1->FirstFragment().PaintOffset());
  const auto* space2 = GetLayoutObjectByElementId("space2");
  EXPECT_EQ(1u, NumFragments(space2));
  EXPECT_EQ(nullptr, space2->FirstFragment().PaintProperties());
  EXPECT_EQ(LayoutPoint(100, 0), space2->FirstFragment().PaintOffset());

  // The spanner's normal flow.
  LayoutObject* spanner = GetLayoutObjectByElementId("spanner");
  EXPECT_EQ(1u, NumFragments(spanner));
  EXPECT_EQ(nullptr, spanner->FirstFragment().PaintProperties());
  EXPECT_EQ(LayoutPoint(0, 30), spanner->FirstFragment().PaintOffset());
  LayoutObject* normal = GetLayoutObjectByElementId("normal");
  EXPECT_EQ(1u, NumFragments(normal));
  EXPECT_EQ(nullptr, normal->FirstFragment().PaintProperties());
  EXPECT_EQ(LayoutPoint(0, 30), normal->FirstFragment().PaintOffset());

  // Below the spanner.
  const auto* space3 = GetLayoutObjectByElementId("space3");
  EXPECT_EQ(1u, NumFragments(space3));
  EXPECT_EQ(nullptr, space3->FirstFragment().PaintProperties());
  EXPECT_EQ(LayoutPoint(0, 80), space3->FirstFragment().PaintOffset());
  const auto* space4 = GetLayoutObjectByElementId("space4");
  EXPECT_EQ(1u, NumFragments(space4));
  EXPECT_EQ(nullptr, space4->FirstFragment().PaintProperties());
  EXPECT_EQ(LayoutPoint(100, 80), space4->FirstFragment().PaintOffset());

  // Out-of-flow positioned descendants of the spanner. They are laid out in
  // the relative-position container.

  // "top-left" should be aligned to the top-left corner of space1.
  const auto* top_left = GetLayoutObjectByElementId("top-left");
  EXPECT_EQ(1u, NumFragments(top_left));
  EXPECT_EQ(LayoutPoint(0, 0), top_left->FirstFragment().PaintOffset());
  fragment_clip = top_left->FirstFragment().PaintProperties()->FragmentClip();
  EXPECT_EQ(FragmentAt(flowthread, 0).PaintProperties()->FragmentClip(),
            fragment_clip->Parent());

  // "bottom-right" should be aligned to the bottom-right corner of space4.
  const auto* bottom_right = GetLayoutObjectByElementId("bottom-right");
  EXPECT_EQ(1u, NumFragments(bottom_right));
  EXPECT_EQ(LayoutPoint(180, 90), bottom_right->FirstFragment().PaintOffset());
  fragment_clip =
      bottom_right->FirstFragment().PaintProperties()->FragmentClip();
  EXPECT_EQ(FragmentAt(flowthread, 3).PaintProperties()->FragmentClip(),
            fragment_clip->Parent());
}

TEST_P(PaintPropertyTreeBuilderTest,
       FragmentsUnderMultiColumnVerticalRLWithOverflow) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='multicol' style='columns:2; column-fill:auto; column-gap: 0;
        width: 200px; height: 200px; writing-mode: vertical-rl'>
      <div style='width: 100px'>
        <div id='content' style='width: 400px'></div>
      </div>
    </div>
  )HTML");

  LayoutObject* thread =
      GetLayoutObjectByElementId("multicol")->SlowFirstChild();
  EXPECT_TRUE(thread->IsLayoutFlowThread());
  EXPECT_EQ(2u, NumFragments(thread));
  EXPECT_EQ(LayoutPoint(100, 0), FragmentAt(thread, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 0), FragmentAt(thread, 0).PaginationOffset());
  EXPECT_EQ(LayoutUnit(), FragmentAt(thread, 0).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(300, 100), FragmentAt(thread, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(200, 100), FragmentAt(thread, 1).PaginationOffset());
  EXPECT_EQ(LayoutUnit(200), FragmentAt(thread, 1).LogicalTopInFlowThread());

  LayoutObject* content = GetLayoutObjectByElementId("content");
  EXPECT_EQ(2u, NumFragments(content));
  EXPECT_EQ(LayoutPoint(-200, 0), FragmentAt(content, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 0), FragmentAt(content, 0).PaginationOffset());
  EXPECT_EQ(LayoutUnit(), FragmentAt(content, 0).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(0, 100), FragmentAt(content, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(200, 100), FragmentAt(content, 1).PaginationOffset());
  EXPECT_EQ(LayoutUnit(200), FragmentAt(content, 1).LogicalTopInFlowThread());
}

TEST_P(PaintPropertyTreeBuilderTest, CompositedUnderMultiColumn) {
  SetBodyInnerHTML(R"HTML(
    <style>body { margin: 0; }</style>
    <div id='multicol' style='columns:3; column-fill:auto; column-gap: 0;
        width: 300px; height: 200px'>
      <div style='height: 300px'></div>
      <div id='composited' style='will-change: transform; height: 300px'>
        <div id='non-composited-child' style='height: 150px'></div>
        <div id='composited-child'
             style='will-change: transform; height: 150px'></div>
      </div>
    </div>
  )HTML");

  LayoutObject* thread =
      GetLayoutObjectByElementId("multicol")->SlowFirstChild();
  EXPECT_TRUE(thread->IsLayoutFlowThread());
  EXPECT_EQ(3u, NumFragments(thread));
  EXPECT_EQ(LayoutPoint(0, 0), FragmentAt(thread, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 0), FragmentAt(thread, 0).PaginationOffset());
  EXPECT_EQ(LayoutUnit(), FragmentAt(thread, 0).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(100, -200), FragmentAt(thread, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(100, -200), FragmentAt(thread, 1).PaginationOffset());
  EXPECT_EQ(LayoutUnit(200), FragmentAt(thread, 1).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(200, -400), FragmentAt(thread, 2).PaintOffset());
  EXPECT_EQ(LayoutPoint(200, -400), FragmentAt(thread, 2).PaginationOffset());
  EXPECT_EQ(LayoutUnit(400), FragmentAt(thread, 2).LogicalTopInFlowThread());

  LayoutObject* composited = GetLayoutObjectByElementId("composited");
  LayoutObject* non_composited_child =
      GetLayoutObjectByElementId("non-composited-child");
  LayoutObject* composited_child =
      GetLayoutObjectByElementId("composited-child");
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    // Compositing doesn't affect SPv2 fragmentation.
    EXPECT_EQ(2u, NumFragments(composited));
    EXPECT_EQ(LayoutPoint(100, 100), FragmentAt(composited, 0).PaintOffset());
    EXPECT_EQ(LayoutPoint(100, -200),
              FragmentAt(composited, 0).PaginationOffset());
    EXPECT_EQ(LayoutUnit(200),
              FragmentAt(composited, 0).LogicalTopInFlowThread());
    EXPECT_EQ(LayoutPoint(200, -100), FragmentAt(composited, 1).PaintOffset());
    EXPECT_EQ(LayoutPoint(200, -400),
              FragmentAt(composited, 1).PaginationOffset());
    EXPECT_EQ(LayoutUnit(400),
              FragmentAt(composited, 1).LogicalTopInFlowThread());
    EXPECT_EQ(2u, NumFragments(non_composited_child));
    EXPECT_EQ(LayoutPoint(100, 100),
              FragmentAt(non_composited_child, 0).PaintOffset());
    EXPECT_EQ(LayoutPoint(100, -200),
              FragmentAt(non_composited_child, 0).PaginationOffset());
    EXPECT_EQ(LayoutUnit(200),
              FragmentAt(non_composited_child, 0).LogicalTopInFlowThread());
    EXPECT_EQ(LayoutPoint(200, -100),
              FragmentAt(non_composited_child, 1).PaintOffset());
    EXPECT_EQ(LayoutPoint(200, -400),
              FragmentAt(non_composited_child, 1).PaginationOffset());
    EXPECT_EQ(LayoutUnit(400),
              FragmentAt(non_composited_child, 1).LogicalTopInFlowThread());
    EXPECT_EQ(1u, NumFragments(composited_child));
    EXPECT_EQ(LayoutPoint(200, 50),
              FragmentAt(composited_child, 0).PaintOffset());
    EXPECT_EQ(LayoutPoint(200, -400),
              FragmentAt(composited_child, 0).PaginationOffset());
    EXPECT_EQ(LayoutUnit(400),
              FragmentAt(composited_child, 0).LogicalTopInFlowThread());
  } else {
    // SPv1 forces single fragment for composited layers.
    EXPECT_EQ(1u, NumFragments(composited));
    EXPECT_EQ(LayoutPoint(100, 100), FragmentAt(composited, 0).PaintOffset());
    EXPECT_EQ(LayoutPoint(100, -200),
              FragmentAt(composited, 0).PaginationOffset());
    EXPECT_EQ(LayoutUnit(200),
              FragmentAt(composited, 0).LogicalTopInFlowThread());
    EXPECT_EQ(1u, NumFragments(non_composited_child));
    EXPECT_EQ(LayoutPoint(100, 100),
              FragmentAt(non_composited_child, 0).PaintOffset());
    EXPECT_EQ(LayoutPoint(100, -200),
              FragmentAt(non_composited_child, 0).PaginationOffset());
    EXPECT_EQ(LayoutUnit(200),
              FragmentAt(non_composited_child, 0).LogicalTopInFlowThread());
    EXPECT_EQ(1u, NumFragments(composited_child));
    EXPECT_EQ(LayoutPoint(100, 250),
              FragmentAt(composited_child, 0).PaintOffset());
    EXPECT_EQ(LayoutPoint(100, -200),
              FragmentAt(composited_child, 0).PaginationOffset());
    EXPECT_EQ(LayoutUnit(200),
              FragmentAt(composited_child, 0).LogicalTopInFlowThread());
  }
}

TEST_P(PaintPropertyTreeBuilderTest, FragmentsInPagedY) {
  SetBodyInnerHTML(R"HTML(
    <div id='paged' style='overflow: -webkit-paged-y; column-gap: 0;
        width: 100px; height: 100px'>
      <div id='content' style='height: 250px'></div>
    </div>
  )HTML");

  LayoutObject* content = GetLayoutObjectByElementId("content");
  EXPECT_EQ(3u, NumFragments(content));
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 0).PaginationOffset());
  EXPECT_EQ(LayoutUnit(), FragmentAt(content, 0).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 1).PaginationOffset());
  EXPECT_EQ(LayoutUnit(100), FragmentAt(content, 1).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 2).PaintOffset());
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 2).PaginationOffset());
  EXPECT_EQ(LayoutUnit(200), FragmentAt(content, 2).LogicalTopInFlowThread());
}

TEST_P(PaintPropertyTreeBuilderTest, FragmentsInPagedYWithGap) {
  SetBodyInnerHTML(R"HTML(
    <div id='paged' style='overflow: -webkit-paged-y; column-gap: 10px;
        width: 100px; height: 100px'>
      <div id='content' style='height: 250px'></div>
    </div>
  )HTML");

  LayoutObject* content = GetLayoutObjectByElementId("content");
  EXPECT_EQ(3u, NumFragments(content));
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 0).PaginationOffset());
  EXPECT_EQ(LayoutUnit(), FragmentAt(content, 0).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(0, 10), FragmentAt(content, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 10), FragmentAt(content, 1).PaginationOffset());
  EXPECT_EQ(LayoutUnit(100), FragmentAt(content, 1).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(0, 20), FragmentAt(content, 2).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 20), FragmentAt(content, 2).PaginationOffset());
  EXPECT_EQ(LayoutUnit(200), FragmentAt(content, 2).LogicalTopInFlowThread());
}

TEST_P(PaintPropertyTreeBuilderTest, FragmentsInPagedX) {
  SetBodyInnerHTML(R"HTML(
    <div id='paged' style='overflow: -webkit-paged-x; column-gap: 0;
        width: 100px; height: 100px'>
      <div id='content' style='height: 250px'></div>
    </div>
  )HTML");

  LayoutObject* content = GetLayoutObjectByElementId("content");
  EXPECT_EQ(3u, NumFragments(content));
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 0).PaginationOffset());
  EXPECT_EQ(LayoutUnit(), FragmentAt(content, 0).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(100, -100), FragmentAt(content, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(100, -100), FragmentAt(content, 1).PaginationOffset());
  EXPECT_EQ(LayoutUnit(100), FragmentAt(content, 1).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(200, -200), FragmentAt(content, 2).PaintOffset());
  EXPECT_EQ(LayoutPoint(200, -200), FragmentAt(content, 2).PaginationOffset());
  EXPECT_EQ(LayoutUnit(200), FragmentAt(content, 2).LogicalTopInFlowThread());
}

TEST_P(PaintPropertyTreeBuilderTest, FragmentsInPagedYVerticalRL) {
  SetBodyInnerHTML(R"HTML(
    <div id='paged' style='overflow: -webkit-paged-y; column-gap: 0;
        width: 100px; height: 100px; writing-mode: vertical-rl'>
      <div id='content' style='width: 250px'></div>
    </div>
  )HTML");

  LayoutObject* content = GetLayoutObjectByElementId("content");
  EXPECT_EQ(3u, NumFragments(content));
  EXPECT_EQ(LayoutPoint(-150, 0), FragmentAt(content, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(), FragmentAt(content, 0).PaginationOffset());
  EXPECT_EQ(LayoutUnit(), FragmentAt(content, 0).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(-50, 100), FragmentAt(content, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(100, 100), FragmentAt(content, 1).PaginationOffset());
  EXPECT_EQ(LayoutUnit(100), FragmentAt(content, 1).LogicalTopInFlowThread());
  EXPECT_EQ(LayoutPoint(50, 200), FragmentAt(content, 2).PaintOffset());
  EXPECT_EQ(LayoutPoint(200, 200), FragmentAt(content, 2).PaginationOffset());
  EXPECT_EQ(LayoutUnit(200), FragmentAt(content, 2).LogicalTopInFlowThread());
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
  GetDocument().View()->UpdateAllLifecyclePhases();

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
  GetDocument().View()->UpdateAllLifecyclePhases();

  // TODO(crbug.com/797779): Add code to verify fragments under the iframe.
}

TEST_P(PaintPropertyTreeBuilderTest,
       BecomingUnfragmentedClearsPaginationOffsetAndLogicalTopInFlowThread) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
         width: 30px; height: 20px; position: relative;
      }
    </style>
    <div style='columns: 2; height: 20px width: 400px'>
       <div style='height: 20px'></div>
       <div id=target></div>
     </div>
    </div>
  )HTML");

  LayoutObject* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(LayoutPoint(LayoutUnit(392.5f), LayoutUnit(-20)),
            target->FirstFragment().PaginationOffset());
  EXPECT_EQ(LayoutUnit(20), target->FirstFragment().LogicalTopInFlowThread());
  Element* target_element = GetDocument().getElementById("target");

  target_element->setAttribute(HTMLNames::styleAttr, "position: absolute");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutPoint(0, 0), target->FirstFragment().PaginationOffset());
  EXPECT_EQ(LayoutUnit(), target->FirstFragment().LogicalTopInFlowThread());
}

TEST_P(PaintPropertyTreeBuilderTest, Reflection) {
  SetBodyInnerHTML(
      "<div id='filter' style='-webkit-box-reflect: below; height:1000px;'>"
      "</div>");
  const ObjectPaintProperties* filter_properties =
      GetLayoutObjectByElementId("filter")->FirstFragment().PaintProperties();
  EXPECT_TRUE(filter_properties->Filter()->Parent()->IsRoot());
  EXPECT_EQ(DocScrollTranslation(),
            filter_properties->Filter()->LocalTransformSpace());
  EXPECT_EQ(DocContentClip(), filter_properties->Filter()->OutputClip());
  EXPECT_EQ(FloatPoint(8, 8), filter_properties->Filter()->FiltersOrigin());
}

TEST_P(PaintPropertyTreeBuilderTest, SimpleFilter) {
  SetBodyInnerHTML(
      "<div id='filter' style='filter:opacity(0.5); height:1000px;'>"
      "</div>");
  const ObjectPaintProperties* filter_properties =
      GetLayoutObjectByElementId("filter")->FirstFragment().PaintProperties();
  EXPECT_TRUE(filter_properties->Filter()->Parent()->IsRoot());
  EXPECT_EQ(DocScrollTranslation(),
            filter_properties->Filter()->LocalTransformSpace());
  EXPECT_EQ(DocContentClip(), filter_properties->Filter()->OutputClip());
  EXPECT_EQ(FloatPoint(8, 8), filter_properties->Filter()->FiltersOrigin());
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
            filter_properties->Filter()->LocalTransformSpace());
  EXPECT_EQ(FloatPoint(8, 8), filter_properties->Filter()->FiltersOrigin());

  const PropertyTreeState& child_paint_state =
      GetLayoutObjectByElementId("child")
          ->FirstFragment()
          .LocalBorderBoxProperties();

  // This will change once we added clip expansion node.
  EXPECT_EQ(filter_properties->Filter()->OutputClip(),
            child_paint_state.Clip());
  EXPECT_EQ(filter_properties->Filter(), child_paint_state.Effect());
}

TEST_P(PaintPropertyTreeBuilderTest, TransformOriginWithAndWithoutTransform) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0 }
      div {
        width: 400px;
        height: 100px;
      }
      #transform {
        transform: translate(100px, 200px);
        transform-origin: 75% 75% 0;
      }
      #willChange {
        will-change: opacity;
        transform-origin: 75% 75% 0;
      }
    </style>
    <div id='transform'></div>
    <div id='willChange'></div>
  )HTML");

  auto* transform = GetLayoutObjectByElementId("transform");
  EXPECT_EQ(
      TransformationMatrix().Translate3d(100, 200, 0),
      transform->FirstFragment().PaintProperties()->Transform()->Matrix());
  EXPECT_EQ(
      FloatPoint3D(300, 75, 0),
      transform->FirstFragment().PaintProperties()->Transform()->Origin());

  auto* will_change = GetLayoutObjectByElementId("willChange");
  EXPECT_EQ(
      TransformationMatrix().Translate3d(0, 0, 0),
      will_change->FirstFragment().PaintProperties()->Transform()->Matrix());
  EXPECT_EQ(
      FloatPoint3D(0, 0, 0),
      will_change->FirstFragment().PaintProperties()->Transform()->Origin());
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
        will-change: opacity;
        transform-origin: 50% 50% 0;
      }
    </style>
    <div id='motionPath'></div>
    <div id='willChange'></div>
  )HTML");

  auto* motion_path = GetLayoutObjectByElementId("motionPath");
  EXPECT_EQ(
      TransformationMatrix().Translate3d(50, 150, 0),
      motion_path->FirstFragment().PaintProperties()->Transform()->Matrix());
  EXPECT_EQ(
      FloatPoint3D(50, 50, 0),
      motion_path->FirstFragment().PaintProperties()->Transform()->Origin());

  auto* will_change = GetLayoutObjectByElementId("willChange");
  EXPECT_EQ(
      TransformationMatrix().Translate3d(0, 0, 0),
      will_change->FirstFragment().PaintProperties()->Transform()->Matrix());
  EXPECT_EQ(
      FloatPoint3D(0, 0, 0),
      will_change->FirstFragment().PaintProperties()->Transform()->Origin());
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
            descendant->FirstFragment().LocalBorderBoxProperties().Clip());

  ToElement(ancestor->GetNode())
      ->setAttribute(HTMLNames::styleAttr, "position: static");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_NE(ancestor->FirstFragment().PaintProperties()->OverflowClip(),
            descendant->FirstFragment().LocalBorderBoxProperties().Clip());
}

TEST_P(PaintPropertyTreeBuilderTest,
       TransformNodeNotAnimatedStillHasCompositorElementId) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML("<div id='target' style='transform: translateX(2em)'></div");
  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  EXPECT_TRUE(properties->Transform());
  EXPECT_NE(CompositorElementId(),
            properties->Transform()->GetCompositorElementId());
}

TEST_P(PaintPropertyTreeBuilderTest,
       EffectNodeNotAnimatedStillHasCompositorElementId) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  SetBodyInnerHTML("<div id='target' style='opacity: 0.5'></div");
  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  EXPECT_TRUE(properties->Effect());
  EXPECT_NE(CompositorElementId(),
            properties->Effect()->GetCompositorElementId());
}

TEST_P(PaintPropertyTreeBuilderTest,
       TransformNodeAnimatedHasCompositorElementId) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  LoadTestData("transform-animation.html");
  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  EXPECT_TRUE(properties->Transform());
  EXPECT_NE(CompositorElementId(),
            properties->Transform()->GetCompositorElementId());
  EXPECT_TRUE(properties->Transform()->RequiresCompositingForAnimation());
}

TEST_P(PaintPropertyTreeBuilderTest, EffectNodeAnimatedHasCompositorElementId) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

  LoadTestData("opacity-animation.html");
  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  EXPECT_TRUE(properties->Effect());
  EXPECT_NE(CompositorElementId(),
            properties->Effect()->GetCompositorElementId());
  EXPECT_TRUE(properties->Effect()->RequiresCompositingForAnimation());
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
  EXPECT_EQ(LayoutPoint(66, 55), target->FirstFragment().PaintOffset());
  EXPECT_EQ(effect,
            target->FirstFragment().LocalBorderBoxProperties().Effect());
}

TEST_P(PaintPropertyTreeBuilderTest, ScrollNodeHasCompositorElementId) {
  if (!RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

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

  LayoutBoxModelObject* clipper =
      ToLayoutBoxModelObject(GetLayoutObjectByElementId("clipper"));
  const ObjectPaintProperties* clip_properties =
      clipper->FirstFragment().PaintProperties();

  EXPECT_EQ(LayoutPoint(FloatPoint(31.5, 20)),
            clipper->FirstFragment().PaintOffset());
  // Result is pixel-snapped.
  EXPECT_EQ(FloatRect(32, 20, 400, 300),
            clip_properties->OverflowClip()->ClipRect().Rect());
}

TEST_P(PaintPropertyTreeBuilderTest, MaskSimple) {
  SetBodyInnerHTML(R"HTML(
    <div id='target' style='width:300px; height:200px;
        -webkit-mask:linear-gradient(red,red)'>
      Lorem ipsum
    </div>
  )HTML");

  const ObjectPaintProperties* properties = PaintPropertiesForElement("target");
  const ClipPaintPropertyNode* output_clip = properties->MaskClip();

  const auto* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(output_clip,
            target->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(DocContentClip(), output_clip->Parent());
  EXPECT_EQ(FloatRoundedRect(8, 8, 300, 200), output_clip->ClipRect());

  EXPECT_EQ(properties->Effect(),
            target->FirstFragment().LocalBorderBoxProperties().Effect());
  EXPECT_TRUE(properties->Effect()->Parent()->IsRoot());
  EXPECT_EQ(SkBlendMode::kSrcOver, properties->Effect()->BlendMode());
  EXPECT_EQ(output_clip, properties->Effect()->OutputClip());

  EXPECT_EQ(properties->Effect(), properties->Mask()->Parent());
  EXPECT_EQ(SkBlendMode::kDstIn, properties->Mask()->BlendMode());
  EXPECT_EQ(output_clip, properties->Mask()->OutputClip());
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
  const ClipPaintPropertyNode* output_clip = properties->MaskClip();

  const auto* target = GetLayoutObjectByElementId("target");
  EXPECT_EQ(output_clip,
            target->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(DocContentClip(), output_clip->Parent());
  EXPECT_EQ(FloatRoundedRect(-12, -2, 340, 220), output_clip->ClipRect());

  EXPECT_EQ(properties->Effect(),
            target->FirstFragment().LocalBorderBoxProperties().Effect());
  EXPECT_TRUE(properties->Effect()->Parent()->IsRoot());
  EXPECT_EQ(SkBlendMode::kSrcOver, properties->Effect()->BlendMode());
  EXPECT_EQ(output_clip, properties->Effect()->OutputClip());

  EXPECT_EQ(properties->Effect(), properties->Mask()->Parent());
  EXPECT_EQ(SkBlendMode::kDstIn, properties->Mask()->BlendMode());
  EXPECT_EQ(output_clip, properties->Mask()->OutputClip());
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
  const ClipPaintPropertyNode* overflow_clip1 =
      target_properties->MaskClip()->Parent();
  const ClipPaintPropertyNode* mask_clip = target_properties->MaskClip();
  const ClipPaintPropertyNode* overflow_clip2 =
      target_properties->OverflowClip();
  const auto* target = GetLayoutObjectByElementId("target");
  const TransformPaintPropertyNode* scroll_translation =
      target->FirstFragment().LocalBorderBoxProperties().Transform();

  const ObjectPaintProperties* scroll_properties =
      PaintPropertiesForElement("scroll");

  EXPECT_EQ(DocContentClip(), overflow_clip1->Parent());
  EXPECT_EQ(FloatRoundedRect(0, 0, 300, 200), overflow_clip1->ClipRect());
  EXPECT_EQ(scroll_properties->PaintOffsetTranslation(),
            overflow_clip1->LocalTransformSpace());

  EXPECT_EQ(mask_clip,
            target->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(overflow_clip1, mask_clip->Parent());
  EXPECT_EQ(FloatRoundedRect(0, 0, 220, 320), mask_clip->ClipRect());
  EXPECT_EQ(scroll_translation, mask_clip->LocalTransformSpace());

  EXPECT_EQ(mask_clip, overflow_clip2->Parent());
  EXPECT_EQ(FloatRoundedRect(10, 10, 200, 300), overflow_clip2->ClipRect());
  EXPECT_EQ(scroll_translation, overflow_clip2->LocalTransformSpace());

  EXPECT_EQ(target_properties->Effect(),
            target->FirstFragment().LocalBorderBoxProperties().Effect());
  EXPECT_TRUE(target_properties->Effect()->Parent()->IsRoot());
  EXPECT_EQ(SkBlendMode::kSrcOver, target_properties->Effect()->BlendMode());
  EXPECT_EQ(mask_clip, target_properties->Effect()->OutputClip());

  EXPECT_EQ(target_properties->Effect(), target_properties->Mask()->Parent());
  EXPECT_EQ(SkBlendMode::kDstIn, target_properties->Mask()->BlendMode());
  EXPECT_EQ(mask_clip, target_properties->Mask()->OutputClip());

  const auto* absolute = GetLayoutObjectByElementId("absolute");
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(),
              absolute->FirstFragment().LocalBorderBoxProperties().Transform());
  } else {
    // For SPv1, |absolute| is composited so we created PaintOffsetTranslation.
    EXPECT_EQ(
        absolute->FirstFragment().PaintProperties()->PaintOffsetTranslation(),
        absolute->FirstFragment().LocalBorderBoxProperties().Transform());
  }
  EXPECT_EQ(mask_clip,
            absolute->FirstFragment().LocalBorderBoxProperties().Clip());
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
  const ClipPaintPropertyNode* output_clip = properties->MaskClip();
  const auto* target = GetLayoutObjectByElementId("target");

  EXPECT_EQ(output_clip,
            target->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(DocContentClip(), output_clip->Parent());
  EXPECT_EQ(FloatRoundedRect(104, 21, 432, 16), output_clip->ClipRect());

  EXPECT_EQ(properties->Effect(),
            target->FirstFragment().LocalBorderBoxProperties().Effect());
  EXPECT_TRUE(properties->Effect()->Parent()->IsRoot());
  EXPECT_EQ(SkBlendMode::kSrcOver, properties->Effect()->BlendMode());
  EXPECT_EQ(output_clip, properties->Effect()->OutputClip());

  EXPECT_EQ(properties->Effect(), properties->Mask()->Parent());
  EXPECT_EQ(SkBlendMode::kDstIn, properties->Mask()->BlendMode());
  EXPECT_EQ(output_clip, properties->Mask()->OutputClip());

  const auto* overflowing = GetLayoutObjectByElementId("overflowing");
  EXPECT_EQ(output_clip,
            overflowing->FirstFragment().LocalBorderBoxProperties().Clip());
  EXPECT_EQ(properties->Effect(),
            overflowing->FirstFragment().LocalBorderBoxProperties().Effect());
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

  Element* scroller = GetDocument().getElementById("scroller");
  scroller->setScrollTop(42);

  GetDocument().View()->UpdateAllLifecyclePhases();

  const ObjectPaintProperties* scroll_properties =
      scroller->GetLayoutObject()->FirstFragment().PaintProperties();
  // Because the frameView is does not scroll, overflowHidden's scroll should be
  // under the root.
  auto* scroll_translation = scroll_properties->ScrollTranslation();
  auto* paint_offset_translation = scroll_properties->PaintOffsetTranslation();
  auto* scroll_node = scroll_translation->ScrollNode();
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(GetDocument()
                  .GetPage()
                  ->GetVisualViewport()
                  .GetScrollTranslationNode()
                  ->ScrollNode(),
              scroll_node->Parent());
  } else {
    EXPECT_EQ(DocScroll(), scroll_node->Parent());
  }
  EXPECT_EQ(TransformationMatrix().Translate(0, -42),
            scroll_translation->Matrix());
  // The paint offset node should be offset by the margin.
  EXPECT_EQ(FloatSize(7, 11),
            paint_offset_translation->Matrix().To2DTranslation());
  // And the scroll node should not.
  EXPECT_EQ(IntRect(0, 0, 100, 100), scroll_node->ContainerRect());

  scroller->setAttribute(HTMLNames::styleAttr, "border: 20px solid black;");
  GetDocument().View()->UpdateAllLifecyclePhases();
  // The paint offset node should be offset by the margin.
  EXPECT_EQ(FloatSize(7, 11),
            paint_offset_translation->Matrix().To2DTranslation());
  // The scroll node should be offset by the border.
  EXPECT_EQ(IntRect(20, 20, 100, 100), scroll_node->ContainerRect());

  scroller->setAttribute(HTMLNames::styleAttr,
                         "border: 20px solid black;"
                         "transform: translate(20px, 30px);");
  GetDocument().View()->UpdateAllLifecyclePhases();
  // The scroll node's offset should not include margin if it has already been
  // included in a paint offset node.
  EXPECT_EQ(IntRect(20, 20, 100, 100), scroll_node->ContainerRect());
  EXPECT_EQ(TransformationMatrix().Translate(7, 11),
            scroll_properties->PaintOffsetTranslation()->Matrix());
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
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(nullptr, paint_offset_translation);
    EXPECT_EQ(LayoutPoint(60, 50), target->FirstFragment().PaintOffset());
  } else {
    // For SPv1, |target| is composited so we created PaintOffsetTranslation.
    ASSERT_NE(nullptr, paint_offset_translation);
    EXPECT_EQ(TransformationMatrix().Translate(60, 50),
              paint_offset_translation->Matrix());
    EXPECT_EQ(TransformPaintPropertyNode::BackfaceVisibility::kInherited,
              paint_offset_translation->GetBackfaceVisibility());
  }

  const auto* transform = target_properties->Transform();
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
      RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled()) {
    ASSERT_NE(nullptr, transform);
    EXPECT_TRUE(transform->Matrix().IsIdentity());
    EXPECT_EQ(TransformPaintPropertyNode::BackfaceVisibility::kHidden,
              transform->GetBackfaceVisibility());
  } else {
    EXPECT_EQ(nullptr, transform);
  }

  ToElement(target->GetNode())->setAttribute(HTMLNames::styleAttr, "");
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(LayoutPoint(60, 50), target->FirstFragment().PaintOffset());
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
      }
    </style>
    <iframe id='iframe'></iframe>
  )HTML");

  const auto* properties = PaintPropertiesForElement("iframe");
  const auto* border_radius_clip = properties->OverflowClip();
  ASSERT_NE(nullptr, border_radius_clip);
  FloatSize radius(30, 30);
  EXPECT_EQ(FloatRoundedRect(FloatRect(28, 28, 200, 200), radius, radius,
                             radius, radius),
            border_radius_clip->ClipRect());
  EXPECT_EQ(DocContentClip(), border_radius_clip->Parent());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(), border_radius_clip->LocalTransformSpace());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              border_radius_clip->LocalTransformSpace());
  }
  EXPECT_EQ(nullptr, properties->InnerBorderRadiusClip());
}

TEST_P(PaintPropertyTreeBuilderTest, NoPropertyForSVGTextWithReflection) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <text id='target' style='-webkit-box-reflect: below 2px'>X</text>
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
  const auto* border_radius_clip = properties->OverflowClip();
  ASSERT_NE(nullptr, border_radius_clip);
  FloatSize radius(20, 20);
  EXPECT_EQ(FloatRoundedRect(FloatRect(18, 18, 50, 50), radius, radius, radius,
                             radius),
            border_radius_clip->ClipRect());
  EXPECT_EQ(DocContentClip(), border_radius_clip->Parent());
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(DocPreTranslation(), border_radius_clip->LocalTransformSpace());
  } else {
    EXPECT_EQ(DocScrollTranslation(),
              border_radius_clip->LocalTransformSpace());
  }
  EXPECT_EQ(nullptr, properties->InnerBorderRadiusClip());
}

TEST_P(PaintPropertyTreeBuilderTest, FrameClipWhenPrinting) {
  SetBodyInnerHTML("<iframe></iframe>");
  SetChildFrameHTML("");
  GetDocument().View()->UpdateAllLifecyclePhases();

  // When not printing, both main and child frame views have content clip.
  auto* const main_frame_doc = &GetDocument();
  auto* const child_frame_doc = &ChildDocument();
  ASSERT_NE(nullptr, DocContentClip(main_frame_doc));
  EXPECT_NE(FloatRect(LayoutRect::InfiniteIntRect()),
            DocContentClip(main_frame_doc)->ClipRect().Rect());
  ASSERT_NE(nullptr, DocContentClip(child_frame_doc));
  EXPECT_NE(FloatRect(LayoutRect::InfiniteIntRect()),
            DocContentClip(child_frame_doc)->ClipRect().Rect());

  // When the main frame is printing, it should not have content clip.
  FloatSize page_size(100, 100);
  GetFrame().StartPrinting(page_size, page_size, 1);
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();
  EXPECT_EQ(nullptr, DocContentClip(main_frame_doc));
  ASSERT_NE(nullptr, DocContentClip(child_frame_doc));
  EXPECT_NE(FloatRect(LayoutRect::InfiniteIntRect()),
            DocContentClip(child_frame_doc)->ClipRect().Rect());

  GetFrame().EndPrinting();
  GetDocument().View()->UpdateAllLifecyclePhases();

  // When only the child frame is printing, it should not have content clip but
  // the main frame still have (which doesn't matter though).
  ChildFrame().StartPrinting(page_size, page_size, 1);
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();
  ASSERT_NE(nullptr, DocContentClip(main_frame_doc));
  EXPECT_NE(FloatRect(LayoutRect::InfiniteIntRect()),
            DocContentClip(main_frame_doc)->ClipRect().Rect());
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
  EXPECT_EQ(FloatRect(0, 0, 5, 50), overflow_controls_clip->ClipRect().Rect());

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
  EXPECT_EQ(FloatRect(0, 0, 6, 50), overflow_controls_clip->ClipRect().Rect());

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
  const auto& first_fragment = content->FirstFragment();
  const auto* second_fragment = first_fragment.NextFragment();
  ASSERT_NE(nullptr, second_fragment);

  EXPECT_EQ(LayoutPoint(), first_fragment.PaintOffset());
  EXPECT_EQ(LayoutPoint(390, -10), second_fragment->PaintOffset());
  EXPECT_EQ(LayoutRect(0, 0, 20, 20), first_fragment.VisualRect());
  EXPECT_EQ(LayoutRect(390, -10, 20, 20), second_fragment->VisualRect());
}

// The following test tests that we restrict actual column count, to not run
// into unnecessary performance problems. The code that applies this limitation
// is in MultiColumnFragmentainerGroup::ActualColumnCount().
TEST_P(PaintPropertyTreeBuilderTest, ShortColumnTallContent) {
  SetBodyInnerHTML(R"HTML(
    <div id="multicol" style="columns:3; column-gap:1px; width:101px; height:1px;">
      <div style="height:1000000px;"></div>
    </div>
  )HTML");

  const auto* flow_thread =
      GetLayoutObjectByElementId("multicol")->SlowFirstChild();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());
  EXPECT_EQ(10u, NumFragments(flow_thread));
}

TEST_P(PaintPropertyTreeBuilderTest, FragmentClipPixelSnapped) {
  SetBodyInnerHTML(R"HTML(
    <div id="container" style="columns: 2; column-gap: 0; width: 49.5px">
      <div style="height: 99px"></div>
    </div>
  )HTML");

  const auto* flow_thread =
      GetLayoutObjectByElementId("container")->SlowFirstChild();
  ASSERT_TRUE(flow_thread->IsLayoutFlowThread());
  ASSERT_EQ(2u, NumFragments(flow_thread));
  const auto* first_clip =
      FragmentAt(flow_thread, 0).PaintProperties()->FragmentClip();
  const auto* second_clip =
      FragmentAt(flow_thread, 1).PaintProperties()->FragmentClip();

  EXPECT_EQ(FloatRect(-999992, -999992, 2000000, 1000050),
            first_clip->ClipRect().Rect());
  EXPECT_EQ(FloatRect(-999967, 8, 2000000, 999951),
            second_clip->ClipRect().Rect());
}

TEST_P(PaintPropertyTreeBuilderTest,
       UpdateUnderChangedEffectUnderCompositedLayer) {
  SetBodyInnerHTML(R"HTML(
    <div id="opacity" style="isolation: isolate; width: 100px: height: 100px">
      <div id="target"
          style="will-change: transform; width: 100px: height: 100px">
      </div>
    </div>
  )HTML");

  Element* opacity_element = GetDocument().getElementById("opacity");
  const auto* target = GetLayoutObjectByElementId("target");

  EXPECT_FALSE(ToLayoutBoxModelObject(target)->Layer()->NeedsRepaint());

  opacity_element->setAttribute(HTMLNames::styleAttr, "opacity: 0.5");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  // All paint chunks contained by the new opacity effect node need to be
  // re-painted.
  EXPECT_TRUE(ToLayoutBoxModelObject(target)->Layer()->NeedsRepaint());
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

  const LayoutSVGRoot& root =
      *ToLayoutSVGRoot(GetLayoutObjectByElementId("svg"));
  EXPECT_TRUE(root.FirstFragment().PaintProperties()->Mask());
}

TEST_P(PaintPropertyTreeBuilderTest, SVGRootWithCSSMask) {
  SetBodyInnerHTML(R"HTML(
    <svg id="svg" width="16" height="16" style="-webkit-mask-image: url(fake);">
    </svg>
  )HTML");

  const LayoutSVGRoot& root =
      *ToLayoutSVGRoot(GetLayoutObjectByElementId("svg"));
  EXPECT_TRUE(root.FirstFragment().PaintProperties()->Mask());
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
    EXPECT_TRUE(rect->FirstFragment().PaintProperties()->ClipPath());
  }

  Element* clip = GetDocument().getElementById("clip");
  ASSERT_TRUE(clip);
  clip->remove();
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();

  {
    const auto* rect = GetLayoutObjectByElementId("rect");
    ASSERT_TRUE(rect);
    EXPECT_FALSE(rect->FirstFragment().PaintProperties()->MaskClip());
    EXPECT_FALSE(rect->FirstFragment().PaintProperties()->ClipPath());
  }
}

TEST_P(PaintPropertyTreeBuilderTest, RootHasCompositedScrolling) {
  SetBodyInnerHTML(R"HTML(
    <div id='forceScroll' style='height: 2000px'></div>
  )HTML");

  // When the root scrolls, there should be direct compositing reasons.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    EXPECT_TRUE(DocScrollTranslation()->HasDirectCompositingReasons());

  // Remove scrolling from the root.
  Element* force_scroll_element = GetDocument().getElementById("forceScroll");
  force_scroll_element->setAttribute(HTMLNames::styleAttr, "");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  // TODO(crbug.com/732611): SPv2 invalidations are incorrect if there is
  // scrolling.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_FALSE(DocScrollTranslation());
  } else {
    // Always create scroll translation for layout view even the document does
    // not scroll (not enough content).
    EXPECT_TRUE(DocScrollTranslation());
  }
}

TEST_P(PaintPropertyTreeBuilderTest, IframeDoesNotRequireCompositedScrolling) {
  SetBodyInnerHTML(R"HTML(
    <iframe style='width: 200px; height: 200px;'></iframe>
    <div id='forceScroll' style='height: 2000px'></div>
  )HTML");
  SetChildFrameHTML(R"HTML(
    <div id='forceInnerScroll' style='height: 2000px'></div>
  )HTML");
  GetDocument().View()->UpdateAllLifecyclePhases();

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    EXPECT_TRUE(DocScrollTranslation()->HasDirectCompositingReasons());

  // When the child iframe scrolls, there should not be direct compositing
  // reasons because only the root frame needs scrolling compositing reasons.
  EXPECT_FALSE(
      DocScrollTranslation(&ChildDocument())->HasDirectCompositingReasons());
}

TEST_P(PaintPropertyTreeBuilderTest,
       NoTransformPropertyForWillChangeWithoutLayer) {
  SetBodyInnerHTML("<svg id='target' style='will-change: left'></svg>");
  EXPECT_EQ(nullptr, PaintPropertiesForElement("target")->Transform());
}

TEST_P(PaintPropertyTreeBuilderTest, OmitOverflowClip) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .sized-container { width: 100px; height: 100px }
      .small-content { width: 50px; height: 50px }
      .big-content { width: 200px; height: 200px }
    </style>
    <div id="auto-size" style="overflow: hidden">
      <div class="small-content"></div>
    </div>
    <div id="overflow-hidden-no-overflow" class="sized-container"
        style="overflow: hidden">
      <div class="small-content"></div>
    </div>
    <div id="overflow-hidden-overflow" class="sized-container"
        style="overflow: hidden">
      <div class="big-content"></div>
    </div>
    <div id="contain-paint-no-overflow" class="sized-container"
        style="contain: paint">
      <div class="small-content"></div>
    </div>
    <div id="contain-paint-overflow" class="sized-container"
        style="contain: paint">
      <div class="big-content"></div>
    </div>
    <div id="has-self-painting-descendant" class="sized-container"
        style="overflow: hidden">
      <div class="small-content" style="position: relative; left: 100px"></div>
    </div>
    <div id="overflow-auto-no-overflow" class="sized-container"
        style="overflow: auto">
      <div class="small-content"></div>
    </div>
    <div id="overflow-auto-overflow" class="sized-container"
        style="overflow: auto">
      <div class="big-content"></div>
    </div>
    <input id="button" type="button" value="button">
  )HTML");
  CHECK(GetDocument().GetPage()->GetScrollbarTheme().UsesOverlayScrollbars());

  EXPECT_FALSE(PaintPropertiesForElement("auto-size")->OverflowClip());
  EXPECT_FALSE(
      PaintPropertiesForElement("overflow-hidden-no-overflow")->OverflowClip());
  EXPECT_TRUE(
      PaintPropertiesForElement("overflow-hidden-overflow")->OverflowClip());
  EXPECT_FALSE(
      PaintPropertiesForElement("contain-paint-no-overflow")->OverflowClip());
  EXPECT_TRUE(
      PaintPropertiesForElement("contain-paint-overflow")->OverflowClip());
  EXPECT_TRUE(PaintPropertiesForElement("has-self-painting-descendant")
                  ->OverflowClip());
  EXPECT_FALSE(
      PaintPropertiesForElement("overflow-auto-no-overflow")->OverflowClip());
  EXPECT_TRUE(
      PaintPropertiesForElement("overflow-auto-overflow")->OverflowClip());
  EXPECT_TRUE(PaintPropertiesForElement("button")->OverflowClip());
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
  GetDocument().View()->UpdateAllLifecyclePhases();

  auto* child_layer = ToLayoutBox(GetLayoutObjectByElementId("child"))->Layer();
  EXPECT_FALSE(child_layer->NeedsRepaint());

  GetDocument().body()->setAttribute(HTMLNames::classAttr, "noscrollbars");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(child_layer->NeedsRepaint());
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

  auto* child = ToLayoutBox(GetLayoutObjectByElementId("child"));
  const auto* old_clip_state =
      child->FirstFragment().LocalBorderBoxProperties().Clip();

  child->SetNeedsPaintPropertyUpdate();
  GetDocument().View()->UpdateAllLifecyclePhases();

  const auto* new_clip_state =
      child->FirstFragment().LocalBorderBoxProperties().Clip();
  EXPECT_EQ(old_clip_state, new_clip_state);
}

TEST_P(PaintPropertyTreeBuilderTest, CompositedLayerSkipsFragmentClip) {
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;

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
            GetLayoutObjectByElementId("child-clipped")
                ->FirstFragment()
                .LocalBorderBoxProperties()
                .Clip());

  EXPECT_EQ(DocContentClip(),
            GetLayoutObjectByElementId("composited-without-clip")
                ->FirstFragment()
                .LocalBorderBoxProperties()
                .Clip());
  EXPECT_EQ(DocContentClip(), GetLayoutObjectByElementId("child-unclipped")
                                  ->FirstFragment()
                                  .LocalBorderBoxProperties()
                                  .Clip());
}

TEST_P(PaintPropertyTreeBuilderTest, RepeatingFixedPositionInPagedMedia) {
  SetBodyInnerHTML(R"HTML(
    <div id="fixed" style="position: fixed; top: 20px; left: 20px">
      <div id="fixed-child" style="position: relative; top: 10px"></div>
    </div>
    <div id="normal" style="height: 1000px"></div>
  )HTML");
  GetDocument().domWindow()->scrollTo(0, 200);
  GetDocument().View()->UpdateAllLifecyclePhases();

  const auto* fixed = GetLayoutObjectByElementId("fixed");
  EXPECT_FALSE(fixed->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(1u, NumFragments(fixed));

  const auto* fixed_child = GetLayoutObjectByElementId("fixed-child");
  EXPECT_FALSE(fixed_child->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(1u, NumFragments(fixed_child));

  const auto* normal = GetLayoutObjectByElementId("normal");
  EXPECT_FALSE(normal->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(1u, NumFragments(normal));

  FloatSize page_size(300, 400);
  GetFrame().StartPrinting(page_size, page_size, 1);
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();

  // "fixed" should create fragments to repeat in each printed page.
  EXPECT_TRUE(fixed->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(3u, NumFragments(fixed));
  for (int i = 0; i < 3; i++) {
    const auto& fragment = FragmentAt(fixed, i);
    EXPECT_EQ(LayoutPoint(20, -180 + i * 400), fragment.PaintOffset());
    EXPECT_EQ(LayoutUnit(400 * i), fragment.LogicalTopInFlowThread());
  }

  EXPECT_FALSE(fixed_child->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(3u, NumFragments(fixed_child));
  for (int i = 0; i < 3; i++) {
    const auto& fragment = FragmentAt(fixed_child, i);
    EXPECT_EQ(LayoutPoint(20, -170 + i * 400), fragment.PaintOffset());
    EXPECT_EQ(LayoutUnit(i * 400), fragment.LogicalTopInFlowThread());
  }

  EXPECT_FALSE(normal->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(1u, NumFragments(normal));

  GetFrame().EndPrinting();
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(1u, NumFragments(fixed));
  EXPECT_FALSE(fixed_child->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(1u, NumFragments(fixed_child));
  EXPECT_FALSE(normal->IsFixedPositionObjectInPagedMedia());
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
  GetDocument().View()->UpdateAllLifecyclePhases();

  const auto* fixed = GetLayoutObjectByElementId("fixed");
  EXPECT_FALSE(fixed->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(1u, NumFragments(fixed));

  const auto* fixed_child = GetLayoutObjectByElementId("fixed-child");
  EXPECT_FALSE(fixed_child->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(1u, NumFragments(fixed_child));

  FloatSize page_size(300, 400);
  GetFrame().StartPrinting(page_size, page_size, 1);
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();

  // "fixed" should create fragments to repeat in each printed page.
  EXPECT_TRUE(fixed->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(3u, NumFragments(fixed));
  for (int i = 0; i < 3; i++) {
    const auto& fragment = FragmentAt(fixed, i);
    EXPECT_EQ(LayoutPoint(), fragment.PaintOffset());
    EXPECT_EQ(LayoutUnit(i * 400), fragment.LogicalTopInFlowThread());
    const auto* properties = fragment.PaintProperties();
    EXPECT_EQ(TransformationMatrix().Translate(20, -180 + i * 400),
              properties->PaintOffsetTranslation()->Matrix());
    EXPECT_EQ(TransformationMatrix().Translate(10, 0),
              properties->Transform()->Matrix());
    EXPECT_EQ(properties->PaintOffsetTranslation(),
              properties->Transform()->Parent());
  }

  EXPECT_FALSE(fixed_child->IsFixedPositionObjectInPagedMedia());
  for (int i = 0; i < 3; i++) {
    const auto& fragment = FragmentAt(fixed_child, i);
    EXPECT_EQ(LayoutPoint(0, 10), fragment.PaintOffset());
    EXPECT_EQ(LayoutUnit(i * 400), fragment.LogicalTopInFlowThread());
    EXPECT_EQ(FragmentAt(fixed, i).PaintProperties()->Transform(),
              fragment.LocalBorderBoxProperties().Transform());
  }

  GetFrame().EndPrinting();
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_EQ(1u, NumFragments(fixed));
  EXPECT_FALSE(fixed_child->IsFixedPositionObjectInPagedMedia());
  EXPECT_EQ(1u, NumFragments(fixed_child));
}

TEST_P(PaintPropertyTreeBuilderTest, RepeatingTableSectionInPagedMedia) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      tr { height: 100px; }
      div { height: 500px; }
    </style>
    <div></div>
    <table style="border-spacing: 0">
      <thead id="head"><tr><th>Header</th></tr></thead>
      <tbody>
        <tr><td></td></tr>
        <tr><td></td></tr>
        <tr><td></td></tr>
        <tr><td></td></tr>
      </tbody>
      <tfoot id="foot"><tr><th>Footer</th></tr></tfoot>
    </table>
    <div></div>
  )HTML");

  const auto* head = ToLayoutTableSection(GetLayoutObjectByElementId("head"));
  const auto* foot = ToLayoutTableSection(GetLayoutObjectByElementId("foot"));
  EXPECT_FALSE(head->IsRepeatingHeaderGroup());
  EXPECT_EQ(1u, NumFragments(head));
  EXPECT_EQ(1u, NumFragments(head->FirstRow()));
  EXPECT_EQ(1u, NumFragments(head->FirstRow()->FirstCell()));
  EXPECT_FALSE(foot->IsRepeatingFooterGroup());
  EXPECT_EQ(1u, NumFragments(foot));
  EXPECT_EQ(1u, NumFragments(foot->FirstRow()));
  EXPECT_EQ(1u, NumFragments(foot->FirstRow()->FirstCell()));

  FloatSize page_size(300, 400);
  GetFrame().StartPrinting(page_size, page_size, 1);
  GetDocument().View()->UpdateLifecyclePhasesForPrinting();

  // "fixed" should create fragments to repeat in each printed page.
  EXPECT_TRUE(head->IsRepeatingHeaderGroup());
  EXPECT_TRUE(foot->IsRepeatingFooterGroup());
  auto check_fragments = [&](const LayoutObject* object) {
    ASSERT_EQ(3u, NumFragments(object));
    for (int i = 0; i < 3; i++) {
      EXPECT_EQ(LayoutUnit((i + 1) * 400),
                FragmentAt(object, i).LogicalTopInFlowThread());
    }
  };
  check_fragments(head);
  check_fragments(head->FirstRow());
  check_fragments(head->FirstRow()->FirstCell());
  check_fragments(foot);
  check_fragments(foot->FirstRow());
  check_fragments(foot->FirstRow());

  // The first header is at its normal flow location (0, 100px) in its page.
  // The other repeated ones are at the top of the their pages.
  EXPECT_EQ(LayoutPoint(0, 500), FragmentAt(head, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 800), FragmentAt(head, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 1200), FragmentAt(head, 2).PaintOffset());
  // The last footer is at its normal flow location (0, 200px) in its page.
  // The other repeated ones are at the bottom of their pages.
  EXPECT_EQ(LayoutPoint(0, 700), FragmentAt(foot, 0).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 1100), FragmentAt(foot, 1).PaintOffset());
  EXPECT_EQ(LayoutPoint(0, 1400), FragmentAt(foot, 2).PaintOffset());

  const auto& painting_layer_object = head->PaintingLayer()->GetLayoutObject();
  ASSERT_EQ(1u, NumFragments(&painting_layer_object));

  GetFrame().EndPrinting();
  GetDocument().View()->UpdateAllLifecyclePhases();
  EXPECT_FALSE(head->IsRepeatingHeaderGroup());
  EXPECT_EQ(1u, NumFragments(head));
  EXPECT_EQ(1u, NumFragments(head->FirstRow()));
  EXPECT_EQ(1u, NumFragments(head->FirstRow()->FirstCell()));
  EXPECT_FALSE(foot->IsRepeatingFooterGroup());
  EXPECT_EQ(1u, NumFragments(foot));
  EXPECT_EQ(1u, NumFragments(foot->FirstRow()));
  EXPECT_EQ(1u, NumFragments(foot->FirstRow()->FirstCell()));
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
  EXPECT_EQ(LayoutPoint(0, 0), paint_offset("float-left"));
  EXPECT_EQ(LayoutPoint(85, 100), paint_offset("float-right"));
  EXPECT_EQ(LayoutPoint(15, 0), paint_offset("float-left-rtl"));
  EXPECT_EQ(LayoutPoint(100, 100), paint_offset("float-right-rtl"));
  EXPECT_EQ(LayoutPoint(100, 0), paint_offset("float-left-vrl"));
  EXPECT_EQ(LayoutPoint(0, 85), paint_offset("float-right-vrl"));
  EXPECT_EQ(LayoutPoint(100, 0), paint_offset("float-left-rtl-vrl"));
  EXPECT_EQ(LayoutPoint(0, 85), paint_offset("float-right-rtl-vrl"));
  EXPECT_EQ(LayoutPoint(0, 0), paint_offset("float-left-vlr"));
  EXPECT_EQ(LayoutPoint(100, 85), paint_offset("float-right-vlr"));
  EXPECT_EQ(LayoutPoint(0, 0), paint_offset("float-left-rtl-vlr"));
  EXPECT_EQ(LayoutPoint(100, 85), paint_offset("float-right-rtl-vlr"));
}

TEST_P(PaintPropertyTreeBuilderTest, ClipInvalidationForReplacedElement) {
  // Non-composited LayoutImage has a micro-optimization to embed object-fit
  // and clip to the drawing, thus not creating nodes.
  // SPv2 makes everything non-composited essentially.
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled())
    return;
  // This test verifies clip nodes are correctly updated in response to
  // content box mutation.
  SetBodyInnerHTML(R"HTML(
    <style>
    img {
      box-sizing: border-box;
      width: 8px;
      height: 8px;
      object-fit: none;
      will-change: transform;
    }
    </style>
    <!-- An image of 10x10 white pixels. -->
    <img id="target" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAoAA
        AAKCAIAAAACUFjqAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4gcVABQvx8CBmA
        AAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUHAAAAFUlEQVQY02P
        8//8/A27AxIAXjFRpAKXjAxH/0Dm5AAAAAElFTkSuQmCC"/>
  )HTML");

  {
    const auto* properties = PaintPropertiesForElement("target");
    ASSERT_TRUE(properties);
    ASSERT_TRUE(properties->OverflowClip());
    EXPECT_EQ(FloatRect(0, 0, 8, 8),
              properties->OverflowClip()->ClipRect().Rect());
  }

  GetDocument().getElementById("target")->setAttribute(
      HTMLNames::styleAttr, "padding: 1px 2px 3px 4px;");
  GetDocument().View()->UpdateAllLifecyclePhases();

  {
    const auto* properties = PaintPropertiesForElement("target");
    ASSERT_TRUE(properties);
    ASSERT_TRUE(properties->OverflowClip());
    EXPECT_EQ(FloatRect(4, 1, 2, 4),
              properties->OverflowClip()->ClipRect().Rect());
  }
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
  EXPECT_EQ(IntRect(0, 0, 200, 200), scroll_node->ContainerRect());
  EXPECT_EQ(IntSize(1000, 200), scroll_node->ContentsSize());
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
                             .getElementById("selection")
                             ->GetLayoutObject()
                             ->FirstFragment();

  EXPECT_TRUE(fragment.PaintProperties());
  EXPECT_TRUE(fragment.PaintProperties()->OverflowClip());
  ASSERT_TRUE(fragment.HasLocalBorderBoxProperties());
  EXPECT_EQ(fragment.ContentsProperties().Clip(),
            fragment.PaintProperties()->OverflowClip());
}

TEST_P(PaintPropertyTreeBuilderTest, SkipEmptyClipFragments) {
  SetBodyInnerHTML(R"HTML(
    <!doctype HTML>
    <style>h4 { column-span: all; }</style>
    <div id="container" style="columns:1;">
      lorem
      <h4>hi</h4>
      <div><h4>hello</h4></div>
      ipsum
    </div>
  )HTML");

  const auto* flow_thread = GetDocument()
                                .getElementById("container")
                                ->GetLayoutObject()
                                ->SlowFirstChild();
  EXPECT_TRUE(flow_thread->IsLayoutFlowThread());
  EXPECT_TRUE(ToLayoutFlowThread(flow_thread)->IsLayoutMultiColumnFlowThread());

  // FragmentainerIterator would return 3 things:
  // 1. A fragment that contains "lorem" and is interrupted by the first h4,
  //    since it's column-span: all.
  // 2. A fragment that starts at the inner div of height 0 and is immediately
  //    interrupted by a nested h4.
  // 3. A fragment that contains "ipsum".
  //
  // The second fragment would have an empty clip and the same logical top as
  // the third fragment. This test ensures that this fragment is not present in
  // the LayoutMultiColumnFlowThread's fragments.
  EXPECT_EQ(2u, NumFragments(flow_thread));
  EXPECT_NE(
      flow_thread->FirstFragment().LogicalTopInFlowThread(),
      flow_thread->FirstFragment().NextFragment()->LogicalTopInFlowThread());
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
  GetDocument().getElementById("scroller")->setScrollTop(50);
  GetDocument().View()->UpdateAllLifecyclePhases();

  const auto* outer_properties = PaintPropertiesForElement("outer");
  ASSERT_TRUE(outer_properties && outer_properties->StickyTranslation());
  EXPECT_EQ(TransformationMatrix().Translate(0, 60),
            outer_properties->StickyTranslation()->Matrix());
  ASSERT_NE(nullptr,
            outer_properties->StickyTranslation()->GetStickyConstraint());
  EXPECT_TRUE(
      outer_properties->StickyTranslation()->GetStickyConstraint()->is_sticky);
  EXPECT_EQ(CompositorElementId(), outer_properties->StickyTranslation()
                                       ->GetStickyConstraint()
                                       ->nearest_element_shifting_sticky_box);
  EXPECT_EQ(CompositorElementId(),
            outer_properties->StickyTranslation()
                ->GetStickyConstraint()
                ->nearest_element_shifting_containing_block);

  const auto* middle_properties = PaintPropertiesForElement("middle");
  ASSERT_TRUE(middle_properties && middle_properties->StickyTranslation());
  EXPECT_EQ(TransformationMatrix().Translate(0, 15),
            middle_properties->StickyTranslation()->Matrix());
  ASSERT_NE(nullptr,
            middle_properties->StickyTranslation()->GetStickyConstraint());
  EXPECT_TRUE(
      middle_properties->StickyTranslation()->GetStickyConstraint()->is_sticky);
  EXPECT_EQ(CompositorElementId(), middle_properties->StickyTranslation()
                                       ->GetStickyConstraint()
                                       ->nearest_element_shifting_sticky_box);
  EXPECT_EQ(outer_properties->StickyTranslation()->GetCompositorElementId(),
            middle_properties->StickyTranslation()
                ->GetStickyConstraint()
                ->nearest_element_shifting_containing_block);

  const auto* inner_properties = PaintPropertiesForElement("inner");
  ASSERT_TRUE(inner_properties && inner_properties->StickyTranslation());
  EXPECT_EQ(TransformationMatrix().Translate(0, 20),
            inner_properties->StickyTranslation()->Matrix());
  ASSERT_NE(nullptr,
            inner_properties->StickyTranslation()->GetStickyConstraint());
  EXPECT_TRUE(
      inner_properties->StickyTranslation()->GetStickyConstraint()->is_sticky);
  EXPECT_EQ(middle_properties->StickyTranslation()->GetCompositorElementId(),
            inner_properties->StickyTranslation()
                ->GetStickyConstraint()
                ->nearest_element_shifting_sticky_box);
  EXPECT_EQ(outer_properties->StickyTranslation()->GetCompositorElementId(),
            inner_properties->StickyTranslation()
                ->GetStickyConstraint()
                ->nearest_element_shifting_containing_block);
}

TEST_P(PaintPropertyTreeBuilderTest, NonScrollableSticky) {
  // This test verifies the property tree builder applies sticky offset
  // correctly when the clipping container cannot be scrolled, and
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
  GetDocument().getElementById("scroller")->setScrollTop(50);
  GetDocument().View()->UpdateAllLifecyclePhases();

  const auto* outer_properties = PaintPropertiesForElement("outer");
  ASSERT_TRUE(outer_properties && outer_properties->StickyTranslation());
  EXPECT_EQ(TransformationMatrix().Translate(0, 60),
            outer_properties->StickyTranslation()->Matrix());
  EXPECT_EQ(nullptr,
            outer_properties->StickyTranslation()->GetStickyConstraint());

  const auto* middle_properties = PaintPropertiesForElement("middle");
  ASSERT_TRUE(middle_properties && middle_properties->StickyTranslation());
  EXPECT_EQ(TransformationMatrix().Translate(0, 15),
            middle_properties->StickyTranslation()->Matrix());
  EXPECT_EQ(nullptr,
            middle_properties->StickyTranslation()->GetStickyConstraint());

  const auto* inner_properties = PaintPropertiesForElement("inner");
  ASSERT_TRUE(inner_properties && inner_properties->StickyTranslation());
  EXPECT_EQ(TransformationMatrix().Translate(0, 20),
            inner_properties->StickyTranslation()->Matrix());
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

  auto* div = GetDocument().getElementById("div");
  div->setAttribute(HTMLNames::classAttr, "transluscent");
  GetDocument().View()->UpdateAllLifecyclePhasesExceptPaint();
  EXPECT_FALSE(ToLayoutBox(div->GetLayoutObject())->Layer()->NeedsRepaint());

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

TEST_P(PaintPropertyTreeBuilderTest, SVGRootCompositedClipPath) {
  SetBodyInnerHTML(R"HTML(
    <svg id='svg' style='clip-path: circle(); will-change: transform'></svg>
  )HTML");

  const auto* properties = PaintPropertiesForElement("svg");

  ASSERT_NE(nullptr, properties->PaintOffsetTranslation());
  const auto* transform = properties->Transform();
  ASSERT_NE(nullptr, transform);
  EXPECT_EQ(properties->PaintOffsetTranslation(), transform->Parent());
  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled() ||
      RuntimeEnabledFeatures::BlinkGenPropertyTreesEnabled())
    EXPECT_TRUE(transform->HasDirectCompositingReasons());

  if (RuntimeEnabledFeatures::SlimmingPaintV2Enabled()) {
    EXPECT_EQ(nullptr, properties->MaskClip());

    const auto* clip_path_clip = properties->ClipPathClip();
    ASSERT_NE(nullptr, clip_path_clip);
    EXPECT_EQ(DocContentClip(), clip_path_clip->Parent());
    EXPECT_EQ(FloatRect(75, 0, 150, 150), clip_path_clip->ClipRect().Rect());
    EXPECT_EQ(transform, clip_path_clip->LocalTransformSpace());
    EXPECT_NE(nullptr, clip_path_clip->ClipPath());

    const auto* overflow_clip = properties->OverflowClip();
    ASSERT_NE(nullptr, overflow_clip);
    EXPECT_EQ(clip_path_clip, overflow_clip->Parent());
    EXPECT_EQ(FloatRect(0, 0, 300, 150), overflow_clip->ClipRect().Rect());
    EXPECT_EQ(transform, overflow_clip->LocalTransformSpace());

    // TODO(wangxianzhu): Are the following correct?
    EXPECT_EQ(nullptr, properties->Effect());
    EXPECT_EQ(nullptr, properties->Mask());
    EXPECT_EQ(nullptr, properties->ClipPath());
  } else {
    const auto* mask_clip = properties->MaskClip();
    ASSERT_NE(nullptr, mask_clip);
    EXPECT_EQ(DocContentClip(), mask_clip->Parent());
    EXPECT_EQ(FloatRect(75, 0, 150, 150), mask_clip->ClipRect().Rect());
    EXPECT_EQ(nullptr, mask_clip->ClipPath());
    EXPECT_EQ(transform, mask_clip->LocalTransformSpace());

    const auto* clip_path_clip = properties->ClipPathClip();
    ASSERT_NE(nullptr, clip_path_clip);
    EXPECT_EQ(mask_clip, clip_path_clip->Parent());
    EXPECT_EQ(FloatRect(75, 0, 150, 150), clip_path_clip->ClipRect().Rect());
    EXPECT_EQ(transform, clip_path_clip->LocalTransformSpace());
    EXPECT_NE(nullptr, clip_path_clip->ClipPath());

    const auto* overflow_clip = properties->OverflowClip();
    ASSERT_NE(nullptr, overflow_clip);
    EXPECT_EQ(mask_clip, overflow_clip->Parent());
    EXPECT_EQ(FloatRect(0, 0, 300, 150), overflow_clip->ClipRect().Rect());
    EXPECT_EQ(transform, overflow_clip->LocalTransformSpace());

    const auto* effect = properties->Effect();
    ASSERT_NE(nullptr, effect);
    EXPECT_EQ(&EffectPaintPropertyNode::Root(), effect->Parent());
    EXPECT_EQ(transform, effect->LocalTransformSpace());
    EXPECT_EQ(mask_clip, effect->OutputClip());
    EXPECT_EQ(SkBlendMode::kSrcOver, effect->BlendMode());

    const auto* mask = properties->Mask();
    ASSERT_NE(nullptr, mask);
    EXPECT_EQ(effect, mask->Parent());
    EXPECT_EQ(transform, mask->LocalTransformSpace());
    EXPECT_EQ(mask_clip, mask->OutputClip());
    EXPECT_EQ(SkBlendMode::kDstIn, mask->BlendMode());

    EXPECT_EQ(nullptr, properties->ClipPath());
  }
}

}  // namespace blink
