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
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource.h"
#include "third_party/blink/renderer/core/loader/resource/video_timing.h"
#include "third_party/blink/renderer/core/paint/timing/largest_contentful_paint_manager.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_detector.h"
#include "third_party/blink/renderer/core/paint/timing/paint_timing_test_base.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/testing/paint_test_configurations.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

#define TRANSPARENT_PLACEHOLDER_IMAGE \
  "data:image/gif;base64,"            \
  "R0lGODlhAQABAIAAAP///////yH5BAEKAAEALAAAAAABAAEAAAICTAEAOw=="

using UkmPaintTiming = ukm::builders::Blink_PaintTiming;

class ImagePaintTimingDetectorTestBase : public PaintTimingTestBase,
                                         public LcpTestSupport {
 public:
  ImagePaintTimingDetectorTestBase() = default;

  void SetUp() override {
    PaintTimingTestBase::SetUp();

    main_frame_client_ =
        MakeGarbageCollected<PaintTimingRecordObserverClient>();
    GetPaintTiming().AddClient(main_frame_client_.Get());

    AttachTo(GetDocument());
  }

  bool HasPersistentImageState() {
    return GetPaintTimingDetector()
        .GetImagePaintTimingDetector()
        .HasPersistentImageStateForTest();
  }

  bool HasLargestIgnoredImage() {
    return GetPaintTiming()
        .GetLargestContentfulPaintManager()
        ->HasLargestIgnoredImageForTest();
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
  Persistent<PaintTimingRecordObserverClient> main_frame_client_;
};

class ImagePaintTimingDetectorTest : public PaintTestConfigurations,
                                     public ImagePaintTimingDetectorTestBase {
 public:
  ImagePaintTimingDetectorTest() = default;
};

INSTANTIATE_PAINT_TEST_SUITE_P(ImagePaintTimingDetectorTest);

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_NoImage) {
  SetMainFrameBodyContent(R"HTML(
    <div></div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_OneImage) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetMainFrameBodyContent(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();

  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), GetElementById("target"));
  EXPECT_GT(CurrentLcpCandidate()->loadTime(), 0.0);
  EXPECT_EQ(CurrentLcpCandidate()->size(), 25u);

  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks::Now());
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);

  // Simulate input to force recording the debugging ukm entry.
  SimulateKeyDown();
  auto entries = test_ukm_recorder.GetEntriesByName(UkmPaintTiming::kEntryName);
  EXPECT_EQ(entries.size(), 1u);
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
  SetImageContent("image1", 5, 5);

  auto* image2 = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image2->setAttribute(html_names::kIdAttr, AtomicString("image2"));
  GetDocument().body()->AppendChild(image2);
  SetImageContent("image2", 5, 5);

  auto* image3 = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image3->setAttribute(html_names::kIdAttr, AtomicString("image3"));
  GetDocument().body()->AppendChild(image3);
  SetImageContent("image3", 5, 5);

  SimulateRenderingAndPresentationTime();

  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), image1);
  EXPECT_EQ(CurrentLcpCandidate()->size(), 25u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_TraceEvent_Candidate) {
  using trace_analyzer::Query;
  trace_analyzer::Start("loading");
  {
    SetMainFrameBodyContent(R"HTML(
      <img id="target"></img>
    )HTML");
    SetImageContent("target", 5, 5);
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
    SetChildFrameImageContent("target", 5, 5);
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
  // Initially, there should be no image candidate for metrics.
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time, std::nullopt);

  // Load and render an image.
  SetMainFrameBodyContent(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();

  // The metrics candidate should be updated.
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks::Now());
}

TEST_P(ImagePaintTimingDetectorTest, UpdatePerformanceTimingAfterImageRemoved) {
  // Load and render an image.
  SetMainFrameBodyContent(R"HTML(
    <img id="target"></img>
  )HTML");
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  auto presentation_time = base::TimeTicks::Now();

  // The metrics candidate should be updated.
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            presentation_time);

  // Remove the image. This should have no effect on the metrics data.
  GetDocument().body()->RemoveChild(GetElementById("target"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            presentation_time);
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
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
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
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
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
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
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
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);
}

TEST_P(ImagePaintTimingDetectorTest,
       IgnoreImageUntilInvalidatedRectSizeNonZero) {
  SetMainFrameBodyContent(R"HTML(
    <img id="target"></img>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);

  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_Largest) {
  SetMainFrameBodyContent(R"HTML(
    <style>img { display:block }</style>
    <img id="smaller"></img>
    <img id="medium"></img>
    <img id="larger"></img>
  )HTML");
  SetImageContent("smaller", 5, 5);
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), GetElementById("smaller"));
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);

  SetImageContent("larger", 9, 9);
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 2u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), GetElementById("larger"));
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 81u);
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
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_UpdateOnRemovingTheLastImage) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  Element* target = GetElementById("target");
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), target);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);

  GetElementById("parent")->RemoveChild(target);
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), nullptr);
  EXPECT_EQ(CurrentLcpCandidate()->id(), "target");
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_UpdateOnRemoving) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target1"></img>
      <img id="target2"></img>
    </div>
  )HTML");
  Element* target1 = GetElementById("target1");
  SetImageContent("target1", 5, 5);
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), target1);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  std::optional<base::TimeTicks> first_largest_image_paint =
      LcpDetailsForReporting().merged_unclamped_paint_time;
  EXPECT_EQ(first_largest_image_paint, base::TimeTicks::Now());

  Element* target2 = GetElementById("target2");
  SetImageContent("target2", 10, 10);
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 2u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), target2);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  std::optional<base::TimeTicks> second_largest_image_paint =
      LcpDetailsForReporting().merged_unclamped_paint_time;
  EXPECT_EQ(second_largest_image_paint, base::TimeTicks::Now());

  EXPECT_NE(first_largest_image_paint, second_largest_image_paint);

  GetElementById("parent")->RemoveChild(target2);
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 2u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), nullptr);
  EXPECT_EQ(CurrentLcpCandidate()->id(), "target2");
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            second_largest_image_paint);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 100u);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_NodeRemovedBetweenRegistrationAndInvocation) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageContent("target", 5, 5);
  SimulateRendering();

  GetElementById("parent")->RemoveChild(GetElementById("target"));

  SimulatePresentationTime();

  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterImageRemoval) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(HasPersistentImageState());

  GetElementById("parent")->RemoveChild(GetElementById("target"));
  EXPECT_FALSE(HasPersistentImageState());
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
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(HasPersistentImageState());

  GetDocument().body()->RemoveChild(GetElementById("parent"));
  EXPECT_FALSE(HasPersistentImageState());
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
  EXPECT_TRUE(HasPersistentImageState());

  GetElementById("parent")->RemoveChild(GetElementById("target"));
  EXPECT_FALSE(HasPersistentImageState());
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterImageRemovedBeforePresentation) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageContent("target", 5, 5);
  SimulateRendering();
  EXPECT_TRUE(HasPersistentImageState());

  GetElementById("parent")->RemoveChild(GetElementById("target"));
  EXPECT_FALSE(HasPersistentImageState());
}

TEST_P(ImagePaintTimingDetectorTest,
       RemoveRecordFromAllContainersAfterImageRemovedAfterPresentation) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(HasPersistentImageState());

  GetElementById("parent")->RemoveChild(GetElementById("target"));
  EXPECT_FALSE(HasPersistentImageState());
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_ReattachedNodeNotTreatedAsNew) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
    </div>
  )HTML");
  auto* image = MakeGarbageCollected<HTMLImageElement>(GetDocument());
  image->setAttribute(html_names::kIdAttr, AtomicString("target"));
  GetElementById("parent")->AppendChild(image);
  SetImageContent("target", 5, 5);
  FastForwardBy(base::Seconds(1));
  SimulateRenderingAndPresentationTime();
  auto initial_presentation_time = base::TimeTicks::Now();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            initial_presentation_time);

  GetElementById("parent")->RemoveChild(image);
  FastForwardBy(base::Seconds(1));
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            initial_presentation_time);

  GetElementById("parent")->AppendChild(image);
  SetImageContent("target", 5, 5);
  FastForwardBy(base::Seconds(1));
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            initial_presentation_time);
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

  SetImageContent("smaller", 5, 5);
  SimulateRendering();
  SimulatePassOfTime();

  SetImageContent("larger", 9, 9);
  SimulateRendering();
  SimulatePassOfTime();

  // Invoke callbacks for the first frame.
  SimulatePresentationTime();
  // The first frame's paint is the smaller image.
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->size(), 25u);
  const DOMHighResTimeStamp record1_time = CurrentLcpCandidate()->renderTime();
  EXPECT_GT(record1_time, 0.0);

  // Invoke callbacks for the second frame.
  SimulatePassOfTime();
  SimulatePresentationTime();
  // The second frame's paint is the larger image.
  ASSERT_EQ(LcpCandidateCount(), 2u);
  EXPECT_EQ(CurrentLcpCandidate()->size(), 81u);
  EXPECT_NE(record1_time, CurrentLcpCandidate()->renderTime());
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestImagePaint_UpdateResultWhenLargestChanged) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target1"></img>
      <img id="target2"></img>
    </div>
  )HTML");

  SetImageContent("target1", 5, 5);
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks::Now());
  double image_paint_time1 = LcpDetailsForReporting().image_paint_time;
  EXPECT_GT(image_paint_time1, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);

  SetImageContent("target2", 10, 10);
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks::Now());
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, image_paint_time1);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 100u);
}

TEST_P(ImagePaintTimingDetectorTest, OnePresentationPromiseForOneFrame) {
  SetMainFrameBodyContent(R"HTML(
    <style>img { display:block }</style>
    <div id="parent">
      <img id="1"></img>
      <img id="2"></img>
    </div>
  )HTML");
  SetImageContent("1", 5, 5);
  SimulateRendering();
  SimulatePassOfTime();

  SetImageContent("2", 9, 9);
  SimulateRendering();
  SimulatePassOfTime();

  // This callback only assigns a time to the 5x5 image.
  SimulatePresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->size(), 25u);

  // This callback assigns a time to the 9x9 image.
  SimulatePresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 2u);
  EXPECT_EQ(CurrentLcpCandidate()->size(), 81u);
}

TEST_P(ImagePaintTimingDetectorTest, VideoImage) {
  SetMainFrameBodyContent(R"HTML(
    <video id="target" poster=")HTML" LARGE_IMAGE R"HTML("></video>
  )HTML");
  // Poster image rendering requires flushing pending tasks first.
  test::RunPendingTasks();
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
  ASSERT_TRUE(LcpDetailsForReporting().merged_unclamped_paint_time.has_value());
  EXPECT_NE(*LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks());
}

TEST_P(ImagePaintTimingDetectorTest, VideoImage_ImageNotLoaded) {
  SetMainFrameBodyContent("<video id='target'></video>");

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
}

TEST_P(ImagePaintTimingDetectorTest, VideoImage_DefaultPosterIgnored) {
  GetDocument().GetSettings()->SetDefaultVideoPosterURL(
      AtomicString(LARGE_IMAGE));
  SetMainFrameBodyContent(R"HTML(
    <video id="target" width="300" height="200"></video>
  )HTML");
  test::RunPendingTasks();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_FALSE(HasPersistentImageState());

  // Verify that a subsequent first video frame is not blocked by the ignored
  // default poster image.
  Element* video_element = GetElementById("target");
  ASSERT_TRUE(video_element);
  VideoTiming* video_timing = MakeGarbageCollected<VideoTiming>();
  video_timing->SetFirstVideoFrameTime(base::TimeTicks::Now());
  video_timing->SetIsSufficientContentLoadedForPaint();
  video_timing->SetUrl(KURL("http://test.com/video.mp4"));
  video_timing->SetContentSizeForEntropy(1024 * 1024);

  SimulateFirstVideoFrame(video_element, video_timing, 300, 200);
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), video_element);
  EXPECT_EQ(CurrentLcpCandidate()->url(), String("http://test.com/video.mp4"));
  EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
}

TEST_P(ImagePaintTimingDetectorTest,
       VideoImage_ExplicitPosterRecordedWhenDefaultSet) {
  GetDocument().GetSettings()->SetDefaultVideoPosterURL(
      AtomicString(SIMPLE_IMAGE));
  SetMainFrameBodyContent(R"HTML(
    <video id="target" width="300" height="200"></video>
  )HTML");
  test::RunPendingTasks();
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_FALSE(HasPersistentImageState());

  // Changing the poster to an explicit image after the default poster loaded
  // should record the new explicit poster.
  Element* video_element = GetElementById("target");
  ASSERT_TRUE(video_element);
  video_element->setAttribute(html_names::kPosterAttr,
                              AtomicString(LARGE_IMAGE));
  test::RunPendingTasks();
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), video_element);
  EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
  EXPECT_TRUE(HasPersistentImageState());

  // Removing the explicit poster attribute reverts to the default poster,
  // which should remove the explicit poster record and not record the default.
  video_element->removeAttribute(html_names::kPosterAttr);
  test::RunPendingTasks();
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(HasPersistentImageState());
}

TEST_P(ImagePaintTimingDetectorTest, SVGImage) {
  SetMainFrameBodyContent(R"HTML(
    <svg>
      <image id="target" width="10" height="10"/>
    </svg>
  )HTML");

  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();

  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks::Now());
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

  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks::Now());
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

  SetImageContent("target", 1, 1);
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 2u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 1u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreBody) {
  SetMainFrameBodyContent("<style>body { background-image: url(" SIMPLE_IMAGE
                          ")}</style>");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreBodyPseudoElement) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      body::before {
        content: "";
        display: block;
        width: 100px;
        height: 100px;
        background-image: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreHtml) {
  SetMainFrameBodyContent("<style>html { background-image: url(" SIMPLE_IMAGE
                          ")}</style>");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest, BackgroundImage_IgnoreHtmlPseudoElement) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      html::before {
        content: "";
        display: block;
        width: 100px;
        height: 100px;
        background-image: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       BackgroundImage_IgnoreDisplayContentsPseudoElement) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      #target {
        display: contents;
      }
      #target::before {
        content: "";
        display: block;
        width: 100px;
        height: 100px;
        background-image: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
    <div id="target"></div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  EXPECT_FALSE(target->GetLayoutObject());
  ASSERT_TRUE(target->GetPseudoElement(kPseudoIdBefore));
  EXPECT_TRUE(target->GetPseudoElement(kPseudoIdBefore)->GetLayoutObject());
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
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
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
}

// We put two background images in the same object, and test whether FCP++ can
// find two different images.
TEST_P(ImagePaintTimingDetectorTest, BackgroundImageTrackedSeparately) {
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
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 2u);
}

TEST_P(ImagePaintTimingDetectorTest, DeactivateAfterUserInput) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SimulateScroll();
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(GetPaintTiming().GetLargestContentfulPaintManager());
}

TEST_P(ImagePaintTimingDetectorTest, ContinueAfterKeyUp) {
  SetMainFrameBodyContent(R"HTML(
    <div id="parent">
      <img id="target"></img>
    </div>
  )HTML");
  SimulateKeyUp();
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_TRUE(GetPaintTiming().GetLargestContentfulPaintManager());
}

TEST_P(ImagePaintTimingDetectorTest, Iframe) {
  SetMainFrameBodyContent(R"HTML(
    <iframe width=100px height=100px></iframe>
  )HTML");
  SetChildFrameBodyContent(R"HTML(
    <style>img { display:block }</style>
    <img id="target"></img>
  )HTML");
  SetChildFrameImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  // Ensure main frame doesn't capture this image.
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);

  // Ensure the image size is not clipped (5*5).
  LcpTestSupport child_lcp_support(ChildDocument());
  EXPECT_EQ(child_lcp_support.LcpDetailsForReporting().image_paint_size, 25u);
  EXPECT_EQ(child_lcp_support.LcpCandidateCount(), 1u);
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
  SetChildFrameImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  LcpTestSupport child_lcp_support(ChildDocument());
  EXPECT_EQ(child_lcp_support.LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(child_lcp_support.LcpCandidateCount(), 0u);
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
  SetChildFrameImageContent("target", 10, 10);
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(LcpCandidateCount(), 0u);

  LcpTestSupport child_lcp_support(ChildDocument());
  EXPECT_EQ(child_lcp_support.LcpCandidateCount(), 1u);
  EXPECT_LT(child_lcp_support.LcpDetailsForReporting().image_paint_size, 100u);
  EXPECT_GT(child_lcp_support.LcpDetailsForReporting().image_paint_size, 0u);
}

TEST_P(ImagePaintTimingDetectorTest, SameSizeShouldNotBeIgnored) {
  SetMainFrameBodyContent(R"HTML(
    <style>img { display:block }</style>
    <img id='1'></img>
    <img id='2'></img>
    <img id='3'></img>
  )HTML");
  SetImageContent("1", 5, 5);
  SetImageContent("2", 5, 5);
  SetImageContent("3", 5, 5);
  SimulateRendering();
  EXPECT_EQ(main_frame_client_->ImageFirstPaintCount(), 3u);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 3u);
}

TEST_P(ImagePaintTimingDetectorTest, UseIntrinsicSizeIfSmaller_Image) {
  SetMainFrameBodyContent(R"HTML(
    <img height="300" width="300" display="block" id="target">
    </img>
  )HTML");
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);
}

TEST_P(ImagePaintTimingDetectorTest, NotUseIntrinsicSizeIfLarger_Image) {
  SetMainFrameBodyContent(R"HTML(
    <img height="1" width="1" display="block" id="target">
    </img>
  )HTML");
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 1u);
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
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 1u);
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
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);
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
  SetImageContent("target", 5, 5);

  // Clients should not be notified for the image paint, and LCP should not be
  // updated.
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_TRUE(HasLargestIgnoredImage());
  EXPECT_EQ(LcpCandidateCount(), 0u);

  // Change the opacity of documentElement, now the img should be a candidate.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_FALSE(HasLargestIgnoredImage());
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);

  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 25u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            base::TimeTicks::Now());
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
  SetImageContent("target", 5, 5);

  // The image should not be tracked as "ignored" for LCP since that does not
  // apply to opacity set on elements, and clients should not have been notified
  // for the image paint.
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_FALSE(HasLargestIgnoredImage());
  EXPECT_EQ(LcpCandidateCount(), 0u);

  // Toggling opacity on the documentElement will have no effect.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 0"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_FALSE(HasLargestIgnoredImage());
  EXPECT_EQ(LcpCandidateCount(), 0u);

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_FALSE(HasLargestIgnoredImage());
  EXPECT_EQ(LcpCandidateCount(), 0u);
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
  SetImageContent("target", 256, 256);
  SimulateRenderingAndPresentationTime();
  // Ensure the record is not sent to clients yet.
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);

  // Simulate input to stop LCP.
  SimulateKeyDown();

  // Change the opacity of documentElement. The img should not be a candidate
  // because LCP stops on input. Additionally, other clients are not notified
  // about the painted image because the largest ignored image is tracked by the
  // LCP manager.
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_time, 0.0);

  // FCP and first image paint should not be marked, since this feature is tied
  // to hard LCP.
  //
  // Note: `PaintTiming` doesn't support `MockPaintTimingCallbackManager`, so
  // check the paint time instead of presentation time.
  base::TimeTicks fcp_timestamp =
      GetPaintTiming()
          .FirstContentfulPaintRenderedButNotPresentedAsMonotonicTime();
  EXPECT_TRUE(fcp_timestamp.is_null());

  base::TimeTicks image_timestamp =
      GetPaintTiming().FirstImagePaintRenderedButNotPresentedAsMonotonicTime();
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
  SetImageContent("target", 256, 256);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_TRUE(HasLargestIgnoredImage());

  GetDocument().body()->RemoveChild(GetElementById("target"));
  EXPECT_FALSE(HasLargestIgnoredImage());
  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestImagePaint_FullViewportImage) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  SetMainFrameBodyContent(R"HTML(
    <style>body {margin: 0px;}</style>
    <img id="target"></img>
  )HTML");
  SetImageContent("target", 3000, 3000);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  // Simulate input to force recording the debugging ukm entry.
  SimulateKeyDown();
  auto entries = test_ukm_recorder.GetEntriesByName(UkmPaintTiming::kEntryName);
  EXPECT_EQ(entries.size(), 1u);
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
  SetChildFrameImageContent("target", 5, 5);
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
  SetMainFrameBodyContent(R"HTML(
    <video id="target" width=300 height=200></video>
  )HTML");

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);

  Element* video_element = GetElementById("target");
  ASSERT_TRUE(video_element);
  ASSERT_TRUE(video_element->GetLayoutObject());

  VideoTiming* video_timing = MakeGarbageCollected<VideoTiming>();
  video_timing->SetFirstVideoFrameTime(base::TimeTicks::Now());
  video_timing->SetIsSufficientContentLoadedForPaint();
  video_timing->SetUrl(KURL("http://test.com/video"));
  video_timing->SetContentSizeForEntropy(1024 * 1024);

  SimulateFirstVideoFrame(video_element, video_timing, 300, 100);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);

  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), video_element);
  EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
}

TEST_P(ImagePaintTimingDetectorTest, FirstVideoFrameRacesWithPosterImage) {
  SetMainFrameBodyContent(R"HTML(
    <video id="target" width=300 height=200></video>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);

  Element* video_element = GetElementById("target");
  ASSERT_TRUE(video_element);
  ASSERT_TRUE(video_element->GetLayoutObject());

  // First, simulate painting the pending poster image.
  ImageResourceContent* image_timing =
      CreateImageForTest(300, 200, /*bytes=*/0, ImageStatus::kPending);
  SimulateImagePaint(video_element, image_timing, 300, 200);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(main_frame_client_->ImageFirstPaintCount(), 1u);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);

  // Next, simulate the first video frame while the poster image is still
  // pending.
  VideoTiming* video_timing = MakeGarbageCollected<VideoTiming>();
  video_timing->SetFirstVideoFrameTime(base::TimeTicks::Now());
  video_timing->SetIsSufficientContentLoadedForPaint();
  video_timing->SetUrl(KURL("http://test.com/video"));
  video_timing->SetContentSizeForEntropy(1024 * 1024);

  // The first video frame should replace the poster image as the <video>'s
  // media.
  SimulateFirstVideoFrame(video_element, video_timing, 300, 200);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  // There should be a first paint notification for the first video frame, but
  // the image won't be sent to clients for the sufficiently loaded paint until
  // the next frame.
  EXPECT_EQ(main_frame_client_->ImageFirstPaintCount(), 2u);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);

  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), video_element);
  EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(CurrentLcpCandidate()->url(), String("http://test.com/video"));
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
}

TEST_P(ImagePaintTimingDetectorTest, LargestIgnoredImageRemovedBeforePaint) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
    </style>
    <img id="target"></img>
  )HTML");
  SetImageContent("target", 5, 5);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_TRUE(HasLargestIgnoredImage());

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(HasLargestIgnoredImage());

  GetElementById("target")->remove();
  EXPECT_FALSE(HasLargestIgnoredImage());

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
}

TEST_P(ImagePaintTimingDetectorTest,
       LargestIgnoredPseudoElementBackgroundImageRemovedBeforePaint) {
  SetMainFrameBodyContent(R"HTML(
    <style>
      :root {
        opacity: 0;
        will-change: opacity;
      }
      #target::before {
        content: "";
        display: block;
        width: 5px;
        height: 5px;
        background-image: url()HTML" SIMPLE_IMAGE R"HTML();
      }
    </style>
    <div id="target"></div>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_TRUE(HasLargestIgnoredImage());

  GetDocument().documentElement()->setAttribute(html_names::kStyleAttr,
                                                AtomicString("opacity: 1"));
  GetDocument().UpdateStyleAndLayoutTree();
  EXPECT_TRUE(HasLargestIgnoredImage());

  GetElementById("target")->remove();
  EXPECT_FALSE(HasLargestIgnoredImage());

  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
}

// Ensure that when changing a <video>'s src between notifying paint timing and
// the subsequent frame, and GCing the corresponding `VideoTiming`, that the
// first video frame is ignored and doesn't cause a CHECK failure. Regression
// test for crbug.com/562498378.
TEST_P(ImagePaintTimingDetectorTest,
       VideoSourceChangedBetweenFirstFrameAndMainFrame) {
  SetMainFrameBodyContent(R"HTML(
    <video id="target" width=300 height=200 elementtiming="foo"></video>
  )HTML");
  SimulateRenderingAndPresentationTime();

  WeakPersistent<VideoTiming> video_timing =
      MakeGarbageCollected<VideoTiming>();
  video_timing->SetFirstVideoFrameTime(base::TimeTicks::Now());
  video_timing->SetIsSufficientContentLoadedForPaint();
  video_timing->SetIsCorsSameOrigin(true);
  video_timing->SetUrl(KURL("http://test.com/video"));
  video_timing->SetContentSizeForEntropy(1024 * 1024);
  SimulateFirstVideoFrame(GetElementById("target"), video_timing, 300, 100);

  // In production, the `video_timing` is kept alive by the <video> element.
  // Since `ImageRecord` holds `video_timing` weakly, changing the video src and
  // rendering the new first frame between the initial first video frame
  // notification and subsequent main frame makes the `video_timing` eligible
  // for GC, leaving the `ImageRecord` with a null `MediaTiming` at the end of
  // paint. Simulate this by GCing the `video_timing` before the next paint.
  ThreadState::Current()->CollectAllGarbageForTesting();
  EXPECT_FALSE(video_timing);
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
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
    ThreadState::Current()->CollectAllGarbageForTesting();
  }

 protected:
  void SetTransparentPlaceholderImageAndPaint(const char* id) {
    Element* element = GetElementById(id);
    ImageResource* resource = ImageResource::CreateForTest(
        url_test_helpers::ToKURL(TRANSPARENT_PLACEHOLDER_IMAGE));
    To<HTMLImageElement>(element)->SetImageForTest(resource->GetContent());
  }
};

INSTANTIATE_PAINT_TEST_SUITE_P(
    ImagePaintTimingDetectorTransparentPlaceholderImageTest);

TEST_P(ImagePaintTimingDetectorTransparentPlaceholderImageTest,
       LargestImagePaint) {
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_time, 0.0);
  SetMainFrameBodyContent(R"HTML(
      <img id="placeholder"></img>
    )HTML");
  SetTransparentPlaceholderImageAndPaint("placeholder");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 1u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
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
  bool IsCorsSameOrigin() const override { return true; }

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
      <img id="target" style="width:100px;height:100px"></img>
    )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(main_frame_client_->ImageFirstPaintCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  auto* timing = MakeGarbageCollected<FakeAnimatedImageTiming>();

  // Simulate a paint without the first frame or sufficiently loaded content.
  SimulateImagePaint(target, timing, 100, 100);
  // Clients should have been notified about the first paint.
  EXPECT_EQ(main_frame_client_->ImageFirstPaintCount(), 1u);
  // Clients should not have been notified about the last paint yet.
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_time, 0.0);

  // Simulate a paint with the first frame painted.
  timing->SetIsPaintedFirstFrame();
  SimulateImagePaint(target, timing, 100, 100);

  // Simulate presentation time. This should set the first animated frame time
  // with and without the feature, and set the paint time with the feature.
  SimulateRenderingAndPresentationTime();
  auto expected_first_frame_presentation_time = base::TimeTicks::Now();

  if (IsReportFirstFrameTimeAsRenderTimeEnabled()) {
    EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
    ASSERT_EQ(LcpCandidateCount(), 1u);
    EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
    EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  } else {
    EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
    EXPECT_EQ(LcpCandidateCount(), 0u);
    EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
    EXPECT_EQ(LcpDetailsForReporting().image_paint_time, 0.0);
  }

  // Finally, simulate a paint with the `timing` sufficiently loaded. This
  // should be a no-op if with the feature enabled, and it should cause the
  // record to be reported without.
  timing->SetIsSufficientContentLoadedForPaint();
  SimulateImagePaint(target, timing, 100, 100);
  SimulateRenderingAndPresentationTime();

  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  // The first frame should be used for metrics regardless of the feature.
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            expected_first_frame_presentation_time);
}

TEST_P(ImagePaintTimingDetectorAnimatedImageTest, DelayedPresentationFeedback) {
  SetMainFrameBodyContent(R"HTML(
      <img id="target" style="width:100px;height:100px"></img>
    )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);

  Element* target = GetElementById("target");
  ASSERT_TRUE(target);
  auto* timing = MakeGarbageCollected<FakeAnimatedImageTiming>();
  timing->SetIsPaintedFirstFrame();

  // Simulate a paint with the first animated frame painted. Clients will be
  // updated for the first paint, but nothing should be reported yet.
  SimulateImagePaint(target, timing, 100, 100);
  SimulateRendering();
  EXPECT_EQ(main_frame_client_->ImageFirstPaintCount(), 1u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);

  // Simulate the sufficiently loaded paint while presentation time is still
  // pending.
  timing->SetIsSufficientContentLoadedForPaint();
  SimulateImagePaint(target, timing, 100, 100);
  SimulateRendering();

  // Client state should remain the same, except that clients will be notified
  // in both cases for the sufficiently loaded image.
  EXPECT_EQ(main_frame_client_->ImageFirstPaintCount(), 1u);
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);

  // Now, simulate presentation time for the first frame. This should set the
  // first animated frame time with and without the feature enabled, and set the
  // paint time only with the feature enabled.
  SimulatePresentationTime();
  auto expected_first_frame_presentation_time = base::TimeTicks::Now();

  if (IsReportFirstFrameTimeAsRenderTimeEnabled()) {
    ASSERT_EQ(LcpCandidateCount(), 1u);
    EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
    EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  } else {
    EXPECT_EQ(LcpCandidateCount(), 0u);
    EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
    EXPECT_EQ(LcpDetailsForReporting().image_paint_time, 0.0);
  }

  // Finally, simulate the presentation time of the second frame. This should
  // set the paint time with the feature disabled.
  SimulatePresentationTime();
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  // The first frame should be used for metrics regardless of the feature, and
  // it should not be overwritten by queuing a second entry while the first is
  // pending.
  EXPECT_EQ(LcpDetailsForReporting().merged_unclamped_paint_time,
            expected_first_frame_presentation_time);
}

TEST_P(ImagePaintTimingDetectorAnimatedImageTest,
       FirstVideoFrameRacesWithAnimatedPosterImage) {
  SetMainFrameBodyContent(R"HTML(
    <video id="target" width=300 height=200></video>
  )HTML");
  SimulateRenderingAndPresentationTime();
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);

  Element* video_element = GetElementById("target");
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
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_EQ(main_frame_client_->ImageFirstPaintCount(), 1u);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(),
            IsReportFirstFrameTimeAsRenderTimeEnabled() ? 1u : 0u);

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
  EXPECT_EQ(LcpCandidateCount(), 0u);
  EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);

  // Simulate presentation time for the animated image frame.
  SimulatePresentationTime();
  if (IsReportFirstFrameTimeAsRenderTimeEnabled()) {
    ASSERT_EQ(LcpCandidateCount(), 1u);
    EXPECT_EQ(CurrentLcpCandidate()->element(), video_element);
    EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
    EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
    EXPECT_EQ(CurrentLcpCandidate()->url(),
              String("http://test.com/animated.gif"));
    EXPECT_EQ(main_frame_client_->ImageFirstPaintCount(), 1u);
    EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
  } else {
    EXPECT_EQ(LcpCandidateCount(), 0u);
    EXPECT_EQ(LcpDetailsForReporting().image_paint_size, 0u);
    EXPECT_EQ(main_frame_client_->ImageFirstPaintCount(), 2u);
    EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 0u);
  }

  // Simulate rendering and presentation time to flush the first video frame.
  SimulateRenderingAndPresentationTime();
  ASSERT_EQ(LcpCandidateCount(), 1u);
  EXPECT_EQ(main_frame_client_->PaintedImageRecordCount(), 1u);
  EXPECT_EQ(CurrentLcpCandidate()->element(), video_element);
  EXPECT_GT(LcpDetailsForReporting().image_paint_size, 0u);
  EXPECT_GT(LcpDetailsForReporting().image_paint_time, 0.0);
  EXPECT_EQ(CurrentLcpCandidate()->url(),
            IsReportFirstFrameTimeAsRenderTimeEnabled()
                ? String("http://test.com/animated.gif")
                : String("http://test.com/video"));
}

}  // namespace blink
