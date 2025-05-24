// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

using testing::ElementsAre;

namespace blink {

class FrameSchedulerTest : public SimTest {};

TEST_F(FrameSchedulerTest, BackForwardCacheOptOut_FrameNavigated) {
  SimRequest main_resource_1("https://example.com/example_1.html", "text/html");
  LoadURL("https://example.com/example_1.html");
  main_resource_1.Complete(R"HTML(
      <!DOCTYPE html>
      <a id="anchorlink" href="#bottom">Link to bottom of the page</a>
      <div style="height: 1000px;"></div>
      <input id="bottom">Bottom of the page</input>
    )HTML");
  FrameScheduler* frame_scheduler = MainFrame().GetFrame()->GetFrameScheduler();

  const auto bf_cache_metrics_contains =
      [&frame_scheduler](SchedulingPolicy::Feature feature) {
        return frame_scheduler
            ->GetActiveFeaturesTrackedForBackForwardCacheMetrics()
            .Contains(feature);
      };
  EXPECT_FALSE(
      bf_cache_metrics_contains(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_FALSE(bf_cache_metrics_contains(
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore));

  auto feature_handle = frame_scheduler->RegisterFeature(
      SchedulingPolicy::Feature::kWebSocket,
      {SchedulingPolicy::DisableBackForwardCache()});

  EXPECT_TRUE(bf_cache_metrics_contains(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_FALSE(bf_cache_metrics_contains(
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore));

  frame_scheduler->RegisterStickyFeature(
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore,
      {SchedulingPolicy::DisableBackForwardCache()});

  EXPECT_TRUE(bf_cache_metrics_contains(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_TRUE(bf_cache_metrics_contains(
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore));

  // Click on the anchor element. This will cause a synchronous same-document
  // navigation. Same document navigations don't affect anything.
  auto* anchor = To<HTMLAnchorElement>(
      GetDocument().getElementById(AtomicString("anchorlink")));
  anchor->click();

  EXPECT_TRUE(bf_cache_metrics_contains(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_TRUE(bf_cache_metrics_contains(
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore));

  // Regular navigations should reset the registered features.
  SimRequest main_resource_2("https://example.com/example_2.html", "text/html");
  LoadURL("https://example.com/example_2.html");
  main_resource_2.Complete(R"HTML(
    <!DOCTYPE HTML>
    <body>
    </body>
  )HTML");
  EXPECT_FALSE(
      bf_cache_metrics_contains(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_FALSE(bf_cache_metrics_contains(
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore));

  // Resetting a feature handle after navigation shouldn't do anything.
  feature_handle.reset();

  EXPECT_FALSE(
      bf_cache_metrics_contains(SchedulingPolicy::Feature::kWebSocket));
  EXPECT_FALSE(bf_cache_metrics_contains(
      SchedulingPolicy::Feature::kMainResourceHasCacheControlNoStore));
}

// Regression test for crbug.com/375923380. Ensures scheduled features scoped to
// a document will not be scheduled when the frame is reused but the document is
// replaced.
TEST_F(FrameSchedulerTest, SchedulingFeatureHandlesResetWhenDocumentReplaced) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE HTML>
    <body>
    </body>
  )HTML");

  // Register an arbitrary feature for scheduling.
  Document* initial_document = MainFrame().GetFrame()->GetDocument();
  auto scheduling_feature_handle =
      MainFrame().GetFrame()->GetFrameScheduler()->RegisterFeature(
          SchedulingPolicy::Feature::kWebSocket, {});
  EXPECT_TRUE(scheduling_feature_handle);

  // Perform a frame discard operation, this should replace the main frame's
  // document.
  MainFrame().GetFrame()->Discard();
  Document* final_document = MainFrame().GetFrame()->GetDocument();
  EXPECT_NE(initial_document, final_document);

  // The scheduling feature handle should no longer be valid after the frame's
  // document is replaced.
  EXPECT_FALSE(scheduling_feature_handle);
}

class FrameSchedulerFrameTypeTest : public SimTest {};

TEST_F(FrameSchedulerFrameTypeTest, GetFrameType) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE HTML>
    <body>
    <iframe src="about:blank"></iframe>
    </body>
  )HTML");

  EXPECT_EQ(FrameScheduler::FrameType::kMainFrame,
            MainFrame().GetFrame()->GetFrameScheduler()->GetFrameType());

  Frame* child = MainFrame().GetFrame()->Tree().FirstChild();
  EXPECT_EQ(FrameScheduler::FrameType::kSubframe,
            To<LocalFrame>(child)->GetFrameScheduler()->GetFrameType());
}

class FencedFrameFrameSchedulerTest
    : private ScopedFencedFramesForTest,
      public testing::WithParamInterface<const char*>,
      public SimTest {
 public:
  FencedFrameFrameSchedulerTest() : ScopedFencedFramesForTest(true) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(FencedFrameFrameSchedulerTest, GetFrameType) {
  InitializeFencedFrameRoot(
      blink::FencedFrame::DeprecatedFencedFrameMode::kDefault);
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"HTML(
    <!DOCTYPE HTML>
    <body>
    </body>
  )HTML");

  // A fenced frame root will should be treated as a main frame but
  // marked in an embedded frame tree.
  EXPECT_EQ(FrameScheduler::FrameType::kMainFrame,
            MainFrame().GetFrame()->GetFrameScheduler()->GetFrameType());
  EXPECT_TRUE(
      MainFrame().GetFrame()->GetFrameScheduler()->IsInEmbeddedFrameTree());
}

}  // namespace blink
