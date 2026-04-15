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
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

using ::testing::_;
using ::testing::Return;
using ::testing::UnorderedElementsAreArray;

namespace {

// Helper class to mock `RequestVisibility` callbacks.
class RequestVisibilityCallback {
 public:
  RequestVisibilityCallback() = default;
  RequestVisibilityCallback(const RequestVisibilityCallback&) = delete;
  RequestVisibilityCallback(RequestVisibilityCallback&&) = delete;
  RequestVisibilityCallback& operator=(const RequestVisibilityCallback&) =
      delete;

  MediaVideoVisibilityTracker::OnDemandRequestVisibilityCb
  VisibilityCallback() {
    meets_visibility_ = std::nullopt;
    // base::Unretained() is safe since no further tasks can run after
    // RunLoop::Run() returns.
    return BindOnce(&RequestVisibilityCallback::RequestVisibility,
                    Unretained(this));
  }

  void WaitUntilDone() {
    if (meets_visibility_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  bool MeetsVisibility() {
    DCHECK(meets_visibility_);
    return meets_visibility_.value();
  }

 private:
  void RequestVisibility(bool meets_visibility) {
    meets_visibility_ = meets_visibility;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::optional<bool> meets_visibility_;
};

// Helper class to mock `RequestVisibilityRatio` callbacks.
class RequestVisibilityRatioCallback {
 public:
  RequestVisibilityRatioCallback() = default;
  RequestVisibilityRatioCallback(const RequestVisibilityRatioCallback&) =
      delete;
  RequestVisibilityRatioCallback(RequestVisibilityRatioCallback&&) = delete;
  RequestVisibilityRatioCallback& operator=(
      const RequestVisibilityRatioCallback&) = delete;

  MediaVideoVisibilityTracker::OnDemandRequestVisibilityRatioCb
  VisibilityRatioCallback() {
    visibility_ratio_ = std::nullopt;
    // base::Unretained() is safe since no further tasks can run after
    // RunLoop::Run() returns.
    return BindOnce(&RequestVisibilityRatioCallback::RequestVisibilityRatio,
                    Unretained(this));
  }

  void WaitUntilDone() {
    if (visibility_ratio_) {
      return;
    }
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  double VisibilityRatio() {
    DCHECK(visibility_ratio_);
    return visibility_ratio_.value();
  }

 private:
  void RequestVisibilityRatio(double visibility_ratio) {
    visibility_ratio_ = visibility_ratio;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  std::unique_ptr<base::RunLoop> run_loop_;
  std::optional<double> visibility_ratio_;
};

}  // namespace

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
    HTMLVideoElement* video_element = To<HTMLVideoElement>(
        GetDocument().QuerySelector(AtomicString("video")));
    DCHECK(video_element);
    return video_element;
  }

  MediaVideoVisibilityTracker* CreateTracker(
      const int visibility_threshold,
      MediaVideoVisibilityTracker::ReportContinuousVisibilityCb callback) {
    DCHECK(!tracker_);
    tracker_ = MakeGarbageCollected<MediaVideoVisibilityTracker>(
        *VideoElement(), visibility_threshold, std::move(callback));
    return tracker_;
  }

  MediaVideoVisibilityTracker* CreateTracker(const int visibility_threshold) {
    return CreateTracker(visibility_threshold,
                         ReportContinuousVisibilityCb().Get());
  }

  MediaVideoVisibilityTracker* CreateAndAttachVideoVisibilityTracker(
      const int visibility_threshold,
      MediaVideoVisibilityTracker::ReportContinuousVisibilityCb callback) {
    CreateTracker(visibility_threshold, std::move(callback));
    tracker_->Attach();
    return tracker_;
  }

  MediaVideoVisibilityTracker* CreateAndAttachVideoVisibilityTracker(
      const int visibility_threshold) {
    return CreateAndAttachVideoVisibilityTracker(
        visibility_threshold, ReportContinuousVisibilityCb().Get());
  }

  float ComputeArea(const PhysicalRect& rect) {
    return static_cast<float>(ToRoundedSize(rect.size).Area64());
  }

  base::MockRepeatingCallback<void(bool)>& ReportContinuousVisibilityCb() {
    return report_continuous_visibility_cb_;
  }

  const PhysicalRect IntersectionRect() const {
    DCHECK(tracker_);
    return tracker_->occlusion_state_.intersection_rect;
  }

  const VectorOf<SkIRect> OccludingRects() const {
    DCHECK(tracker_);
    return tracker_->occlusion_state_.occluding_rects;
  }

  float OccludedArea() const {
    DCHECK(tracker_);
    return tracker_->occlusion_state_.occluded_area;
  }

  const MediaVideoVisibilityTracker::ClientIdsSet GetClientIdsSet(
      DisplayItemClientId start_after_display_item_client_id) const {
    DCHECK(tracker_);
    return tracker_->GetClientIdsSet(start_after_display_item_client_id);
  }

  bool IsTrackerAttached() const {
    DCHECK(tracker_);
    return tracker_->tracker_attached_to_document_ != nullptr;
  }

  void SetRequestVisibilityCbForTesting(
      RequestVisibilityCallback& request_visibility_callback) {
    DCHECK(tracker_);
    tracker_->on_demand_visibility_cb_ =
        request_visibility_callback.VisibilityCallback();
  }

  void DetachVideoVisibilityTracker() {
    DCHECK(tracker_);
    tracker_->Detach();
  }

 private:
  Persistent<MediaVideoVisibilityTracker> tracker_;
  base::MockRepeatingCallback<void(bool)> report_continuous_visibility_cb_;
};

#if DCHECK_IS_ON()
TEST_F(MediaVideoVisibilityTrackerTest, InvalidThreshold) {
  auto* video = GetDocument().CreateRawElement(html_names::kVideoTag);
  GetDocument().body()->AppendChild(video);
  EXPECT_DEATH_IF_SUPPORTED(CreateAndAttachVideoVisibilityTracker(0), "");
}
#endif  // DCHECK_IS_ON()

TEST_F(MediaVideoVisibilityTrackerTest,
       NoOcclusionDoesNotMeetVisibilityThreshold) {
  LoadMainResource(R"HTML(
    <style>
      video {
        object-fit: fill;
        width: 50px;
        height: 50px;
      }
    </style>
    <video></video>
  )HTML");
  EXPECT_FALSE(VideoElement()->ShouldShowControls());

  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());
}

TEST_F(MediaVideoVisibilityTrackerTest, NoOcclusionMeetsVisibilityThreshold) {
  LoadMainResource(R"HTML(
    <style>
      video {
        object-fit: fill;
        width: 150px;
        height: 150px;
      }
    </style>
    <video></video>
  )HTML");
  EXPECT_FALSE(VideoElement()->ShouldShowControls());

  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

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
        object-fit: fill;
        width: 150px;
        height: 150px;
      }
    </style>
    <video controls></video>
  )HTML");
  EXPECT_TRUE(VideoElement()->ShouldShowControls());

  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());
}

TEST_F(MediaVideoVisibilityTrackerTest, NoViewPortIntersection) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(500, 500));
  LoadMainResource(R"HTML(
    <style>
      video {
        object-fit: fill;
        width: 150px;
        height: 150px;
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

  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));

  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 500), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);

  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rect{
      SkIRect::MakeXYWH(8, -158, 150, 150)};
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
        object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

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
        object-fit: fill;
        position: relative;
        width: 500px;
        height: 500px;
        top:0;
        left:0
      }
      div {
        background-color: blue;
        width: 490px;
        height: 490px;
        position: absolute;
        top: 0;
        left: 0;
      }
    </style>
    <video></video>
    <div></div>
  )HTML");
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rects{
      SkIRect::MakeXYWH(0, 0, 490, 490)};
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
        object-fit: fill;
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
  const int visibility_threshold = 125000;
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
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
            ComputeArea(IntersectionRect()) - OccludedArea());
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
        object-fit: fill;
        position: relative;
        width: 100%;
        height: 100%;
      }
      .occluding_div_1 {
        background-color: blue;
        width: 250px;
        height: 500px;
        position: absolute;
        top: 0;
        left: 0;
      }
      .occluding_div_2 {
        background-color: red;
        width: 250px;
        height: 490px;
        position: absolute;
        top: 0;
        right: 0;
      }
    </style>
    <video></video>
    <div class="occluding_div_1"></div>
    <div class="occluding_div_2"></div>
  )HTML");
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rects{
      SkIRect::MakeXYWH(250, 0, 250, 490), SkIRect::MakeXYWH(0, 0, 250, 500)};
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
        object-fit: fill;
        position: relative;
        width: 500px;
        height: 500px;
        top:0;
        left:0
      }
      div {
        background-color: blue;
        width: 500px;
        height: 500px;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  EXPECT_EQ(VectorOf<SkIRect>{SkIRect::MakeXYWH(0, 0, 500, 500)},
            OccludingRects());

  // Now set opacity to zero and verify that the video is considered visible.
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  div->setAttribute(html_names::kStyleAttr, AtomicString("opacity: 0.0"));

  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));

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
        object-fit: fill;
        position: relative;
        width: 100%;
        height: 100%;
      }
      .occluding_div_1 {
        background-color: blue;
        width: 250px;
        height: 500px;
        position: absolute;
        top: 0;
        left: 0;
      }
      .occluding_div_2 {
        background-color: red;
        width: 250px;
        height: 500px;
        position: absolute;
        top: 0;
        right: 0;
      }
    </style>
    <div class="occluding_div_1"></div>
    <div class="occluding_div_2"></div>
    <video></video>
  )HTML");
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

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
        object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(10000);

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
      object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

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
        object-fit: fill;
        position: relative;
        width: 100%;
        height: 100%;
      }
      .occluding_div_1 {
        background-color: blue;
        width: 100%;
        height: 10px;
        position: absolute;
        top: 485px;
        left: 0;
      }
      .occluding_div_2 {
        background-color: red;
        width: 100%;
        height: 360px;
        position: absolute;
        top: 125px;
        left: 0;
      }
      .occluding_div_3 {
        background-color: yellow;
        width: 100%;
        height: 125px;
        position: absolute;
        top: 0;
        left: 0;
      }
    </style>
    <video></video>
    <div class="occluding_div_1"></div>
    <div class="occluding_div_2"></div>
    <div class="occluding_div_3"></div>
  )HTML");
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  VectorOf<SkIRect> expected_occludning_rects{
      SkIRect::MakeXYWH(0, 0, 500, 125), SkIRect::MakeXYWH(0, 125, 500, 360)};
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
        object-fit: fill;
        width: 100px;
        height: 100px;
      }
    </style>
    <video></video>
  )HTML");
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());

  // Scroll page and verify that the visibility threshold is not met.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 500), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));

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
        object-fit: fill;
        width: 100px;
        height: 100px;
        position: absolute;
        top: 600px;
        left: 0px;
      }
    </style>
    <video></video>
  )HTML");
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  EXPECT_TRUE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());

  // Scroll page and verify that the visibility threshold is not met.
  GetDocument().View()->LayoutViewport()->SetScrollOffset(
      ScrollOffset(0, 600), mojom::blink::ScrollType::kProgrammatic,
      cc::ScrollSourceType::kNone);
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));

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
        object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

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
      object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));
  CreateAndAttachVideoVisibilityTracker(10000);

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
        object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));

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

TEST_F(MediaVideoVisibilityTrackerTest, ClientIdsSetContents) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
  <style>
    body {
      margin: 0;
    }
    video {
      object-fit: fill;
      position: relative;
      width: 500px;
      height: 500px;
      top:0;
      left:0;
    }
    #ignored_div {
      background-color: blue;
      width: 50px;
      height: 50px;
      position: relative;
    }
    #visible_div {
      background-color: yellow;
      width: 50px;
      height: 50px;
      position: absolute;
      top: 0;
      left: 0;
    }
    #invisible_div {
      width: 50px;
      height: 50px;
      position: absolute;
      top: 0;
      left: 0;
    }
  </style>
  <video></video>
  <div id="ignored_div"></div>
  <div id="visible_div"></div>
  <div id="invisible_div"></div>
  )HTML");
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  // Verify that the DisplayItemClientId passed as a parameter to
  // `GetClientIdsSet` is not in the set.
  auto* ignored_div = GetDocument().getElementById(AtomicString("ignored_div"));
  ASSERT_TRUE(ignored_div);
  const auto set = GetClientIdsSet(ignored_div->GetLayoutObject()->Id());
  EXPECT_FALSE(set.Contains(ignored_div->GetLayoutObject()->Id()));

  // Verify that elements that do not produce visual output are not in the set.
  auto* invisible_div =
      GetDocument().getElementById(AtomicString("invisible_div"));
  ASSERT_TRUE(invisible_div);
  EXPECT_FALSE(set.Contains(invisible_div->GetLayoutObject()->Id()));

  // Verify that elements that produce visual output are in the set.
  auto* visible_div = GetDocument().getElementById(AtomicString("visible_div"));
  ASSERT_TRUE(visible_div);
  EXPECT_TRUE(set.Contains(visible_div->GetLayoutObject()->Id()));
}

TEST_F(MediaVideoVisibilityTrackerTest, ClientIdsSetEndIndexEqualToStartIndex) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
  <style>
    body {
      margin: 0;
    }
    video {
      object-fit: fill;
      position: relative;
      width: 500px;
      height: 500px;
      top:0;
      left:0;
    }
    div {
      background-color: yellow;
      width: 50px;
      height: 50px;
      position: absolute;
      top: 0;
      left: 0;
    }
  </style>
  <video></video>
  <div id="target_div"></div>
  )HTML");
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  auto* target_div = GetDocument().getElementById(AtomicString("target_div"));
  ASSERT_TRUE(target_div);
  const auto set = GetClientIdsSet(target_div->GetLayoutObject()->Id());

  EXPECT_EQ(0u, set.size());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ClientIdsSetBeginIndexGreaterThanEndIndex) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
  <style>
    body {
      margin: 0;
    }
    video {
      object-fit: fill;
      position: relative;
      width: 500px;
      height: 500px;
      top:0;
      left:0;
    }
    div {
      background-color: yellow;
      width: 50px;
      height: 50px;
      position: absolute;
      top: 0;
      left: 0;
    }
  </style>
  <video></video>
  <div></div>
  )HTML");
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  CreateAndAttachVideoVisibilityTracker(10000);

  Compositor().BeginFrame();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  const auto set = GetClientIdsSet(VideoElement()->GetLayoutObject()->Id());

  EXPECT_EQ(0u, set.size());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ComputeVisibilityOnDemandReportsFalseWhenTrackerNotAttached) {
  auto* video = GetDocument().CreateRawElement(html_names::kVideoTag);
  GetDocument().body()->AppendChild(video);

  // Verify that `ReportContinuousVisibilityCb` does not run, when computing
  // visibility on demand, for a document's LocalFrameView in
  // `DocumentUpdateReason::kPaintClean` state.
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(_)).Times(0);

  // Create tracker and verify that it is not attached.
  auto* tracker = CreateTracker(10000);
  ASSERT_FALSE(IsTrackerAttached());

  // Create a `RequestVisibilityCallback` and verify that
  // `MeetsVisibility` returns false.
  RequestVisibilityCallback request_visibility_callback;
  tracker->RequestVisibility(request_visibility_callback.VisibilityCallback());
  request_visibility_callback.WaitUntilDone();
  EXPECT_FALSE(request_visibility_callback.MeetsVisibility());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ComputeVisibilityOnDemandReportsTrueWhenVideoMeetsVisibility) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  auto* tracker = CreateAndAttachVideoVisibilityTracker(10000);

  // Initially set the lifecycle state to a value <
  // DocumentUpdateReason::kPaintClean. The `RequestVisibilityCallback`
  // should run with the `false` cached value.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  // Create the `RequestVisibilityCallback`, and verify that: no
  // visibility computations are performed when the tracker takes the callback,
  // and we report that visibility is not met, since the document lifecycle
  // state is not `DocumentUpdateReason::kPaintClean`.
  RequestVisibilityCallback request_visibility_callback;
  tracker->RequestVisibility(request_visibility_callback.VisibilityCallback());
  request_visibility_callback.WaitUntilDone();
  EXPECT_TRUE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());
  EXPECT_FALSE(request_visibility_callback.MeetsVisibility());

  // Update the lifecycle state to `DocumentUpdateReason::kPaintClean`, request
  // visibility, and wait for the `RequestVisibilityCallback` to run.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();
  tracker->RequestVisibility(request_visibility_callback.VisibilityCallback());
  request_visibility_callback.WaitUntilDone();

  // Verify that: the `RequestVisibilityCallback` callback ran,
  // visibility computations took place, and the video meets the visibility
  // threshold.
  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());
  EXPECT_TRUE(request_visibility_callback.MeetsVisibility());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ComputeVisibilityOnDemandReportsFalseWhenVideoDoesNotMeetVisibility) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        object-fit: fill;
        position: relative;
        width: 500px;
        height: 500px;
        top:0;
        left:0
      }
      div {
        background-color: blue;
        width: 500px;
        height: 500px;
        position: absolute;
        top: 0;
        left: 0;
      }
    </style>
    <video></video>
    <div></div>
  )HTML");
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false));
  auto* tracker = CreateAndAttachVideoVisibilityTracker(10000);

  // Initially set the lifecycle state to a value <
  // DocumentUpdateReason::kPaintClean. The `RequestVisibilityCallback`
  // should run with the `false` cached value.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  // Create the `RequestVisibilityCallback`, and verify that: no
  // visibility computations are performed when the tracker takes the callback,
  // and we report that visibility is not met, since the document lifecycle
  // state is not `DocumentUpdateReason::kPaintClean`.
  RequestVisibilityCallback request_visibility_callback;
  tracker->RequestVisibility(request_visibility_callback.VisibilityCallback());
  request_visibility_callback.WaitUntilDone();
  EXPECT_TRUE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());
  EXPECT_FALSE(request_visibility_callback.MeetsVisibility());

  // Update the lifecycle state to `DocumentUpdateReason::kPaintClean`, request
  // visibility, and wait for the `RequestVisibilityCallback` to run.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();
  tracker->RequestVisibility(request_visibility_callback.VisibilityCallback());
  request_visibility_callback.WaitUntilDone();

  // Verify that: the `RequestVisibilityCallback` callback ran,
  // visibility computations took place, and the video does not meet the
  // visibility threshold.
  EXPECT_FALSE(IntersectionRect().IsEmpty());
  EXPECT_FALSE(OccludingRects().empty());
  EXPECT_FALSE(request_visibility_callback.MeetsVisibility());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ComputeVisibilityOnDemandHitTestTimerIgnored) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  auto* tracker = CreateAndAttachVideoVisibilityTracker(10000);

  // Note that we do not fast forward the virtual time. This will let us verify
  // that the `hit_test_timer_` is ignored.
  Compositor().BeginFrame();
  test::RunPendingTasks();

  // Create a `RequestVisibilityCallback` and verify that: the callback
  // is executed, and `MeetsVisibility` returns true.
  RequestVisibilityCallback request_visibility_callback;
  tracker->RequestVisibility(request_visibility_callback.VisibilityCallback());
  request_visibility_callback.WaitUntilDone();
  EXPECT_TRUE(request_visibility_callback.MeetsVisibility());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ComputeVisibilityOnDemandNewCallbacksTakePriority) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  auto* tracker = CreateAndAttachVideoVisibilityTracker(10000);

  // Directly set the `RequestVisibilityCallback`, and verify that no
  // visibility computations are performed.
  RequestVisibilityCallback request_visibility_callback;
  SetRequestVisibilityCbForTesting(request_visibility_callback);
  EXPECT_TRUE(IntersectionRect().IsEmpty());
  EXPECT_TRUE(OccludingRects().empty());

  // Create a new `RequestVisibilityCallback` and have the tracker take
  // the callback.
  RequestVisibilityCallback new_request_visibility_callback;
  tracker->RequestVisibility(
      new_request_visibility_callback.VisibilityCallback());

  // Verify that both callbacks are run with `false`.
  request_visibility_callback.WaitUntilDone();
  EXPECT_FALSE(request_visibility_callback.MeetsVisibility());
  new_request_visibility_callback.WaitUntilDone();
  EXPECT_FALSE(new_request_visibility_callback.MeetsVisibility());

  // Update the lifecycle state to `DocumentUpdateReason::kPaintClean`, and
  // re-request visibility.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  // Verify that the visibility threshold is met.
  tracker->RequestVisibility(
      new_request_visibility_callback.VisibilityCallback());
  new_request_visibility_callback.WaitUntilDone();
  EXPECT_TRUE(new_request_visibility_callback.MeetsVisibility());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ComputeVisibilityOnDemandUsesCachedVisibilityValue) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  auto* tracker = CreateAndAttachVideoVisibilityTracker(10000);

  // Update the lifecycle state to `DocumentUpdateReason::kPaintClean`, this
  // should cache the visibility value.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  // Set the lifecycle state to a value < `DocumentUpdateReason::kPaintClean`.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  // Request visibility.
  RequestVisibilityCallback request_visibility_callback;
  tracker->RequestVisibility(request_visibility_callback.VisibilityCallback());

  // Verify that `MeetsVisibility` returns true, even though the document is not
  // in a `DocumentUpdateReason::kPaintClean`, since the cached visibility value
  // should be used.
  request_visibility_callback.WaitUntilDone();
  EXPECT_TRUE(request_visibility_callback.MeetsVisibility());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ComputeVisibilityOnDemandAttachesTracker) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        object-fit: fill;
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
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(true));
  auto* tracker = CreateAndAttachVideoVisibilityTracker(10000);

  // Update the lifecycle state to `DocumentUpdateReason::kPaintClean`, this
  // should cache the visibility value.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();
  test::RunPendingTasks();
  task_environment().FastForwardUntilNoTasksRemain();

  // Request visibility and verify that `MeetsVisibility` returns true.
  RequestVisibilityCallback request_visibility_callback;
  tracker->RequestVisibility(request_visibility_callback.VisibilityCallback());
  request_visibility_callback.WaitUntilDone();
  EXPECT_TRUE(request_visibility_callback.MeetsVisibility());

  // Detach the tracker.
  DetachVideoVisibilityTracker();
  ASSERT_FALSE(IsTrackerAttached());

  // Request visibility and verify that `MeetsVisibility` returns true, because
  // RequestVisibility() now attaches the tracker.
  RequestVisibilityCallback request_visibility_callback2;
  tracker->RequestVisibility(request_visibility_callback2.VisibilityCallback());
  request_visibility_callback2.WaitUntilDone();
  EXPECT_TRUE(request_visibility_callback2.MeetsVisibility());
  EXPECT_TRUE(IsTrackerAttached());
}

TEST_F(MediaVideoVisibilityTrackerTest, DetachesWhenNoActiveRequests) {
  LoadMainResource(R"HTML(
    <style>
      video {
        object-fit: fill;
        width: 150px;
        height: 150px;
      }
    </style>
    <video></video>
  )HTML");
  EXPECT_FALSE(VideoElement()->ShouldShowControls());

  auto* tracker = CreateAndAttachVideoVisibilityTracker(
      10000, MediaVideoVisibilityTracker::ReportContinuousVisibilityCb());
  EXPECT_TRUE(IsTrackerAttached());

  tracker->UpdateVisibilityTrackerState();
  EXPECT_FALSE(IsTrackerAttached());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       RequestVisibilityRatioSatisfiedImmediatelyIfPaintClean) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        object-fit: fill;
        width: 100px;
        height: 100px;
      }
    </style>
    <video></video>
  )HTML");

  // Ensure we are PaintClean.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  auto* tracker = CreateTracker(
      10000, MediaVideoVisibilityTracker::ReportContinuousVisibilityCb());
  ASSERT_FALSE(IsTrackerAttached());

  RequestVisibilityRatioCallback request_visibility_ratio_callback;
  tracker->RequestVisibilityRatio(
      request_visibility_ratio_callback.VisibilityRatioCallback());

  // Should be satisfied immediately without waiting for a lifecycle update.
  request_visibility_ratio_callback.WaitUntilDone();
  EXPECT_NEAR(1.0, request_visibility_ratio_callback.VisibilityRatio(), 0.01);

  // Since it was a one-shot request and we aren't doing continuous tracking,
  // it should have detached itself.
  EXPECT_FALSE(IsTrackerAttached());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       RequestVisibilityRatioSatisfiedAfterLifecycleUpdateIfNotPaintClean) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      body {
        margin: 0;
      }
      video {
        object-fit: fill;
        width: 100px;
        height: 100px;
      }
    </style>
    <video></video>
  )HTML");

  auto* tracker = CreateTracker(
      10000, MediaVideoVisibilityTracker::ReportContinuousVisibilityCb());
  ASSERT_FALSE(IsTrackerAttached());

  // Set the lifecycle state to a value < DocumentUpdateReason::kPaintClean.
  GetDocument().View()->UpdateLifecycleToLayoutClean(
      DocumentUpdateReason::kTest);

  RequestVisibilityRatioCallback request_visibility_ratio_callback;
  tracker->RequestVisibilityRatio(
      request_visibility_ratio_callback.VisibilityRatioCallback());

  // Should NOT be satisfied yet.
  EXPECT_TRUE(IsTrackerAttached());

  // Update lifecycle to PaintClean.
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Now it should be satisfied.
  request_visibility_ratio_callback.WaitUntilDone();
  EXPECT_NEAR(1.0, request_visibility_ratio_callback.VisibilityRatio(), 0.01);
  EXPECT_FALSE(IsTrackerAttached());
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ElementWithZeroOpacityAncestorIsIgnored) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      .container {
        opacity: 0;
      }
      video {
        width: 100px;
        height: 100px;
      }
    </style>
    <div class="container">
      <video></video>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  RequestVisibilityRatioCallback request_visibility_ratio_callback;
  auto* tracker = CreateAndAttachVideoVisibilityTracker(1000);

  Compositor().BeginFrame();
  test::RunPendingTasks();

  tracker->RequestVisibilityRatio(
      request_visibility_ratio_callback.VisibilityRatioCallback());
  request_visibility_ratio_callback.WaitUntilDone();

  // The video itself has opacity 1.0, but its ancestor has opacity 0.0.
  // We expect visibility ratio to be 0.0.
  EXPECT_EQ(request_visibility_ratio_callback.VisibilityRatio(), 0.0);
}

TEST_F(MediaVideoVisibilityTrackerTest,
       ElementWithZeroOpacityAncestorDoesNotMeetThreshold) {
  WebView().MainFrameViewWidget()->Resize(gfx::Size(800, 800));
  LoadMainResource(R"HTML(
    <style>
      .container {
        opacity: 0;
      }
      video {
        width: 100px;
        height: 100px;
      }
    </style>
    <div class="container">
      <video></video>
    </div>
  )HTML");

  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Threshold is 5000 (50% of 100x100).
  CreateAndAttachVideoVisibilityTracker(5000);

  // We expect the tracker to report false because of the zero opacity ancestor.
  EXPECT_CALL(ReportContinuousVisibilityCb(), Run(false))
      .Times(testing::AtLeast(1));

  Compositor().BeginFrame();
  test::RunPendingTasks();
}

}  // namespace blink
