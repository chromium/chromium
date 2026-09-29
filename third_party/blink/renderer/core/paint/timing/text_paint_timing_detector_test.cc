// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"

#include "base/test/tracing/trace_event_analyzer.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_client.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_base.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class TextPaintTimingDetectorTest : public PaintTimingTestBase,
                                    public LcpTestSupport {
 public:
  TextPaintTimingDetectorTest() = default;

  void SetUp() override {
    PaintTimingTestBase::SetUp();
    AttachTo(GetDocument());
    main_frame_client_ =
        MakeGarbageCollected<PaintTimingRecordObserverClient>();
    GetPaintTiming().AddClient(main_frame_client_.Get());
  }

 protected:
  void InitializeChildFramePaintTimingClient() {
    child_frame_client_ =
        MakeGarbageCollected<PaintTimingRecordObserverClient>();
    PaintTiming::From(ChildDocument()).AddClient(child_frame_client_.Get());
  }

  LocalFrameView& GetChildFrameView() { return *ChildFrame().View(); }

  TextPaintTimingDetector& GetTextPaintTimingDetector() {
    return GetPaintTimingDetector().GetTextPaintTimingDetector();
  }

  bool HasLargestIgnoredText() {
    return GetPaintTiming()
        .GetLargestContentfulPaintManager()
        ->HasLargestIgnoredTextForTest();
  }

  Element* AppendDivElementToBody(String content, String style = "") {
    Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
    div->setAttribute(html_names::kStyleAttr, AtomicString(style));
    Text* text = GetDocument().createTextNode(content);
    div->AppendChild(text);
    GetDocument().body()->AppendChild(div);
    return div;
  }

  void SetElementStyle(Element* element, String style) {
    element->setAttribute(html_names::kStyleAttr, AtomicString(style));
  }

  void RemoveElement(Element* element) {
    element->GetLayoutObject()->Parent()->GetNode()->removeChild(element);
  }

  bool IsRecordingLargestTextPaint() {
    return !!GetPaintTiming().GetLargestContentfulPaintManager();
  }

  Persistent<PaintTimingRecordObserverClient> main_frame_client_;
  Persistent<PaintTimingRecordObserverClient> child_frame_client_;
};

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_NoText) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_time, 0.0);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_OneText) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* only_text = AppendDivElementToBody("The only text");
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), only_text);
  EXPECT_GT(LcpDetailsForReporting().text_paint_size, 0u);
  EXPECT_GT(LcpDetailsForReporting().text_paint_time, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks::Now());
}

TEST_F(TextPaintTimingDetectorTest, LaterSameSizeCandidate) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* first = AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();

  AppendDivElementToBody("text");
  AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), first);
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_FontSizeChange_MultipleUpdates) {
  SetMainFrameBodyContent(R"HTML()HTML");
  Element* text = AppendDivElementToBody("text");
  SetElementStyle(text, "font-size: 200px");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);

  SetElementStyle(text, "font-size: 300px");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_TraceEvent_Candidate) {
  base::test::TracingEnvironment tracing_environment;
  using trace_analyzer::Query;
  trace_analyzer::Start("loading");
  {
    SetMainFrameBodyContent(R"HTML(
      )HTML");
    AppendDivElementToBody("The only text");
    SimulateRenderingAndPresentationTime();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestTextPaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::DictValue arg_dict = events[0]->GetKnownArgAsDict("data");
  EXPECT_GT(arg_dict.FindInt("DOMNodeId").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("size").value_or(-1), 0);
  EXPECT_EQ(arg_dict.FindInt("candidateIndex").value_or(-1), 1);
  std::optional<bool> is_main_frame = arg_dict.FindBool("isMainFrame");
  EXPECT_TRUE(is_main_frame.has_value());
  EXPECT_EQ(true, is_main_frame.value());
  std::optional<bool> is_outermost_main_frame =
      arg_dict.FindBool("isOutermostMainFrame");
  EXPECT_TRUE(is_outermost_main_frame.has_value());
  EXPECT_EQ(true, is_outermost_main_frame.value());
  std::optional<bool> is_embedded_frame = arg_dict.FindBool("isEmbeddedFrame");
  EXPECT_TRUE(is_embedded_frame.has_value());
  EXPECT_EQ(false, is_embedded_frame.value());
  EXPECT_GT(arg_dict.FindInt("frame_x").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("frame_y").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("frame_width").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("frame_height").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("root_x").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("root_y").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("root_width").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("root_height").value_or(-1), 0);
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_TraceEvent_Candidate_Frame) {
  base::test::TracingEnvironment tracing_environment;
  using trace_analyzer::Query;
  trace_analyzer::Start("loading");
  {
    GetDocument().SetBaseURLOverride(KURL("http://test.com"));
    SetMainFrameBodyContent(R"HTML(
      <style>body { margin: 15px; } iframe { display: block; position: relative; margin-top: 50px; } </style>
      <iframe> </iframe>
    )HTML");
    SetChildFrameBodyContent(R"HTML(
      <style>body { margin: 10px;} #target { width: 200px; height: 200px; }
      </style>
      <div>Some content</div>
    )HTML");
    SimulateRenderingAndPresentationTime();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestTextPaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::DictValue arg_dict = events[0]->GetKnownArgAsDict("data");
  EXPECT_GT(arg_dict.FindInt("DOMNodeId").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("size").value_or(-1), 0);
  EXPECT_EQ(arg_dict.FindInt("candidateIndex").value_or(-1), 1);
  std::optional<bool> is_main_frame = arg_dict.FindBool("isMainFrame");
  EXPECT_TRUE(is_main_frame.has_value());
  EXPECT_EQ(false, is_main_frame.value());
  std::optional<bool> is_outermost_main_frame =
      arg_dict.FindBool("isOutermostMainFrame");
  EXPECT_TRUE(is_outermost_main_frame.has_value());
  EXPECT_EQ(false, is_outermost_main_frame.value());
  std::optional<bool> is_embedded_frame = arg_dict.FindBool("isEmbeddedFrame");
  EXPECT_TRUE(is_embedded_frame.has_value());
  EXPECT_EQ(false, is_embedded_frame.value());
  // There's sometimes a 1 pixel offset for the y dimensions.
  EXPECT_EQ(arg_dict.FindInt("frame_x").value_or(-1), 10);
  EXPECT_GE(arg_dict.FindInt("frame_y").value_or(-1), 9);
  EXPECT_LE(arg_dict.FindInt("frame_y").value_or(-1), 10);
  EXPECT_GT(arg_dict.FindInt("frame_width").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("frame_height").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("root_x").value_or(-1), 25);
  EXPECT_GT(arg_dict.FindInt("root_y").value_or(-1), 50);
  EXPECT_GT(arg_dict.FindInt("root_width").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("root_height").value_or(-1), 0);
}

TEST_F(TextPaintTimingDetectorTest, AggregationBySelfPaintingInlineElement) {
  SetMainFrameBodyContent(R"HTML(
    <div style="background: yellow">
      tiny
      <span id="target"
        style="position: relative; background: blue; top: 100px; left: 100px">
        this is the largest text in the world.</span>
    </div>
  )HTML");
  Element* span = GetElementById("target");
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), span);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_OpacityZero) {
  SetMainFrameBodyContent(R"HTML(
    <style>
    div {
      opacity: 0;
    }
    </style>
    )HTML");
  SimulateRenderingAndPresentationTime();

  AppendDivElementToBody("The only text");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);
}

TEST_F(TextPaintTimingDetectorTest,
       NodeRemovedBeforeAssigningPresentationTime) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <div id="remove">The only text</div>
    </div>
  )HTML");
  SimulateRendering();

  GetElementById("parent")->RemoveChild(GetElementById("remove"));
  SimulatePresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_LargestText) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  // Render some initial text, which will be an LCP candidate.
  Element* initial_text = AppendDivElementToBody("medium text");
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), initial_text);

  // Render some larger text, which will be a new LCP candidate.
  Element* large_text = AppendDivElementToBody("a long-long-long text");
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 2u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), large_text);

  // Render some smaller text, which will not be a new LCP candidate.
  AppendDivElementToBody("small");
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 2u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), large_text);
}

TEST_F(TextPaintTimingDetectorTest, UpdateResultWhenCandidateChanged) {
  SetMainFrameBodyContent(R"HTML(
    <div>small text</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks::Now());
  ASSERT_EQ(LcpCandidateCount(), 1u);

  AppendDivElementToBody("a long-long-long text");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks::Now());
  ASSERT_EQ(LcpCandidateCount(), 2u);
}

// There is a risk that a text element that is just recorded is selected to be
// the metric candidate. The algorithm should skip the text record if its paint
// time hasn't been recorded yet.
TEST_F(TextPaintTimingDetectorTest, PendingTextIsLargest) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  SimulateRendering();
  // We do not call presentation-time callback here in order to not set the
  // paint time.
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);
}

// The same node may be visited by recordText for twice before the paint time
// is set. In some previous design, this caused the node to be recorded twice.
TEST_F(TextPaintTimingDetectorTest, VisitSameNodeTwiceBeforePaintTimeIsSet) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  SimulateRendering();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);

  // Change a property of the text to trigger repaint.
  text->setAttribute(html_names::kStyleAttr, AtomicString("color:red;"));
  SimulateRendering();
  // Clients should not be notified again for the same text.
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);

  SimulatePresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), text);

  // No change.
  SimulatePresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), text);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_ReportFirstPaintTime) {
  AdvanceClock(base::Seconds(1));
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  auto expected_presentation_time = base::TimeTicks::Now();
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            expected_presentation_time);
  ASSERT_EQ(LcpCandidateCount(), 1u);

  // Trigger a repaint of text. This should not emit a new candidate or update
  // metrics.
  AdvanceClock(base::Seconds(1));
  text->setAttribute(html_names::kStyleAttr,
                     AtomicString("position:fixed;left:30px"));
  SimulateRenderingAndPresentationTime();
  AdvanceClock(base::Seconds(1));
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            expected_presentation_time);
  ASSERT_EQ(LcpCandidateCount(), 1u);
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_IgnoreTextOutsideViewport) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      div.out {
        position: fixed;
        top: -100px;
      }
    </style>
    <div class='out'>text outside of viewport</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_RemovedText) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* large_text = AppendDivElementToBody(
      "(large text)(large text)(large text)(large text)(large text)(large "
      "text)");
  AppendDivElementToBody("small text");
  SimulateRenderingAndPresentationTime();
  auto initial_presentation_time = base::TimeTicks::Now();

  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), large_text);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            initial_presentation_time);
  uint64_t size_before_remove = LcpDetailsForReporting().text_paint_size;
  EXPECT_GT(size_before_remove, 0u);

  RemoveElement(large_text);
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), nullptr);
  // LCP values should remain unchanged.
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, size_before_remove);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            initial_presentation_time);
}

TEST_F(TextPaintTimingDetectorTest,
       DestroyLargestTextPaintMangerAfterUserInput) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(IsRecordingLargestTextPaint());

  SimulateKeyDown();
  EXPECT_FALSE(IsRecordingLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, DoNotStopRecordingLCPAfterKeyUp) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(IsRecordingLargestTextPaint());

  SimulateKeyUp();
  EXPECT_TRUE(IsRecordingLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_CompareVisualSizeNotActualSize) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("a long text", "position:fixed;left:-10px");
  Element* short_text = AppendDivElementToBody("short");
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), short_text);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_CompareSizesAtFirstPaint) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* shortening_long_text = AppendDivElementToBody("123456789");
  AppendDivElementToBody("12345678");  // 1 letter shorter than the above.
  SimulateRenderingAndPresentationTime();
  // The visual size becomes smaller when less portion intersecting with
  // viewport.
  SetElementStyle(shortening_long_text, "position:fixed;left:-10px");
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), shortening_long_text);
}

TEST_F(TextPaintTimingDetectorTest, TreatEllipsisAsText) {
  LoadAhem();
  SetMainFrameBodyContent(R"HTML(
    <div style="font:10px Ahem;white-space:nowrap;width:50px;overflow:hidden;text-overflow:ellipsis;">
    00000000000000000000000000000000000000000000000000000000000000000000000000
    00000000000000000000000000000000000000000000000000000000000000000000000000
    </div>
  )HTML");
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
  EXPECT_GT(LcpDetailsForReporting().text_paint_size, 0u);
  ASSERT_EQ(LcpCandidateCount(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, CaptureFileUploadController) {
  SetMainFrameBodyContent("<input type='file'>");
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
  EXPECT_GT(LcpDetailsForReporting().text_paint_size, 0u);
  ASSERT_EQ(LcpCandidateCount(), 1u);
  // Element attribution from shadow tree is not exposed in web performance
  // entries.
  EXPECT_EQ(CurrentLcpCandidate()->element(), nullptr);
}

TEST_F(TextPaintTimingDetectorTest, CapturingListMarkers) {
  SetMainFrameBodyContent(R"HTML(
    <ul>
      <li>List item</li>
    </ul>
    <ol>
      <li>Another list item</li>
    </ol>
  )HTML");

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 3u);
}

TEST_F(TextPaintTimingDetectorTest, CaptureSVGText) {
  SetMainFrameBodyContent(R"HTML(
    <svg height="40" width="300">
      <text x="0" y="15">A SVG text.</text>
    </svg>
  )HTML");

  auto* elem = To<SVGTextContentElement>(
      GetDocument().QuerySelector(AtomicString("text")));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), elem);
}

// This is for comparison with the ClippedByViewport test.
TEST_F(TextPaintTimingDetectorTest, NormalTextUnclipped) {
  SetMainFrameBodyContent(R"HTML(
    <div id='d'>text</div>
  )HTML");
  SimulateRendering();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, ClippedByViewport) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #d { margin-top: 1234567px }
    </style>
    <div id='d'>text</div>
  )HTML");
  SimulateRendering();
  // Make sure the margin-top is larger than the viewport height.
  EXPECT_LT(GetViewportRect(GetFrameView()).height(), 1234567);
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
}

TEST_F(TextPaintTimingDetectorTest, ClippedByParentVisibleRect) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #outer1 {
        overflow: hidden;
        height: 1px;
        width: 1px;
      }
      #outer2 {
        overflow: hidden;
        height: 2px;
        width: 2px;
      }
    </style>
    <div id='outer1'></div>
    <div id='outer2'></div>
  )HTML");
  // Rendering the initial content should be a noop.
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);

  Element* div1 = GetDocument().CreateRawElement(html_names::kDivTag);
  Text* text1 = GetDocument().createTextNode(
      "########################################################################"
      "######################################################################"
      "#");
  div1->AppendChild(text1);
  GetElementById("outer1")->AppendChild(div1);

  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), div1);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 1u);

  Element* div2 = GetDocument().CreateRawElement(html_names::kDivTag);
  Text* text2 = GetDocument().createTextNode(
      "########################################################################"
      "######################################################################"
      "#");
  div2->AppendChild(text2);
  GetElementById("outer2")->AppendChild(div2);

  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 2u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), div2);
  // This size is larger than the size of the first object . But the exact size
  // depends on different platforms. We only need to ensure this size is larger
  // than the first size.
  EXPECT_GT(LcpDetailsForReporting().text_paint_size, 1u);
}

TEST_F(TextPaintTimingDetectorTest, Iframe) {
  SetMainFrameBodyContent(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  InitializeChildFramePaintTimingClient();
  LcpTestSupport child_lcp_support(ChildDocument());

  SetChildFrameBodyContent("A");
  SimulateRendering();
  EXPECT_EQ(child_frame_client_->PaintedTextRecordCount(), 1u);
  SimulatePresentationTime();
  // Ensure main frame doesn't capture this text.
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);

  EXPECT_GT(child_lcp_support.LcpDetailsForReporting().text_paint_size, 0u);
  EXPECT_GT(child_lcp_support.LcpDetailsForReporting().text_paint_time, 0.0);
  EXPECT_EQ(child_lcp_support.LcpCandidateCount(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, Iframe_ClippedByViewport) {
  SetMainFrameBodyContent(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  InitializeChildFramePaintTimingClient();
  SetChildFrameBodyContent(R"HTML(
    <style>
      #d { margin-top: 200px }
    </style>
    <div id='d'>text</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(GetViewportRect(GetChildFrameView()).height(), 100);
  // Clients get notified in this case, but LCP filters this out because the
  // effective visual size is 0.
  EXPECT_EQ(child_frame_client_->PaintedTextRecordCount(), 1u);
  LcpTestSupport child_lcp_support(ChildDocument());
  EXPECT_EQ(child_lcp_support.LcpCandidateCount(), 0u);
  EXPECT_EQ(child_lcp_support.LcpDetailsForReporting().text_paint_size, 0u);
  EXPECT_EQ(child_lcp_support.LcpDetailsForReporting().text_paint_time, 0.0);
}

TEST_F(TextPaintTimingDetectorTest, SameSizeShouldNotBeIgnored) {
  SetMainFrameBodyContent(R"HTML(
    <div>text</div>
    <div>text</div>
    <div>text</div>
    <div>text</div>
  )HTML");
  SimulateRendering();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 4u);
}

TEST_F(TextPaintTimingDetectorTest, VisibleTextAfterUserInput) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);

  SimulateKeyDown();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, VisibleTextAfterUserScroll) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);

  SimulateScroll();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, OpacityZeroHTML) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <div>Text</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);
  EXPECT_TRUE(HasLargestIgnoredText());
  EXPECT_EQ(LcpCandidateCount(), 0u);

  // Change the opacity of documentElement, now the text should be a candidate.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
  EXPECT_GT(LcpDetailsForReporting().text_paint_size, 0u);
  EXPECT_FALSE(HasLargestIgnoredText());
  ASSERT_EQ(LcpCandidateCount(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, OpacityZeroHTML2) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #target {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <div id="target">Text</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 0"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
}

TEST_F(TextPaintTimingDetectorTest, OpacityZeroHTMLTextRecordedOnce) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <div id="target">Text</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);

  // Change the opacity of documentElement, now the <div> should be a candidate.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
  EXPECT_GT(LcpDetailsForReporting().text_paint_size, 0u);
  ASSERT_EQ(LcpCandidateCount(), 1u);

  // Update the <div>'s text. This should not cause the `target` to be
  // reconsidered for timing since it was already recorded.
  Element* target = GetElementById("target");
  To<HTMLElement>(target)->setInnerText("Text Text Text");

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
  ASSERT_EQ(LcpCandidateCount(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, OpacityZeroHTMLWithInput) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <div>Text</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);

  SimulateKeyDown();

  // Change the opacity of documentElement. The div should not be a candidate
  // because LCP stops on input. Additionally, other clients are not notified
  // about the painted text because the largest ignored text is tracked by the
  // LCP manager.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);

  // FCP should not be marked, since this feature is tied to hard LCP.
  //
  // Note: `PaintTiming` doesn't support `MockPaintTimingCallbackManager`, so
  // check the paint time instead of presentation time.
  base::TimeTicks fcp_timestamp =
      GetPaintTiming()
          .FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime();
  EXPECT_TRUE(fcp_timestamp.is_null());
}

TEST_F(TextPaintTimingDetectorTest, OpacityZeroHTMLRemoveElement) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <div id="target">Text</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_TRUE(HasLargestIgnoredText());

  RemoveElement(GetElementById("target"));
  EXPECT_FALSE(HasLargestIgnoredText());
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);
}

TEST_F(TextPaintTimingDetectorTest, LargestIgnoredTextRemovedBeforePaint) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <div id="target">Text</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_TRUE(HasLargestIgnoredText());

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(HasLargestIgnoredText());

  GetElementById("target")->remove();
  EXPECT_FALSE(HasLargestIgnoredText());

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().text_paint_size, 0u);
}

TEST_F(TextPaintTimingDetectorTest,
       QueuedRecordsWaitForCorrectPresentationFeedback) {
  SetMainFrameBodyContent(R"HTML(
    <div id="target1"></div>
    <div id="target2"></div>
  )HTML");

  // Simulate painting one of the two text divs. This should queue up a
  // presentation callback for this frame.
  Element* target1 = GetElementById("target1");
  To<HTMLElement>(target1)->setInnerText("short");
  SimulateRendering();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);
  EXPECT_EQ(LcpCandidateCount(), 0u);

  // Simulate a second text paint, before getting presentation for the first.
  // This should queue up another presentation callback, for this frame.
  Element* target2 = GetElementById("target2");
  To<HTMLElement>(target2)->setInnerText("loooooooooooooooong");
  SimulateRendering();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 2u);
  EXPECT_EQ(LcpCandidateCount(), 0u);

  // Invoking the first presentation callback should only dequeue one text
  // record, since only `target1` was painted in the first frame.
  SimulatePresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), target1);

  // And this should dequeue the record associated with `target2`, painted in
  // the second frame.
  SimulatePresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 2u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), target2);
}

namespace {

class TestClient : public GarbageCollected<TestClient>,
                   public PaintTimingClient {
 public:
  void Trace(Visitor*) const override {}

  void OnPaintFinished(
      const HeapVector<Member<ImageRecord>>&,
      const HeapVector<Member<TextRecord>>& text_records) override {
    for (auto& record : text_records) {
      record->SetIsNeededForLargestContentfulPaint(true);
    }
  }
};

}  // namespace

TEST_F(TextPaintTimingDetectorTest, NodeModifiedWhileRecordPending) {
  SetMainFrameBodyContent(R"HTML(
    <div id="target"></div>
  )HTML");

  // LCP ignores repainted elements, so ensure we can still get the timing for
  // the repaint.
  GetPaintTiming().AddClient(MakeGarbageCollected<TestClient>());

  // Simulate painting the text node. This should queue a presentation callback
  // for this frame.
  Element* target = GetElementById("target");
  To<HTMLElement>(target)->setInnerText("text");
  SimulateRendering();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 1u);

  // Now simulate modifying the same node with its eligibility reset. This
  // should queue a second entry for the same node.
  GetTextPaintTimingDetector().ResetPaintTrackingOnInteraction(
      *target->GetLayoutObject());
  To<Text>(target->firstChild())->setData("new text");
  SimulateRendering();
  EXPECT_EQ(main_frame_client_->PaintedTextRecordCount(), 2u);
}

}  // namespace blink
