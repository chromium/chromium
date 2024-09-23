// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_trace_processor.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/synthetic_gesture_controller.h"
#include "content/common/input/synthetic_smooth_scroll_gesture.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"

namespace content {

class ScrollTracingBrowserTest : public ContentBrowserTest {
 public:
  ScrollTracingBrowserTest() {
    scoped_feature_list_.InitWithFeatures({ukm::kUkmFeature}, {});
  }

  ScrollTracingBrowserTest(const ScrollTracingBrowserTest&) = delete;
  ScrollTracingBrowserTest& operator=(const ScrollTracingBrowserTest&) = delete;

  ~ScrollTracingBrowserTest() override = default;

  void PreRunTestOnMainThread() override {
    ContentBrowserTest::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  RenderWidgetHostImpl* GetRenderWidgetHostImpl() {
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetPrimaryFrameTree()
                              .root();
    return root->current_frame_host()->GetRenderWidgetHost();
  }

  void DoScroll(gfx::Point starting_point,
                std::vector<gfx::Vector2d> distances,
                content::mojom::GestureSourceType source) {
    // Create and queue gestures
    for (const auto distance : distances) {
      SyntheticSmoothScrollGestureParams params;
      params.gesture_source_type = source;
      params.anchor = gfx::PointF(starting_point);
      params.distances.push_back(-distance);
      params.granularity = ui::ScrollGranularity::kScrollByPrecisePixel;
      auto gesture = std::make_unique<SyntheticSmoothScrollGesture>(params);

      base::RunLoop run_loop;
      GetRenderWidgetHostImpl()->QueueSyntheticGesture(
          std::move(gesture),
          base::BindLambdaForTesting([&](SyntheticGesture::Result result) {
            EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
            run_loop.Quit();
          }));
      run_loop.Run();

      // Update the previous start point.
      starting_point = gfx::Point(starting_point.x() + distance.x(),
                                  starting_point.y() + distance.y());
    }
  }

  void ValidateUkm(GURL url,
                   std::string_view entry_name,
                   std::map<std::string_view, int64_t> expected_values) {
    const auto& entries =
        test_ukm_recorder_->GetMergedEntriesByName(entry_name);
    EXPECT_EQ(1u, entries.size());
    for (const auto& kv : entries) {
      test_ukm_recorder_->ExpectEntrySourceHasUrl(kv.second.get(), url);
      for (const auto& expected_kv : expected_values) {
        EXPECT_TRUE(test_ukm_recorder_->EntryHasMetric(kv.second.get(),
                                                       expected_kv.first));
        if (*(test_ukm_recorder_->GetEntryMetric(kv.second.get(),
                                                 expected_kv.first)) != 0) {
          test_ukm_recorder_->ExpectEntryMetric(
              kv.second.get(), expected_kv.first,
              ukm::GetExponentialBucketMinForCounts1000(expected_kv.second));
        }
      }
    }
  }

  int64_t ConvertToHistogramValue(std::string query_value) {
    int64_t result;
    base::StringToInt64(query_value, &result);
    return result;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

// NOTE:  Mac doesn't support touch events, and will not record scrolls with
// touch input. Linux bots are inconsistent.
#if BUILDFLAG(IS_ANDROID)
// Basic parity matching test between trace events and UKM if both are recorded
// during a scroll.
IN_PROC_BROWSER_TEST_F(ScrollTracingBrowserTest, ScrollingMetricsParity) {
  base::test::TestTraceProcessor ttp_;
  ttp_.StartTrace("input.scrolling");

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/scrollable_page_with_content.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Scroll with 3 updates to ensure:
  //  1) One frame is missed (maximum 2 may be missed).
  //  2) Predictor jank occurs.
  DoScroll(gfx::Point(10, 10), {gfx::Vector2d(0, 100), gfx::Vector2d(0, -100)},
           content::mojom::GestureSourceType::kTouchInput);

  RunUntilInputProcessed(GetRenderWidgetHostImpl());
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), "did_scroll;"));

  absl::Status status = ttp_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  // Use the values in chrome_scroll_interactions to validate values in UKM.
  auto result = ttp_.RunQuery(R"(
    INCLUDE PERFETTO MODULE chrome.scroll_interactions;

    SELECT
      frame_count,
      vsync_count,
      missed_vsync_max,
      missed_vsync_sum,
      delayed_frame_count,
      predictor_janky_frame_count
    FROM chrome_scroll_interactions
    LIMIT 1;
  )");

  ASSERT_TRUE(result.has_value()) << result.error();
  auto scroll_metrics_result = result.value();

  // Validate that there are two rows in the output. The first row is the
  // column names, the second row is the values. If there is only one row,
  // then the query produced no data (a.k.a the data model was not populated).
  EXPECT_EQ(scroll_metrics_result.size(), 2u);

  // Validate that each column of the query is present.
  ASSERT_TRUE(scroll_metrics_result[1].size() == 6);

  auto scroll_metrics = scroll_metrics_result[1];

  // frame_count
  EXPECT_GE(ConvertToHistogramValue(scroll_metrics[0]), 1);
  // vsync_count
  EXPECT_GE(ConvertToHistogramValue(scroll_metrics[1]), 1);

  ValidateUkm(
      url, ukm::builders::Event_Scroll::kEntryName,
      {
          {ukm::builders::Event_Scroll::kFrameCountName,
           ConvertToHistogramValue(scroll_metrics[0])},
          {ukm::builders::Event_Scroll::kVsyncCountName,
           ConvertToHistogramValue(scroll_metrics[1])},
          {ukm::builders::Event_Scroll::kScrollJank_MissedVsyncsMaxName,
           ConvertToHistogramValue(scroll_metrics[2])},
          {ukm::builders::Event_Scroll::kScrollJank_MissedVsyncsSumName,
           ConvertToHistogramValue(scroll_metrics[3])},
          {ukm::builders::Event_Scroll::kScrollJank_DelayedFrameCountName,
           ConvertToHistogramValue(scroll_metrics[4])},
          {ukm::builders::Event_Scroll::kPredictorJankyFrameCountName,
           ConvertToHistogramValue(scroll_metrics[5])},
      });
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace content
