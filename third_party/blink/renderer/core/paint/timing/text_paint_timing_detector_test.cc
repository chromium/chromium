// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"

#include "base/test/tracing/trace_event_analyzer.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_manager.h"
#include "third_party/blink/renderer/core/paint/timing/mock_paint_timing_callback_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_base.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class TextPaintTimingDetectorTest : public PaintTimingTestBase {
 public:
  TextPaintTimingDetectorTest() = default;

  void SetUp() override {
    PaintTimingTestBase::SetUp();
    // Cache the main frame LCP calculator so it can still be accessed after
    // input events.
    main_frame_lcp_calculator_ =
        PaintTiming::From(GetDocument())
            .GetLargestContentfulPaintManager()
            ->LargestContentfulPaintCalculatorForTest();
  }

 protected:
  LocalFrameView& GetChildFrameView() { return *ChildFrame().View(); }

  TextPaintTimingDetector& GetChildFrameTextPaintTimingDetector() {
    return PaintTimingDetector::From(ChildDocument())
        .GetTextPaintTimingDetector();
  }

  Element* GetElement(const char* name) {
    return GetDocument().getElementById(AtomicString(name));
  }

  TextPaintTimingDetector& GetTextPaintTimingDetector() {
    return GetPaintTimingDetector().GetTextPaintTimingDetector();
  }

  wtf_size_t RecordedSetSize() {
    return GetTextPaintTimingDetector().recorded_set_.size();
  }

  wtf_size_t MainFrameTextQueuedForPaintTimeSize() {
    return GetTextPaintTimingDetector().texts_queued_for_paint_time_.size();
  }

  wtf_size_t ChildFrameTextQueuedForPaintTimeSize() {
    return GetChildFrameTextPaintTimingDetector()
        .texts_queued_for_paint_time_.size();
  }

  bool HasLargestIgnoredText() {
    return PaintTiming::From(GetDocument())
        .GetLargestContentfulPaintManager()
        ->HasLargestIgnoredTextForTest();
  }

  void SimulateInputEvent() {
    GetPaintTimingDetector().NotifyInputEvent(WebInputEvent::Type::kMouseDown);
  }

  base::TimeTicks LargestPaintTime() {
    return main_frame_lcp_calculator_->LatestLcpDetails()
        .largest_text_paint_time;
  }

  uint64_t LargestPaintSize() {
    return main_frame_lcp_calculator_->LatestLcpDetails()
        .largest_text_paint_size;
  }

  void CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(wtf_size_t size) {
    SimulateRendering();
    EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), size);
    SimulatePresentationTime();
  }

  Element* AppendFontBlockToBody(String content) {
    Element* font = GetDocument().CreateRawElement(html_names::kFontTag);
    font->setAttribute(html_names::kSizeAttr, AtomicString("5"));
    Text* text = GetDocument().createTextNode(content);
    font->AppendChild(text);
    Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
    div->AppendChild(font);
    GetDocument().body()->AppendChild(div);
    return font;
  }

  Element* AppendDivElementToBody(String content, String style = "") {
    Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
    div->setAttribute(html_names::kStyleAttr, AtomicString(style));
    Text* text = GetDocument().createTextNode(content);
    div->AppendChild(text);
    GetDocument().body()->AppendChild(div);
    return div;
  }

  TextRecord* TextRecordOfLargestTextPaint() {
    return main_frame_lcp_calculator_->LargestTextForTest();
  }

  TextRecord* ChildFrameTextRecordOfLargestTextPaint() {
    LargestContentfulPaintCalculator* calculator =
        PaintTiming::From(ChildDocument())
            .GetLargestContentfulPaintManager()
            ->LargestContentfulPaintCalculatorForTest();
    return calculator->LargestTextForTest();
  }

  void SetFontSize(Element* font_element, uint16_t font_size) {
    DCHECK_EQ(font_element->nodeName(), "FONT");
    font_element->setAttribute(html_names::kSizeAttr,
                               AtomicString(String::Number(font_size)));
  }

  void SetElementStyle(Element* element, String style) {
    element->setAttribute(html_names::kStyleAttr, AtomicString(style));
  }

  void RemoveElement(Element* element) {
    element->GetLayoutObject()->Parent()->GetNode()->removeChild(element);
  }

  Persistent<LargestContentfulPaintCalculator> main_frame_lcp_calculator_;
};

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_NoText) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_OneText) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* only_text = AppendDivElementToBody("The only text");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode(), only_text);
}

TEST_F(TextPaintTimingDetectorTest, LaterSameSizeCandidate) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* first = AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();

  AppendDivElementToBody("text");
  AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode(), first);
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_FontSizeChange_MultipleUpdates) {
  SetMainFrameBodyContent(R"HTML()HTML");
  Element* text = AppendDivElementToBody("text");
  SetElementStyle(text, "font-size: 200px");
  SimulateRenderingAndPresentationTime();

  SetElementStyle(text, "font-size: 300px");
  CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(0u);
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
  Element* span = GetDocument().getElementById(AtomicString("target"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode(), span);
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
  EXPECT_EQ(TextRecordOfLargestTextPaint(), nullptr);
}

TEST_F(TextPaintTimingDetectorTest,
       NodeRemovedBeforeAssigningPresentationTime) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <div id="remove">The only text</div>
    </div>
  )HTML");
  SimulateRendering();

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("remove")));
  SimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint(), nullptr);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_LargestText) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("medium text");
  SimulateRenderingAndPresentationTime();

  Element* large_text = AppendDivElementToBody("a long-long-long text");
  SimulateRenderingAndPresentationTime();

  AppendDivElementToBody("small");
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode(), large_text);
}

TEST_F(TextPaintTimingDetectorTest, UpdateResultWhenCandidateChanged) {
  base::TimeTicks time1 = NowTicks();
  SetMainFrameBodyContent(R"HTML(
    <div>small text</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  base::TimeTicks time2 = NowTicks();
  base::TimeTicks first_largest = LargestPaintTime();
  EXPECT_GE(first_largest, time1);
  EXPECT_GE(time2, first_largest);

  AppendDivElementToBody("a long-long-long text");
  SimulateRenderingAndPresentationTime();
  base::TimeTicks time3 = NowTicks();
  base::TimeTicks second_largest = LargestPaintTime();
  EXPECT_GE(second_largest, time2);
  EXPECT_GE(time3, second_largest);
}

// There is a risk that a text that is just recorded is selected to be the
// metric candidate. The algorithm should skip the text record if its paint time
// hasn't been recorded yet.
TEST_F(TextPaintTimingDetectorTest, PendingTextIsLargest) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  SimulateRendering();
  // We do not call presentation-time callback here in order to not set the
  // paint time.
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

// The same node may be visited by recordText for twice before the paint time
// is set. In some previous design, this caused the node to be recorded twice.
TEST_F(TextPaintTimingDetectorTest, VisitSameNodeTwiceBeforePaintTimeIsSet) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  SimulateRendering();
  // Change a property of the text to trigger repaint.
  text->setAttribute(html_names::kStyleAttr, AtomicString("color:red;"));
  SimulateRendering();
  SimulatePresentationTime();
  SimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode(), text);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_ReportFirstPaintTime) {
  base::TimeTicks start_time = NowTicks();
  AdvanceClock(base::Seconds(1));
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();

  AdvanceClock(base::Seconds(1));
  text->setAttribute(html_names::kStyleAttr,
                     AtomicString("position:fixed;left:30px"));
  SimulateRenderingAndPresentationTime();
  AdvanceClock(base::Seconds(1));
  TextRecord* record = TextRecordOfLargestTextPaint();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->PaintTime(),
            start_time + base::Seconds(1) + kQuantumOfTime);
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
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_RemovedText) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* large_text = AppendDivElementToBody(
      "(large text)(large text)(large text)(large text)(large text)(large "
      "text)");
  AppendDivElementToBody("small text");
  SimulateRenderingAndPresentationTime();
  TextRecord* record = TextRecordOfLargestTextPaint();
  EXPECT_NE(record, nullptr);
  EXPECT_EQ(record->GetNode(), large_text);
  uint64_t size_before_remove = LargestPaintSize();
  base::TimeTicks time_before_remove = LargestPaintTime();
  EXPECT_GT(size_before_remove, 0u);
  EXPECT_GT(time_before_remove, base::TimeTicks());

  RemoveElement(large_text);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint(), record);
  // LCP values should remain unchanged.
  EXPECT_EQ(LargestPaintSize(), size_before_remove);
  EXPECT_EQ(LargestPaintTime(), time_before_remove);
}

TEST_F(TextPaintTimingDetectorTest,
       DestroyLargestTextPaintMangerAfterUserInput) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(GetTextPaintTimingDetector().IsRecordingLargestTextPaint());

  SimulateInputEvent();
  EXPECT_FALSE(GetTextPaintTimingDetector().IsRecordingLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, DoNotStopRecordingLCPAfterKeyUp) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(GetTextPaintTimingDetector().IsRecordingLargestTextPaint());

  SimulateKeyUp();
  EXPECT_TRUE(GetTextPaintTimingDetector().IsRecordingLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_TextRecordAfterRemoval) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text to remove");
  SimulateRenderingAndPresentationTime();
  TextRecord* record = TextRecordOfLargestTextPaint();
  EXPECT_NE(record, nullptr);
  EXPECT_EQ(record->GetNode(), text);
  base::TimeTicks largest_paint_time = LargestPaintTime();
  EXPECT_NE(largest_paint_time, base::TimeTicks());
  uint64_t largest_paint_size = LargestPaintSize();
  EXPECT_NE(largest_paint_size, 0u);

  RemoveElement(text);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint(), record);
  // LCP values should remain unchanged.
  EXPECT_EQ(largest_paint_time, LargestPaintTime());
  EXPECT_EQ(largest_paint_size, LargestPaintSize());
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_CompareVisualSizeNotActualSize) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("a long text", "position:fixed;left:-10px");
  Element* short_text = AppendDivElementToBody("short");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode(), short_text);
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
  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode(), shortening_long_text);
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

  EXPECT_EQ(RecordedSetSize(), 1u);
  EXPECT_NE(TextRecordOfLargestTextPaint(), nullptr);
}

TEST_F(TextPaintTimingDetectorTest, CaptureFileUploadController) {
  SetMainFrameBodyContent("<input type='file'>");
  Element* element = GetDocument().QuerySelector(AtomicString("input"));
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(RecordedSetSize(), 1u);
  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode()->OwnerShadowHost(),
            element);
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

  CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(3u);
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
  EXPECT_EQ(RecordedSetSize(), 1u);
  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode(), elem);
}

// This is for comparison with the ClippedByViewport test.
TEST_F(TextPaintTimingDetectorTest, NormalTextUnclipped) {
  SetMainFrameBodyContent(R"HTML(
    <div id='d'>text</div>
  )HTML");
  SimulateRendering();
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 1u);
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
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 0u);
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
  EXPECT_EQ(TextRecordOfLargestTextPaint(), nullptr);

  Element* div1 = GetDocument().CreateRawElement(html_names::kDivTag);
  Text* text1 = GetDocument().createTextNode(
      "########################################################################"
      "######################################################################"
      "#");
  div1->AppendChild(text1);
  GetDocument()
      .body()
      ->getElementById(AtomicString("outer1"))
      ->AppendChild(div1);

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode(), div1);
  EXPECT_EQ(TextRecordOfLargestTextPaint()->EffectiveVisualSize(), 1u);

  Element* div2 = GetDocument().CreateRawElement(html_names::kDivTag);
  Text* text2 = GetDocument().createTextNode(
      "########################################################################"
      "######################################################################"
      "#");
  div2->AppendChild(text2);
  GetDocument()
      .body()
      ->getElementById(AtomicString("outer2"))
      ->AppendChild(div2);

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->GetNode(), div2);
  // This size is larger than the size of the first object . But the exact size
  // depends on different platforms. We only need to ensure this size is larger
  // than the first size.
  EXPECT_GT(TextRecordOfLargestTextPaint()->EffectiveVisualSize(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, Iframe) {
  SetMainFrameBodyContent(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildFrameBodyContent("A");
  SimulateRendering();
  EXPECT_EQ(ChildFrameTextQueuedForPaintTimeSize(), 1u);
  SimulatePresentationTime();
  TextRecord* text = ChildFrameTextRecordOfLargestTextPaint();
  EXPECT_TRUE(text);
}

TEST_F(TextPaintTimingDetectorTest, Iframe_ClippedByViewport) {
  SetMainFrameBodyContent(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildFrameBodyContent(R"HTML(
    <style>
      #d { margin-top: 200px }
    </style>
    <div id='d'>text</div>
  )HTML");
  SimulateRendering();
  EXPECT_EQ(GetViewportRect(GetChildFrameView()).height(), 100);
  EXPECT_EQ(ChildFrameTextQueuedForPaintTimeSize(), 0u);
}

TEST_F(TextPaintTimingDetectorTest, SameSizeShouldNotBeIgnored) {
  SetMainFrameBodyContent(R"HTML(
    <div>text</div>
    <div>text</div>
    <div>text</div>
    <div>text</div>
  )HTML");
  CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(4u);
}

TEST_F(TextPaintTimingDetectorTest, VisibleTextAfterUserInput) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(RecordedSetSize(), 1u);

  SimulateInputEvent();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(RecordedSetSize(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, VisibleTextAfterUserScroll) {
  SetMainFrameBodyContent(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(RecordedSetSize(), 1u);

  SimulateScroll();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(RecordedSetSize(), 1u);
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
  CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(0u);
  EXPECT_TRUE(HasLargestIgnoredText());

  // Change the opacity of documentElement, now the img should be a candidate.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(TextRecordOfLargestTextPaint());
  EXPECT_FALSE(HasLargestIgnoredText());
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
  CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 0"));
  CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(0u);
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
  CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(0u);

  // Change the opacity of documentElement, now the <div> should be a candidate.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(TextRecordOfLargestTextPaint());

  // Update the <div>'s text. This should not cause the `target` to be
  // reconsidered for timing since it was already recorded.
  Element* target = GetElement("target");
  To<HTMLElement>(target)->setInnerText("Text Text Text");

  SimulateRendering();
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 0);
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
  CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(0u);

  SimulateInputEvent();

  // Change the opacity of documentElement. The div should not be a candidate
  // because LCP stops on input.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());

  // FCP should not be marked, since this feature is tied to hard LCP.
  //
  // Note: `PaintTiming` doesn't support `MockPaintTimingCallbackManager`, so
  // check the paint time instead of presentation time.
  base::TimeTicks fcp_timestamp =
      PaintTiming::From(GetDocument())
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
  CheckSizeOfTextQueuedForPaintTimeAfterBeginMainFrame(0u);
  EXPECT_TRUE(HasLargestIgnoredText());

  RemoveElement(GetElement("target"));
  EXPECT_FALSE(HasLargestIgnoredText());
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest,
       QueuedRecordsWaitForCorrectPresentationFeedback) {
  SetMainFrameBodyContent(R"HTML(
    <div id="target1"></div>
    <div id="target2"></div>
  )HTML");

  // Simulate painting one of the two text nodes. This should queue up a
  // presentation callback for this frame.
  Element* target1 = GetElement("target1");
  To<HTMLElement>(target1)->setInnerText("text 1");
  SimulateRendering();
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 1);

  // Simulate a second text paint, before getting presentation for the first.
  // This should queue up another presentation callback, for this frame.
  Element* target2 = GetElement("target2");
  To<HTMLElement>(target2)->setInnerText("text 2");
  SimulateRendering();
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 2);

  // Invoking the first presentation callback should only dequeue one text
  // record, since only `target1` was painted in the first frame.
  SimulatePresentationTime();
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 1);
  // And this should dequeue the record associated with `target2`, painted in
  // the second frame.
  SimulatePresentationTime();
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 0);
}

TEST_F(TextPaintTimingDetectorTest, NodeModifiedWhileRecordPending) {
  SetMainFrameBodyContent(R"HTML(
    <div id="target"></div>
  )HTML");

  // Simulate painting the text node. This should queue a presentation callback
  // for this frame.
  Element* target = GetElement("target");
  To<HTMLElement>(target)->setInnerText("text");
  SimulateRendering();
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 1);

  // Now simulate modifying the same node with its eligibility reset. This
  // should queue a second entry for the same node.
  GetTextPaintTimingDetector().ResetPaintTrackingOnInteraction(
      *target->GetLayoutObject());
  To<Text>(target->firstChild())->setData("new text");
  SimulateRendering();
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 2);

  SimulatePresentationTime();
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 1);

  SimulatePresentationTime();
  EXPECT_EQ(MainFrameTextQueuedForPaintTimeSize(), 0);
}

}  // namespace blink
