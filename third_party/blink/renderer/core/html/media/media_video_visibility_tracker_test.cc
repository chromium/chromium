// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_video_visibility_tracker.h"

#include "base/test/mock_callback.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

using ::testing::Return;
using ::testing::UnorderedElementsAreArray;

class MediaVideoVisibilityTrackerTest : public SimTest {
 public:
  MediaVideoVisibilityTrackerTest()
      : SimTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  void TearDown() override {
    if (tracker_) {
      tracker_->Detach();
    }
    SimTest::TearDown();
  }

  void LoadMainResource(const String& html) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");
    main_resource.Complete(html);
    ASSERT_NE(VideoElement(), nullptr);
  }

  HTMLVideoElement* VideoElement() {
    return To<HTMLVideoElement>(
        GetDocument().QuerySelector(AtomicString("video")));
  }

  MediaVideoVisibilityTracker* CreateAndAttachVideoVisibilityTracker(
      float visibility_threshold) {
    tracker_ = MakeGarbageCollected<MediaVideoVisibilityTracker>(
        *VideoElement(), visibility_threshold, ReportVisibilityCb().Get());
    tracker_->Attach();
    return tracker_;
  }

  float ComputeArea(const PhysicalRect& rect) {
    return static_cast<float>(ToRoundedSize(rect.size).Area64());
  }

  base::MockRepeatingCallback<void(bool)>& ReportVisibilityCb() {
    return report_visibility_cb_;
  }

  const PhysicalRect IntersectionRect() const {
    DCHECK(tracker_);
    return tracker_->intersection_rect_;
  }

  const VectorOf<SkIRect> OccludingRects() const {
    DCHECK(tracker_);
    return tracker_->occluding_rects_;
  }

  float OccludedArea() const {
    DCHECK(tracker_);
    return tracker_->occluded_area_;
  }

 private:
  Persistent<MediaVideoVisibilityTracker> tracker_;
  base::MockRepeatingCallback<void(bool)> report_visibility_cb_;
};

#if DCHECK_IS_ON()
TEST_F(MediaVideoVisibilityTrackerTest, InvalidThresholdEqualToZero) {
  GetDocument().CreateRawElement(html_names::kVideoTag);
  EXPECT_DEATH_IF_SUPPORTED(CreateAndAttachVideoVisibilityTracker(0.0), "");
}

TEST_F(MediaVideoVisibilityTrackerTest, InvalidThresholdGreaterThanOne) {
  GetDocument().CreateRawElement(html_names::kVideoTag);
  EXPECT_DEATH_IF_SUPPORTED(CreateAndAttachVideoVisibilityTracker(1.1), "");
}
#endif  // DCHECK_IS_ON()

TEST_F(MediaVideoVisibilityTrackerTest, NoOcclusion) {
  LoadMainResource(R"HTML(
    <style>
      video {
        width: 50px;
        height: 50px;
      }
    </style>
    <video></video>
  )HTML");
  EXPECT_FALSE(VideoElement()->ShouldShowControls());

  EXPECT_CALL(ReportVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());
}

TEST_F(MediaVideoVisibilityTrackerTest, VideoControlsAreIgnored) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(700, 700));
  LoadMainResource(R"HTML(
    <style>
      video {
        width: 50px;
        height: 50px;
      }
    </style>
    <video controls></video>
  )HTML");
  EXPECT_TRUE(VideoElement()->ShouldShowControls());

  EXPECT_CALL(ReportVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());
}

TEST_F(MediaVideoVisibilityTrackerTest, NoViewPortIntersection) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(10, 10));
  LoadMainResource(R"HTML(
    <style>
      video {
        width: 50px;
        height: 50px;
      }
      .spacer {
        height: 500px;
      }
    </style>
    <video></video>
    <div class="spacer"></div>
  )HTML");
  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_CALL(ReportVisibilityCb(), Run(false));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 500), mojom::blink::ScrollType::kProgrammatic);

  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rect{
      SkIRect::MakeXYWH(8, -492, 50, 50)};
  EXPECT_EQ(expected_occludning_rect, OccludingRects());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       SingleElementNotOccludingAboveThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        position: relative;
        width: 500px;
        height: 500px;
        top:0;
        left:0
      }
      div {
        background-color: blue;
        width: 100px;
        height: 100px;
        position: absolute;
        top: 0;
        left: 0;
      }
    </style>
    <video></video>
    <div></div>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rect{SkIRect::MakeXYWH(0, 0, 100, 100)};
  EXPECT_EQ(expected_occludning_rect, OccludingRects());
}

TEST_F(MediaVideoVisibilityTrackerTest, SingleElementOccludingAboveThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(700, 700));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        position: relative;
        width: 500px;
        height: 500px;
        top:0;
        left:0
      }
      div {
        background-color: blue;
        width: 400px;
        height: 400px;
        position: absolute;
        top: 0;
        left: 0;
      }
    </style>
    <video></video>
    <div></div>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(0.80);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rects{
      SkIRect::MakeXYWH(0, 0, 400, 400)};
  EXPECT_EQ(expected_occludning_rects, OccludingRects());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       MultipleElementsOccludingEqualToThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        position: relative;
        width: 100%;
        height: 100%;
      }
      .occluding_div_1 {
        background-color: blue;
        width: 250px;
        height: 250px;
        position: absolute;
        top: 0;
        left: 0;
      }
      .occluding_div_2 {
        background-color: red;
        width: 250px;
        height: 250px;
        position: absolute;
        top: 0;
        right: 0;
      }
    </style>
    <video></video>
    <div class="occluding_div_1"></div>
    <div class="occluding_div_2"></div>
  )HTML");
  const float visibility_threshold = 0.5;
  EXPECT_CALL(ReportVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(visibility_threshold);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rects{
      SkIRect::MakeXYWH(250, 0, 250, 250), SkIRect::MakeXYWH(0, 0, 250, 250)};
  EXPECT_THAT(expected_occludning_rects,
              UnorderedElementsAreArray(OccludingRects()));
  EXPECT_EQ(visibility_threshold,
            OccludedArea() / ComputeArea(IntersectionRect()));
}

TEST_F(MediaVideoVisibilityTrackerTest,
       MultipleElementsOccludingGreaterThanThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        position: relative;
        width: 100%;
        height: 100%;
      }
      .occluding_div_1 {
        background-color: blue;
        width: 250px;
        height: 250px;
        position: absolute;
        top: 0;
        left: 0;
      }
      .occluding_div_2 {
        background-color: red;
        width: 270px;
        height: 270px;
        position: absolute;
        top: 0;
        right: 0;
      }
    </style>
    <video></video>
    <div class="occluding_div_1"></div>
    <div class="occluding_div_2"></div>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rects{
      SkIRect::MakeXYWH(230, 0, 270, 270), SkIRect::MakeXYWH(0, 0, 250, 250)};
  EXPECT_THAT(expected_occludning_rects,
              UnorderedElementsAreArray(OccludingRects()));
}

TEST_F(MediaVideoVisibilityTrackerTest, ElementWithZeroOpacityIsIgnored) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(700, 700));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        position: relative;
        width: 500px;
        height: 500px;
        top:0;
        left:0
      }
      div {
        background-color: blue;
        width: 400px;
        height: 400px;
        position: absolute;
        top: 0;
        left: 0;
        opacity: 0.2;
      }
    </style>
    <video></video>
    <div></div>
  )HTML");
  // First ensure that we report the video as not visible, when an element has
  // opacity greater than zero and occludes an area greater than what's allowed
  // by the threshold.
  EXPECT_CALL(ReportVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(0.80);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  EXPECT_EQ(VectorOf<SkIRect>{SkIRect::MakeXYWH(0, 0, 400, 400)},
            OccludingRects());

  // Now set opacity to zero and verify that the video is considered visible.
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  div->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.0"));

  EXPECT_CALL(ReportVisibilityCb(), Run(true));

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());
}

TEST_F(MediaVideoVisibilityTrackerTest, ElementsBehindVideoAreIgnored) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        position: relative;
        width: 100%;
        height: 100%;
      }
      .occluding_div_1 {
        background-color: blue;
        width: 250px;
        height: 250px;
        position: absolute;
        top: 0;
        left: 0;
      }
      .occluding_div_2 {
        background-color: red;
        width: 270px;
        height: 270px;
        position: absolute;
        top: 0;
        right: 0;
      }
    </style>
    <div class="occluding_div_1"></div>
    <div class="occluding_div_2"></div>
    <video></video>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       MultipleIntersectingElementsOccludingGreaterThanThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        position: relative;
        width: 100%;
        height: 100%;
      }
      .occluding_div_1 {
        background-color: blue;
        width: 100%;
        height: 100%;
        position: absolute;
        top: 0;
        left: 0;
      }
      .occluding_div_2 {
        background-color: red;
        width: 100%;
        height: 50px;
        position: absolute;
        top: 0;
        left: 0;
      }
    </style>
    <video></video>
    <div class="occluding_div_1"></div>
    <div class="occluding_div_2"></div>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rects{
      SkIRect::MakeXYWH(0, 0, 500, 50), SkIRect::MakeXYWH(0, 0, 500, 500)};
  EXPECT_THAT(expected_occludning_rects,
              UnorderedElementsAreArray(OccludingRects()));
  EXPECT_EQ(1, OccludedArea() / ComputeArea(IntersectionRect()));
}

TEST_F(MediaVideoVisibilityTrackerTest,
       MultipleIntersectingElementsOccludingBelowThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  LoadMainResource(R"HTML(
  <style>
    body {
      margin: 0;
    }
    video {
      position: relative;
      width: 500px;
      height: 500px;
    }
    .occluding_div_1 {
      background-color: blue;
      width: 500px;
      height: 50px;
      position: absolute;
      top: 0;
      left: 0;
    }
    .occluding_div_2 {
      background-color: red;
      width: 500px;
      height: 50px;
      position: absolute;
      top: 25px;
      left: 0;
    }
    .occluding_div_3 {
      background-color: yellow;
      width: 500px;
      height: 50px;
      position: absolute;
      top: 25px;
      left: 0;
    }
    .occluding_div_4 {
      background-color: green;
      width: 500px;
      height: 50px;
      position: absolute;
      top: 25px;
      left: 0;
    }
  </style>
  <video></video>
  <div class="occluding_div_1"></div>
  <div class="occluding_div_2"></div>
  <div class="occluding_div_3"></div>
  <div class="occluding_div_4"></div>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(0.80);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  // Verify that overlapping intersections were not counted multiple times.
  EXPECT_EQ(37500, OccludedArea());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       HitTestingStopsWhenOcclusionAboveThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        position: relative;
        width: 100%;
        height: 100%;
      }
      .occluding_div_1 {
        background-color: blue;
        width: 100%;
        height: 125px;
        position: absolute;
        top: 0;
        left: 0;
      }
      .occluding_div_2 {
        background-color: red;
        width: 100%;
        height: 130px;
        position: absolute;
        top: 125;
        left: 0;
      }
      .occluding_div_3 {
        background-color: yellow;
        width: 100%;
        height: 125px;
        position: absolute;
        top: 255px;
        left: 0;
      }
    </style>
    <video></video>
    <div class="occluding_div_1"></div>
    <div class="occluding_div_2"></div>
    <div class="occluding_div_3"></div>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rects{
      SkIRect::MakeXYWH(0, 255, 500, 125), SkIRect::MakeXYWH(0, 125, 500, 130)};
  EXPECT_THAT(expected_occludning_rects,
              UnorderedElementsAreArray(OccludingRects()));
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ViewportIntersectionFromVisibleToNotVisibleWithNoOcclusion) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  LoadMainResource(R"HTML(
    <style>
      body {
        height: 2000px;
      }
      video {
        width: 50px;
        height: 50px;
      }
    </style>
    <video></video>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());

  // Scroll page and verify that the visibility threshold is not met.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 500), mojom::blink::ScrollType::kProgrammatic);
  EXPECT_CALL(ReportVisibilityCb(), Run(false));

  Compositor().BeginFrame();
  test::RunPendingTasks();

  EXPECT_TRUE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ViewportIntersectionFromNotVisibleToVisibleWithNoOcclusion) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  LoadMainResource(R"HTML(
    <style>
      body {
        height: 2000px;
      }
      video {
        width: 50px;
        height: 50px;
        position: absolute;
        top: 600px;
        left: 0px;
      }
    </style>
    <video></video>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  // Scroll page and verify that the visibility threshold is not met.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 600), mojom::blink::ScrollType::kProgrammatic);
  EXPECT_CALL(ReportVisibilityCb(), Run(true));

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       SingleElementPartiallyIntersectingNotOccludingAboveThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        position: relative;
        width: 200px;
        height: 200px;
        top: 0;
        left: 0}
      div {
        background-color: blue;
        width: 650px;
        height: 100px;
        position: absolute;
        top: 0;
        right: 0;
      }
    </style>
    <video></video>
    <div></div>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rect{
      SkIRect::MakeXYWH(150, 0, 50, 100)};
  EXPECT_EQ(expected_occludning_rect, OccludingRects());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       NonUserAgentShadowDomElementOccludingAboveThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
  <style>
    body {
      margin: 0;
    }
    video {
      position: relative;
      width: 100%;
      height: 100%;
    }
  </style>
  <video></video>
  <host-element>
    <template shadowrootmode="open">
      <style>
        div {
          position: absolute;
          top: 0;
          bottom: 0;
          width: 100%;
          height: 100%;
          background-color: blue;
        }
      </style>
      <div>
      </div>
    </template>
  </host-element>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rect{SkIRect::MakeXYWH(0, 0, 800, 800)};
  EXPECT_EQ(expected_occludning_rect, OccludingRects());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ElementAddedAfterPlayOccludingAboveThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        position: relative;
        width: 500px;
        height: 500px;
        top:0;
        left:0
      }
      div {
        background-color: blue;
        width: 100px;
        height: 100px;
        position: absolute;
        top: 0;
        left: 0;
      }
    </style>
    <video></video>
    <div></div>
  )HTML");
  EXPECT_CALL(ReportVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(0.5);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rect{SkIRect::MakeXYWH(0, 0, 100, 100)};
  EXPECT_EQ(expected_occludning_rect, OccludingRects());

  // Update div to a size that would cause the video to not meet the visibility
  // threshold.
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  div->setAttribute(html_names::kStyleAttr,
                    AtomicString("width:100%; height: 100%;"));
  EXPECT_CALL(ReportVisibilityCb(), Run(false));

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  // Observe that video does not meet visibility threshold, now that the div
  // occludes 100% of the video.
  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  expected_occludning_rect = {SkIRect::MakeXYWH(0, 0, 500, 500)};
  EXPECT_EQ(expected_occludning_rect, OccludingRects());
}

}  // namespace blink
