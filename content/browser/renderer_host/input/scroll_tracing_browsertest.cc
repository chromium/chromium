// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/strings/string_number_conversions.h"
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

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

std::optional<int64_t> ConvertToIntValue(std::string query_value) {
  int64_t result;
  if (base::StringToInt64(query_value, &result)) {
    return result;
  }
  return std::nullopt;
}

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
  SimulateEndOfPaintHoldingOnPrimaryMainFrame(shell()->web_contents());

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
  std::optional<int64_t> metrics = ConvertToIntValue(scroll_metrics[0]);
  EXPECT_TRUE(metrics.has_value());
  EXPECT_GE(metrics.value(), 1);
  // vsync_count
  metrics = ConvertToIntValue(scroll_metrics[1]);
  EXPECT_TRUE(metrics.has_value());
  EXPECT_GE(metrics.value(), 1);

  ValidateUkm(
      url, ukm::builders::Event_Scroll::kEntryName,
      {
          {ukm::builders::Event_Scroll::kFrameCountName,
           ConvertToIntValue(scroll_metrics[0]).value()},
          {ukm::builders::Event_Scroll::kVsyncCountName,
           ConvertToIntValue(scroll_metrics[1]).value()},
          {ukm::builders::Event_Scroll::kScrollJank_MissedVsyncsMaxName,
           ConvertToIntValue(scroll_metrics[2]).value()},
          {ukm::builders::Event_Scroll::kScrollJank_MissedVsyncsSumName,
           ConvertToIntValue(scroll_metrics[3]).value()},
          {ukm::builders::Event_Scroll::kScrollJank_DelayedFrameCountName,
           ConvertToIntValue(scroll_metrics[4]).value()},
          {ukm::builders::Event_Scroll::kPredictorJankyFrameCountName,
           ConvertToIntValue(scroll_metrics[5]).value()},
      });
}

/**
 * Helper classes to make it more convenient to iterate on the results returned
 * by `TestTraceProcessor::RunQuery()`. This is the basic version that is only
 * used in one test at the moment. If more tests start using
 *`TestTraceProcessor`, we can improve the helpers and move them to
 *`TestTraceProcessor`.
 **/
class Column {
 public:
  explicit Column(std::vector<std::string> column) : column_(column) {}

  std::vector<std::optional<int64_t>> GetIntValuesOrNulls() const {
    std::vector<std::optional<int64_t>> result;
    for (const std::string& row : column_) {
      result.push_back(ConvertToIntValue(row));
    }
    return result;
  }

 private:
  const std::vector<std::string> column_;
};

class Row {
 public:
  Row(const std::vector<std::string>& row,
      const std::map<std::string, int64_t>& column_name_to_index_)
      : row_(row), column_name_to_index_(column_name_to_index_) {}

  bool HasValue(const std::string& column_name) {
    const auto it = column_name_to_index_->find(column_name);
    EXPECT_NE(it, column_name_to_index_->end())
        << "Column " << column_name << " not found";
    return (*row_)[it->second] != "[NULL]";
  }

  std::optional<int64_t> GetIntValueOrNull(
      const std::string& column_name) const {
    const auto it = column_name_to_index_->find(column_name);
    EXPECT_NE(it, column_name_to_index_->end())
        << "Column " << column_name << " not found";
    return ConvertToIntValue((*row_)[it->second]);
  }

 private:
  const raw_ref<const std::vector<std::string>> row_;
  const raw_ref<const std::map<std::string, int64_t>> column_name_to_index_;
};

class Result {
 public:
  explicit Result(std::vector<std::vector<std::string>>& results)
      : results_(results) {
    const std::vector<std::string>& column_names = results_[0];

    for (size_t i = 0; i < column_names.size(); ++i) {
      column_name_to_index_[column_names[i]] = i;
    }

    // Remove the header with column names.
    results_.erase(results_.begin());
  }

  Column GetColumn(const std::string& column_name) const {
    std::vector<std::string> column;
    auto it = column_name_to_index_.find(column_name);
    EXPECT_NE(it, column_name_to_index_.end())
        << "Column " << column_name << " not found";
    for (const auto& result : results_) {
      column.push_back(result[it->second]);
    }
    return Column(std::move(column));
  }

  std::vector<Row> GetRows() const {
    std::vector<Row> result;
    for (const std::vector<std::string>& row : results_) {
      result.emplace_back(row, column_name_to_index_);
    }
    return result;
  }

 private:
  std::map<std::string, int64_t> column_name_to_index_;
  std::vector<std::vector<std::string>> results_;
};

std::vector<std::optional<int64_t>> GetNullableValues(
    const Row& row,
    const std::vector<const char*>& column_names) {
  std::vector<std::optional<int64_t>> nullable_values;

  for (const char* column_name : column_names) {
    nullable_values.push_back(row.GetIntValueOrNull(column_name));
  }

  return nullable_values;
}

std::vector<int64_t> FilterOutNulls(
    const std::vector<std::optional<int64_t>>& nullable_values) {
  std::vector<int64_t> result;
  for (const auto& nullable_value : nullable_values) {
    if (nullable_value) {
      result.push_back(nullable_value.value());
    }
  }
  return result;
}

// Verifies that the scroll updates in the tracing standard library
// have correct properties and expected sequence of steps.
IN_PROC_BROWSER_TEST_F(ScrollTracingBrowserTest, ScrollUpdateInfo) {
  using base::test::TestTraceProcessor;
  TestTraceProcessor ttp_;
  ttp_.StartTrace("input");

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(
      embedded_test_server()->GetURL("/scrollable_page_with_content.html"));
  ASSERT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  SimulateEndOfPaintHoldingOnPrimaryMainFrame(shell()->web_contents());

  // Do a single scroll.
  DoScroll(gfx::Point(10, 10), {gfx::Vector2d(0, 10), gfx::Vector2d(0, 10)},
           content::mojom::GestureSourceType::kTouchInput);

  RunUntilInputProcessed(GetRenderWidgetHostImpl());
  ASSERT_EQ(true, EvalJs(shell()->web_contents(), "did_scroll;"));

  absl::Status status = ttp_.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  // Select chrome scroll updates.
  base::expected<TestTraceProcessor::QueryResult, std::string> raw_result =
      ttp_.RunQuery(R"(
    INCLUDE PERFETTO MODULE chrome.chrome_scrolls;

    SELECT
      id,
      is_presented,
      is_first_scroll_update_in_scroll,
      is_first_scroll_update_in_frame,
      previous_input_id,
      presentation_timestamp,
      generation_ts,
      touch_move_received_ts,
      scroll_update_created_ts,
      compositor_dispatch_ts,
      compositor_on_begin_frame_ts,
      compositor_generate_compositor_frame_ts,
      compositor_submit_compositor_frame_ts,
      viz_receive_compositor_frame_ts,
      viz_draw_and_swap_ts,
      viz_swap_buffers_ts,
      latch_timestamp,
      generation_to_browser_main_dur,
      touch_move_processing_dur,
      scroll_update_processing_dur,
      browser_to_compositor_delay_dur,
      compositor_dispatch_dur,
      compositor_dispatch_to_on_begin_frame_delay_dur,
      compositor_on_begin_frame_dur,
      compositor_on_begin_frame_to_generation_delay_dur,
      compositor_generate_frame_to_submit_frame_dur,
      compositor_submit_frame_dur,
      compositor_to_viz_delay_dur,
      viz_receive_compositor_frame_dur,
      viz_wait_for_draw_dur,
      viz_draw_and_swap_dur,
      viz_to_gpu_delay_dur,
      viz_swap_buffers_dur,
      viz_swap_buffers_to_latch_dur,
      viz_latch_to_presentation_dur
    FROM
      chrome_scroll_update_info
    ORDER BY id
  )");

  ASSERT_TRUE(raw_result.has_value()) << raw_result.error();
  TestTraceProcessor::QueryResult result_value = raw_result.value();

  Result result(result_value);
  EXPECT_GE(result.GetRows().size(), 1u);

  std::vector<std::optional<int64_t>> nullable_scroll_update_ids =
      result.GetColumn("id").GetIntValuesOrNulls();
  std::vector<int64_t> scroll_update_ids =
      FilterOutNulls(nullable_scroll_update_ids);

  // Check that latency ids are not duplicated
  std::set<int64_t> unique_ids(std::begin(scroll_update_ids),
                               std::end(scroll_update_ids));
  EXPECT_THAT(scroll_update_ids,
              testing::UnorderedElementsAreArray(unique_ids));

  // Check that at least one frame was presented and at least
  // one update was the first in the frame.
  EXPECT_THAT(
      FilterOutNulls(result.GetColumn("is_presented").GetIntValuesOrNulls()),
      testing::Contains(1))
      << "No rows with is_presented = 1";
  EXPECT_THAT(FilterOutNulls(result.GetColumn("is_first_scroll_update_in_frame")
                                 .GetIntValuesOrNulls()),
              testing::Contains(1))
      << "No rows with is_first_scroll_update_in_frame = 1";

  size_t row_number = 0;
  for (Row row : result.GetRows()) {
    std::optional<int64_t> maybe_is_presented =
        row.GetIntValueOrNull("is_presented");
    EXPECT_TRUE(maybe_is_presented.has_value());
    bool is_presented = maybe_is_presented.value() == 1;
    LOG(ERROR) << "is presented = " << is_presented;
    if (is_presented) {
      EXPECT_TRUE(row.HasValue("presentation_timestamp"))
          << "No presentation timestamp for update in row " << row_number
          << ", result:\n"
          << result_value;
    }

    std::optional<int64_t> maybe_is_first_scroll_update_in_frame =
        row.GetIntValueOrNull("is_first_scroll_update_in_frame");
    EXPECT_TRUE(maybe_is_first_scroll_update_in_frame.has_value());
    bool is_first_scroll_update_in_frame =
        maybe_is_first_scroll_update_in_frame.value() == 1;

    std::optional<int64_t> maybe_is_first_scroll_update_in_scroll =
        row.GetIntValueOrNull("is_first_scroll_update_in_scroll");
    EXPECT_TRUE(maybe_is_first_scroll_update_in_scroll.has_value());
    bool is_first_scroll_update_in_scroll =
        maybe_is_first_scroll_update_in_scroll.value() == 1;

    std::optional<int64_t> maybe_previous_input_id =
        row.GetIntValueOrNull("previous_input_id");

    if (is_first_scroll_update_in_scroll) {
      EXPECT_FALSE(maybe_previous_input_id.has_value())
          << "Previous input id is not null for the first update in a scroll "
             "in row "
          << row_number << ", result:\n"
          << result_value;
      EXPECT_TRUE(is_first_scroll_update_in_frame)
          << "First update in scroll is not first update in frame in row "
          << row_number << ", result:\n"
          << result_value;
    }

    // Static constant array of timestamp column names.
    static const std::vector<const char*> kTimestampColumnNames = {
        "generation_ts",
        "touch_move_received_ts",
        "scroll_update_created_ts",
        "compositor_dispatch_ts",
        "compositor_on_begin_frame_ts",
        "compositor_generate_compositor_frame_ts",
        "compositor_submit_compositor_frame_ts",
        "viz_receive_compositor_frame_ts",
        "viz_draw_and_swap_ts",
        "viz_swap_buffers_ts",
        "latch_timestamp"};

    std::vector<std::optional<int64_t>> nullable_timestamps =
        GetNullableValues(row, kTimestampColumnNames);
    std::vector<int64_t> timestamps = FilterOutNulls(nullable_timestamps);

    std::vector<int64_t> expected_timestamps = timestamps;

    // The non-NULL timestamps for consecutive stages should be increasing.
    std::sort(expected_timestamps.begin(), expected_timestamps.end());
    EXPECT_EQ(timestamps, expected_timestamps)
        << "Timestamps for consecutive stages are not increasing in row "
        << row_number << ", result:\n"
        << result_value;

    static const std::vector<const char*> kDurationColumnNames = {
        "generation_to_browser_main_dur", "touch_move_processing_dur",
        "scroll_update_processing_dur", "browser_to_compositor_delay_dur",
        "compositor_dispatch_dur",
        // TODO(b:381273884): fix negative stage duration
        // "compositor_dispatch_to_on_begin_frame_delay_dur",
        "compositor_on_begin_frame_dur",
        "compositor_on_begin_frame_to_generation_delay_dur",
        "compositor_generate_frame_to_submit_frame_dur",
        "compositor_submit_frame_dur", "compositor_to_viz_delay_dur",
        "viz_receive_compositor_frame_dur", "viz_wait_for_draw_dur",
        "viz_draw_and_swap_dur", "viz_to_gpu_delay_dur", "viz_swap_buffers_dur",
        "viz_swap_buffers_to_latch_dur", "viz_latch_to_presentation_dur"};

    std::vector<std::optional<int64_t>> nullable_durations =
        GetNullableValues(row, kDurationColumnNames);
    std::vector<int64_t> durations = FilterOutNulls(nullable_durations);

    EXPECT_THAT(durations, testing::Not(testing::Contains(testing::Lt(0))))
        << "Negative duration(s) in row " << row_number << ", result:\n"
        << result_value;
    row_number++;
  }
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace content
