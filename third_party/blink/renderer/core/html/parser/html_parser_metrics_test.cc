// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_parser_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

class HTMLMetricsTest : public testing::Test {
 public:
  HTMLMetricsTest() {
    helper_.Initialize(nullptr, nullptr, nullptr);
    // TODO(crbug.com/1329535): Remove if threaded preload scanner doesn't
    // launch.
    // Turn off preload scanning since it can mess with parser yield logic.
    helper_.LocalMainFrame()
        ->GetFrame()
        ->GetDocument()
        ->GetSettings()
        ->SetDoHtmlPreloadScanning(false);
  }

  ~HTMLMetricsTest() override = default;

  void SetUp() override {}

  void TearDown() override {}

  void LoadHTML(const std::string& html) {
    frame_test_helpers::LoadHTMLString(
        helper_.GetWebView()->MainFrameImpl(), html,
        url_test_helpers::ToKURL("https://www.foo.com/"));
  }

 protected:
  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;
};

// https://crbug.com/1222653
TEST_F(HTMLMetricsTest, DISABLED_ReportSingleChunk) {
  // Although the tests use a mock clock, the metrics recorder checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  base::HistogramTester histogram_tester;
  LoadHTML(R"HTML(
    <div></div>
  )HTML");

  // Should have one of each metric, except the yield times because with
  // a single chunk they should not report.
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ChunkCount4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeMax4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeMin4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeTotal4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedMax4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedMin4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedAverage4",
                                    1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedTotal4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeMax4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeMin4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeAverage4", 1);

  // Expect specific values for the chunks and tokens counts
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.ChunkCount4", 1, 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedMax4", 5,
                                      1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedMin4", 5,
                                      1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedAverage4",
                                      5, 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedTotal4", 5,
                                      1);

  // Expect that the times have moved from the default and the max and min
  // and total are all the same (within the same bucket)
  std::vector<base::Bucket> parsing_time_max_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeMax4");
  std::vector<base::Bucket> parsing_time_min_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeMin4");
  std::vector<base::Bucket> parsing_time_total_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeTotal4");
  EXPECT_EQ(parsing_time_max_buckets.size(), 1u);
  EXPECT_EQ(parsing_time_min_buckets.size(), 1u);
  EXPECT_EQ(parsing_time_total_buckets.size(), 1u);
  EXPECT_GT(parsing_time_max_buckets[0].min, 0);
  EXPECT_GT(parsing_time_min_buckets[0].min, 0);
  EXPECT_GT(parsing_time_total_buckets[0].min, 0);

  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.InputCharacterCount4",
                                      19, 1);
}

// https://crbug.com/1222653
TEST_F(HTMLMetricsTest, DISABLED_HistogramReportsTwoChunks) {
  // Although the tests use a mock clock, the metrics recorder checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  base::HistogramTester histogram_tester;

  // This content processes many tokens before a script tag used as the yield
  // threshold. If the yield behavior changes this test may need updating.
  // See the HTMLDocumentParser::PumpTokenizer method for the current yielding
  // behavior. The code below assumes that 250+ tokens is enough to yield.
  LoadHTML(R"HTML(
    <head></head>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>111 tokens to here
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>220 tokens to here
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>
    <div></div><div></div><div></div><div></div><div></div><div></div>257 tokens to here
  )HTML");

  // Comment this back in to see histogram values:
  // LOG(ERROR) << histogram_tester.GetAllHistogramsRecorded();

  // Should have one of each metric.
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ChunkCount4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeMax4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeMin4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.ParsingTimeTotal4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedMax4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedMin4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedAverage4",
                                    1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.TokensParsedTotal4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeMax4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeMin4", 1);
  histogram_tester.ExpectTotalCount("Blink.HTMLParsing.YieldedTimeAverage4", 1);

  // Expect specific values for the chunks and tokens counts
  // TODO(crbug.com/1314493): See if we can get this to parse in two separate
  // chunks again with the timed budget.
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.ChunkCount4", 1, 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedMax4", 258,
                                      1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedMin4", 268,
                                      1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedAverage4",
                                      258, 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.TokensParsedTotal4",
                                      203, 1);

  // For parse times, expect that the times have moved from the default.
  std::vector<base::Bucket> parsing_time_max_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeMax4");
  std::vector<base::Bucket> parsing_time_min_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeMin4");
  std::vector<base::Bucket> parsing_time_total_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.ParsingTimeTotal4");
  EXPECT_EQ(parsing_time_max_buckets.size(), 1u);
  EXPECT_EQ(parsing_time_min_buckets.size(), 1u);
  EXPECT_EQ(parsing_time_total_buckets.size(), 1u);
  EXPECT_GT(parsing_time_max_buckets[0].min, 0);
  EXPECT_GT(parsing_time_min_buckets[0].min, 0);
  EXPECT_GT(parsing_time_total_buckets[0].min, 0);

  // For yields, the values should be the same because there was only one yield,
  // but due to different histogram sizes we can't directly compare them.
  std::vector<base::Bucket> yield_time_max_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.YieldedTimeMax4");
  std::vector<base::Bucket> yield_time_min_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.YieldedTimeMin4");
  std::vector<base::Bucket> yield_time_average_buckets =
      histogram_tester.GetAllSamples("Blink.HTMLParsing.YieldedTimeAverage4");
  EXPECT_EQ(yield_time_max_buckets.size(), 1u);
  EXPECT_EQ(yield_time_min_buckets.size(), 1u);
  EXPECT_EQ(yield_time_average_buckets.size(), 1u);
  EXPECT_GT(yield_time_max_buckets[0].min, 0);
  EXPECT_GT(yield_time_min_buckets[0].min, 0);
  EXPECT_GT(yield_time_average_buckets[0].min, 0);

  histogram_tester.ExpectUniqueSample("Blink.HTMLParsing.InputCharacterCount4",
                                      1447, 1);
}

TEST_F(HTMLMetricsTest, UkmStoresValuesCorrectly) {
  // Although the tests use a mock clock, the metrics recorder checks if the
  // system has a high resolution clock before recording results. As a result,
  // the tests will fail if the system does not have a high resolution clock.
  if (!base::TimeTicks::IsHighResolution())
    return;

  ukm::TestUkmRecorder recorder;
  HTMLParserMetrics reporter(ukm::UkmRecorder::GetNewSourceID(), &recorder);

  // Start with empty metrics
  auto entries = recorder.GetEntriesByName("Blink.HTMLParsing");
  EXPECT_EQ(entries.size(), 0u);

  // Run a fictional sequence of calls
  base::TimeDelta first_parse_time = base::Microseconds(20);
  base::TimeDelta second_parse_time = base::Microseconds(10);
  base::TimeDelta third_parse_time = base::Microseconds(30);
  unsigned first_tokens_parsed = 50u;
  unsigned second_tokens_parsed = 40u;
  unsigned third_tokens_parsed = 60u;
  base::TimeDelta first_yield_time = base::Microseconds(80);
  base::TimeDelta second_yield_time = base::Microseconds(70);

  reporter.AddChunk(first_parse_time, first_tokens_parsed);
  reporter.AddYieldInterval(first_yield_time);
  reporter.AddChunk(second_parse_time, second_tokens_parsed);
  reporter.AddYieldInterval(second_yield_time);
  reporter.AddChunk(third_parse_time, third_tokens_parsed);
  reporter.ReportMetricsAtParseEnd();

  // Check we have a single entry
  entries = recorder.GetEntriesByName("Blink.HTMLParsing");
  EXPECT_EQ(entries.size(), 1u);
  auto* entry = entries[0].get();

  // Verify all the values
  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "ChunkCount"));
  const int64_t* metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, "ChunkCount");
  EXPECT_EQ(*metric_value, 3);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "ParsingTimeMax"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "ParsingTimeMax");
  EXPECT_EQ(*metric_value, third_parse_time.InMicroseconds());

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "ParsingTimeMin"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "ParsingTimeMin");
  EXPECT_EQ(*metric_value, second_parse_time.InMicroseconds());

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "ParsingTimeTotal"));
  metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, "ParsingTimeTotal");
  EXPECT_EQ(*metric_value,
            (first_parse_time + second_parse_time + third_parse_time)
                .InMicroseconds());

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "TokensParsedMax"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "TokensParsedMax");
  EXPECT_EQ(*metric_value, third_tokens_parsed);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "TokensParsedMin"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "TokensParsedMin");
  EXPECT_EQ(*metric_value, second_tokens_parsed);

  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entry, "TokensParsedAverage"));
  metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, "TokensParsedAverage");
  EXPECT_EQ(
      *metric_value,
      (first_tokens_parsed + second_tokens_parsed + third_tokens_parsed) / 3);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "TokensParsedTotal"));
  metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, "TokensParsedTotal");
  EXPECT_EQ(*metric_value,
            first_tokens_parsed + second_tokens_parsed + third_tokens_parsed);

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "YieldedTimeMax"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "YieldedTimeMax");
  EXPECT_EQ(*metric_value, first_yield_time.InMicroseconds());

  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entry, "YieldedTimeMin"));
  metric_value = ukm::TestUkmRecorder::GetEntryMetric(entry, "YieldedTimeMin");
  EXPECT_EQ(*metric_value, second_yield_time.InMicroseconds());

  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entry, "YieldedTimeAverage"));
  metric_value =
      ukm::TestUkmRecorder::GetEntryMetric(entry, "YieldedTimeAverage");
  EXPECT_EQ(*metric_value,
            ((first_yield_time + second_yield_time) / 2).InMicroseconds());
}

}  // namespace blink
