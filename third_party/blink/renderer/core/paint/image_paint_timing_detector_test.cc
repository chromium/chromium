// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"

#include "base/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/web/web_performance.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/paint/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/paint_timing_test_helper.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_timing.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

#define SIMPLE_IMAGE       \
  "data:image/gif;base64," \
  "R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="

#define LARGE_IMAGE                                                            \
  "data:image/gif;base64,"                                                     \
  "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfAhkiAAAAAlwSF" \
  "lzAAAN1wAADdcBQiibeAAAAb5JREFUOMulkr1KA0EQgGdvTwwnYmER0gQsrFKmSy+pLESw9Qm0" \
  "F/ICNnba+h6iEOuAEWslKJKTOyJJvIT72d1xZuOFC0giOLA77O7Mt/PnNptN+I+49Xr9GhH3f3" \
  "mb0v1ht9vtLAUYYw5ItkgDL3KyD8PhcLvdbl/WarXT3DjLMnAcR/f7/YfxeKwtgC5RKQVhGILW" \
  "eg4hQ6hUKjWyucmhLFEUuWR3QYBWAZABQ9i5CCmXy16pVALP80BKaaG+70MQBLvzFMjRKKXh8j" \
  "6FSYKF7ITdEWLa4/ktokN74wiqjSMpnVcbQZqmEJHz+ckeCPFjWKwULpyspAqhdXVXdcnZcPjs" \
  "Ign+2BsVA8jVYuWlgJ3yBj0icgq2uoK+lg4t+ZvLomSKamSQ4AI5BcMADtMhyNoSgNIISUaFNt" \
  "wlazcDcBc4gjjVwCWid2usCWroYEhnaqbzFJLUzAHIXRDChXCcQP8zhkSZ5eNLgHAUzwDcRu4C" \
  "oIRn/wsGUQIIy4Vr9TH6SYFCNzw4nALn5627K4vIttOUOwfa5YnrDYzt/9OLv9I5l8kk5hZ3XL" \
  "O20b7tbR7zHLy/BX8G0IeBEM7ZN1NGIaFUaKLgAAAAAElFTkSuQmCC"

using UkmPaintTiming = ukm::builders::Blink_PaintTiming;

class ImagePaintTimingDetectorTest : public testing::Test,
                                     public PaintTestConfigurations {
 public:
  ImagePaintTimingDetectorTest()
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
  }

 protected:
  LocalFrameView& GetFrameView() { return *GetFrame()->View(); }
  LocalFrameView& GetChildFrameView() { return *GetChildFrame()->View(); }
  Document& GetDocument() { return *GetFrame()->GetDocument(); }
  Document* GetChildDocument() { return GetChildFrame()->GetDocument(); }
  PaintTimingDetector& GetPaintTimingDetector() {
    return GetFrameView().GetPaintTimingDetector();
  }
  PaintTimingDetector& GetChildPaintTimingDetector() {
    return GetChildFrameView().GetPaintTimingDetector();
  }

  const PerformanceTiming& GetPerformanceTiming() {
    PerformanceTiming* performance =
        DOMWindowPerformance::performance(*GetFrame()->DomWindow())->timing();
    return *performance;
  }

  IntRect GetViewportRect(LocalFrameView& view) {
    ScrollableArea* scrollable_area = view.GetScrollableArea();
    DCHECK(scrollable_area);
    return scrollable_area->VisibleContentRect();
  }

  ImageRecord* FindLargestPaintCandidate() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        ->records_manager_.FindLargestPaintCandidate();
  }

  ImageRecord* FindChildFrameLargestPaintCandidate() {
    return GetChildFrameView()
        .GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        ->records_manager_.FindLargestPaintCandidate();
  }

  size_t CountVisibleImageRecords() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        ->records_manager_.visible_images_.size();
  }

  size_t CountInvisibleRecords() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        ->records_manager_.invisible_images_.size();
  }

  size_t ContainerTotalSize() {
    return GetPaintTimingDetector()
               .GetImagePaintTimingDetector()
               ->records_manager_.invisible_images_.size() +
           GetPaintTimingDetector()
               .GetImagePaintTimingDetector()
               ->records_manager_.visible_images_.size() +
           GetPaintTimingDetector()
               .GetImagePaintTimingDetector()
               ->records_manager_.size_ordered_set_.size() +
           GetPaintTimingDetector()
               .GetImagePaintTimingDetector()
               ->records_manager_.images_queued_for_paint_time_.size() +
           GetPaintTimingDetector()
               .GetImagePaintTimingDetector()
               ->records_manager_.image_finished_times_.size();
  }

  size_t CountChildFrameRecords() {
    return GetChildPaintTimingDetector()
        .GetImagePaintTimingDetector()
        ->records_manager_.visible_images_.size();
  }

  size_t CountRankingSetRecords() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        ->records_manager_.size_ordered_set_.size();
  }

  void UpdateCandidate() {
    GetPaintTimingDetector().GetImagePaintTimingDetector()->UpdateCandidate();
  }

  void UpdateCandidateForChildFrame() {
    GetChildPaintTimingDetector()
        .GetImagePaintTimingDetector()
        ->UpdateCandidate();
  }

  base::TimeTicks LargestPaintTime() {
    return GetPaintTimingDetector().largest_image_paint_time_;
  }

  uint64_t LargestPaintSize() {
    return GetPaintTimingDetector().largest_image_paint_size_;
  }

  base::TimeTicks ExperimentalLargestPaintTime() {
    return GetPaintTimingDetector().experimental_largest_image_paint_time_;
  }

  uint64_t ExperimentalLargestPaintSize() {
    return GetPaintTimingDetector().experimental_largest_image_paint_size_;
  }

  static constexpr base::TimeDelta kQuantumOfTime =
      base::TimeDelta::FromMilliseconds(10);

  void SimulatePassOfTime() {
    test_task_runner_->FastForwardBy(kQuantumOfTime);
  }

  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  }

  void UpdateAllLifecyclePhasesAndInvokeCallbackIfAny() {
    UpdateAllLifecyclePhases();
    SimulatePassOfTime();
    while (mock_callback_manager_->CountCallbacks() > 0)
      InvokePresentationTimeCallback(mock_callback_manager_);
  }

  void SetBodyInnerHTML(const std::string& content) {
    frame_test_helpers::LoadHTMLString(
        web_view_helper_.GetWebView()->MainFrameImpl(), content,
        KURL("http://test.com"));
    mock_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        ->ResetCallbackManager(mock_callback_manager_);
    UpdateAllLifecyclePhases();
  }

  void SetChildBodyInnerHTML(const String& content) {
    GetChildDocument()->SetBaseURLOverride(KURL("http://test.com"));
    GetChildDocument()->body()->setInnerHTML(content, ASSERT_NO_EXCEPTION);
    child_mock_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    GetChildPaintTimingDetector()
        .GetImagePaintTimingDetector()
        ->ResetCallbackManager(child_mock_callback_manager_);
    UpdateAllLifecyclePhases();
  }

  void InvokeCallback() {
    DCHECK_GT(mock_callback_manager_->CountCallbacks(), 0UL);
    InvokePresentationTimeCallback(mock_callback_manager_);
  }

  void InvokeChildFrameCallback() {
    DCHECK_GT(child_mock_callback_manager_->CountCallbacks(), 0UL);
    InvokePresentationTimeCallback(child_mock_callback_manager_);
    UpdateCandidateForChildFrame();
  }

  void InvokePresentationTimeCallback(
      MockPaintTimingCallbackManager* image_callback_manager) {
    image_callback_manager->InvokePresentationTimeCallback(
        test_task_runner_->NowTicks());
    UpdateCandidate();
  }

  void SetImageAndPaint(AtomicString id, int width, int height) {
    Element* element = GetDocument().getElementById(id);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    To<HTMLImageElement>(element)->SetImageForTest(content);
  }

  void SetChildFrameImageAndPaint(AtomicString id, int width, int height) {
    DCHECK(GetChildDocument());
    Element* element = GetChildDocument()->getElementById(id);
    DCHECK(element);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    To<HTMLImageElement>(element)->SetImageForTest(content);
  }

  void SetSVGImageAndPaint(AtomicString id, int width, int height) {
    Element* element = GetDocument().getElementById(id);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    To<SVGImageElement>(element)->SetImageForTest(content);
  }

  void SimulateScroll() {
    GetPaintTimingDetector().NotifyScroll(mojom::blink::ScrollType::kUser);
  }

  void SimulateKeyDown() {
    GetPaintTimingDetector().NotifyInputEvent(WebInputEvent::Type::kKeyDown);
  }

  void SimulateKeyUp() {
    GetPaintTimingDetector().NotifyInputEvent(WebInputEvent::Type::kKeyUp);
  }

  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  frame_test_helpers::WebViewHelper web_view_helper_;

 private:
  LocalFrame* GetFrame() {
    return web_view_helper_.GetWebView()->MainFrameImpl()->GetFrame();
  }
  LocalFrame* GetChildFrame() {
    return To<LocalFrame>(GetFrame()->Tree().FirstChild());
  }
  ImageResourceContent* CreateImageForTest(int width, int height) {
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurface::MakeRaster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    ImageResourceContent* original_image_content =
        ImageResourceContent::CreateLoaded(
            UnacceleratedStaticBitmapImage::Create(image).get());
    return original_image_content;
  }

  PaintTimingCallbackManager::CallbackQueue callback_queue_;
  Persistent<MockPaintTimingCallbackManager> mock_callback_manager_;
  Persistent<MockPaintTimingCallbackManager> child_mock_callback_manager_;
};

constexpr base::TimeDelta ImagePaintTimingDetectorTest::kQuantumOfTime;

INSTANTIATE_PAINT_TEST_SUITE_P(ImagePaintTimingDetectorTest);

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_NoImage) {
  SetBodyInnerHTML(R"HTML(
    <div></div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
  EXPECT_EQ(ExperimentalLargestPaintTime(), base::TimeTicks());
  EXPECT_EQ(ExperimentalLargestPaintSize(), 0ul);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_OneImage) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25ul);
  EXPECT_TRUE(record->loaded);
  EXPECT_EQ(ExperimentalLargestPaintSize(), 25ul);
  // Simulate some input event to force StopRecordEntries().
  SimulateKeyDown();
  auto entries = test_ukm_recorder.GetEntriesByName(UkmPaintTiming::kEntryName);
  EXPECT_EQ(1ul, entries.size());
  auto* entry = entries[0];
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmPaintTiming::kLCPDebugging_HasViewportImageName, false);
}

TEST_P(ImagePaintTimingDetectorTest, InsertionOrderIsSecondaryRankingKey) {
  SetBodyInnerHTML(R"HTML(
  )HTML");

  auto* image1 = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image1->setAttribute("id", "image1");
  GetDocument().body()->AppendChild(image1);
  SetImageAndPaint("image1", 5, 5);

  auto* image2 = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image2->setAttribute("id", "image2");
  GetDocument().body()->AppendChild(image2);
  SetImageAndPaint("image2", 5, 5);

  auto* image3 = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image3->setAttribute("id", "image3");
  GetDocument().body()->AppendChild(image3);
  SetImageAndPaint("image3", 5, 5);

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();

  EXPECT_EQ(FindLargestPaintCandidate()->node_id,
            DOMNodeIds::ExistingIdForNode(image1));
  EXPECT_EQ(ExperimentalLargestPaintSize(), 25ul);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_TraceEvent_Candidate) {
  using trace_analyzer::Query;
  trace_analyzer::Start("loading");
  {
    SetBodyInnerHTML(R"HTML(
      <img id="target"></img>
    )HTML");
    SetImageAndPaint("target", 5, 5);
    UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestImagePaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasArg("frame"));

  EXPECT_TRUE(events[0]->HasArg("data"));
  base::Value arg;
  EXPECT_TRUE(events[0]->GetArgAsValue("data", &arg));
  base::DictionaryValue* arg_dict;
  EXPECT_TRUE(arg.GetAsDictionary(&arg_dict));
  DOMNodeId node_id;
  EXPECT_TRUE(arg_dict->GetInteger("DOMNodeId", &node_id));
  EXPECT_GT(node_id, 0);
  int size;
  EXPECT_TRUE(arg_dict->GetInteger("size", &size));
  EXPECT_GT(size, 0);
  DOMNodeId candidate_index;
  EXPECT_TRUE(arg_dict->GetInteger("candidateIndex", &candidate_index));
  EXPECT_EQ(candidate_index, 2);
  bool isMainFrame;
  EXPECT_TRUE(arg_dict->GetBoolean("isMainFrame", &isMainFrame));
  EXPECT_EQ(true, isMainFrame);
  bool isOOPIF;
  EXPECT_TRUE(arg_dict->GetBoolean("isOOPIF", &isOOPIF));
  EXPECT_EQ(false, isOOPIF);
  int x;
  EXPECT_TRUE(arg_dict->GetInteger("frame_x", &x));
  EXPECT_EQ(x, 8);
  int y;
  EXPECT_TRUE(arg_dict->GetInteger("frame_y", &y));
  EXPECT_EQ(y, 8);
  int width;
  EXPECT_TRUE(arg_dict->GetInteger("frame_width", &width));
  EXPECT_EQ(width, 5);
  int height;
  EXPECT_TRUE(arg_dict->GetInteger("frame_height", &height));
  EXPECT_EQ(height, 5);
  EXPECT_TRUE(arg_dict->GetInteger("root_x", &x));
  EXPECT_EQ(x, 8);
  EXPECT_TRUE(arg_dict->GetInteger("root_y", &y));
  EXPECT_EQ(y, 8);
  EXPECT_TRUE(arg_dict->GetInteger("root_width", &width));
  EXPECT_EQ(width, 5);
  EXPECT_TRUE(arg_dict->GetInteger("root_height", &height));
  EXPECT_EQ(height, 5);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_TraceEvent_Candidate_Frame) {
  using trace_analyzer::Query;
  trace_analyzer::Start("loading");
  {
    GetDocument().SetBaseURLOverride(KURL("http://test.com"));
    SetBodyInnerHTML(R"HTML(
      <style>iframe { display: block; position: relative; margin-left: 30px; margin-top: 50px; width: 250px; height: 250px;} </style>
      <iframe> </iframe>
    )HTML");
    SetChildBodyInnerHTML(R"HTML(
    <style>body { margin: 10px;} #target { width: 200px; height: 200px; }
    </style>
    <img id="target"></img>
  )HTML");
    SetChildFrameImageAndPaint("target", 5, 5);
    UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
    InvokeChildFrameCallback();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestImagePaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasArg("frame"));

  EXPECT_TRUE(events[0]->HasArg("data"));
  base::Value arg;
  EXPECT_TRUE(events[0]->GetArgAsValue("data", &arg));
  base::DictionaryValue* arg_dict;
  EXPECT_TRUE(arg.GetAsDictionary(&arg_dict));
  DOMNodeId node_id;
  EXPECT_TRUE(arg_dict->GetInteger("DOMNodeId", &node_id));
  EXPECT_GT(node_id, 0);
  int size;
  EXPECT_TRUE(arg_dict->GetInteger("size", &size));
  EXPECT_GT(size, 0);
  DOMNodeId candidate_index;
  EXPECT_TRUE(arg_dict->GetInteger("candidateIndex", &candidate_index));
  EXPECT_EQ(candidate_index, 2);
  bool isMainFrame;
  EXPECT_TRUE(arg_dict->GetBoolean("isMainFrame", &isMainFrame));
  EXPECT_EQ(false, isMainFrame);
  bool isOOPIF;
  EXPECT_TRUE(arg_dict->GetBoolean("isOOPIF", &isOOPIF));
  EXPECT_EQ(false, isOOPIF);
  int x;
  EXPECT_TRUE(arg_dict->GetInteger("frame_x", &x));
  EXPECT_EQ(x, 10);
  int y;
  EXPECT_TRUE(arg_dict->GetInteger("frame_y", &y));
  EXPECT_EQ(y, 10);
  int width;
  EXPECT_TRUE(arg_dict->GetInteger("frame_width", &width));
  EXPECT_EQ(width, 200);
  int height;
  EXPECT_TRUE(arg_dict->GetInteger("frame_height", &height));
  EXPECT_EQ(height, 200);
  EXPECT_TRUE(arg_dict->GetInteger("root_x", &x));
  EXPECT_GT(x, 40);
  EXPECT_TRUE(arg_dict->GetInteger("root_y", &y));
  EXPECT_GT(y, 60);
  EXPECT_TRUE(arg_dict->GetInteger("root_width", &width));
  EXPECT_EQ(width, 200);
  EXPECT_TRUE(arg_dict->GetInteger("root_height", &height));
  EXPECT_EQ(height, 200);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_TraceEvent_NoCandidate) {
  using trace_analyzer::Query;
  trace_analyzer::Start("*");
  {
    SetBodyInnerHTML(R"HTML(
      <img id="target"></img>
    )HTML");
    SetImageAndPaint("target", 5, 5);
    UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
    GetDocument().getElementById("target")->remove();
    UpdateAllLifecyclePhases();
    // LCP size still 25, not affected by removal.
    EXPECT_EQ(LargestPaintSize(), 25ul);
    EXPECT_EQ(ExperimentalLargestPaintSize(), 0ul);
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestImagePaint::NoCandidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(2u, events.size());

  {
    EXPECT_EQ("loading", events[0]->category);
    EXPECT_TRUE(events[0]->HasArg("frame"));
    EXPECT_TRUE(events[0]->HasArg("data"));
    base::Value arg;
    EXPECT_TRUE(events[0]->GetArgAsValue("data", &arg));
    base::DictionaryValue* arg_dict;
    EXPECT_TRUE(arg.GetAsDictionary(&arg_dict));
    DOMNodeId candidate_index;
    EXPECT_TRUE(arg_dict->GetInteger("candidateIndex", &candidate_index));
    EXPECT_EQ(candidate_index, 1);
    bool is_main_frame;
    EXPECT_TRUE(arg_dict->GetBoolean("isMainFrame", &is_main_frame));
    EXPECT_EQ(true, is_main_frame);
    bool is_oopif;
    EXPECT_TRUE(arg_dict->GetBoolean("isOOPIF", &is_oopif));
    EXPECT_EQ(false, is_oopif);
  }

  // Use block to reuse the temp variable names.
  {
    EXPECT_TRUE(events[1]->HasArg("data"));
    base::Value arg;
    EXPECT_TRUE(events[1]->GetArgAsValue("data", &arg));
    base::DictionaryValue* arg_dict;
    EXPECT_TRUE(arg.GetAsDictionary(&arg_dict));
    DOMNodeId candidate_index;
    EXPECT_TRUE(arg_dict->GetInteger("candidateIndex", &candidate_index));
    EXPECT_EQ(candidate_index, 3);
  }
}

TEST_P(ImagePaintTimingDetectorTest, UpdatePerformanceTiming) {
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 0u);
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaint(), 0u);
  EXPECT_EQ(GetPerformanceTiming().ExperimentalLargestImagePaintSize(), 0u);
  EXPECT_EQ(GetPerformanceTiming().ExperimentalLargestImagePaint(), 0u);
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 25u);
  EXPECT_GT(GetPerformanceTiming().LargestImagePaint(), 0u);
  EXPECT_EQ(GetPerformanceTiming().ExperimentalLargestImagePaintSize(), 25u);
  EXPECT_GT(GetPerformanceTiming().ExperimentalLargestImagePaint(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       PerformanceTimingHasZeroTimeNonZeroSizeWhenTheLargestIsNotPainted) {
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 0u);
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaint(), 0u);
  EXPECT_EQ(GetPerformanceTiming().ExperimentalLargestImagePaintSize(), 0u);
  EXPECT_EQ(GetPerformanceTiming().ExperimentalLargestImagePaint(), 0u);
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 25u);
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaint(), 0u);
  EXPECT_EQ(GetPerformanceTiming().ExperimentalLargestImagePaintSize(), 25u);
  EXPECT_EQ(GetPerformanceTiming().ExperimentalLargestImagePaint(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, UpdatePerformanceTimingToZero) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 25u);
  EXPECT_GT(GetPerformanceTiming().LargestImagePaint(), 0u);
  EXPECT_EQ(GetPerformanceTiming().ExperimentalLargestImagePaintSize(), 25u);
  EXPECT_GT(GetPerformanceTiming().ExperimentalLargestImagePaint(), 0u);
  GetDocument().body()->RemoveChild(GetDocument().getElementById("target"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 25u);
  EXPECT_GT(GetPerformanceTiming().LargestImagePaint(), 0u);
  // Experimental values are reset.
  EXPECT_EQ(GetPerformanceTiming().ExperimentalLargestImagePaintSize(), 0u);
  EXPECT_EQ(GetPerformanceTiming().ExperimentalLargestImagePaint(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_OpacityZero) {
  SetBodyInnerHTML(R"HTML(
    <style>
    img {
      opacity: 0;
    }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_VisibilityHidden) {
  SetBodyInnerHTML(R"HTML(
    <style>
    img {
      visibility: hidden;
    }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_DisplayNone) {
  SetBodyInnerHTML(R"HTML(
    <style>
    img {
      display: none;
    }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_OpacityNonZero) {
  SetBodyInnerHTML(R"HTML(
    <style>
    img {
      opacity: 0.01;
    }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 1u);
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
}

TEST_P(ImagePaintTimingDetectorTest,
       IgnoreImageUntilInvalidatedRectSizeNonZero) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(CountVisibleImageRecords(), 1u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_Largest) {
  SetBodyInnerHTML(R"HTML(
    <style>img { display:block }</style>
    <img id="smaller"></img>
    <img id="medium"></img>
    <img id="larger"></img>
  )HTML");
  SetImageAndPaint("smaller", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25ul);

  SetImageAndPaint("larger", 9, 9);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(LargestPaintSize(), 81ul);
  EXPECT_EQ(ExperimentalLargestPaintSize(), 81ul);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_IgnoreThoseOutsideViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      img {
        position: fixed;
        top: -100px;
      }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_UpdateOnRemovingTheLastImage) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  EXPECT_EQ(LargestPaintSize(), 25ul);
  EXPECT_NE(ExperimentalLargestPaintTime(), base::TimeTicks());
  EXPECT_EQ(ExperimentalLargestPaintSize(), 25ul);

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
  // Only experimental values reset after removal.
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  EXPECT_EQ(LargestPaintSize(), 25u);
  EXPECT_EQ(ExperimentalLargestPaintTime(), base::TimeTicks());
  EXPECT_EQ(ExperimentalLargestPaintSize(), 0ul);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_UpdateOnRemoving) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target1"></img>
      <img id="target2"></img>
    </div>
  )HTML");
  SetImageAndPaint("target1", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record1 = FindLargestPaintCandidate();
  EXPECT_TRUE(record1);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  base::TimeTicks first_largest_image_paint = LargestPaintTime();

  SetImageAndPaint("target2", 10, 10);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record2 = FindLargestPaintCandidate();
  EXPECT_TRUE(record2);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  base::TimeTicks second_largest_image_paint = LargestPaintTime();

  EXPECT_NE(record1, record2);
  EXPECT_NE(first_largest_image_paint, second_largest_image_paint);

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target2"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record1_2 = FindLargestPaintCandidate();
  EXPECT_EQ(record1, record1_2);
  EXPECT_EQ(second_largest_image_paint, LargestPaintTime());
  EXPECT_EQ(LargestPaintSize(), 100u);
  EXPECT_EQ(first_largest_image_paint, ExperimentalLargestPaintTime());
  EXPECT_EQ(ExperimentalLargestPaintSize(), 25u);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_NodeRemovedBetweenRegistrationAndInvocation) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhases();

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target"));

  InvokeCallback();

  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterImageRemoval) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(ContainerTotalSize(), 3u);

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target"));
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterInvisibleImageRemoved) {
  // TODO(wangxianzhu): Fix this test for CompositeAfterPaint.
  if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled())
    return;

  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        position: relative;
        left: 100px;
      }
      #parent {
        background-color: yellow;
        height: 50px;
        width: 50px;
        overflow: scroll;
      }
    </style>
    <div id='parent'>
      <img id='target'></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(ContainerTotalSize(), 2u);
  EXPECT_EQ(CountInvisibleRecords(), 1u);

  GetDocument().body()->RemoveChild(GetDocument().getElementById("parent"));
  EXPECT_EQ(ContainerTotalSize(), 0u);
  EXPECT_EQ(CountInvisibleRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterBackgroundImageRemoval) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        background-image: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
    <div id="parent">
      <div id="target">
        place-holder
      </div>
    </div>
  )HTML");
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(ContainerTotalSize(), 3u);

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target"));
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterImageRemovedAndCallbackInvoked) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(ContainerTotalSize(), 4u);

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target"));
  EXPECT_EQ(ContainerTotalSize(), 1u);
  InvokeCallback();
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_ReattachedNodeTreatedAsNew) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
    </div>
  )HTML");
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute("id", "target");
  GetDocument().getElementById("parent")->AppendChild(image);
  SetImageAndPaint("target", 5, 5);
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSecondsD(1));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  // UpdateAllLifecyclePhasesAndInvokeCallbackIfAny() moves time forward
  // kQuantumOfTime so we should take that into account.
  EXPECT_EQ(
      record->paint_time,
      base::TimeTicks() + base::TimeDelta::FromSecondsD(1) + kQuantumOfTime);

  GetDocument().getElementById("parent")->RemoveChild(image);
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSecondsD(1));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);

  GetDocument().getElementById("parent")->AppendChild(image);
  SetImageAndPaint("target", 5, 5);
  test_task_runner_->FastForwardBy(base::TimeDelta::FromSecondsD(1));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  // UpdateAllLifecyclePhasesAndInvokeCallbackIfAny() moves time forward
  // kQuantumOfTime so we should take that into account.
  EXPECT_EQ(record->paint_time, base::TimeTicks() +
                                    base::TimeDelta::FromSecondsD(3) +
                                    3 * kQuantumOfTime);
}

// This is to prove that a presentation time is assigned only to nodes of the
// frame who register the presentation time. In other words, presentation time A
// should match frame A; presentation time B should match frame B.
TEST_P(ImagePaintTimingDetectorTest,
       MatchPresentationTimeToNodesOfDifferentFrames) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img height="5" width="5" id="smaller"></img>
      <img height="9" width="9" id="larger"></img>
    </div>
  )HTML");

  SetImageAndPaint("larger", 9, 9);
  UpdateAllLifecyclePhases();
  SimulatePassOfTime();
  SetImageAndPaint("smaller", 5, 5);
  UpdateAllLifecyclePhases();
  SimulatePassOfTime();
  InvokeCallback();
  // record1 is the larger.
  ImageRecord* record1 = FindLargestPaintCandidate();
  const base::TimeTicks record1Time = record1->paint_time;
  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("larger"));
  UpdateAllLifecyclePhases();
  SimulatePassOfTime();
  InvokeCallback();
  // record2 is the smaller.
  ImageRecord* record2 = FindLargestPaintCandidate();
  EXPECT_NE(record1Time, record2->paint_time);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_UpdateResultWhenLargestChanged) {
  base::TimeTicks time1 = test_task_runner_->NowTicks();
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target1"></img>
      <img id="target2"></img>
    </div>
  )HTML");
  SetImageAndPaint("target1", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  base::TimeTicks time2 = test_task_runner_->NowTicks();
  base::TimeTicks result1 = LargestPaintTime();
  EXPECT_GE(result1, time1);
  EXPECT_GE(time2, result1);
  EXPECT_EQ(result1, ExperimentalLargestPaintTime());

  SetImageAndPaint("target2", 10, 10);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  base::TimeTicks time3 = test_task_runner_->NowTicks();
  base::TimeTicks result2 = LargestPaintTime();
  EXPECT_GE(result2, time2);
  EXPECT_GE(time3, result2);
  EXPECT_EQ(result2, ExperimentalLargestPaintTime());
}

TEST_P(ImagePaintTimingDetectorTest, OnePresentationPromiseForOneFrame) {
  SetBodyInnerHTML(R"HTML(
    <style>img { display:block }</style>
    <div id="parent">
      <img id="1"></img>
      <img id="2"></img>
    </div>
  )HTML");
  SetImageAndPaint("1", 5, 5);
  UpdateAllLifecyclePhases();
  SimulatePassOfTime();

  SetImageAndPaint("2", 9, 9);
  UpdateAllLifecyclePhases();
  SimulatePassOfTime();

  // This callback only assigns a time to the 5x5 image.
  InvokeCallback();
  ImageRecord* record;
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 81ul);
  EXPECT_TRUE(record->paint_time.is_null());

  // This callback assigns a time to the 9x9 image.
  InvokeCallback();
  record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 81ul);
  EXPECT_FALSE(record->paint_time.is_null());
}

TEST_P(ImagePaintTimingDetectorTest, VideoImage) {
  SetBodyInnerHTML(R"HTML(
    <video id="target" poster=")HTML" LARGE_IMAGE R"HTML("></video>
  )HTML");

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_GT(record->first_size, 0ul);
  EXPECT_TRUE(record->loaded);
}

TEST_P(ImagePaintTimingDetectorTest, VideoImage_ImageNotLoaded) {
  SetBodyInnerHTML("<video id='target'></video>");

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest, SVGImage) {
  SetBodyInnerHTML(R"HTML(
    <svg>
      <image id="target" width="10" height="10"/>
    </svg>
  )HTML");

  SetSVGImageAndPaint("target", 5, 5);

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_GT(record->first_size, 0ul);
  EXPECT_TRUE(record->loaded);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        background-image: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
    <div>place-holder</div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(CountVisibleImageRecords(), 1u);
}

TEST_P(ImagePaintTimingDetectorTest,
       BackgroundImageAndLayoutImageTrackedDifferently) {
  SetBodyInnerHTML(R"HTML(
    <style>
      img {
        background-image: url()HTML" LARGE_IMAGE R"HTML();
      }
    </style>
    <img id="target">
      place-holder
    </img>
  )HTML");
  SetImageAndPaint("target", 1, 1);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 2u);
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 1u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreBody) {
  SetBodyInnerHTML("<style>body { background-image: url(" SIMPLE_IMAGE
                   ")}</style>");
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreHtml) {
  SetBodyInnerHTML("<style>html { background-image: url(" SIMPLE_IMAGE
                   ")}</style>");
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreGradient) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        background-image: linear-gradient(blue, yellow);
      }
    </style>
    <div>
      place-holder
    </div>
  )HTML");
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
}

// We put two background images in the same object, and test whether FCP++ can
// find two different images.
TEST_P(ImagePaintTimingDetectorTest, BackgroundImageTrackedDifferently) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #d {
        width: 50px;
        height: 50px;
        background-image:
          url()HTML" SIMPLE_IMAGE "), url(" LARGE_IMAGE R"HTML();
      }
    </style>
    <div id="d"></div>
  )HTML");
  EXPECT_EQ(CountVisibleImageRecords(), 2u);
}

TEST_P(ImagePaintTimingDetectorTest, DeactivateAfterUserInput) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SimulateScroll();
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_FALSE(GetPaintTimingDetector().GetImagePaintTimingDetector());
}

TEST_P(ImagePaintTimingDetectorTest, ContinueAfterKeyUp) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SimulateKeyUp();
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_TRUE(GetPaintTimingDetector().GetImagePaintTimingDetector());
}

TEST_P(ImagePaintTimingDetectorTest, NullTimeNoCrash) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhases();
  UpdateCandidate();
}

TEST_P(ImagePaintTimingDetectorTest, Iframe) {
  SetBodyInnerHTML(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildBodyInnerHTML(R"HTML(
    <style>img { display:block }</style>
    <img id="target"></img>
  )HTML");
  SetChildFrameImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhases();
  // Ensure main frame doesn't capture this image.
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
  EXPECT_EQ(CountChildFrameRecords(), 1u);
  InvokeChildFrameCallback();
  ImageRecord* image = FindChildFrameLargestPaintCandidate();
  EXPECT_TRUE(image);
  // Ensure the image size is not clipped (5*5).
  EXPECT_EQ(image->first_size, 25ul);
}

TEST_P(ImagePaintTimingDetectorTest, Iframe_ClippedByMainFrameViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f { margin-top: 1234567px }
    </style>
    <iframe id="f" width=100px height=100px></iframe>
  )HTML");
  SetChildBodyInnerHTML(R"HTML(
    <style>img { display:block }</style>
    <img id="target"></img>
  )HTML");
  // Make sure the iframe is out of main-frame's viewport.
  DCHECK_LT(GetViewportRect(GetFrameView()).Height(), 1234567);
  SetChildFrameImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, Iframe_HalfClippedByMainFrameViewport) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #f { margin-left: -5px; }
    </style>
    <iframe id="f" width=10px height=10px></iframe>
  )HTML");
  SetChildBodyInnerHTML(R"HTML(
    <style>img { display:block }</style>
    <img id="target"></img>
  )HTML");
  SetChildFrameImageAndPaint("target", 10, 10);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
  EXPECT_EQ(CountChildFrameRecords(), 1u);
  InvokeChildFrameCallback();
  ImageRecord* image = FindChildFrameLargestPaintCandidate();
  EXPECT_TRUE(image);
  EXPECT_LT(image->first_size, 100ul);
}

TEST_P(ImagePaintTimingDetectorTest, SameSizeShouldNotBeIgnored) {
  SetBodyInnerHTML(R"HTML(
    <style>img { display:block }</style>
    <img id='1'></img>
    <img id='2'></img>
    <img id='3'></img>
  )HTML");
  SetImageAndPaint("1", 5, 5);
  SetImageAndPaint("2", 5, 5);
  SetImageAndPaint("3", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountRankingSetRecords(), 3u);
}

TEST_P(ImagePaintTimingDetectorTest, UseIntrinsicSizeIfSmaller_Image) {
  SetBodyInnerHTML(R"HTML(
    <img height="300" width="300" display="block" id="target">
    </img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25u);
}

TEST_P(ImagePaintTimingDetectorTest, NotUseIntrinsicSizeIfLarger_Image) {
  SetBodyInnerHTML(R"HTML(
    <img height="1" width="1" display="block" id="target">
    </img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 1u);
}

TEST_P(ImagePaintTimingDetectorTest,
       UseIntrinsicSizeIfSmaller_BackgroundImage) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #d {
        width: 50px;
        height: 50px;
        background-image: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
    <div id="d"></div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 1u);
}

TEST_P(ImagePaintTimingDetectorTest,
       NotUseIntrinsicSizeIfLarger_BackgroundImage) {
  // The image is in 16x16.
  SetBodyInnerHTML(R"HTML(
    <style>
      #d {
        width: 5px;
        height: 5px;
        background-image: url()HTML" LARGE_IMAGE R"HTML();
      }
    </style>
    <div id="d"></div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25u);
}

TEST_P(ImagePaintTimingDetectorTest, OpacityZeroHTML) {
  SetBodyInnerHTML(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 0u);

  // Change the opacity of documentElement, now the img should be a candidate.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                "opacity: 1");
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 1u);
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 25u);
  EXPECT_GT(GetPerformanceTiming().LargestImagePaint(), 0u);
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(),
            GetPerformanceTiming().ExperimentalLargestImagePaintSize());
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaint(),
            GetPerformanceTiming().ExperimentalLargestImagePaint());
}

TEST_P(ImagePaintTimingDetectorTest, OpacityZeroHTML2) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        opacity: 0;
      }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                "opacity: 0");
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                "opacity: 1");
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_FullViewportImage) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0px;}</style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 3000, 3000);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
  // Simulate some input event to force StopRecordEntries().
  SimulateKeyDown();
  auto entries = test_ukm_recorder.GetEntriesByName(UkmPaintTiming::kEntryName);
  EXPECT_EQ(1ul, entries.size());
  auto* entry = entries[0];
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmPaintTiming::kLCPDebugging_HasViewportImageName, true);
}

}  // namespace blink
