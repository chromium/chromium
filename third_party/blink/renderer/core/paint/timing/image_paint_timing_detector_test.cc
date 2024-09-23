// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/trace_event_analyzer.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_helper.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
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

#define TRANSPARENT_PLACEHOLDER_IMAGE \
  "data:image/gif;base64,"            \
  "R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="

using UkmPaintTiming = ukm::builders::Blink_PaintTiming;
using ::testing::Optional;

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

  const PerformanceTimingForReporting& GetPerformanceTimingForReporting() {
    PerformanceTimingForReporting* performance_for_reporting =
        DOMWindowPerformance::performance(*GetFrame()->DomWindow())
            ->timingForReporting();
    return *performance_for_reporting;
  }

  gfx::Rect GetViewportRect(LocalFrameView& view) {
    ScrollableArea* scrollable_area = view.GetScrollableArea();
    DCHECK(scrollable_area);
    return scrollable_area->VisibleContentRect();
  }

  ImageRecord* LargestImage() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .records_manager_.LargestImage();
  }

  ImageRecord* LargestPaintedImage() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .records_manager_.largest_painted_image_.Get();
  }

  ImageRecord* ChildFrameLargestImage() {
    return GetChildFrameView()
        .GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .records_manager_.LargestImage();
  }

  size_t CountImageRecords() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .records_manager_.recorded_images_.size();
  }

  size_t ContainerTotalSize() {
    size_t result = GetPaintTimingDetector()
                        .GetImagePaintTimingDetector()
                        .records_manager_.recorded_images_.size() +
                    GetPaintTimingDetector()
                        .GetImagePaintTimingDetector()
                        .records_manager_.pending_images_.size() +
                    GetPaintTimingDetector()
                        .GetImagePaintTimingDetector()
                        .records_manager_.images_queued_for_paint_time_.size() +
                    GetPaintTimingDetector()
                        .GetImagePaintTimingDetector()
                        .records_manager_.image_finished_times_.size();

    return result;
  }

  size_t CountChildFrameRecords() {
    return GetChildPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .records_manager_.recorded_images_.size();
  }

  void UpdateCandidate() { GetPaintTimingDetector().UpdateLcpCandidate(); }

  void UpdateCandidateForChildFrame() {
    GetChildPaintTimingDetector().UpdateLcpCandidate();
  }

  base::TimeTicks LargestPaintTime() {
    return GetPaintTimingDetector()
        .LatestLcpDetailsForTest()
        .largest_image_paint_time;
  }

  uint64_t LargestPaintSize() {
    return GetPaintTimingDetector()
        .LatestLcpDetailsForTest()
        .largest_image_paint_size;
  }

  static constexpr base::TimeDelta kQuantumOfTime = base::Milliseconds(10);

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
    GetPaintTimingDetector().GetImagePaintTimingDetector().ResetCallbackManager(
        mock_callback_manager_);
    UpdateAllLifecyclePhases();
  }

  void SetChildBodyInnerHTML(const String& content) {
    GetChildDocument()->SetBaseURLOverride(KURL("http://test.com"));
    GetChildDocument()->body()->setInnerHTML(content, ASSERT_NO_EXCEPTION);
    child_mock_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    GetChildPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .ResetCallbackManager(child_mock_callback_manager_);
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

  void SetImageAndPaint(const char* id, int width, int height) {
    Element* element = GetDocument().getElementById(AtomicString(id));
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    To<HTMLImageElement>(element)->SetImageForTest(content);
  }

  void SetChildFrameImageAndPaint(const char* id, int width, int height) {
    DCHECK(GetChildDocument());
    Element* element = GetChildDocument()->getElementById(AtomicString(id));
    DCHECK(element);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    To<HTMLImageElement>(element)->SetImageForTest(content);
  }

  void SetSVGImageAndPaint(const char* id, int width, int height) {
    Element* element = GetDocument().getElementById(AtomicString(id));
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

  LocalFrame* GetChildFrame() {
    return To<LocalFrame>(GetFrame()->Tree().FirstChild());
  }

  test::TaskEnvironment task_environment_;
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  frame_test_helpers::WebViewHelper web_view_helper_;

 private:
  LocalFrame* GetFrame() {
    return web_view_helper_.GetWebView()->MainFrameImpl()->GetFrame();
  }
  ImageResourceContent* CreateImageForTest(int width, int height) {
    sk_sp<SkColorSpace> src_rgb_color_space = SkColorSpace::MakeSRGB();
    SkImageInfo raster_image_info =
        SkImageInfo::MakeN32Premul(width, height, src_rgb_color_space);
    sk_sp<SkSurface> surface(SkSurfaces::Raster(raster_image_info));
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    scoped_refptr<UnacceleratedStaticBitmapImage> original_image_data =
        UnacceleratedStaticBitmapImage::Create(image);
    // To ensure that the image may be considered as an LCP candidate, allocate
    // a small amount of memory for the image (0.1bpp should exceed the LCP
    // entropy threshold).
    int bytes = (width * height / 80) + 1;
    scoped_refptr<SharedBuffer> shared_buffer =
        SharedBuffer::Create(Vector<char>(bytes));
    original_image_data->SetData(shared_buffer, /*all_data_received=*/true);
    ImageResourceContent* original_image_content =
        ImageResourceContent::CreateLoaded(original_image_data.get());
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
  ImageRecord* record = LargestImage();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_OneImage) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->recorded_size, 25ul);
  EXPECT_FALSE(record->load_time.is_null());
  // Simulate some input event to force StopRecordEntries().
  SimulateKeyDown();
  auto entries = test_ukm_recorder.GetEntriesByName(UkmPaintTiming::kEntryName);
  EXPECT_EQ(1ul, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmPaintTiming::kLCPDebugging_HasViewportImageName, false);
}

TEST_P(ImagePaintTimingDetectorTest, InsertionOrderIsSecondaryRankingKey) {
  SetBodyInnerHTML(R"HTML(
  )HTML");

  auto* image1 = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image1->setAttribute(html_names::kIdAttr, AtomicString("image1"));
  GetDocument().body()->AppendChild(image1);
  SetImageAndPaint("image1", 5, 5);

  auto* image2 = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image2->setAttribute(html_names::kIdAttr, AtomicString("image2"));
  GetDocument().body()->AppendChild(image2);
  SetImageAndPaint("image2", 5, 5);

  auto* image3 = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image3->setAttribute(html_names::kIdAttr, AtomicString("image3"));
  GetDocument().body()->AppendChild(image3);
  SetImageAndPaint("image3", 5, 5);

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();

  EXPECT_EQ(LargestImage()->node_id, DOMNodeIds::ExistingIdForNode(image1));
  EXPECT_EQ(LargestPaintSize(), 25ul);
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

  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::Value::Dict arg_dict = events[0]->GetKnownArgAsDict("data");
  EXPECT_GT(arg_dict.FindInt("DOMNodeId").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("size").value_or(-1), 0);
  EXPECT_EQ(arg_dict.FindInt("candidateIndex").value_or(-1), 1);
  std::optional<bool> isMainFrame = arg_dict.FindBool("isMainFrame");
  EXPECT_TRUE(isMainFrame.has_value());
  EXPECT_EQ(true, isMainFrame.value());
  std::optional<bool> is_outermost_main_frame =
      arg_dict.FindBool("isOutermostMainFrame");
  EXPECT_TRUE(is_outermost_main_frame.has_value());
  EXPECT_EQ(true, is_outermost_main_frame.value());
  std::optional<bool> is_embedded_frame = arg_dict.FindBool("isEmbeddedFrame");
  EXPECT_TRUE(is_embedded_frame.has_value());
  EXPECT_EQ(false, is_embedded_frame.value());
  EXPECT_EQ(arg_dict.FindInt("frame_x").value_or(-1), 8);
  EXPECT_EQ(arg_dict.FindInt("frame_y").value_or(-1), 8);
  EXPECT_EQ(arg_dict.FindInt("frame_width").value_or(-1), 5);
  EXPECT_EQ(arg_dict.FindInt("frame_height").value_or(-1), 5);
  EXPECT_EQ(arg_dict.FindInt("root_x").value_or(-1), 8);
  EXPECT_EQ(arg_dict.FindInt("root_y").value_or(-1), 8);
  EXPECT_EQ(arg_dict.FindInt("root_width").value_or(-1), 5);
  EXPECT_EQ(arg_dict.FindInt("root_height").value_or(-1), 5);
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

  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::Value::Dict arg_dict = events[0]->GetKnownArgAsDict("data");
  EXPECT_GT(arg_dict.FindInt("DOMNodeId").value_or(-1), 0);
  EXPECT_GT(arg_dict.FindInt("size").value_or(-1), 0);
  EXPECT_EQ(arg_dict.FindInt("candidateIndex").value_or(-1), 1);
  std::optional<bool> isMainFrame = arg_dict.FindBool("isMainFrame");
  EXPECT_TRUE(isMainFrame.has_value());
  EXPECT_EQ(false, isMainFrame.value());
  std::optional<bool> is_outermost_main_frame =
      arg_dict.FindBool("isOutermostMainFrame");
  EXPECT_TRUE(is_outermost_main_frame.has_value());
  EXPECT_EQ(false, is_outermost_main_frame.value());
  std::optional<bool> is_embedded_frame = arg_dict.FindBool("isEmbeddedFrame");
  EXPECT_TRUE(is_embedded_frame.has_value());
  EXPECT_EQ(false, is_embedded_frame.value());
  EXPECT_EQ(arg_dict.FindInt("frame_x").value_or(-1), 10);
  EXPECT_EQ(arg_dict.FindInt("frame_y").value_or(-1), 10);
  EXPECT_EQ(arg_dict.FindInt("frame_width").value_or(-1), 200);
  EXPECT_EQ(arg_dict.FindInt("frame_height").value_or(-1), 200);
  EXPECT_GT(arg_dict.FindInt("root_x").value_or(-1), 40);
  EXPECT_GT(arg_dict.FindInt("root_y").value_or(-1), 60);
  EXPECT_EQ(arg_dict.FindInt("root_width").value_or(-1), 200);
  EXPECT_EQ(arg_dict.FindInt("root_height").value_or(-1), 200);
}

TEST_P(ImagePaintTimingDetectorTest, UpdatePerformanceTiming) {
  LargestContentfulPaintDetailsForReporting largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 0u);
  EXPECT_EQ(largest_contentful_paint_details.image_paint_time, 0u);
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 25u);
  EXPECT_GT(largest_contentful_paint_details.image_paint_time, 0u);
}

TEST_P(ImagePaintTimingDetectorTest, UpdatePerformanceTimingToZero) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  auto largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 25u);
  EXPECT_GT(largest_contentful_paint_details.image_paint_time, 0u);
  GetDocument().body()->RemoveChild(
      GetDocument().getElementById(AtomicString("target")));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 25u);
  EXPECT_GT(largest_contentful_paint_details.image_paint_time, 0u);
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
  EXPECT_EQ(CountImageRecords(), 0u);
  ImageRecord* record = LargestImage();
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
  EXPECT_EQ(CountImageRecords(), 0u);
  ImageRecord* record = LargestImage();
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
  EXPECT_EQ(CountImageRecords(), 0u);
  ImageRecord* record = LargestImage();
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
  EXPECT_EQ(CountImageRecords(), 1u);
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
}

TEST_P(ImagePaintTimingDetectorTest,
       IgnoreImageUntilInvalidatedRectSizeNonZero) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountImageRecords(), 0u);
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(CountImageRecords(), 1u);
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
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->recorded_size, 25ul);

  SetImageAndPaint("larger", 9, 9);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(LargestPaintSize(), 81ul);
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
  ImageRecord* record = LargestImage();
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
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  EXPECT_EQ(LargestPaintSize(), 25ul);

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target")));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  EXPECT_EQ(LargestPaintSize(), 25u);
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
  ImageRecord* record1 = LargestImage();
  EXPECT_TRUE(record1);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  base::TimeTicks first_largest_image_paint = LargestPaintTime();

  SetImageAndPaint("target2", 10, 10);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record2 = LargestImage();
  EXPECT_TRUE(record2);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  base::TimeTicks second_largest_image_paint = LargestPaintTime();

  EXPECT_NE(record1, record2);
  EXPECT_NE(first_largest_image_paint, second_largest_image_paint);

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target2")));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record3 = LargestImage();
  EXPECT_EQ(record2, record3);
  EXPECT_EQ(second_largest_image_paint, LargestPaintTime());
  EXPECT_EQ(LargestPaintSize(), 100u);
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

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target")));

  InvokeCallback();

  ImageRecord* record;
  record = LargestImage();
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
  EXPECT_EQ(ContainerTotalSize(), 2u);

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target")));
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterInvisibleImageRemoved) {
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
  // The out-of-viewport image will not have been recorded yet.
  EXPECT_EQ(ContainerTotalSize(), 1u);

  GetDocument().body()->RemoveChild(
      GetDocument().getElementById(AtomicString("parent")));
  EXPECT_EQ(ContainerTotalSize(), 0u);
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
  EXPECT_EQ(ContainerTotalSize(), 2u);

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target")));
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

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target")));
  // Lazy deletion from |images_queued_for_paint_time_|.
  EXPECT_EQ(ContainerTotalSize(), 1u);
  InvokeCallback();
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_ReattachedNodeNotTreatedAsNew) {
  SetBodyInnerHTML(R"HTML(
    <div id="parent">
    </div>
  )HTML");
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kIdAttr, AtomicString("target"));
  GetDocument().getElementById(AtomicString("parent"))->AppendChild(image);
  SetImageAndPaint("target", 5, 5);
  test_task_runner_->FastForwardBy(base::Seconds(1));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record;
  record = LargestImage();
  EXPECT_TRUE(record);
  // UpdateAllLifecyclePhasesAndInvokeCallbackIfAny() moves time forward
  // kQuantumOfTime so we should take that into account.
  EXPECT_EQ(record->paint_time,
            base::TimeTicks() + base::Seconds(1) + kQuantumOfTime);

  GetDocument().getElementById(AtomicString("parent"))->RemoveChild(image);
  test_task_runner_->FastForwardBy(base::Seconds(1));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->paint_time,
            base::TimeTicks() + base::Seconds(1) + kQuantumOfTime);

  GetDocument().getElementById(AtomicString("parent"))->AppendChild(image);
  SetImageAndPaint("target", 5, 5);
  test_task_runner_->FastForwardBy(base::Seconds(1));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->paint_time,
            base::TimeTicks() + base::Seconds(1) + kQuantumOfTime);
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

  SetImageAndPaint("smaller", 5, 5);
  UpdateAllLifecyclePhases();
  SimulatePassOfTime();
  SetImageAndPaint("larger", 9, 9);
  UpdateAllLifecyclePhases();
  SimulatePassOfTime();
  InvokeCallback();
  // record1 is the smaller.
  ImageRecord* record1 = LargestPaintedImage();
  DCHECK_EQ(record1->recorded_size, 25ul);
  const base::TimeTicks record1Time = record1->paint_time;
  UpdateAllLifecyclePhases();
  SimulatePassOfTime();
  InvokeCallback();
  // record2 is the larger.
  ImageRecord* record2 = LargestPaintedImage();
  DCHECK_EQ(record2->recorded_size, 81ul);
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

  SetImageAndPaint("target2", 10, 10);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  base::TimeTicks time3 = test_task_runner_->NowTicks();
  base::TimeTicks result2 = LargestPaintTime();
  EXPECT_GE(result2, time2);
  EXPECT_GE(time3, result2);
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
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->recorded_size, 81ul);
  EXPECT_TRUE(record->paint_time.is_null());

  // This callback assigns a time to the 9x9 image.
  InvokeCallback();
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->recorded_size, 81ul);
  EXPECT_FALSE(record->paint_time.is_null());
}

TEST_P(ImagePaintTimingDetectorTest, VideoImage) {
  SetBodyInnerHTML(R"HTML(
    <video id="target" poster=")HTML" LARGE_IMAGE R"HTML("></video>
  )HTML");

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_GT(record->recorded_size, 0ul);
  EXPECT_FALSE(record->paint_time.is_null());
}

TEST_P(ImagePaintTimingDetectorTest, VideoImage_ImageNotLoaded) {
  SetBodyInnerHTML("<video id='target'></video>");

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = LargestImage();
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
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_GT(record->recorded_size, 0ul);
  EXPECT_FALSE(record->paint_time.is_null());
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
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(CountImageRecords(), 1u);
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
  EXPECT_EQ(CountImageRecords(), 2u);
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->recorded_size, 1u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreBody) {
  SetBodyInnerHTML("<style>body { background-image: url(" SIMPLE_IMAGE
                   ")}</style>");
  EXPECT_EQ(CountImageRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreHtml) {
  SetBodyInnerHTML("<style>html { background-image: url(" SIMPLE_IMAGE
                   ")}</style>");
  EXPECT_EQ(CountImageRecords(), 0u);
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
  EXPECT_EQ(CountImageRecords(), 0u);
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
  EXPECT_EQ(CountImageRecords(), 2u);
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
  EXPECT_FALSE(GetPaintTimingDetector()
                   .GetImagePaintTimingDetector()
                   .IsRecordingLargestImagePaint());
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
  EXPECT_TRUE(GetPaintTimingDetector()
                  .GetImagePaintTimingDetector()
                  .IsRecordingLargestImagePaint());
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
  EXPECT_EQ(CountImageRecords(), 0u);
  EXPECT_EQ(CountChildFrameRecords(), 1u);
  InvokeChildFrameCallback();
  ImageRecord* image = ChildFrameLargestImage();
  EXPECT_TRUE(image);
  // Ensure the image size is not clipped (5*5).
  EXPECT_EQ(image->recorded_size, 25ul);
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
  DCHECK_LT(GetViewportRect(GetFrameView()).height(), 1234567);
  SetChildFrameImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(CountImageRecords(), 0u);
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
  EXPECT_EQ(CountImageRecords(), 0u);
  EXPECT_EQ(CountChildFrameRecords(), 1u);
  InvokeChildFrameCallback();
  ImageRecord* image = ChildFrameLargestImage();
  EXPECT_TRUE(image);
  EXPECT_LT(image->recorded_size, 100ul);
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
  EXPECT_EQ(CountImageRecords(), 3u);
}

TEST_P(ImagePaintTimingDetectorTest, UseIntrinsicSizeIfSmaller_Image) {
  SetBodyInnerHTML(R"HTML(
    <img height="300" width="300" display="block" id="target">
    </img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->recorded_size, 25u);
}

TEST_P(ImagePaintTimingDetectorTest, NotUseIntrinsicSizeIfLarger_Image) {
  SetBodyInnerHTML(R"HTML(
    <img height="1" width="1" display="block" id="target">
    </img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->recorded_size, 1u);
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
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->recorded_size, 1u);
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
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->recorded_size, 25u);
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
  EXPECT_EQ(CountImageRecords(), 0u);

  // Change the opacity of documentElement, now the img should be a candidate.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountImageRecords(), 1u);
  auto largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 25u);
  EXPECT_GT(largest_contentful_paint_details.image_paint_time, 0u);
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
  EXPECT_EQ(CountImageRecords(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 0"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountImageRecords(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(CountImageRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_FullViewportImage) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0px;}</style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 3000, 3000);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = LargestImage();
  EXPECT_FALSE(record);
  // Simulate some input event to force StopRecordEntries().
  SimulateKeyDown();
  auto entries = test_ukm_recorder.GetEntriesByName(UkmPaintTiming::kEntryName);
  EXPECT_EQ(1ul, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmPaintTiming::kLCPDebugging_HasViewportImageName, true);
}

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/1353921): This test is flaky on Android. Fix it.
// https://chrome-swarming.appspot.com/task?id=60c68038be22f011
// The first EXPECT_EQ(0u, events.size()) below failed.
#define MAYBE_LargestImagePaint_Detached_Frame \
  DISABLED_LargestImagePaint_Detached_Frame
#else
#define MAYBE_LargestImagePaint_Detached_Frame LargestImagePaint_Detached_Frame
#endif

TEST_P(ImagePaintTimingDetectorTest, MAYBE_LargestImagePaint_Detached_Frame) {
  using trace_analyzer::Query;
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
  LocalFrame* child_frame = GetChildFrame();
  PaintTimingDetector* child_detector =
      &child_frame->View()->GetPaintTimingDetector();
  GetDocument().body()->setInnerHTML("", ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(child_frame->IsDetached());

  // Start tracing, we only want to capture it during the ReportPaintTime.
  trace_analyzer::Start("loading");
  viz::FrameTimingDetails presentation_details;
  presentation_details.presentation_feedback.timestamp =
      test_task_runner_->NowTicks();
  child_detector->callback_manager_->ReportPaintTime(
      std::make_unique<PaintTimingCallbackManager::CallbackQueue>(),
      presentation_details);

  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestImagePaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(0u, events.size());
  q = Query::EventNameIs("LargestImagePaint::NoCandidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(0u, events.size());
}

class ImagePaintTimingDetectorFencedFrameTest
    : private ScopedFencedFramesForTest,
      public ImagePaintTimingDetectorTest {
 public:
  ImagePaintTimingDetectorFencedFrameTest() : ScopedFencedFramesForTest(true) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

  void InitializeFencedFrameRoot(
      blink::FencedFrame::DeprecatedFencedFrameMode mode) {
    web_view_helper_.InitializeWithOpener(/*opener=*/nullptr,
                                          /*frame_client=*/nullptr,
                                          /*view_client=*/nullptr,
                                          /*update_settings_func=*/nullptr,
                                          mode);
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

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ImagePaintTimingDetectorFencedFrameTest);

TEST_P(ImagePaintTimingDetectorFencedFrameTest, NotReported) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  InitializeFencedFrameRoot(
      blink::FencedFrame::DeprecatedFencedFrameMode::kDefault);
  GetDocument().SetBaseURLOverride(KURL("https://test.com"));
  SetBodyInnerHTML(R"HTML(
      <body></body>
    )HTML");

  SetBodyInnerHTML(R"HTML(
    <style>body {margin: 0px;}</style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 3000, 3000);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = LargestImage();
  EXPECT_EQ(record, nullptr);
  // Simulate some input event to force StopRecordEntries().
  SimulateKeyDown();
  auto entries = test_ukm_recorder.GetEntriesByName(UkmPaintTiming::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

class ImagePaintTimingDetectorTransparentPlaceholderImageTest
    : public ImagePaintTimingDetectorTest {
 public:
  ImagePaintTimingDetectorTransparentPlaceholderImageTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSimplifyLoadingTransparentPlaceholderImage);
  }
  ~ImagePaintTimingDetectorTransparentPlaceholderImageTest() override {
    // Must destruct all objects before toggling back feature flags.
    std::unique_ptr<base::test::TaskEnvironment> task_environment;
    if (!base::ThreadPoolInstance::Get()) {
      // Create a TaskEnvironment for the garbage collection below.
      task_environment = std::make_unique<base::test::TaskEnvironment>();
    }
    scoped_feature_list_.Reset();
    WebHeap::CollectAllGarbageForTesting();
  }

 protected:
  void SetTransparentPlaceholderImageAndPaint(const char* id) {
    Element* element = GetDocument().getElementById(AtomicString(id));
    ImageResource* resource = ImageResource::CreateForTest(
        url_test_helpers::ToKURL(TRANSPARENT_PLACEHOLDER_IMAGE));
    To<HTMLImageElement>(element)->SetImageForTest(resource->GetContent());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(
    ImagePaintTimingDetectorTransparentPlaceholderImageTest);

TEST_P(ImagePaintTimingDetectorTransparentPlaceholderImageTest,
       LargestImagePaint) {
  LargestContentfulPaintDetailsForReporting largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 0u);
  EXPECT_EQ(largest_contentful_paint_details.image_paint_time, 0u);
  SetBodyInnerHTML(R"HTML(
      <img id="placeholder"></img>
    )HTML");
  SetTransparentPlaceholderImageAndPaint("placeholder");
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 1u);
  EXPECT_GT(largest_contentful_paint_details.image_paint_time, 0u);
}

}  // namespace blink
