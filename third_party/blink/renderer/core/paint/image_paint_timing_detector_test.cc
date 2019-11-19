// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/image_paint_timing_detector.h"

#include "base/bind.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "third_party/blink/public/web/web_performance.h"
#include "third_party/blink/public/web/web_widget_client.h"
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
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

class ImagePaintTimingDetectorTest : public testing::Test {
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
    frame_impl.ViewImpl()->MainFrameWidget()->Resize(WebSize(640, 480));

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

  base::TimeTicks LargestPaintStoredResult() {
    return GetPaintTimingDetector().largest_image_paint_time_;
  }

  static constexpr base::TimeDelta kQuantumOfTime =
      base::TimeDelta::FromMilliseconds(10);

  void SimulatePassOfTime() {
    test_task_runner_->FastForwardBy(kQuantumOfTime);
  }

  void UpdateAllLifecyclePhases() {
    GetDocument().View()->UpdateAllLifecyclePhases(
        DocumentLifecycle::LifecycleUpdateReason::kTest);
  }

  void UpdateAllLifecyclePhasesAndInvokeCallbackIfAny() {
    UpdateAllLifecyclePhases();
    SimulatePassOfTime();
    while (mock_callback_manager_->CountCallbacks() > 0)
      InvokeSwapTimeCallback(mock_callback_manager_);
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
    GetChildDocument()->body()->SetInnerHTMLFromString(content,
                                                       ASSERT_NO_EXCEPTION);
    child_mock_callback_manager_ =
        MakeGarbageCollected<MockPaintTimingCallbackManager>();
    GetChildPaintTimingDetector()
        .GetImagePaintTimingDetector()
        ->ResetCallbackManager(child_mock_callback_manager_);
    UpdateAllLifecyclePhases();
  }

  void InvokeCallback() {
    DCHECK_GT(mock_callback_manager_->CountCallbacks(), 0UL);
    InvokeSwapTimeCallback(mock_callback_manager_);
  }

  void InvokeChildFrameCallback() {
    DCHECK_GT(child_mock_callback_manager_->CountCallbacks(), 0UL);
    InvokeSwapTimeCallback(child_mock_callback_manager_);
  }

  void InvokeSwapTimeCallback(
      MockPaintTimingCallbackManager* image_callback_manager) {
    image_callback_manager->InvokeSwapTimeCallback(
        test_task_runner_->NowTicks());
    UpdateCandidate();
  }

  void SetImageAndPaint(AtomicString id, int width, int height) {
    Element* element = GetDocument().getElementById(id);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    ToHTMLImageElement(element)->SetImageForTest(content);
  }

  void SetChildFrameImageAndPaint(AtomicString id, int width, int height) {
    DCHECK(GetChildDocument());
    Element* element = GetChildDocument()->getElementById(id);
    DCHECK(element);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    ToHTMLImageElement(element)->SetImageForTest(content);
  }

  void SetVideoImageAndPaint(AtomicString id, int width, int height) {
    Element* element = GetDocument().getElementById(id);
    DCHECK(element);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    ToHTMLVideoElement(element)->SetImageForTest(content);
  }

  void SetSVGImageAndPaint(AtomicString id, int width, int height) {
    Element* element = GetDocument().getElementById(id);
    // Set image and make it loaded.
    ImageResourceContent* content = CreateImageForTest(width, height);
    ToSVGImageElement(element)->SetImageForTest(content);
  }

  void SimulateScroll() { GetPaintTimingDetector().NotifyScroll(kUserScroll); }

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
    ImageResourceContent* original_image_resource =
        ImageResourceContent::CreateLoaded(
            StaticBitmapImage::Create(image).get());
    return original_image_resource;
  }

  PaintTimingCallbackManager::CallbackQueue callback_queue_;
  Persistent<MockPaintTimingCallbackManager> mock_callback_manager_;
  Persistent<MockPaintTimingCallbackManager> child_mock_callback_manager_;
};

constexpr base::TimeDelta ImagePaintTimingDetectorTest::kQuantumOfTime;

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_NoImage) {
  SetBodyInnerHTML(R"HTML(
    <div></div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_OneImage) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25ul);
  EXPECT_TRUE(record->loaded);
}

TEST_F(ImagePaintTimingDetectorTest, InsertionOrderIsSecondaryRankingKey) {
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
}

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_TraceEvent_Candidate) {
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
  std::unique_ptr<base::Value> arg;
  EXPECT_TRUE(events[0]->GetArgAsValue("data", &arg));
  base::DictionaryValue* arg_dict;
  EXPECT_TRUE(arg->GetAsDictionary(&arg_dict));
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
}

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_TraceEvent_NoCandidate) {
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
    std::unique_ptr<base::Value> arg;
    EXPECT_TRUE(events[0]->GetArgAsValue("data", &arg));
    base::DictionaryValue* arg_dict;
    EXPECT_TRUE(arg->GetAsDictionary(&arg_dict));
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
    std::unique_ptr<base::Value> arg;
    EXPECT_TRUE(events[1]->GetArgAsValue("data", &arg));
    base::DictionaryValue* arg_dict;
    EXPECT_TRUE(arg->GetAsDictionary(&arg_dict));
    DOMNodeId candidate_index;
    EXPECT_TRUE(arg_dict->GetInteger("candidateIndex", &candidate_index));
    EXPECT_EQ(candidate_index, 3);
  }
}

TEST_F(ImagePaintTimingDetectorTest, UpdatePerformanceTiming) {
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 0u);
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaint(), 0u);
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 25u);
  EXPECT_GT(GetPerformanceTiming().LargestImagePaint(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest,
       PerformanceTimingHasZeroTimeNonZeroSizeWhenTheLargestIsNotPainted) {
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 0u);
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaint(), 0u);
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhases();
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 25u);
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaint(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest, UpdatePerformanceTimingToZero) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 25u);
  EXPECT_GT(GetPerformanceTiming().LargestImagePaint(), 0u);
  GetDocument().body()->RemoveChild(GetDocument().getElementById("target"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaintSize(), 0u);
  EXPECT_EQ(GetPerformanceTiming().LargestImagePaint(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_OpacityZero) {
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

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_VisibilityHidden) {
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

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_DisplayNone) {
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

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_OpacityNonZero) {
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

TEST_F(ImagePaintTimingDetectorTest,
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

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_Largest) {
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
}

TEST_F(ImagePaintTimingDetectorTest,
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

TEST_F(ImagePaintTimingDetectorTest,
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
  EXPECT_NE(LargestPaintStoredResult(), base::TimeTicks());

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
  EXPECT_EQ(LargestPaintStoredResult(), base::TimeTicks());
}

TEST_F(ImagePaintTimingDetectorTest, LargestImagePaint_UpdateOnRemoving) {
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
  EXPECT_NE(LargestPaintStoredResult(), base::TimeTicks());
  base::TimeTicks first_largest_image_paint = LargestPaintStoredResult();

  SetImageAndPaint("target2", 10, 10);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record2 = FindLargestPaintCandidate();
  EXPECT_TRUE(record2);
  EXPECT_NE(LargestPaintStoredResult(), base::TimeTicks());
  base::TimeTicks second_largest_image_paint = LargestPaintStoredResult();

  EXPECT_NE(record1, record2);
  EXPECT_NE(first_largest_image_paint, second_largest_image_paint);

  GetDocument().getElementById("parent")->RemoveChild(
      GetDocument().getElementById("target2"));
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record1_2 = FindLargestPaintCandidate();
  EXPECT_EQ(record1, record1_2);
  EXPECT_EQ(first_largest_image_paint, LargestPaintStoredResult());
}

TEST_F(ImagePaintTimingDetectorTest,
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

TEST_F(ImagePaintTimingDetectorTest,
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

TEST_F(ImagePaintTimingDetectorTest,
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
  EXPECT_EQ(ContainerTotalSize(), 2u);
  EXPECT_EQ(CountInvisibleRecords(), 1u);

  GetDocument().body()->RemoveChild(GetDocument().getElementById("parent"));
  EXPECT_EQ(ContainerTotalSize(), 0u);
  EXPECT_EQ(CountInvisibleRecords(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterBackgroundImageRemoval) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #target {
        background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
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

TEST_F(ImagePaintTimingDetectorTest,
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

TEST_F(ImagePaintTimingDetectorTest,
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

// This is to prove that a swap time is assigned only to nodes of the frame who
// register the swap time. In other words, swap time A should match frame A;
// swap time B should match frame B.
TEST_F(ImagePaintTimingDetectorTest, MatchSwapTimeToNodesOfDifferentFrames) {
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

TEST_F(ImagePaintTimingDetectorTest,
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
  base::TimeTicks result1 = LargestPaintStoredResult();
  EXPECT_GE(result1, time1);
  EXPECT_GE(time2, result1);

  SetImageAndPaint("target2", 10, 10);
  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  base::TimeTicks time3 = test_task_runner_->NowTicks();
  base::TimeTicks result2 = LargestPaintStoredResult();
  EXPECT_GE(result2, time2);
  EXPECT_GE(time3, result2);
}

TEST_F(ImagePaintTimingDetectorTest, OneSwapPromiseForOneFrame) {
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

TEST_F(ImagePaintTimingDetectorTest, VideoImage) {
  SetBodyInnerHTML(R"HTML(
    <video id="target"></video>
  )HTML");

  SetVideoImageAndPaint("target", 5, 5);

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_GT(record->first_size, 0ul);
  EXPECT_TRUE(record->loaded);
}

TEST_F(ImagePaintTimingDetectorTest, VideoImage_ImageNotLoaded) {
  SetBodyInnerHTML(R"HTML(
    <video id="target"></video>
  )HTML");

  UpdateAllLifecyclePhasesAndInvokeCallbackIfAny();
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_FALSE(record);
}

TEST_F(ImagePaintTimingDetectorTest, SVGImage) {
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

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage) {
  SetBodyInnerHTML(R"HTML(
    <style>
      div {
        background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
    <div>
      place-holder
    </div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(CountVisibleImageRecords(), 1u);
}

TEST_F(ImagePaintTimingDetectorTest,
       BackgroundImageAndLayoutImageTrackedDifferently) {
  SetBodyInnerHTML(R"HTML(
    <style>
      img {
        background-image: url(data:image/gif;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfAhkiAAAAAlwSFlzAAAN1wAADdcBQiibeAAAAb5JREFUOMulkr1KA0EQgGdvTwwnYmER0gQsrFKmSy+pLESw9Qm0F/ICNnba+h6iEOuAEWslKJKTOyJJvIT72d1xZuOFC0giOLA77O7Mt/PnNptN+I+49Xr9GhH3f3mb0v1ht9vtLAUYYw5ItkgDL3KyD8PhcLvdbl/WarXT3DjLMnAcR/f7/YfxeKwtgC5RKQVhGILWeg4hQ6hUKjWyucmhLFEUuWR3QYBWAZABQ9i5CCmXy16pVALP80BKaaG+70MQBLvzFMjRKKXh8j6FSYKF7ITdEWLa4/ktokN74wiqjSMpnVcbQZqmEJHz+ckeCPFjWKwULpyspAqhdXVXdcnZcPjsIgn+2BsVA8jVYuWlgJ3yBj0icgq2uoK+lg4t+ZvLomSKamSQ4AI5BcMADtMhyNoSgNIISUaFNtwlazcDcBc4gjjVwCWid2usCWroYEhnaqbzFJLUzAHIXRDChXCcQP8zhkSZ5eNLgHAUzwDcRu4CoIRn/wsGUQIIy4Vr9TH6SYFCNzw4nALn5627K4vIttOUOwfa5YnrDYzt/9OLv9I5l8kk5hZ3XLO20b7tbR7zHLy/BX8G0IeBEM7ZN1NGIaFUaKLgAAAAAElFTkSuQmCC);
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

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreBody) {
  SetBodyInnerHTML(R"HTML(
    <style>
      body {
        background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
  )HTML");
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreHtml) {
  SetBodyInnerHTML(R"HTML(
    <html>
    <style>
      html {
        background-image: url(data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==);
      }
    </style>
    </html>
  )HTML");
  EXPECT_EQ(CountVisibleImageRecords(), 0u);
}

TEST_F(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreGradient) {
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
TEST_F(ImagePaintTimingDetectorTest, BackgroundImageTrackedDifferently) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #d {
        width: 50px;
        height: 50px;
        background-image:
          url("data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="),
          url("data:image/gif;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfAhkiAAAAAlwSFlzAAAN1wAADdcBQiibeAAAAb5JREFUOMulkr1KA0EQgGdvTwwnYmER0gQsrFKmSy+pLESw9Qm0F/ICNnba+h6iEOuAEWslKJKTOyJJvIT72d1xZuOFC0giOLA77O7Mt/PnNptN+I+49Xr9GhH3f3mb0v1ht9vtLAUYYw5ItkgDL3KyD8PhcLvdbl/WarXT3DjLMnAcR/f7/YfxeKwtgC5RKQVhGILWeg4hQ6hUKjWyucmhLFEUuWR3QYBWAZABQ9i5CCmXy16pVALP80BKaaG+70MQBLvzFMjRKKXh8j6FSYKF7ITdEWLa4/ktokN74wiqjSMpnVcbQZqmEJHz+ckeCPFjWKwULpyspAqhdXVXdcnZcPjsIgn+2BsVA8jVYuWlgJ3yBj0icgq2uoK+lg4t+ZvLomSKamSQ4AI5BcMADtMhyNoSgNIISUaFNtwlazcDcBc4gjjVwCWid2usCWroYEhnaqbzFJLUzAHIXRDChXCcQP8zhkSZ5eNLgHAUzwDcRu4CoIRn/wsGUQIIy4Vr9TH6SYFCNzw4nALn5627K4vIttOUOwfa5YnrDYzt/9OLv9I5l8kk5hZ3XLO20b7tbR7zHLy/BX8G0IeBEM7ZN1NGIaFUaKLgAAAAAElFTkSuQmCC");
      }
    </style>
    <div id="d"></div>
  )HTML");
  EXPECT_EQ(CountVisibleImageRecords(), 2u);
}

TEST_F(ImagePaintTimingDetectorTest, DeactivateAfterUserInput) {
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

TEST_F(ImagePaintTimingDetectorTest, NullTimeNoCrash) {
  SetBodyInnerHTML(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  UpdateAllLifecyclePhases();
  UpdateCandidate();
}

TEST_F(ImagePaintTimingDetectorTest, Iframe) {
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

TEST_F(ImagePaintTimingDetectorTest, Iframe_ClippedByMainFrameViewport) {
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

TEST_F(ImagePaintTimingDetectorTest, Iframe_HalfClippedByMainFrameViewport) {
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

TEST_F(ImagePaintTimingDetectorTest, SameSizeShouldNotBeIgnored) {
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

TEST_F(ImagePaintTimingDetectorTest, UseIntrinsicSizeIfSmaller_Image) {
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

TEST_F(ImagePaintTimingDetectorTest, NotUseIntrinsicSizeIfLarger_Image) {
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

TEST_F(ImagePaintTimingDetectorTest,
       UseIntrinsicSizeIfSmaller_BackgroundImage) {
  SetBodyInnerHTML(R"HTML(
    <style>
      #d {
        width: 50px;
        height: 50px;
        background-image: url("data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==");
      }
    </style>
    <div id="d"></div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 1u);
}

TEST_F(ImagePaintTimingDetectorTest,
       NotUseIntrinsicSizeIfLarger_BackgroundImage) {
  // The image is in 16x16.
  SetBodyInnerHTML(R"HTML(
    <style>
      #d {
        width: 5px;
        height: 5px;
        background-image: url("data:image/gif;base64,iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABHNCSVQICAgIfAhkiAAAAAlwSFlzAAAN1wAADdcBQiibeAAAAb5JREFUOMulkr1KA0EQgGdvTwwnYmER0gQsrFKmSy+pLESw9Qm0F/ICNnba+h6iEOuAEWslKJKTOyJJvIT72d1xZuOFC0giOLA77O7Mt/PnNptN+I+49Xr9GhH3f3mb0v1ht9vtLAUYYw5ItkgDL3KyD8PhcLvdbl/WarXT3DjLMnAcR/f7/YfxeKwtgC5RKQVhGILWeg4hQ6hUKjWyucmhLFEUuWR3QYBWAZABQ9i5CCmXy16pVALP80BKaaG+70MQBLvzFMjRKKXh8j6FSYKF7ITdEWLa4/ktokN74wiqjSMpnVcbQZqmEJHz+ckeCPFjWKwULpyspAqhdXVXdcnZcPjsIgn+2BsVA8jVYuWlgJ3yBj0icgq2uoK+lg4t+ZvLomSKamSQ4AI5BcMADtMhyNoSgNIISUaFNtwlazcDcBc4gjjVwCWid2usCWroYEhnaqbzFJLUzAHIXRDChXCcQP8zhkSZ5eNLgHAUzwDcRu4CoIRn/wsGUQIIy4Vr9TH6SYFCNzw4nALn5627K4vIttOUOwfa5YnrDYzt/9OLv9I5l8kk5hZ3XLO20b7tbR7zHLy/BX8G0IeBEM7ZN1NGIaFUaKLgAAAAAElFTkSuQmCC");
      }
    </style>
    <div id="d"></div>
  )HTML");
  ImageRecord* record = FindLargestPaintCandidate();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->first_size, 25u);
}

}  // namespace blink
