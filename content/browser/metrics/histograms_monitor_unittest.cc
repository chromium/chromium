// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/metrics/histograms_monitor.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr std::string kEmptyFilter = "";

}  // namespace

class HistogramsMonitorTest : public testing::Test {
 public:
  HistogramsMonitorTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(HistogramsMonitorTest, StartMonitoringThenGetDiff) {
  base::StatisticsRecorder::ForgetHistogramForTesting("MonitorHistogram1");
  base::StatisticsRecorder::ForgetHistogramForTesting("MonitorHistogram2");
  base::HistogramBase* histogram1 = base::Histogram::FactoryGet(
      "MonitorHistogram1", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram1->Add(30);
  HistogramsMonitor monitor;
  monitor.StartMonitoring();

  // Get diff immediately should return nothing.
  base::Value::List diff = monitor.GetDiff(kEmptyFilter);
  ASSERT_EQ(diff.size(), 0ull);

  // Add more data to histogram.
  histogram1->Add(30);
  histogram1->Add(40);

  diff = monitor.GetDiff(kEmptyFilter);
  ASSERT_EQ(diff.size(), 1ull);
  std::string* header1 = diff[0].GetDict().FindString("header");
  EXPECT_EQ(*header1,
            "Histogram: MonitorHistogram1 recorded 2 samples, mean = 35.0");

  // Add another histogram
  base::HistogramBase* histogram2 = base::Histogram::FactoryGet(
      "MonitorHistogram2", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram2->Add(50);
  diff = monitor.GetDiff(kEmptyFilter);
  ASSERT_EQ(diff.size(), 2ull);
  std::string* header2 = diff[1].GetDict().FindString("header");
  EXPECT_EQ(*header2,
            "Histogram: MonitorHistogram2 recorded 1 samples, mean = 50.0");
}

TEST_F(HistogramsMonitorTest, StartMonitoringWithQueryThenGetDiff) {
  base::StatisticsRecorder::ForgetHistogramForTesting("MonitorHistogram1");
  base::StatisticsRecorder::ForgetHistogramForTesting("MonitorHistogram2");
  base::HistogramBase* histogram1 = base::Histogram::FactoryGet(
      "MonitorHistogram1", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram1->Add(30);
  HistogramsMonitor monitor;
  monitor.StartMonitoring();

  base::HistogramBase* histogram2 = base::Histogram::FactoryGet(
      "MonitorHistogram2", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram2->Add(50);
  base::Value::List diff = monitor.GetDiff("MonitorHistogram1");
  ASSERT_EQ(diff.size(), 0ull);
}

TEST_F(HistogramsMonitorTest, CaseInsensitiveQuery) {
  base::StatisticsRecorder::ForgetHistogramForTesting("MonitorHistogram1");
  base::StatisticsRecorder::ForgetHistogramForTesting("MonitorHistogram2");

  base::HistogramBase* histogram1 = base::Histogram::FactoryGet(
      "MonitorHistogram1", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram1->Add(30);

  HistogramsMonitor monitor;
  monitor.StartMonitoring();

  base::HistogramBase* histogram2 = base::Histogram::FactoryGet(
      "MonitorHistogram2", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram2->Add(50);

  // The query shouldn't match "MonitorHistogram2".
  base::Value::List diff = monitor.GetDiff("histogram1");
  ASSERT_EQ(diff.size(), 0ull);

  histogram1->Add(10);

  // The query should match "MonitorHistogram1", since it's case insensitive.
  diff = monitor.GetDiff("histogram1");
  ASSERT_EQ(diff.size(), 1ull);
  std::string* header2 = diff[0].GetDict().FindString("header");
  EXPECT_EQ(*header2,
            "Histogram: MonitorHistogram1 recorded 1 samples, mean = 10.0");
}

TEST_F(HistogramsMonitorTest, MonitoringBaselineDoesntChangeWithFilter) {
  base::StatisticsRecorder::ForgetHistogramForTesting("MonitorHistogram1");
  base::StatisticsRecorder::ForgetHistogramForTesting("MonitorHistogram2");
  base::HistogramBase* histogram1 = base::Histogram::FactoryGet(
      "MonitorHistogram1", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram1->Add(30);

  base::HistogramBase* histogram2 = base::Histogram::FactoryGet(
      "MonitorHistogram2", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram2->Add(50);
  HistogramsMonitor monitor;

  monitor.StartMonitoring();

  // Get diff immediately should return nothing.
  base::Value::List diff = monitor.GetDiff(kEmptyFilter);
  ASSERT_EQ(diff.size(), 0ull);

  // Add more data to histogram.
  histogram1->Add(30);
  histogram1->Add(40);

  histogram2->Add(20);

  // Filter the query to only return "MonitorHistogram1", the baseline should
  // not change.
  diff = monitor.GetDiff("MonitorHistogram1");
  ASSERT_EQ(diff.size(), 1ull);
  std::string* header1 = diff[0].GetDict().FindString("header");
  EXPECT_EQ(*header1,
            "Histogram: MonitorHistogram1 recorded 2 samples, mean = 35.0");


  // Add more data to histogram2 and expect both samples after baseline to be
  // returned.
  histogram2->Add(10);
  diff = monitor.GetDiff("MonitorHistogram2");
  ASSERT_EQ(diff.size(), 1ull);
  std::string* header2 = diff[0].GetDict().FindString("header");
  EXPECT_EQ(*header2,
            "Histogram: MonitorHistogram2 recorded 2 samples, mean = 15.0");

  // Add more data to histogram1 and expect that both histograms are still
  // returned for empty filter as they changed compared to the baseline.
  histogram1->Add(50);
  diff = monitor.GetDiff(kEmptyFilter);
  ASSERT_EQ(diff.size(), 2ull);

  std::vector<std::string> headers;
  headers.push_back(*diff[0].GetDict().FindString("header"));
  headers.push_back(*diff[1].GetDict().FindString("header"));
  EXPECT_THAT(headers,
              testing::UnorderedElementsAre(
                  "Histogram: MonitorHistogram1 recorded 3 samples, "
                  "mean = 40.0",
                  "Histogram: MonitorHistogram2 recorded 2 samples, "
                  "mean = 15.0"));
}

}  // namespace content
