// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/text_paint_timing_detector.h"

#include "base/test/test_mock_time_task_runner.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_helper.h"
#include "third_party/blink/renderer/core/svg/svg_text_content_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

class TextPaintTimingDetectorTest : public testing::Test {
 public:
  TextPaintTimingDetectorTest()
      : test_task_runner_(
            base::MakeRefCounted<base::TestMockTimeTaskRunner>()) {}

  void SetUp() override {
    web_view_helper_.Initialize();

    // Enable compositing on the page before running the document lifecycle.
    web_view_helper_.GetWebView()
        ->GetPage()
        ->GetSettings()
        .SetAcceleratedCompositingEnabled(true);

    WebLocalFrameImpl& frame_impl = *web_view_helper_.LocalMainFrame();
    frame_impl.ViewImpl()->MainFrameViewWidget()->Resize(gfx::Size(640, 480));

    frame_test_helpers::LoadFrame(
        web_view_helper_.GetWebView()->MainFrameImpl(), "about:blank");
    GetDocument().View()->SetParentVisible(true);
    GetDocument().View()->SetSelfVisible(true);
    // Advance clock so it isn't 0 as rendering code asserts in that case.
    AdvanceClock(base::Microseconds(1));
  }

 protected:
  LocalFrameView& GetFrameView() { return *GetFrame()->View(); }
  PaintTimingDetector& GetPaintTimingDetector() {
    return GetFrameView().GetPaintTimingDetector();
  }
  Document& GetDocument() { return *GetFrame()->GetDocument(); }

  gfx::Rect GetViewportRect(LocalFrameView& view) {
    ScrollableArea* scrollable_area = view.GetScrollableArea();
    DCHECK(scrollable_area);
    return scrollable_area->VisibleContentRect();
  }

  LocalFrameView& GetChildFrameView() {
    return *To<LocalFrame>(GetFrame()->Tree().FirstChild())->View();
  }
  Document* GetChildDocument() {
    return To<LocalFrame>(GetFrame()->Tree().FirstChild())->GetDocument();
  }

  TextPaintTimingDetector* GetTextPaintTimingDetector() {
    return &GetPaintTimingDetector().GetTextPaintTimingDetector();
  }

  TextPaintTimingDetector& GetChildFrameTextPaintTimingDetector() {
    return GetChildFrameView()
        .GetPaintTimingDetector()
        .GetTextPaintTimingDetector();
  }

  LargestTextPaintManager& GetLargestTextPaintManager() {
    return *GetTextPaintTimingDetector()->ltp_manager_;
  }

  wtf_size_t CountRecordedSize() {
    DCHECK(GetTextPaintTimingDetector());
    return GetTextPaintTimingDetector()->recorded_set_.size();
  }

  wtf_size_t TextQueuedForPaintTimeSize(const LocalFrameView& view) {
    return view.GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        .texts_queued_for_paint_time_.size();
  }

  wtf_size_t ContainerTotalSize() {
    return CountRecordedSize() + TextQueuedForPaintTimeSize(GetFrameView());
  }

  void SimulateInputEvent() {
    GetPaintTimingDetector().NotifyInputEvent(WebInputEvent::Type::kMouseDown);
  }

  void SimulateScroll() {
    GetPaintTimingDetector().NotifyScroll(mojom::blink::ScrollType::kUser);
  }

  void SimulateKeyUp() {
    GetPaintTimingDetector().NotifyInputEvent(WebInputEvent::Type::kKeyUp);
  }

  void InvokeCallback() {
    DCHECK_GT(mock_callback_manager_->CountCallbacks(), 0u);
    InvokePresentationTimeCallback(mock_callback_manager_);
    // Outside the tests, this is invoked by
    // |PaintTimingCallbackManagerImpl::ReportPaintTime|.
    GetLargestTextPaintManager().UpdateMetricsCandidate();
  }

  void ChildFramePresentationTimeCallBack() {
    DCHECK_GT(child_frame_mock_callback_manager_->CountCallbacks(), 0u);
    InvokePresentationTimeCallback(child_frame_mock_callback_manager_);
    // Outside the tests, this is invoked by
    // |PaintTimingCallbackManagerImpl::ReportPaintTime|.
    GetChildFrameTextPaintTimingDetector().UpdateMetricsCandidate();
  }

  void InvokePresentationTimeCallback(
      MockPaintTimingCallbackManager* callback_manager) {
    callback_manager->InvokePresentationTimeCallback(
        test_task_runner_->NowTicks());
  }

  base::TimeTicks LargestPaintTime() {
    return GetPaintTimingDetector()
        .LatestLcpDetailsForTest()
        .largest_text_paint_time;
  }

  uint64_t LargestPaintSize() {
    return GetPaintTimingDetector()
        .LatestLcpDetailsForTest()
        .largest_text_paint_size;
  }

  void SetBodyInnerHTML(const std::string& content) {
    frame_test_helpers::LoadHTMLString(
        web_view_helper_.GetWebView()->MainFrameImpl(), content,
        KURL("http://test.com"));
    mock_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    GetTextPaintTimingDetector()->ResetCallbackManager(mock_callback_manager_);
    UpdateAllLifecyclePhases();
  }

  void SetChildBodyInnerHTML(const String& content) {
    GetChildDocument()->SetBaseURLOverride(KURL("http://test.com"));
    GetChildDocument()->body()->setInnerHTML(content, ASSERT_NO_EXCEPTION);
    child_frame_mock_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    GetChildFrameTextPaintTimingDetector().ResetCallbackManager(
        child_frame_mock_callback_manager_);
    UpdateAllLifecyclePhases();
  }

  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  static constexpr base::TimeDelta kQuantumOfTime = base::Milliseconds(10);

  // This only triggers ReportPresentationTime in main frame.
  void UpdateAllLifecyclePhasesAndSimulatePresentationTime() {
    UpdateAllLifecyclePhases();
    // Advance the clock for a bit so different presentation callbacks get
    // different times.
    AdvanceClock(kQuantumOfTime);
    while (mock_callback_manager_->CountCallbacks() > 0)
      InvokeCallback();
  }

  void SimulatePresentationTime() {
    AdvanceClock(kQuantumOfTime);
    while (mock_callback_manager_->CountCallbacks() > 0)
      InvokeCallback();
  }

  void CheckSizeOfTextQueuedForPaintTimeAfterUpdateLifecyclePhases(
      wtf_size_t size) {
    UpdateAllLifecyclePhases();
    EXPECT_EQ(TextQueuedForPaintTimeSize(GetFrameView()), size);
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
    return GetLargestTextPaintManager().LargestText();
  }

  TextRecord* ChildFrameTextRecordOfLargestTextPaint() {
    return GetChildFrameView()
        .GetPaintTimingDetector()
        .GetTextPaintTimingDetector()
        .ltp_manager_->LargestText();
  }

  void SetFontSize(Element* font_element, uint16_t font_size) {
    DCHECK_EQ(font_element->nodeName(), "FONT");
    font_element->setAttribute(html_names::kSizeAttr,
                               AtomicString(WTF::String::Number(font_size)));
  }

  void SetElementStyle(Element* element, String style) {
    element->setAttribute(html_names::kStyleAttr, AtomicString(style));
  }

  void RemoveElement(Element* element) {
    element->GetLayoutObject()->Parent()->GetNode()->removeChild(element);
  }

  base::TimeTicks NowTicks() const { return test_task_runner_->NowTicks(); }

  void AdvanceClock(base::TimeDelta delta) {
    test_task_runner_->FastForwardBy(delta);
  }

  void LoadAhem() { web_view_helper_.LoadAhem(); }

 private:
  LocalFrame* GetFrame() {
    return web_view_helper_.GetWebView()->MainFrameImpl()->GetFrame();
  }

  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper web_view_helper_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  Persistent<MockPaintTimingCallbackManager> mock_callback_manager_;
  Persistent<MockPaintTimingCallbackManager> child_frame_mock_callback_manager_;
};

constexpr base::TimeDelta TextPaintTimingDetectorTest::kQuantumOfTime;

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_NoText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_OneText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* only_text = AppendDivElementToBody("The only text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, only_text);
}

TEST_F(TextPaintTimingDetectorTest, LaterSameSizeCandidate) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* first = AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  AppendDivElementToBody("text");
  AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, first);
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_FontSizeChange_MultipleUpdates) {
  SetBodyInnerHTML(R"HTML()HTML");
  Element* text = AppendDivElementToBody("text");
  SetElementStyle(text, "font-size: 200px");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  SetElementStyle(text, "font-size: 300px");
  CheckSizeOfTextQueuedForPaintTimeAfterUpdateLifecyclePhases(0u);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_TraceEvent_Candidate) {
  using trace_analyzer::Query;
  trace_analyzer::Start("loading");
  {
    SetBodyInnerHTML(R"HTML(
      )HTML");
    AppendDivElementToBody("The only text");
    UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestTextPaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::Value::Dict arg_dict = events[0]->GetKnownArgAsDict("data");
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
  using trace_analyzer::Query;
  trace_analyzer::Start("loading");
  {
    GetDocument().SetBaseURLOverride(KURL("http://test.com"));
    SetBodyInnerHTML(R"HTML(
      <style>body { margin: 15px; } iframe { display: block; position: relative; margin-top: 50px; } </style>
      <iframe> </iframe>
    )HTML");
    SetChildBodyInnerHTML(R"HTML(
    <style>body { margin: 10px;} #target { width: 200px; height: 200px; }
    </style>
    <div>Some content</div>
  )HTML");
    UpdateAllLifecyclePhasesAndSimulatePresentationTime();
    ChildFramePresentationTimeCallBack();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestTextPaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::Value::Dict arg_dict = events[0]->GetKnownArgAsDict("data");
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
  SetBodyInnerHTML(R"HTML(
    <div style="background: yellow">
      tiny
      <span id="target"
        style="position: relative; background: blue; top: 100px; left: 100px">
        this is the largest text in the world.</span>
    </div>
  )HTML");
  Element* span = GetDocument().getElementById(AtomicString("target"));
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, span);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_OpacityZero) {
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      opacity: 0;
    }
    </style>
  )HTML");
  AppendDivElementToBody("The only text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint(), nullptr);
}

TEST_F(TextPaintTimingDetectorTest,
       NodeRemovedBeforeAssigningPresentationTime) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <div id="remove">The only text</div>
    </div>
  )HTML");
  UpdateAllLifecyclePhases();
  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("remove")));
  InvokeCallback();
  EXPECT_EQ(TextRecordOfLargestTextPaint(), nullptr);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_LargestText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("medium text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  Element* large_text = AppendDivElementToBody("a long-long-long text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  AppendDivElementToBody("small");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, large_text);
}

TEST_F(TextPaintTimingDetectorTest, UpdateResultWhenCandidateChanged) {
  base::TimeTicks time1 = NowTicks();
  SetBodyInnerHTML(R"HTML(
    <div>small text</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  base::TimeTicks time2 = NowTicks();
  base::TimeTicks first_largest = LargestPaintTime();
  EXPECT_GE(first_largest, time1);
  EXPECT_GE(time2, first_largest);

  AppendDivElementToBody("a long-long-long text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  base::TimeTicks time3 = NowTicks();
  base::TimeTicks second_largest = LargestPaintTime();
  EXPECT_GE(second_largest, time2);
  EXPECT_GE(time3, second_largest);
}

// There is a risk that a text that is just recorded is selected to be the
// metric candidate. The algorithm should skip the text record if its paint time
// hasn't been recorded yet.
TEST_F(TextPaintTimingDetectorTest, PendingTextIsLargest) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  GetFrameView().UpdateAllLifecyclePhasesForTest();
  // We do not call presentation-time callback here in order to not set the
  // paint time.
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

// The same node may be visited by recordText for twice before the paint time
// is set. In some previous design, this caused the node to be recorded twice.
TEST_F(TextPaintTimingDetectorTest, VisitSameNodeTwiceBeforePaintTimeIsSet) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  GetFrameView().UpdateAllLifecyclePhasesForTest();
  // Change a property of the text to trigger repaint.
  text->setAttribute(html_names::kStyleAttr, AtomicString("color:red;"));
  GetFrameView().UpdateAllLifecyclePhasesForTest();
  InvokeCallback();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, text);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_ReportFirstPaintTime) {
  base::TimeTicks start_time = NowTicks();
  AdvanceClock(base::Seconds(1));
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  AdvanceClock(base::Seconds(1));
  text->setAttribute(html_names::kStyleAttr,
                     AtomicString("position:fixed;left:30px"));
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  AdvanceClock(base::Seconds(1));
  TextRecord* record = TextRecordOfLargestTextPaint();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->paint_time, start_time + base::Seconds(1) + kQuantumOfTime);
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_IgnoreTextOutsideViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div.out {
        position: fixed;
        top: -100px;
      }
    </style>
    <div class='out'>text outside of viewport</div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_FALSE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_RemovedText) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* large_text = AppendDivElementToBody(
      "(large text)(large text)(large text)(large text)(large text)(large "
      "text)");
  AppendDivElementToBody("small text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  TextRecord* record = TextRecordOfLargestTextPaint();
  EXPECT_NE(record, nullptr);
  EXPECT_EQ(record->node_, large_text);
  uint64_t size_before_remove = LargestPaintSize();
  base::TimeTicks time_before_remove = LargestPaintTime();
  EXPECT_GT(size_before_remove, 0u);
  EXPECT_GT(time_before_remove, base::TimeTicks());

  RemoveElement(large_text);
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint(), record);
  // LCP values should remain unchanged.
  EXPECT_EQ(LargestPaintSize(), size_before_remove);
  EXPECT_EQ(LargestPaintTime(), time_before_remove);
}

TEST_F(TextPaintTimingDetectorTest,
       RemoveRecordFromAllContainerAfterTextRemoval) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 1u);

  RemoveElement(text);
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_F(TextPaintTimingDetectorTest,
       RemoveRecordFromAllContainerAfterRepeatedAttachAndDetach) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text1 = AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 1u);

  Element* text2 = AppendDivElementToBody("text2");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 2u);

  RemoveElement(text1);
  EXPECT_EQ(ContainerTotalSize(), 1u);

  GetDocument().body()->AppendChild(text1);
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 2u);

  RemoveElement(text1);
  EXPECT_EQ(ContainerTotalSize(), 1u);

  RemoveElement(text2);
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_F(TextPaintTimingDetectorTest,
       DestroyLargestTextPaintMangerAfterUserInput) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_TRUE(GetTextPaintTimingDetector()->IsRecordingLargestTextPaint());

  SimulateInputEvent();
  EXPECT_FALSE(GetTextPaintTimingDetector()->IsRecordingLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, DoNotStopRecordingLCPAfterKeyUp) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_TRUE(GetTextPaintTimingDetector()->IsRecordingLargestTextPaint());

  SimulateKeyUp();
  EXPECT_TRUE(GetTextPaintTimingDetector()->IsRecordingLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_TextRecordAfterRemoval) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* text = AppendDivElementToBody("text to remove");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  TextRecord* record = TextRecordOfLargestTextPaint();
  EXPECT_NE(record, nullptr);
  EXPECT_EQ(record->node_, text);
  base::TimeTicks largest_paint_time = LargestPaintTime();
  EXPECT_NE(largest_paint_time, base::TimeTicks());
  uint64_t largest_paint_size = LargestPaintSize();
  EXPECT_NE(largest_paint_size, 0u);

  RemoveElement(text);
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint(), record);
  // LCP values should remain unchanged.
  EXPECT_EQ(largest_paint_time, LargestPaintTime());
  EXPECT_EQ(largest_paint_size, LargestPaintSize());
}

TEST_F(TextPaintTimingDetectorTest,
       LargestTextPaint_CompareVisualSizeNotActualSize) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("a long text", "position:fixed;left:-10px");
  Element* short_text = AppendDivElementToBody("short");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, short_text);
}

TEST_F(TextPaintTimingDetectorTest, LargestTextPaint_CompareSizesAtFirstPaint) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  Element* shortening_long_text = AppendDivElementToBody("123456789");
  AppendDivElementToBody("12345678");  // 1 letter shorter than the above.
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  // The visual size becomes smaller when less portion intersecting with
  // viewport.
  SetElementStyle(shortening_long_text, "position:fixed;left:-10px");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, shortening_long_text);
}

TEST_F(TextPaintTimingDetectorTest, TreatEllipsisAsText) {
  LoadAhem();
  SetBodyInnerHTML(R"HTML(
    <div style="font:10px Ahem;white-space:nowrap;width:50px;overflow:hidden;text-overflow:ellipsis;">
    00000000000000000000000000000000000000000000000000000000000000000000000000
    00000000000000000000000000000000000000000000000000000000000000000000000000
    </div>
  )HTML");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  EXPECT_EQ(CountRecordedSize(), 1u);
  EXPECT_NE(TextRecordOfLargestTextPaint(), nullptr);
}

TEST_F(TextPaintTimingDetectorTest, CaptureFileUploadController) {
  SetBodyInnerHTML("<input type='file'>");
  Element* element = GetDocument().QuerySelector(AtomicString("input"));
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();

  EXPECT_EQ(CountRecordedSize(), 1u);
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, element);
}

TEST_F(TextPaintTimingDetectorTest, CapturingListMarkers) {
  SetBodyInnerHTML(R"HTML(
    <ul>
      <li>List item</li>
    </ul>
    <ol>
      <li>Another list item</li>
    </ol>
  )HTML");

  CheckSizeOfTextQueuedForPaintTimeAfterUpdateLifecyclePhases(3u);
}

TEST_F(TextPaintTimingDetectorTest, CaptureSVGText) {
  SetBodyInnerHTML(R"HTML(
    <svg height="40" width="300">
      <text x="0" y="15">A SVG text.</text>
    </svg>
  )HTML");

  auto* elem = To<SVGTextContentElement>(
      GetDocument().QuerySelector(AtomicString("text")));
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountRecordedSize(), 1u);
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, elem);
}

// This is for comparison with the ClippedByViewport test.
TEST_F(TextPaintTimingDetectorTest, NormalTextUnclipped) {
  SetBodyInnerHTML(R"HTML(
    <div id='d'>text</div>
  )HTML");
  EXPECT_EQ(TextQueuedForPaintTimeSize(GetFrameView()), 1u);
}

TEST_F(TextPaintTimingDetectorTest, ClippedByViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #d { margin-top: 1234567px }
    </style>
    <div id='d'>text</div>
  )HTML");
  // Make sure the margin-top is larger than the viewport height.
  DCHECK_LT(GetViewportRect(GetFrameView()).height(), 1234567);
  EXPECT_EQ(TextQueuedForPaintTimeSize(GetFrameView()), 0u);
}

TEST_F(TextPaintTimingDetectorTest, ClippedByParentVisibleRect) {
  SetBodyInnerHTML(R"HTML(
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

  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, div1);
  EXPECT_EQ(TextRecordOfLargestTextPaint()->recorded_size, 1u);

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

  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(TextRecordOfLargestTextPaint()->node_, div2);
  // This size is larger than the size of the first object . But the exact size
  // depends on different platforms. We only need to ensure this size is larger
  // than the first size.
  EXPECT_GT(TextRecordOfLargestTextPaint()->recorded_size, 1u);
}

TEST_F(TextPaintTimingDetectorTest, Iframe) {
  SetBodyInnerHTML(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildBodyInnerHTML("A");
  UpdateAllLifecyclePhases();
  EXPECT_EQ(TextQueuedForPaintTimeSize(GetChildFrameView()), 1u);
  ChildFramePresentationTimeCallBack();
  TextRecord* text = ChildFrameTextRecordOfLargestTextPaint();
  EXPECT_TRUE(text);
}

TEST_F(TextPaintTimingDetectorTest, Iframe_ClippedByViewport) {
  SetBodyInnerHTML(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildBodyInnerHTML(R"HTML(
    <style>
      #d { margin-top: 200px }
    </style>
    <div id='d'>text</div>
  )HTML");
  DCHECK_EQ(GetViewportRect(GetChildFrameView()).height(), 100);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(TextQueuedForPaintTimeSize(GetChildFrameView()), 0u);
}

TEST_F(TextPaintTimingDetectorTest, SameSizeShouldNotBeIgnored) {
  SetBodyInnerHTML(R"HTML(
    <div>text</div>
    <div>text</div>
    <div>text</div>
    <div>text</div>
  )HTML");
  CheckSizeOfTextQueuedForPaintTimeAfterUpdateLifecyclePhases(4u);
}

TEST_F(TextPaintTimingDetectorTest, VisibleTextAfterUserInput) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountRecordedSize(), 1u);

  SimulateInputEvent();
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountRecordedSize(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, VisibleTextAfterUserScroll) {
  SetBodyInnerHTML(R"HTML(
  )HTML");
  AppendDivElementToBody("text");
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountRecordedSize(), 1u);

  SimulateScroll();
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_EQ(CountRecordedSize(), 1u);
}

TEST_F(TextPaintTimingDetectorTest, OpacityZeroHTML) {
  SetBodyInnerHTML(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <div>Text</div>
  )HTML");
  CheckSizeOfTextQueuedForPaintTimeAfterUpdateLifecyclePhases(0u);

  // Change the opacity of documentElement, now the img should be a candidate.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  UpdateAllLifecyclePhasesAndSimulatePresentationTime();
  EXPECT_TRUE(TextRecordOfLargestTextPaint());
}

TEST_F(TextPaintTimingDetectorTest, OpacityZeroHTML2) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <div id="target">Text</div>
  )HTML");
  CheckSizeOfTextQueuedForPaintTimeAfterUpdateLifecyclePhases(0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 0"));
  CheckSizeOfTextQueuedForPaintTimeAfterUpdateLifecyclePhases(0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  CheckSizeOfTextQueuedForPaintTimeAfterUpdateLifecyclePhases(0u);
}

}  // namespace blink
