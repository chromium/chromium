// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/image_paint_timing_detector.h"

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/tracing/trace_event_analyzer.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/loader/resource/video_timing.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_calculator.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_manager.h"
#include "third_party/blink/renderer/core/paint/timing/mock_paint_timing_callback_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_record.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_base.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/core/timing/performance_timing_for_reporting.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/size.h"

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

class ImagePaintTimingDetectorTestBase : public PaintTimingTestBase {
 public:
  ImagePaintTimingDetectorTestBase() = default;

  const PerformanceTimingForReporting& GetPerformanceTimingForReporting() {
    PerformanceTimingForReporting* performance_for_reporting =
        DOMWindowPerformance::performance(*GetFrame().DomWindow())
            ->timingForReporting();
    return *performance_for_reporting;
  }

  ImageRecord* LargestImage() {
    return PaintTiming::From(GetDocument())
        .GetLargestContentfulPaintManager()
        ->LargestContentfulPaintCalculatorForTest()
        ->LargestPaintedOrPendingImageForTest();
  }

  ImageRecord* LargestPaintedImage() {
    return PaintTiming::From(GetDocument())
        .GetLargestContentfulPaintManager()
        ->LargestContentfulPaintCalculatorForTest()
        ->LargestPaintedImageForTest();
  }

  ImageRecord* ChildFrameLargestImage() {
    return GetChildPaintTimingDetector()
        .GetPaintTiming()
        .GetLargestContentfulPaintManager()
        ->LargestContentfulPaintCalculatorForTest()
        ->LargestPaintedOrPendingImageForTest();
  }

  size_t CountImageRecords() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .recorded_images_.size();
  }

  size_t ContainerTotalSize() {
    size_t result = GetPaintTimingDetector()
                        .GetImagePaintTimingDetector()
                        .recorded_images_.size() +
                    GetPaintTimingDetector()
                        .GetImagePaintTimingDetector()
                        .pending_images_.size() +
                    GetPaintTimingDetector()
                        .GetImagePaintTimingDetector()
                        .images_queued_for_paint_time_.size() +
                    GetPaintTimingDetector()
                        .GetImagePaintTimingDetector()
                        .image_finished_times_.size();

    return result;
  }

  size_t CountChildFrameRecords() {
    return GetChildPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .recorded_images_.size();
  }

  base::TimeTicks LargestPaintTime() {
    return PaintTiming::From(GetDocument())
        .GetLargestContentfulPaintManager()
        ->LargestContentfulPaintCalculatorForTest()
        ->LatestLcpDetails()
        .largest_image_paint_time;
  }

  uint64_t LargestPaintSize() {
    return PaintTiming::From(GetDocument())
        .GetLargestContentfulPaintManager()
        ->LargestContentfulPaintCalculatorForTest()
        ->LatestLcpDetails()
        .largest_image_paint_size;
  }

  bool HasLargestIgnoredImage() {
    return PaintTiming::From(GetDocument())
        .GetLargestContentfulPaintManager()
        ->HasLargestIgnoredImageForTest();
  }

  void SetImageAndPaint(const char* id, int width, int height) {
    Element* element = GetDocument().getElementById(AtomicString(id));
    ImageResourceContent* content = CreateImageForTest(width, height);
    To<HTMLImageElement>(element)->SetImageForTest(content);
  }

  void SetChildFrameImageAndPaint(const char* id, int width, int height) {
    Element* element = ChildDocument().getElementById(AtomicString(id));
    DCHECK(element);
    ImageResourceContent* content = CreateImageForTest(width, height);
    To<HTMLImageElement>(element)->SetImageForTest(content);
  }

  void SetSVGImageAndPaint(const char* id, int width, int height) {
    Element* element = GetDocument().getElementById(AtomicString(id));
    ImageResourceContent* content = CreateImageForTest(width, height);
    To<SVGImageElement>(element)->SetImageForTest(content);
  }

  void SimulateImagePaint(Element* element,
                          MediaTiming* timing,
                          int width,
                          int height) {
    // Fake the property tree state and border properties since these are
    // invalid when simulating image paints.
    gfx::Rect border(width, height);
    auto property_tree_state = PropertyTreeStateOrAlias::Root();
    GetPaintTimingDetector().NotifyImagePaint(*element->GetLayoutObject(),
                                              gfx::Size(width, height), *timing,
                                              property_tree_state, border);
  }

  void SimulateFirstVideoFrame(Element* element,
                               VideoTiming* timing,
                               int width,
                               int height) {
    // Unlike simulating image paints, the property tree state and border
    // properties should be set for video elements.
    GetPaintTimingDetector().NotifyFirstVideoFrame(
        *element->GetLayoutObject(), gfx::Size(width, height), *timing,
        element->GetLayoutObject()->FirstFragment().LocalBorderBoxProperties(),
        element->GetLayoutObject()->AbsoluteBoundingBoxRect());
  }

 protected:
  base::test::TracingEnvironment tracing_environment_;
};

class ImagePaintTimingDetectorTest : public ImagePaintTimingDetectorTestBase,
                                     public PaintTestConfigurations {
 public:
  ImagePaintTimingDetectorTest() = default;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ImagePaintTimingDetectorTest);

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_NoImage) {
  SetMainFrameBodyContent(R"HTML(
    <div></div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  ImageRecord* record = LargestImage();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_OneImage) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetMainFrameBodyContent(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->EffectiveVisualSize(), 25ul);
  EXPECT_FALSE(record->LoadTime().is_null());
  // Simulate some input event to force StopRecordEntries().
  SimulateKeyDown();
  auto entries = test_ukm_recorder.GetEntriesByName(UkmPaintTiming::kEntryName);
  EXPECT_EQ(1ul, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry, UkmPaintTiming::kLCPDebugging_HasViewportImageName, false);
}

TEST_P(ImagePaintTimingDetectorTest, InsertionOrderIsSecondaryRankingKey) {
  SetMainFrameBodyContent(R"HTML(
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

  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(LargestImage()->GetNode(), image1);
  EXPECT_EQ(LargestPaintSize(), 25ul);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_TraceEvent_Candidate) {
  using trace_analyzer::Query;
  trace_analyzer::Start("loading");
  {
    SetMainFrameBodyContent(R"HTML(
      <img id="target"></img>
    )HTML");
    SetImageAndPaint("target", 5, 5);
    SimulateRenderingAndPresentationTime();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestImagePaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::DictValue arg_dict = events[0]->GetKnownArgAsDict("data");
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
    SetMainFrameBodyContent(R"HTML(
      <style>iframe { display: block; position: relative; margin-left: 30px; margin-top: 50px; width: 250px; height: 250px;} </style>
      <iframe> </iframe>
    )HTML");
    SetChildFrameBodyContent(R"HTML(
      <style>body { margin: 10px;} #target { width: 200px; height: 200px; }
      </style>
      <img id="target"></img>
    )HTML");
    SetChildFrameImageAndPaint("target", 5, 5);
    SimulateRenderingAndPresentationTime();
  }
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestImagePaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(1u, events.size());
  EXPECT_EQ("loading", events[0]->category);

  EXPECT_TRUE(events[0]->HasStringArg("frame"));

  ASSERT_TRUE(events[0]->HasDictArg("data"));
  base::DictValue arg_dict = events[0]->GetKnownArgAsDict("data");
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
  SetMainFrameBodyContent(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 25u);
  EXPECT_GT(largest_contentful_paint_details.image_paint_time, 0u);
}

TEST_P(ImagePaintTimingDetectorTest, UpdatePerformanceTimingToZero) {
  SetMainFrameBodyContent(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  auto largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 25u);
  EXPECT_GT(largest_contentful_paint_details.image_paint_time, 0u);
  GetDocument().body()->RemoveChild(
      GetDocument().getElementById(AtomicString("target")));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 25u);
  EXPECT_GT(largest_contentful_paint_details.image_paint_time, 0u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_OpacityZero) {
  SetMainFrameBodyContent(R"HTML(
    <style>
    img {
      opacity: 0;
    }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);
  ImageRecord* record = LargestImage();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_VisibilityHidden) {
  SetMainFrameBodyContent(R"HTML(
    <style>
    img {
      visibility: hidden;
    }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);
  ImageRecord* record = LargestImage();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_DisplayNone) {
  SetMainFrameBodyContent(R"HTML(
    <style>
    img {
      display: none;
    }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);
  ImageRecord* record = LargestImage();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_OpacityNonZero) {
  SetMainFrameBodyContent(R"HTML(
    <style>
    img {
      opacity: 0.01;
    }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 1u);
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
}

TEST_P(ImagePaintTimingDetectorTest,
       IgnoreImageUntilInvalidatedRectSizeNonZero) {
  SetMainFrameBodyContent(R"HTML(
    <img id="target"></img>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(CountImageRecords(), 1u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_Largest) {
  SetMainFrameBodyContent(R"HTML(
    <style>img { display:block }</style>
    <img id="smaller"></img>
    <img id="medium"></img>
    <img id="larger"></img>
  )HTML");
  SetImageAndPaint("smaller", 5, 5);
  SimulateRenderingAndPresentationTime();
  ImageRecord* record;
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->EffectiveVisualSize(), 25ul);

  SetImageAndPaint("larger", 9, 9);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LargestPaintSize(), 81ul);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_IgnoreThoseOutsideViewport) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      img {
        position: fixed;
        top: -100px;
      }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  ImageRecord* record = LargestImage();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_UpdateOnRemovingTheLastImage) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  ImageRecord* record;
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  EXPECT_EQ(LargestPaintSize(), 25ul);

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target")));
  SimulateRenderingAndPresentationTime();
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  EXPECT_EQ(LargestPaintSize(), 25u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_UpdateOnRemoving) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target1"></img>
      <img id="target2"></img>
    </div>
  )HTML");
  SetImageAndPaint("target1", 5, 5);
  SimulateRenderingAndPresentationTime();
  ImageRecord* record1 = LargestImage();
  EXPECT_TRUE(record1);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  base::TimeTicks first_largest_image_paint = LargestPaintTime();

  SetImageAndPaint("target2", 10, 10);
  SimulateRenderingAndPresentationTime();
  ImageRecord* record2 = LargestImage();
  EXPECT_TRUE(record2);
  EXPECT_NE(LargestPaintTime(), base::TimeTicks());
  base::TimeTicks second_largest_image_paint = LargestPaintTime();

  EXPECT_NE(record1, record2);
  EXPECT_NE(first_largest_image_paint, second_largest_image_paint);

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target2")));
  SimulateRenderingAndPresentationTime();
  ImageRecord* record3 = LargestImage();
  EXPECT_EQ(record2, record3);
  EXPECT_EQ(second_largest_image_paint, LargestPaintTime());
  EXPECT_EQ(LargestPaintSize(), 100u);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_NodeRemovedBetweenRegistrationAndInvocation) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRendering();

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target")));

  SimulatePresentationTime();

  ImageRecord* record;
  record = LargestImage();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterImageRemoval) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 2u);

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target")));
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterInvisibleImageRemoved) {
  SetMainFrameBodyContent(R"HTML(
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
  SimulateRenderingAndPresentationTime();
  // The out-of-viewport image will not have been recorded yet.
  EXPECT_EQ(ContainerTotalSize(), 1u);

  GetDocument().body()->RemoveChild(
      GetDocument().getElementById(AtomicString("parent")));
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterBackgroundImageRemoval) {
  SetMainFrameBodyContent(R"HTML(
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
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 2u);

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target")));
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterImageRemovedAndCallbackInvoked) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRendering();
  EXPECT_EQ(ContainerTotalSize(), 4u);

  GetDocument()
      .getElementById(AtomicString("parent"))
      ->RemoveChild(GetDocument().getElementById(AtomicString("target")));
  // Lazy deletion from |images_queued_for_paint_time_|.
  EXPECT_EQ(ContainerTotalSize(), 1u);
  SimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_ReattachedNodeNotTreatedAsNew) {
  base::TimeTicks start_time = NowTicks();
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
    </div>
  )HTML");
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kIdAttr, AtomicString("target"));
  GetDocument().getElementById(AtomicString("parent"))->AppendChild(image);
  SetImageAndPaint("target", 5, 5);
  FastForwardBy(base::Seconds(1));
  SimulateRenderingAndPresentationTime();
  ImageRecord* record;
  record = LargestImage();
  EXPECT_TRUE(record);
  // SimulateRenderingAndPresentationTime() moves time forward
  // kQuantumOfTime so we should take that into account.
  EXPECT_EQ(record->PaintTime(),
            start_time + base::Seconds(1) + kQuantumOfTime);

  GetDocument().getElementById(AtomicString("parent"))->RemoveChild(image);
  FastForwardBy(base::Seconds(1));
  SimulateRenderingAndPresentationTime();
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->PaintTime(),
            start_time + base::Seconds(1) + kQuantumOfTime);

  GetDocument().getElementById(AtomicString("parent"))->AppendChild(image);
  SetImageAndPaint("target", 5, 5);
  FastForwardBy(base::Seconds(1));
  SimulateRenderingAndPresentationTime();
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->PaintTime(),
            start_time + base::Seconds(1) + kQuantumOfTime);
}

// This is to prove that a presentation time is assigned only to nodes of the
// frame who register the presentation time. In other words, presentation time A
// should match frame A; presentation time B should match frame B.
TEST_P(ImagePaintTimingDetectorTest,
       MatchPresentationTimeToNodesOfDifferentFrames) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img height="5" width="5" id="smaller"></img>
      <img height="9" width="9" id="larger"></img>
    </div>
  )HTML");

  SetImageAndPaint("smaller", 5, 5);
  SimulateRendering();
  SimulatePassOfTime();

  SetImageAndPaint("larger", 9, 9);
  SimulateRendering();
  SimulatePassOfTime();

  // Invoke callbacks for the first frame.
  SimulatePresentationTime();
  // record1 is the smaller.
  ImageRecord* record1 = LargestPaintedImage();
  EXPECT_EQ(record1->EffectiveVisualSize(), 25ul);
  const base::TimeTicks record1Time = record1->PaintTime();

  // Invoke callbacks for the second frame.
  SimulatePassOfTime();
  SimulatePresentationTime();
  // record2 is the larger.
  ImageRecord* record2 = LargestPaintedImage();
  EXPECT_EQ(record2->EffectiveVisualSize(), 81ul);
  EXPECT_NE(record1Time, record2->PaintTime());
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_UpdateResultWhenLargestChanged) {
  base::TimeTicks time1 = NowTicks();
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target1"></img>
      <img id="target2"></img>
    </div>
  )HTML");
  SetImageAndPaint("target1", 5, 5);
  SimulateRenderingAndPresentationTime();
  base::TimeTicks time2 = NowTicks();
  base::TimeTicks result1 = LargestPaintTime();
  EXPECT_GE(result1, time1);
  EXPECT_GE(time2, result1);

  SetImageAndPaint("target2", 10, 10);
  SimulateRenderingAndPresentationTime();
  base::TimeTicks time3 = NowTicks();
  base::TimeTicks result2 = LargestPaintTime();
  EXPECT_GE(result2, time2);
  EXPECT_GE(time3, result2);
}

TEST_P(ImagePaintTimingDetectorTest, OnePresentationPromiseForOneFrame) {
  SetMainFrameBodyContent(R"HTML(
    <style>img { display:block }</style>
    <div id="parent">
      <img id="1"></img>
      <img id="2"></img>
    </div>
  )HTML");
  SetImageAndPaint("1", 5, 5);
  SimulateRendering();
  SimulatePassOfTime();

  SetImageAndPaint("2", 9, 9);
  SimulateRendering();
  SimulatePassOfTime();

  // This callback only assigns a time to the 5x5 image.
  SimulatePresentationTime();
  ImageRecord* record;
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->EffectiveVisualSize(), 81ul);
  EXPECT_FALSE(record->HasPaintTime());

  // This callback assigns a time to the 9x9 image.
  SimulatePresentationTime();
  record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->EffectiveVisualSize(), 81ul);
  EXPECT_TRUE(record->HasPaintTime());
}

TEST_P(ImagePaintTimingDetectorTest, VideoImage) {
  SetMainFrameBodyContent(R"HTML(
    <video id="target" poster=")HTML" LARGE_IMAGE R"HTML("></video>
  )HTML");
  // Poster image rendering requires flushing pending tasks first.
  test::RunPendingTasks();
  SimulateRenderingAndPresentationTime();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_GT(record->EffectiveVisualSize(), 0ul);
  EXPECT_TRUE(record->HasPaintTime());
}

TEST_P(ImagePaintTimingDetectorTest, VideoImage_ImageNotLoaded) {
  SetMainFrameBodyContent("<video id='target'></video>");

  SimulateRenderingAndPresentationTime();
  ImageRecord* record = LargestImage();
  EXPECT_FALSE(record);
}

TEST_P(ImagePaintTimingDetectorTest, SVGImage) {
  SetMainFrameBodyContent(R"HTML(
    <svg>
      <image id="target" width="10" height="10"/>
    </svg>
  )HTML");

  SetSVGImageAndPaint("target", 5, 5);

  SimulateRenderingAndPresentationTime();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_GT(record->EffectiveVisualSize(), 0ul);
  EXPECT_TRUE(record->HasPaintTime());
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      div {
        background-image: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
    <div>place-holder</div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(CountImageRecords(), 1u);
}

TEST_P(ImagePaintTimingDetectorTest,
       BackgroundImageAndLayoutImageTrackedDifferently) {
  SetMainFrameBodyContent(R"HTML(
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
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 2u);
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->EffectiveVisualSize(), 1u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreBody) {
  SetMainFrameBodyContent("<style>body { background-image: url(" SIMPLE_IMAGE
                          ")}</style>");
  SimulateRendering();
  EXPECT_EQ(CountImageRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreHtml) {
  SetMainFrameBodyContent("<style>html { background-image: url(" SIMPLE_IMAGE
                          ")}</style>");
  SimulateRendering();
  EXPECT_EQ(CountImageRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreGradient) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      div {
        background-image: linear-gradient(blue, yellow);
      }
    </style>
    <div>
      place-holder
    </div>
  )HTML");
  SimulateRendering();
  EXPECT_EQ(CountImageRecords(), 0u);
}

// We put two background images in the same object, and test whether FCP++ can
// find two different images.
TEST_P(ImagePaintTimingDetectorTest, BackgroundImageTrackedDifferently) {
  SetMainFrameBodyContent(R"HTML(
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
  SimulateRendering();
  EXPECT_EQ(CountImageRecords(), 2u);
}

TEST_P(ImagePaintTimingDetectorTest, DeactivateAfterUserInput) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SimulateScroll();
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(
      PaintTiming::From(GetDocument()).GetLargestContentfulPaintManager());
}

TEST_P(ImagePaintTimingDetectorTest, ContinueAfterKeyUp) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SimulateKeyUp();
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(
      PaintTiming::From(GetDocument()).GetLargestContentfulPaintManager());
}

TEST_P(ImagePaintTimingDetectorTest, NullTimeNoCrash) {
  SetMainFrameBodyContent(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
}

TEST_P(ImagePaintTimingDetectorTest, Iframe) {
  SetMainFrameBodyContent(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildFrameBodyContent(R"HTML(
    <style>img { display:block }</style>
    <img id="target"></img>
  )HTML");
  SetChildFrameImageAndPaint("target", 5, 5);
  SimulateRendering();
  // Ensure main frame doesn't capture this image.
  EXPECT_EQ(CountImageRecords(), 0u);
  EXPECT_EQ(CountChildFrameRecords(), 1u);
  SimulatePresentationTime();
  ImageRecord* image = ChildFrameLargestImage();
  EXPECT_TRUE(image);
  // Ensure the image size is not clipped (5*5).
  EXPECT_EQ(image->EffectiveVisualSize(), 25ul);
}

TEST_P(ImagePaintTimingDetectorTest, Iframe_ClippedByMainFrameViewport) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #f { margin-top: 1234567px }
    </style>
    <iframe id="f" width=100px height=100px></iframe>
  )HTML");
  SetChildFrameBodyContent(R"HTML(
    <style>img { display:block }</style>
    <img id="target"></img>
  )HTML");
  // Make sure the iframe is out of main-frame's viewport.
  EXPECT_LT(GetViewportRect(GetFrameView()).height(), 1234567);
  SetChildFrameImageAndPaint("target", 5, 5);
  SimulateRendering();
  EXPECT_EQ(CountImageRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, Iframe_HalfClippedByMainFrameViewport) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #f { margin-left: -5px; }
    </style>
    <iframe id="f" width=10px height=10px></iframe>
  )HTML");
  SetChildFrameBodyContent(R"HTML(
    <style>img { display:block }</style>
    <img id="target"></img>
  )HTML");
  SetChildFrameImageAndPaint("target", 10, 10);
  SimulateRendering();
  EXPECT_EQ(CountImageRecords(), 0u);
  EXPECT_EQ(CountChildFrameRecords(), 1u);
  SimulatePresentationTime();
  ImageRecord* image = ChildFrameLargestImage();
  EXPECT_TRUE(image);
  EXPECT_LT(image->EffectiveVisualSize(), 100ul);
}

TEST_P(ImagePaintTimingDetectorTest, SameSizeShouldNotBeIgnored) {
  SetMainFrameBodyContent(R"HTML(
    <style>img { display:block }</style>
    <img id='1'></img>
    <img id='2'></img>
    <img id='3'></img>
  )HTML");
  SetImageAndPaint("1", 5, 5);
  SetImageAndPaint("2", 5, 5);
  SetImageAndPaint("3", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 3u);
}

TEST_P(ImagePaintTimingDetectorTest, UseIntrinsicSizeIfSmaller_Image) {
  SetMainFrameBodyContent(R"HTML(
    <img height="300" width="300" display="block" id="target">
    </img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->EffectiveVisualSize(), 25u);
}

TEST_P(ImagePaintTimingDetectorTest, NotUseIntrinsicSizeIfLarger_Image) {
  SetMainFrameBodyContent(R"HTML(
    <img height="1" width="1" display="block" id="target">
    </img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->EffectiveVisualSize(), 1u);
}

TEST_P(ImagePaintTimingDetectorTest,
       UseIntrinsicSizeIfSmaller_BackgroundImage) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #d {
        width: 50px;
        height: 50px;
        background-image: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
    <div id="d"></div>
  )HTML");
  SimulateRendering();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->EffectiveVisualSize(), 1u);
}

TEST_P(ImagePaintTimingDetectorTest,
       NotUseIntrinsicSizeIfLarger_BackgroundImage) {
  // The image is in 16x16.
  SetMainFrameBodyContent(R"HTML(
    <style>
      #d {
        width: 5px;
        height: 5px;
        background-image: url()HTML" LARGE_IMAGE R"HTML();
      }
    </style>
    <div id="d"></div>
  )HTML");
  SimulateRendering();
  ImageRecord* record = LargestImage();
  EXPECT_TRUE(record);
  EXPECT_EQ(record->EffectiveVisualSize(), 25u);
}

TEST_P(ImagePaintTimingDetectorTest, OpacityZeroHTML) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);
  EXPECT_TRUE(HasLargestIgnoredImage());

  // Change the opacity of documentElement, now the img should be a candidate.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(HasLargestIgnoredImage());
  EXPECT_EQ(CountImageRecords(), 1u);
  auto largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 25u);
  EXPECT_GT(largest_contentful_paint_details.image_paint_time, 0u);
}

TEST_P(ImagePaintTimingDetectorTest, OpacityZeroHTML2) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #target {
        opacity: 0;
      }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 0"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, OpacityZeroHTMLWithInput) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 256, 256);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);

  // Simulate input to stop LCP.
  SimulateKeyDown();

  // Change the opacity of documentElement. The img should not be a candidate
  // because LCP stops on input.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);
  auto largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 0u);
  EXPECT_EQ(largest_contentful_paint_details.image_paint_time, 0u);

  PaintTiming& paint_timing = PaintTiming::From(GetDocument());
  // FCP and first image paint should not be marked, since this feature is tied
  // to hard LCP.
  //
  // Note: `PaintTiming` doesn't support `MockPaintTimingCallbackManager`, so
  // check the paint time instead of presentation time.
  base::TimeTicks fcp_timestamp =
      paint_timing.FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime();
  EXPECT_TRUE(fcp_timestamp.is_null());

  base::TimeTicks image_timestamp =
      paint_timing.FirstImagePaintRenderedButNotPresentedAsMonotonicTime();
  EXPECT_TRUE(image_timestamp.is_null());
}

TEST_P(ImagePaintTimingDetectorTest, OpacityZeroHTMLRemoveElement) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 256, 256);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);
  EXPECT_TRUE(HasLargestIgnoredImage());

  GetDocument().body()->RemoveChild(
      GetDocument().getElementById(AtomicString("target")));
  EXPECT_FALSE(HasLargestIgnoredImage());
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(CountImageRecords(), 0u);

  auto largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 0u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_FullViewportImage) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetMainFrameBodyContent(R"HTML(
    <style>body {margin: 0px;}</style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 3000, 3000);
  SimulateRenderingAndPresentationTime();
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
  SetMainFrameBodyContent(R"HTML(
      <style>iframe { display: block; position: relative; margin-left: 30px; margin-top: 50px; width: 250px; height: 250px;} </style>
      <iframe> </iframe>
    )HTML");
  SetChildFrameBodyContent(R"HTML(
      <style>body { margin: 10px;} #target { width: 200px; height: 200px; }
      </style>
      <img id="target"></img>
    )HTML");
  SetChildFrameImageAndPaint("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  LocalFrame* child_frame = &ChildFrame();
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("",
                                                        ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(child_frame->IsDetached());

  // Start tracing, we only want to capture it during the ReportPaintTime.
  trace_analyzer::Start("loading");
  SimulateRenderingAndPresentationTime();

  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector events;
  Query q = Query::EventNameIs("LargestImagePaint::Candidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(0u, events.size());
  q = Query::EventNameIs("LargestImagePaint::NoCandidate");
  analyzer->FindEvents(q, &events);
  EXPECT_EQ(0u, events.size());
}

TEST_P(ImagePaintTimingDetectorTest, LargestPaintedImageSetForFirstVideoFrame) {
  ScopedReportFirstFrameTimeAsRenderTimeForTest
      scoped_enable_use_first_frame_time(true);
  SetMainFrameBodyContent(R"HTML(
    <video id="target" width=300 height=200></video>
  )HTML");

  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(LargestImage());

  Element* video_element = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(video_element);
  ASSERT_TRUE(video_element->GetLayoutObject());

  VideoTiming* video_timing = MakeGarbageCollected<VideoTiming>();
  video_timing->SetFirstVideoFrameTime(NowTicks());
  video_timing->SetIsSufficientContentLoadedForPaint();
  video_timing->SetUrl(KURL("http://test.com/video"));
  video_timing->SetContentSizeForEntropy(1024 * 1024);

  // Since ReportFirstFrameTimeAsRenderTime is enabled, this should create an
  // `ImageRecord` and set its paint and presentation time. But the image will
  // only be pending until the next animation frame.
  SimulateFirstVideoFrame(video_element, video_timing, 300, 100);
  EXPECT_FALSE(LargestPaintedImage());
  ImageRecord* record = LargestImage();
  ASSERT_TRUE(record);
  EXPECT_GT(record->EffectiveVisualSize(), 0ul);
  EXPECT_TRUE(record->HasPaintTime());

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LargestPaintedImage(), record);
}

TEST_P(ImagePaintTimingDetectorTest, FirstVideoFrameRacesWithPosterImage) {
  SetMainFrameBodyContent(R"HTML(
    <video id="target" width=300 height=200></video>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(LargestImage());

  Element* video_element = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(video_element);
  ASSERT_TRUE(video_element->GetLayoutObject());

  // First, simulate painting the pending poster image.
  ImageResourceContent* image_timing =
      CreateImageForTest(300, 200, /*bytes=*/0, ImageStatus::kPending);
  SimulateImagePaint(video_element, image_timing, 300, 200);
  EXPECT_FALSE(LargestPaintedImage());
  // LCP should track `image_timing` as the largest pending image.
  ImageRecord* record1 = LargestImage();
  ASSERT_TRUE(record1);
  EXPECT_EQ(record1->GetMediaTiming(), image_timing);
  EXPECT_EQ(CountImageRecords(), 1u);

  // Next, simulate the first video frame while the poster image is still
  // pending.
  VideoTiming* video_timing = MakeGarbageCollected<VideoTiming>();
  video_timing->SetFirstVideoFrameTime(NowTicks());
  video_timing->SetIsSufficientContentLoadedForPaint();
  video_timing->SetUrl(KURL("http://test.com/video"));
  video_timing->SetContentSizeForEntropy(1024 * 1024);

  // The first video frame should replace the poster image as the <video>'s
  // media.
  SimulateFirstVideoFrame(video_element, video_timing, 300, 200);
  EXPECT_FALSE(LargestPaintedImage());
  ImageRecord* record2 = LargestImage();
  ASSERT_TRUE(record2);
  EXPECT_NE(record1, record2);
  EXPECT_EQ(record2->GetMediaTiming(), video_timing);
  // There's still only 1 record since the poster image was replaced.
  EXPECT_EQ(CountImageRecords(), 1u);

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LargestPaintedImage(), record2);
}

class ImagePaintTimingDetectorFencedFrameTest
    : private ScopedFencedFramesForTest,
      public ImagePaintTimingDetectorTest {
 public:
  ImagePaintTimingDetectorFencedFrameTest() : ScopedFencedFramesForTest(true) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

 protected:
  void SetUp() override {
    ImagePaintTimingDetectorTest::SetUp();

    GetDocument().GetPage()->SetIsMainFrameFencedFrameRoot();
    ASSERT_TRUE(GetDocument().GetFrame()->IsInFencedFrameTree());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ImagePaintTimingDetectorFencedFrameTest);

TEST_P(ImagePaintTimingDetectorFencedFrameTest, NotReported) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetMainFrameBodyContent(R"HTML(
    <style>body {margin: 0px;}</style>
    <img id="target"></img>
  )HTML");
  SetImageAndPaint("target", 3000, 3000);
  SimulateRenderingAndPresentationTime();
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
  ImagePaintTimingDetectorTransparentPlaceholderImageTest() = default;
  ~ImagePaintTimingDetectorTransparentPlaceholderImageTest() override {
    // Must destruct all objects before toggling back feature flags.
    std::unique_ptr<base::test::TaskEnvironment> task_environment;
    if (!base::ThreadPoolInstance::Get()) {
      // Create a TaskEnvironment for the garbage collection below.
      task_environment = std::make_unique<base::test::TaskEnvironment>();
    }
    WebHeap::CollectAllGarbageForTesting();
  }

 protected:
  void SetTransparentPlaceholderImageAndPaint(const char* id) {
    Element* element = GetDocument().getElementById(AtomicString(id));
    ImageResource* resource = ImageResource::CreateForTest(
        url_test_helpers::ToKURL(TRANSPARENT_PLACEHOLDER_IMAGE));
    To<HTMLImageElement>(element)->SetImageForTest(resource->GetContent());
  }
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
  SetMainFrameBodyContent(R"HTML(
      <img id="placeholder"></img>
    )HTML");
  SetTransparentPlaceholderImageAndPaint("placeholder");
  SimulateRenderingAndPresentationTime();
  largest_contentful_paint_details =
      GetPerformanceTimingForReporting()
          .LargestContentfulPaintDetailsForMetrics();
  EXPECT_EQ(largest_contentful_paint_details.image_paint_size, 1u);
  EXPECT_GT(largest_contentful_paint_details.image_paint_time, 0u);
}

namespace {

class FakeAnimatedImageTiming final
    : public GarbageCollected<FakeAnimatedImageTiming>,
      public MediaTiming {
 public:
  FakeAnimatedImageTiming() = default;

  void SetFirstVideoFrameTime(base::TimeTicks) override { NOTREACHED(); }
  base::TimeTicks GetFirstVideoFrameTime() const override {
    return base::TimeTicks();
  }
  std::optional<WebURLRequest::Priority> RequestPriority() const override {
    return std::nullopt;
  }
  bool IsDataUrl() const override { return false; }
  bool IsBroken() const override { return false; }
  base::TimeTicks DiscoveryTime() const override { return base::TimeTicks(); }
  base::TimeTicks LoadStart() const override { return base::TimeTicks(); }
  base::TimeTicks LoadEnd() const override { return base::TimeTicks(); }

  const KURL& Url() const override { return url_; }
  AtomicString MediaType() const override { return AtomicString("gif"); }

  bool IsAnimatedImage() const override { return true; }

  // Allows tests to control when the first frame was painted.
  void SetIsPaintedFirstFrame() { is_painted_first_frame_ = true; }
  bool IsPaintedFirstFrame() const override { return is_painted_first_frame_; }

  // Allows tests to control when the image is sufficiently loaded.
  void SetIsSufficientContentLoadedForPaint() override {
    is_sufficiently_loaded_for_paint_ = true;
  }
  bool IsSufficientContentLoadedForPaint() const override {
    return is_sufficiently_loaded_for_paint_;
  }

  // Ensure the image has enough entropy to be considered for LCP.
  uint64_t ContentSizeForEntropy() const override { return 100000; }

  void Trace(Visitor*) const override {}

 private:
  bool is_painted_first_frame_ = false;
  bool is_sufficiently_loaded_for_paint_ = false;
  KURL url_{"http://test.com/animated.gif"};
};

}  // namespace

class ImagePaintTimingDetectorAnimatedImageTest
    : public ImagePaintTimingDetectorTestBase,
      public testing::WithParamInterface<bool> {
 protected:
  ImagePaintTimingDetectorAnimatedImageTest()
      : scoped_feature_(IsReportFirstFrameTimeAsRenderTimeEnabled()) {}

  bool IsReportFirstFrameTimeAsRenderTimeEnabled() { return GetParam(); }

 private:
  ScopedReportFirstFrameTimeAsRenderTimeForTest scoped_feature_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ImagePaintTimingDetectorAnimatedImageTest,
    testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return info.param ? "ReportFirstFrameTimeAsRenderTimeEnabled"
                        : "ReportFirstFrameTimeAsRenderTimeDisabled";
    });

TEST_P(ImagePaintTimingDetectorAnimatedImageTest, ImageRenderingSequence) {
  SetMainFrameBodyContent(R"HTML(
      <img id="target" style:"width:100px;height:100px"></img>
    )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(LargestImage());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  auto* timing = MakeGarbageCollected<FakeAnimatedImageTiming>();

  // Simulate a paint without the first frame or sufficiently loaded content.
  SimulateImagePaint(target, timing, 100, 100);
  // The image should be pending and recorded.
  EXPECT_EQ(ContainerTotalSize(), 2u);
  SimulateRenderingAndPresentationTime();
  // For LCP, the largest pending should be set, but the largest should not be.
  EXPECT_TRUE(LargestImage());
  EXPECT_FALSE(LargestPaintedImage());

  // Simulate a paint with the first frame painted.
  timing->SetIsPaintedFirstFrame();
  SimulateImagePaint(target, timing, 100, 100);
  SimulateRendering();
  // The image should be pending, recorded, and queued for paint time for the
  // first image frame (regardless of the feature), and queued for paint time
  // for being sufficiently loaded (with the feature).
  EXPECT_EQ(ContainerTotalSize(),
            IsReportFirstFrameTimeAsRenderTimeEnabled() ? 4u : 3u);

  // Simulate presentation time. This should set the first animated frame time
  // with and without the feature, and set the paint time with the feature.
  SimulatePresentationTime();
  base::TimeTicks expected_first_frame_time = base::TimeTicks::Now();

  // There should be 1 entry if the feature is enabled (recorded) and 2 if not
  // (recorded and pending).
  EXPECT_EQ(ContainerTotalSize(),
            IsReportFirstFrameTimeAsRenderTimeEnabled() ? 1u : 2u);
  // In either case, `record` will be the largest pending image.
  ImageRecord* record = LargestImage();
  ASSERT_TRUE(record);
  EXPECT_EQ(record->FirstAnimatedFrameTime(), expected_first_frame_time);
  EXPECT_EQ(record->GetNode(), target);
  // But the image should only be reported with the feature enabled.
  EXPECT_EQ(record->IsSufficientlyLoadedForReporting(),
            IsReportFirstFrameTimeAsRenderTimeEnabled());
  EXPECT_EQ(record->HasPaintTime(),
            IsReportFirstFrameTimeAsRenderTimeEnabled());
  EXPECT_EQ(!!LargestPaintedImage(),
            IsReportFirstFrameTimeAsRenderTimeEnabled());

  // Finally, simulate a paint with the `timing` sufficiently loaded. This
  // should be a no-op if with the feature enabled, and it should cause the
  // record to be reported without.
  timing->SetIsSufficientContentLoadedForPaint();
  SimulateImagePaint(target, timing, 100, 100);
  SimulateRendering();
  // There should be 1 entry if the feature is enabled (recorded) and 3 if not
  // (recorded, pending, and queued for paint time).
  EXPECT_EQ(ContainerTotalSize(),
            IsReportFirstFrameTimeAsRenderTimeEnabled() ? 1u : 3u);
  SimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 1u);
  record = LargestImage();
  EXPECT_EQ(record->FirstAnimatedFrameTime(), expected_first_frame_time);
  EXPECT_TRUE(record->IsSufficientlyLoadedForReporting());
  EXPECT_EQ(record->GetNode(), target);
  EXPECT_TRUE(record->HasPaintTime());
  EXPECT_EQ(LargestPaintedImage(), record);
}

TEST_P(ImagePaintTimingDetectorAnimatedImageTest, DelayedPresentationFeedback) {
  SetMainFrameBodyContent(R"HTML(
      <img id="target" style:"width:100px;height:100px"></img>
    )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(LargestImage());

  Element* target = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(target);
  auto* timing = MakeGarbageCollected<FakeAnimatedImageTiming>();
  timing->SetIsPaintedFirstFrame();

  // Simulate a paint with the first animated frame painted.
  SimulateImagePaint(target, timing, 100, 100);
  // The image should be pending, recorded, and queued for paint time for the
  // first image frame (regardless of the feature), and queued for paint time
  // for being sufficiently loaded (with the feature).
  EXPECT_EQ(ContainerTotalSize(),
            IsReportFirstFrameTimeAsRenderTimeEnabled() ? 4u : 3u);
  SimulateRendering();
  // For LCP, the largest pending should be set, but the largest should not be.
  EXPECT_TRUE(LargestImage());
  EXPECT_FALSE(LargestPaintedImage());

  // Simulate the sufficiently loaded paint while presentation time is still
  // pending.
  timing->SetIsSufficientContentLoadedForPaint();
  SimulateImagePaint(target, timing, 100, 100);
  SimulateRendering();
  // With the feature enabled, the count should stay the same, but without the
  // feature, there's a second entry queued for first frame since the other is
  // still pending.
  EXPECT_EQ(ContainerTotalSize(),
            IsReportFirstFrameTimeAsRenderTimeEnabled() ? 4u : 5u);
  // The largest pending and painted should be unchanged.
  EXPECT_TRUE(LargestImage());
  EXPECT_FALSE(LargestPaintedImage());

  // Now, simulate presentation time for the first frame. This should set the
  // first animated frame time with and without the feature enabled, and set the
  // paint time only with the feature enabled.
  SimulatePresentationTime();
  base::TimeTicks expected_first_frame_time = base::TimeTicks::Now();

  // There should be 1 entry if the feature is enabled (recorded) and 4 if not
  // (recorded, pending, and 2 queued for the second frame's presentation time).
  EXPECT_EQ(ContainerTotalSize(),
            IsReportFirstFrameTimeAsRenderTimeEnabled() ? 1u : 4u);
  // Without the feature enabled, `record` will be the largest pending image.
  ImageRecord* record = LargestImage();
  ASSERT_TRUE(record);
  EXPECT_EQ(record->FirstAnimatedFrameTime(), expected_first_frame_time);
  EXPECT_EQ(record->GetNode(), target);
  // The record will be sufficiently loaded in either case (since it's set
  // during paint), but setting the paint time needs to wait for the correct
  // frame's feedback.
  EXPECT_TRUE(record->IsSufficientlyLoadedForReporting());
  EXPECT_EQ(record->HasPaintTime(),
            IsReportFirstFrameTimeAsRenderTimeEnabled());
  EXPECT_EQ(!!LargestPaintedImage(),
            IsReportFirstFrameTimeAsRenderTimeEnabled());

  // Finally, simulate the presentation time of the second frame. This should
  // set the paint time with the feature disabled.
  SimulatePresentationTime();
  EXPECT_EQ(ContainerTotalSize(), 1u);
  record = LargestImage();
  // The second frame's presentation time should not overwrite the first
  // animated frame time.
  EXPECT_EQ(record->FirstAnimatedFrameTime(), expected_first_frame_time);
  EXPECT_TRUE(record->IsSufficientlyLoadedForReporting());
  EXPECT_EQ(record->GetNode(), target);
  EXPECT_TRUE(record->HasPaintTime());
  EXPECT_EQ(record, LargestPaintedImage());
}

TEST_P(ImagePaintTimingDetectorAnimatedImageTest,
       FirstVideoFrameRacesWithAnimatedPosterImage) {
  SetMainFrameBodyContent(R"HTML(
    <video id="target" width=300 height=200></video>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(LargestImage());

  Element* video_element = GetDocument().getElementById(AtomicString("target"));
  ASSERT_TRUE(video_element);
  ASSERT_TRUE(video_element->GetLayoutObject());

  // First, simulate painting an animated pending poster image, with the first
  // frame painted but not presented.
  FakeAnimatedImageTiming* image_timing =
      MakeGarbageCollected<FakeAnimatedImageTiming>();
  image_timing->SetIsPaintedFirstFrame();
  SimulateImagePaint(video_element, image_timing, 300, 200);
  SimulateRendering();
  // LCP should consider the poster image as the largest pending image, but it
  // should not be considered painted yet.
  EXPECT_FALSE(LargestPaintedImage());
  ImageRecord* record1 = LargestImage();
  ASSERT_TRUE(record1);
  EXPECT_EQ(record1->GetMediaTiming(), image_timing);
  EXPECT_EQ(CountImageRecords(), 1u);

  // Next, simulate the first video frame while the poster image is still
  // pending.
  VideoTiming* video_timing = MakeGarbageCollected<VideoTiming>();
  video_timing->SetFirstVideoFrameTime(base::TimeTicks::Now());
  video_timing->SetIsSufficientContentLoadedForPaint();
  video_timing->SetUrl(KURL("http://test.com/video"));
  video_timing->SetContentSizeForEntropy(1024 * 1024);
  SimulateFirstVideoFrame(video_element, video_timing, 300, 200);
  // The first video frame should replace the poster image as the <video>'s
  // media *if* not using first animated frame for presentation time. Otherwise,
  // the first video frame should be ignored (first animated frame wins the race
  // since it's only pending presentation time).
  EXPECT_FALSE(LargestPaintedImage());
  ImageRecord* record2 = LargestImage();
  ASSERT_TRUE(record2);

  if (IsReportFirstFrameTimeAsRenderTimeEnabled()) {
    EXPECT_EQ(record1, record2);
    EXPECT_EQ(record2->GetMediaTiming(), image_timing);
  } else {
    EXPECT_NE(record1, record2);
    EXPECT_EQ(record2->GetMediaTiming(), video_timing);
  }

  // There's still only 1 record since either the poster image was replaced or
  // the first video frame was ignored.
  EXPECT_EQ(CountImageRecords(), 1u);

  // Simulate presentation time for the animated image frame.
  SimulatePresentationTime();
  EXPECT_EQ(LargestPaintedImage(),
            IsReportFirstFrameTimeAsRenderTimeEnabled() ? record1 : nullptr);

  // Simulate rendering and presentation time to flush the first video frame.
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LargestPaintedImage(),
            IsReportFirstFrameTimeAsRenderTimeEnabled() ? record1 : record2);
  EXPECT_EQ(LargestImage(), LargestPaintedImage());
}

}  // namespace blink
