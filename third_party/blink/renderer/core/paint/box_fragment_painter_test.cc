// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/box_fragment_painter.h"

#include "base/test/scoped_feature_list.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/surfaces/tracked_element_rects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/webdx_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/paint/paint_controller_paint_test.h"
#include "third_party/blink/renderer/core/paint/paint_event.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record_builder.h"

using testing::ElementsAre;

namespace blink {

namespace {

void ExtractLinks(const PaintRecord& record, std::vector<GURL>* links) {
  for (const cc::PaintOp& op : record) {
    if (op.GetType() == cc::PaintOpType::kAnnotate) {
      const auto& annotate_op = static_cast<const cc::AnnotateOp&>(op);
      links->push_back(GURL(
          std::string(reinterpret_cast<const char*>(annotate_op.data->data()),
                      annotate_op.data->size())));
    } else if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const auto& record_op = static_cast<const cc::DrawRecordOp&>(op);
      ExtractLinks(record_op.record, links);
    }
  }
}

}  // namespace

class BoxFragmentPainterTest : public PaintControllerPaintTest {
 public:
  explicit BoxFragmentPainterTest(
      LocalFrameClient* local_frame_client = nullptr)
      : PaintControllerPaintTest(local_frame_client) {}
};

INSTANTIATE_PAINT_TEST_SUITE_P(BoxFragmentPainterTest);

TEST_P(BoxFragmentPainterTest, ScrollHitTestOrder) {
  SetPreferCompositingToLCDText(false);
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
      ::-webkit-scrollbar { display: none; }
      body { margin: 0; }
      #scroller {
        width: 40px;
        height: 40px;
        overflow: scroll;
        font-size: 200px;
      }
    </style>
    <div id='scroller'>TEXT</div>
  )HTML");
  auto& scroller = *GetLayoutBoxByElementId("scroller");
  const DisplayItemClient& root_fragment = scroller;

  InlineCursor cursor;
  cursor.MoveTo(*scroller.SlowFirstChild());
  const DisplayItemClient& text_fragment =
      *cursor.Current().GetDisplayItemClient();

  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(text_fragment.Id(), kForegroundType)));
  auto* scroll_hit_test = MakeGarbageCollected<HitTestData>();
  scroll_hit_test->scroll_translation =
      scroller.FirstFragment().PaintProperties()->ScrollTranslation();
  scroll_hit_test->scroll_hit_test_rect = gfx::Rect(0, 0, 40, 40);
  EXPECT_THAT(
      ContentPaintChunks(),
      ElementsAre(
          VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
          IsPaintChunk(1, 1,
                       PaintChunk::Id(scroller.Id(), kBackgroundChunkType),
                       scroller.FirstFragment().LocalBorderBoxProperties()),
          IsPaintChunk(
              1, 1,
              PaintChunk::Id(root_fragment.Id(), DisplayItem::kScrollHitTest),
              scroller.FirstFragment().LocalBorderBoxProperties(),
              scroll_hit_test, gfx::Rect(0, 0, 40, 40)),
          IsPaintChunk(1, 2)));
}

TEST_P(BoxFragmentPainterTest, AddUrlRects) {
  SetBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <div>
      <p>
        <a href="https://www.chromium.org">Chromium</a>
      </p>
      <p>
        <a href="https://www.wikipedia.org">Wikipedia</a>
      </p>
    </div>
  )HTML");
  // Use Paint Preview to test this as printing falls back to the legacy layout
  // engine.

  // PaintPreviewTracker records URLs via the GraphicsContext under certain
  // flagsets when painting. This is the simplest way to check if URLs were
  // annotated.
  Document::PaintPreviewScope paint_preview(GetDocument(),
                                            Document::kPaintingPreview,
                                            /*allow_scrollbars=*/false);
  UpdateAllLifecyclePhasesForTest();

  paint_preview::PaintPreviewTracker tracker(base::UnguessableToken::Create(),
                                             std::nullopt, true);
  PaintRecordBuilder builder;
  builder.Context().SetPaintPreviewTracker(&tracker);

  GetDocument().View()->PaintOutsideOfLifecycle(
      builder.Context(),
      PaintFlag::kAddUrlMetadata | PaintFlag::kOmitCompositingInfo,
      CullRect::Infinite());

  auto record = builder.EndRecording();
  std::vector<GURL> links;
  ExtractLinks(record, &links);
  ASSERT_EQ(links.size(), 2U);
  EXPECT_EQ(links[0].spec(), "https://www.chromium.org/");
  EXPECT_EQ(links[1].spec(), "https://www.wikipedia.org/");
}

TEST_P(BoxFragmentPainterTest, SelectionTablePainting) {
  // This test passes if it does not timeout
  // Repro case of crbug.com/1182106.
  SetBodyInnerHTML(R"HTML(
    <!doctype html>
    <table id="t1"><tbody id="b1"><tr id="r1"><td id="c1">
    <table id="t2"><tbody id="b2"><tr id="r2"><td id="c2">
    <table id="t3"><tbody id="b3"><tr id="r3"><td id="c3">
    <table id="t4"><tbody id="b4"><tr id="r4"><td id="c4">
    <table id="t5"><tbody id="b5"><tr id="r5"><td id="c5">
      <table id="target">
        <tbody id="b6">
          <tr id="r6"> <!-- 8388608 steps-->
            <td id="c6.1">
              <table id="t7">
                <tbody id="b7">
                  <tr id="r7">
                    <td><img src="./resources/blue-100.png" style="width:100px">Drag me</td>
                  </tr>
                </tbody>
              </table>
            </td>
            <td id="c6.2">
              <table id="t8" style="float:left;width:100%">
                <tbody id="b8">
                  <tr id="r8">
                    <td id="c8">Float</td>
                  </tr>
                </tbody>
              </table>
            </td>
          </tr>
        </tbody>
      </table>
    </td></tr></tbody></table>
    </td></tr></tbody></table>
    </td></tr></tbody></table>
    </td></tr></tbody></table>
    </td></tr></tbody></table>
  )HTML");
  // Drag image will only paint if there is selection.
  GetDocument().View()->GetFrame().Selection().SelectAll();
  GetDocument().GetLayoutView()->CommitPendingSelection();
  UpdateAllLifecyclePhasesForTest();
  PaintRecordBuilder builder;
  GetDocument().View()->PaintOutsideOfLifecycle(
      builder.Context(),
      PaintFlag::kSelectionDragImageOnly | PaintFlag::kOmitCompositingInfo,
      CullRect::Infinite());

  auto record = builder.EndRecording();
}

TEST_P(BoxFragmentPainterTest, ClippedText) {
  SetBodyInnerHTML(R"HTML(
    <div id="target" style="overflow: hidden; position: relative;
                            width: 100px; height: 100px">
      A<br>B<br>C<br>D
    </div>
  )HTML");
  // Initially all the texts are painted.
  auto num_all_display_items = ContentDisplayItems().size();
  auto* target = GetDocument().getElementById(AtomicString("target"));

  target->SetInlineStyleProperty(CSSPropertyID::kHeight, "0px");
  UpdateAllLifecyclePhasesForTest();
  // None of the texts should be painted.
  EXPECT_EQ(num_all_display_items - 4, ContentDisplayItems().size());

  target->SetInlineStyleProperty(CSSPropertyID::kHeight, "1px");
  UpdateAllLifecyclePhasesForTest();
  // Only "A" should be painted.
  EXPECT_EQ(num_all_display_items - 3, ContentDisplayItems().size());
}

TEST_P(BoxFragmentPainterTest, NodeAtPointWithSvgInline) {
  SetBodyInnerHTML(R"HTML(
<svg xmlns="http://www.w3.org/2000/svg" width="900" height="900"
     viewBox="0 0 100 100" id="svg">
 <g font-size="13">
  <text x="10%" y="25%" id="pass">Expected paragraph.</text>
  <text x="10%" y="54%">
  <tspan id="fail">Should not be selected.</tspan>
  </text>
 </g>
</svg>)HTML");
  UpdateAllLifecyclePhasesForTest();

  auto* root =
      GetDocument().getElementById(AtomicString("svg"))->GetLayoutBox();
  HitTestResult result;
  root->NodeAtPoint(result, HitTestLocation(gfx::PointF(256, 192)),
                    PhysicalOffset(0, 0), HitTestPhase::kForeground);
  EXPECT_EQ(GetDocument().getElementById(AtomicString("pass")),
            result.InnerElement());
}

TEST_P(BoxFragmentPainterTest, TextareaBoxDecorationBackground) {
  SetBodyInnerHTML("<textarea id=textarea style='resize: none'>");

  auto* textarea = GetLayoutObjectByElementId("textarea");
  EXPECT_THAT(ContentDisplayItems(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_DISPLAY_ITEM,
                          IsSameId(textarea->Id(), kBackgroundType)));
}

TEST_P(BoxFragmentPainterTest, TrackElementDiv) {
  SetBodyInnerHTML(R"HTML(
    <div style="overflow: hidden; position: relative;
                            width: 100px; height: 100px">
      <div id="target" style="height:50px;">ABCD</div>
    </div>
  )HTML");
  // Initially all the texts are painted.
  auto* target = GetDocument().getElementById(AtomicString("target"));

  auto element_id = base::Token(1, 2);
  auto element = TrackedElementSubRect(TrackedElementId(element_id));
  viz::TrackedElementFeature feature =
      static_cast<viz::TrackedElementFeature>(1);
  target->SetTrackedElementSubRect(feature, element);

  UpdateAllLifecyclePhasesForTest();

  auto* tracked_element_data = MakeGarbageCollected<TrackedElementRects>();
  // Contains background pixels (not the text pixels)
  tracked_element_data->map.insert_or_assign(
      feature, std::vector<TrackedElementRect>{TrackedElementRect(
                   TrackedElementId(element_id), gfx::Rect(8, 8, 100, 50))});

  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                          IsPaintChunkWithTrackedElementData(
                              1, 2, tracked_element_data)));
}

TEST_P(BoxFragmentPainterTest, TrackElementSpanInlineBox) {
  // Using style="background-color: red;" makes the span an inline box
  // fragment.

  SetBodyInnerHTML(R"HTML(
    <div id="parent" style="overflow: hidden; position: relative;
                            width: 100px; height: 100px">
      <span id="target" style="background-color: red;">A</span>
    </div>
  )HTML");
  // Initially all the texts are painted.
  auto* target = GetDocument().getElementById(AtomicString("target"));

  auto element_id = base::Token(1, 2);
  auto element = TrackedElementSubRect(TrackedElementId(element_id));
  viz::TrackedElementFeature feature =
      static_cast<viz::TrackedElementFeature>(1);
  target->SetTrackedElementSubRect(feature, element);

  UpdateAllLifecyclePhasesForTest();

  auto* tracked_element_data = MakeGarbageCollected<TrackedElementRects>();
  // Contains only text pixels
  tracked_element_data->map.insert_or_assign(
      feature, std::vector<TrackedElementRect>{TrackedElementRect(
                   TrackedElementId(element_id), gfx::Rect(8, 8, 1, 1))});

  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                          IsPaintChunkWithTrackedElementData(
                              1, 3, tracked_element_data)));
}

TEST_P(BoxFragmentPainterTest, TrackElementSpanShouldForceInlineBox) {
  // Blink uses an optimization for inline elements (like <span>) called Culled
  // Inlines. If an inline element has no visual properties (no background,
  // border, or outline), layout determines it doesn't need its own geometry box
  // (PhysicalBoxFragment). Instead, it just paints the text children directly
  // to save memory. Span without any attibutes normaly generates  makes the
  // span an inline box fragment.
  //
  // Adding a tracked element should force skip the culling and
  // requrest for layout regeneration.

  SetBodyInnerHTML(R"HTML(
    <div id="parent" style="overflow: hidden; position: relative;
                            width: 100px; height: 100px">
      <span id="target">A</span>
    </div>
  )HTML");
  // Initially all the texts are painted.
  auto* target = GetDocument().getElementById(AtomicString("target"));

  auto element_id = base::Token(1, 2);
  auto element = TrackedElementSubRect(TrackedElementId(element_id));
  viz::TrackedElementFeature feature =
      static_cast<viz::TrackedElementFeature>(1);
  target->SetTrackedElementSubRect(feature, element);

  UpdateAllLifecyclePhasesForTest();

  auto* tracked_element_data = MakeGarbageCollected<TrackedElementRects>();
  // Contains only text pixels
  tracked_element_data->map.insert_or_assign(
      feature, std::vector<TrackedElementRect>{TrackedElementRect(
                   TrackedElementId(element_id), gfx::Rect(8, 8, 1, 1))});

  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                          IsPaintChunkWithTrackedElementData(
                              1, 2, tracked_element_data)));
}

TEST_P(BoxFragmentPainterTest, TrackElementWithSubRect) {
  SetBodyInnerHTML(R"HTML(
    <div style="overflow: hidden; position: relative;
                            width: 100px; height: 100px">
      <div id="target" style="height:50px;">ABCD</div>
    </div>
  )HTML");
  // Initially all the texts are painted.
  auto* target = GetDocument().getElementById(AtomicString("target"));

  auto element_id = base::Token(1, 2);
  auto element = TrackedElementSubRect(
      TrackedElementId(element_id),
      /*should_add_to_compositor_frame_metadata=*/false,
      /*should_exclude_fixed_and_sticky_occlusions=*/false,
      TrackedElementSubRect::SubRect{
          gfx::Rect(10, 10, 20, 20),
          TrackedElementSubRect::SubRect::Type::kIntersectWithElementRect});
  viz::TrackedElementFeature feature =
      static_cast<viz::TrackedElementFeature>(1);
  target->SetTrackedElementSubRect(feature, element);

  UpdateAllLifecyclePhasesForTest();

  auto* tracked_element_data = MakeGarbageCollected<TrackedElementRects>();
  // 8, 8 is the default body margin. 10, 10 is the sub-rect offset.
  tracked_element_data->map.insert_or_assign(
      feature, std::vector<TrackedElementRect>{TrackedElementRect(
                   TrackedElementId(element_id), gfx::Rect(18, 18, 20, 20))});

  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                          IsPaintChunkWithTrackedElementData(
                              1, 2, tracked_element_data)));
}

TEST_P(BoxFragmentPainterTest, TrackElementWithSubRectNoIntersection) {
  SetBodyInnerHTML(R"HTML(
    <div style="overflow: hidden; position: relative;
                            width: 100px; height: 100px">
      <div id="target" style="width:20px; height:20px; background: blue;"></div>
    </div>
  )HTML");
  auto* target = GetDocument().getElementById(AtomicString("target"));

  auto element_id = base::Token(1, 2);
  // Create a sub-rect that is larger than the element and starts at a negative
  // offset.
  auto element = TrackedElementSubRect(
      TrackedElementId(element_id),
      /*should_add_to_compositor_frame_metadata=*/true,
      /*should_exclude_fixed_and_sticky_occlusions=*/false,
      TrackedElementSubRect::SubRect{
          gfx::Rect(-10, -10, 100, 100),
          TrackedElementSubRect::SubRect::Type::kNoIntersection});
  viz::TrackedElementFeature feature =
      static_cast<viz::TrackedElementFeature>(1);
  target->SetTrackedElementSubRect(feature, element);

  UpdateAllLifecyclePhasesForTest();

  auto* tracked_element_data = MakeGarbageCollected<TrackedElementRects>();
  // 8, 8 (margin) - 10, 10 (offset) = -2, -2. Size remains 100, 100.
  tracked_element_data->map.insert_or_assign(
      feature, std::vector<TrackedElementRect>{TrackedElementRect(
                   TrackedElementId(element_id), gfx::Rect(-2, -2, 100, 100),
                   /*should_add_to_compositor_frame_metadata=*/true)});

  EXPECT_THAT(ContentPaintChunks(),
              ElementsAre(VIEW_SCROLLING_BACKGROUND_CHUNK_COMMON,
                          IsPaintChunkWithTrackedElementData(
                              1, 2, tracked_element_data)));
}

TEST_P(BoxFragmentPainterTest, GapDecorationsUseCountShorthand) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .container {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 10px;
        column-rule: 1px solid red;
      }
    </style>
    <div class="container">
      <div>A</div>
      <div>B</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsWebDXFeatureCounted(
      mojom::blink::WebDXFeature::kGapDecorations));
}

TEST_P(BoxFragmentPainterTest, GapDecorationsUseCountLonghand) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .container {
        display: flex;
        flex-wrap: wrap;
        width: 200px;
        gap: 10px;
        column-rule-style: solid;
        column-rule-width: 1px;
        column-rule-color: red;
      }
    </style>
    <div class="container">
      <div style="width: 80px">A</div>
      <div style="width: 80px">B</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsWebDXFeatureCounted(
      mojom::blink::WebDXFeature::kGapDecorations));
}

TEST_P(BoxFragmentPainterTest, GapDecorationsUseCountRowRule) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .container {
        display: grid;
        grid-template-columns: 1fr 1fr;
        grid-template-rows: auto auto;
        gap: 10px;
        row-rule: 1px solid red;
      }
    </style>
    <div class="container">
      <div>A</div>
      <div>B</div>
      <div>C</div>
      <div>D</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsWebDXFeatureCounted(
      mojom::blink::WebDXFeature::kGapDecorations));
}

TEST_P(BoxFragmentPainterTest, GapDecorationsUseCountNotCounted) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .container {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 10px;
      }
    </style>
    <div class="container">
      <div>A</div>
      <div>B</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().IsWebDXFeatureCounted(
      mojom::blink::WebDXFeature::kGapDecorations));
}

TEST_P(BoxFragmentPainterTest, GapDecorationsUseCountStyleOnly) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .container {
        display: grid;
        grid-template-columns: 1fr 1fr;
        gap: 10px;
        column-rule-style: solid;
      }
    </style>
    <div class="container">
      <div>A</div>
      <div>B</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(GetDocument().IsWebDXFeatureCounted(
      mojom::blink::WebDXFeature::kGapDecorations));
}

TEST_P(BoxFragmentPainterTest, GapDecorationsUseCountWidthOnlyNotCounted) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .container {
        display: flex;
        flex-wrap: wrap;
        width: 200px;
        gap: 10px;
        column-rule-width: 3px;
      }
    </style>
    <div class="container">
      <div style="width: 80px">A</div>
      <div style="width: 80px">B</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().IsWebDXFeatureCounted(
      mojom::blink::WebDXFeature::kGapDecorations));
}

TEST_P(BoxFragmentPainterTest, GapDecorationsUseCountColorOnlyNotCounted) {
  SetBodyInnerHTML(R"HTML(
    <style>
      .container {
        columns: 2;
        column-gap: 10px;
        column-rule-color: red;
      }
    </style>
    <div class="container">
      <p>Some text content to fill the columns.</p>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(GetDocument().IsWebDXFeatureCounted(
      mojom::blink::WebDXFeature::kGapDecorations));
}

TEST_P(BoxFragmentPainterTest, NodeAtPointWithFilterOnInline) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body { margin: 0; }
      #container { position: absolute; left: 0; top: 0; width: 200px; height: 200px; }
      #target { filter: drop-shadow(100px 100px red); background: green; font-size: 20px; }
    </style>
    <div id="container">
      <span id="target">Text</span>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesForTest();

  Element* target = GetDocument().getElementById(AtomicString("target"));
  gfx::PointF hit_point(105, 105);
  HitTestLocation location(hit_point);
  HitTestRequest::HitTestRequestType hit_type =
      HitTestRequest::kReadOnly | HitTestRequest::kHitTestVisualOverflow;
  HitTestResult result =
      GetDocument().GetFrame()->GetEventHandler().HitTestResultAtLocation(
          location, hit_type);
  EXPECT_EQ(result.InnerElement(), target);
}

namespace {

bool ContainsDrawRectWithColor(const PaintRecord& record, SkColor4f color) {
  for (const cc::PaintOp& op : record) {
    if (op.GetType() == cc::PaintOpType::kDrawRect) {
      const auto& rect_op = static_cast<const cc::DrawRectOp&>(op);
      if (rect_op.flags.getColor4f() == color) {
        return true;
      }
    } else if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const auto& record_op = static_cast<const cc::DrawRecordOp&>(op);
      if (ContainsDrawRectWithColor(record_op.record, color)) {
        return true;
      }
    }
  }
  return false;
}

// The label text is the only white text in these tests' pages, so counting
// text blobs drawn in the label color counts the labels.
int CountDrawTextBlobOpsWithColor(const PaintRecord& record, SkColor4f color) {
  int count = 0;
  for (const cc::PaintOp& op : record) {
    if (op.GetType() == cc::PaintOpType::kDrawTextBlob) {
      const auto& text_op = static_cast<const cc::DrawTextBlobOp&>(op);
      if (text_op.flags.getColor4f() == color) {
        ++count;
      }
    } else if (op.GetType() == cc::PaintOpType::kDrawRecord) {
      const auto& record_op = static_cast<const cc::DrawRecordOp&>(op);
      count += CountDrawTextBlobOpsWithColor(record_op.record, color);
    }
  }
  return count;
}

const SkColor4f kRedOverlay =
    SkColor4f::FromColor(SkColorSetARGB(128, 255, 0, 0));
// The label is white text on a Google Red 700 background; see PaintAdLabel().
const SkColor4f kPopupLabelBackground =
    Color::FromRGB(0xC5, 0x22, 0x1F).toSkColor4f();
const SkColor4f kPopupLabelText = Color::kWhite.toSkColor4f();

void SimulateFirstContentfulPaint(Document& document) {
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = base::TimeTicks::Now();
  PaintTiming::From(document).ReportPresentationTime(
      PaintEvent::kFirstContentfulPaint, base::TimeTicks(), details);
}

}  // namespace

// AdLabelText() is a pure function, so this doesn't need the paint test
// fixture or its configurations.
TEST(BoxFragmentPainterAdLabelTest, AdLabelText) {
  using Type = BoxFragmentPainter::AdLabelType;

  EXPECT_EQ(BoxFragmentPainter::AdLabelText({}), "");

  // One type: no separator.
  EXPECT_EQ(BoxFragmentPainter::AdLabelText({Type::kPopup}), "popup");
  EXPECT_EQ(BoxFragmentPainter::AdLabelText({Type::kSticky}), "sticky");
  EXPECT_EQ(BoxFragmentPainter::AdLabelText({Type::kVideo}), "video");

  // Two types: one separator.
  EXPECT_EQ(BoxFragmentPainter::AdLabelText({Type::kPopup, Type::kSticky}),
            "popup, sticky");
  EXPECT_EQ(BoxFragmentPainter::AdLabelText({Type::kSticky, Type::kVideo}),
            "sticky, video");

  // Three types: two separators.
  EXPECT_EQ(BoxFragmentPainter::AdLabelText(
                {Type::kPopup, Type::kSticky, Type::kVideo}),
            "popup, sticky, video");

  // Word order follows `AdLabelType` order, not the order the types were
  // listed in.
  EXPECT_EQ(BoxFragmentPainter::AdLabelText({Type::kVideo, Type::kPopup}),
            "popup, video");
}

TEST_P(BoxFragmentPainterTest, PaintPopupBadgeForOverlayPopupAd) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAdHighlightLabel);

  GetDocument().GetPage()->GetSettings().SetHighlightAds(true);
  SetBodyInnerHTML(R"HTML(
    <div id="normal_ad" style="width: 100px; height: 100px;"></div>
    <div id="popup_ad" style="width: 100px; height: 100px;"></div>
  )HTML");
  SimulateFirstContentfulPaint(GetDocument());

  Element* normal_ad = GetDocument().getElementById(AtomicString("normal_ad"));
  Element* popup_ad = GetDocument().getElementById(AtomicString("popup_ad"));
  ASSERT_TRUE(normal_ad);
  ASSERT_TRUE(popup_ad);
  normal_ad->SetIsAdRelated(NoProvenance{});
  popup_ad->SetIsAdRelated(NoProvenance{});
  popup_ad->SetIsOverlayPopupAd();

  // First lifecycle update runs
  // DisplayAdElementMonitor::DidFinishLifecycleUpdate which updates
  // `should_highlight_` and invalidates paint.
  UpdateAllLifecyclePhasesForTest();
  // Second lifecycle update paints the ad highlight overlays.
  UpdateAllLifecyclePhasesForTest();

  PaintRecordBuilder builder;
  GetDocument().View()->PaintOutsideOfLifecycle(
      builder.Context(), PaintFlag::kOmitCompositingInfo, CullRect::Infinite());
  auto record = builder.EndRecording();

  // Both ads get the same red highlight.
  EXPECT_TRUE(ContainsDrawRectWithColor(record, kRedOverlay));

  // Only the overlay popup ad gets the "popup" label: white text on a solid
  // dark red box.
  EXPECT_TRUE(ContainsDrawRectWithColor(record, kPopupLabelBackground));
  EXPECT_EQ(CountDrawTextBlobOpsWithColor(record, kPopupLabelText), 1);
}

TEST_P(BoxFragmentPainterTest, PopupBadgeAddedAfterAdHighlightIsPainted) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAdHighlightLabel);

  GetDocument().GetPage()->GetSettings().SetHighlightAds(true);
  SetBodyInnerHTML(R"HTML(
    <div id="ad" style="width: 100px; height: 100px;"></div>
  )HTML");
  SimulateFirstContentfulPaint(GetDocument());

  Element* ad = GetDocument().getElementById(AtomicString("ad"));
  ASSERT_TRUE(ad);
  ad->SetIsAdRelated(NoProvenance{});

  UpdateAllLifecyclePhasesForTest();
  UpdateAllLifecyclePhasesForTest();

  {
    PaintRecordBuilder builder;
    GetDocument().View()->PaintOutsideOfLifecycle(
        builder.Context(), PaintFlag::kOmitCompositingInfo,
        CullRect::Infinite());
    auto record = builder.EndRecording();
    EXPECT_TRUE(ContainsDrawRectWithColor(record, kRedOverlay));
    EXPECT_FALSE(ContainsDrawRectWithColor(record, kPopupLabelBackground));
    EXPECT_EQ(CountDrawTextBlobOpsWithColor(record, kPopupLabelText), 0);
  }

  // Now flag the already-painted ad as an overlay popup ad, which is what
  // OverlayInterstitialAdDetector does at runtime. Since the highlight is
  // already shown, this must invalidate paint so the label gets painted.
  ad->SetIsOverlayPopupAd();
  UpdateAllLifecyclePhasesForTest();

  {
    PaintRecordBuilder builder;
    GetDocument().View()->PaintOutsideOfLifecycle(
        builder.Context(), PaintFlag::kOmitCompositingInfo,
        CullRect::Infinite());
    auto record = builder.EndRecording();
    // The highlight stays red; only the label is added.
    EXPECT_TRUE(ContainsDrawRectWithColor(record, kRedOverlay));
    EXPECT_TRUE(ContainsDrawRectWithColor(record, kPopupLabelBackground));
    EXPECT_EQ(CountDrawTextBlobOpsWithColor(record, kPopupLabelText), 1);
  }
}

TEST_P(BoxFragmentPainterTest, NoPopupBadgeWhenOverlayPopupAdIsTooSmall) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kAdHighlightLabel);

  GetDocument().GetPage()->GetSettings().SetHighlightAds(true);
  SetBodyInnerHTML(R"HTML(
    <div id="popup_ad" style="width: 8px; height: 8px;"></div>
  )HTML");
  SimulateFirstContentfulPaint(GetDocument());

  Element* popup_ad = GetDocument().getElementById(AtomicString("popup_ad"));
  ASSERT_TRUE(popup_ad);
  popup_ad->SetIsAdRelated(NoProvenance{});
  popup_ad->SetIsOverlayPopupAd();

  UpdateAllLifecyclePhasesForTest();
  UpdateAllLifecyclePhasesForTest();

  PaintRecordBuilder builder;
  GetDocument().View()->PaintOutsideOfLifecycle(
      builder.Context(), PaintFlag::kOmitCompositingInfo, CullRect::Infinite());
  auto record = builder.EndRecording();

  EXPECT_TRUE(ContainsDrawRectWithColor(record, kRedOverlay));
  EXPECT_FALSE(ContainsDrawRectWithColor(record, kPopupLabelBackground));
  EXPECT_EQ(CountDrawTextBlobOpsWithColor(record, kPopupLabelText), 0);
}

// Even if an element was marked while the feature was on, the label is not
// painted while the feature is off: the highlight is exactly as before.
TEST_P(BoxFragmentPainterTest, NoPopupBadgeWhenFeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kAdHighlightLabel);

  GetDocument().GetPage()->GetSettings().SetHighlightAds(true);
  SetBodyInnerHTML(R"HTML(
    <div id="popup_ad" style="width: 100px; height: 100px;"></div>
  )HTML");
  SimulateFirstContentfulPaint(GetDocument());

  Element* popup_ad = GetDocument().getElementById(AtomicString("popup_ad"));
  ASSERT_TRUE(popup_ad);
  popup_ad->SetIsAdRelated(NoProvenance{});
  popup_ad->SetIsOverlayPopupAd();

  UpdateAllLifecyclePhasesForTest();
  UpdateAllLifecyclePhasesForTest();

  PaintRecordBuilder builder;
  GetDocument().View()->PaintOutsideOfLifecycle(
      builder.Context(), PaintFlag::kOmitCompositingInfo, CullRect::Infinite());
  auto record = builder.EndRecording();

  // The red highlight is unaffected by the feature; only the label is gated.
  EXPECT_TRUE(ContainsDrawRectWithColor(record, kRedOverlay));
  EXPECT_FALSE(ContainsDrawRectWithColor(record, kPopupLabelBackground));
  EXPECT_EQ(CountDrawTextBlobOpsWithColor(record, kPopupLabelText), 0);
}

}  // namespace blink
