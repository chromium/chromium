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
  monitor.StartMonitoring(/*query=*/"");

  // Get diff immediately should return nothing.
  base::Value::List diff = monitor.GetDiff();
  ASSERT_EQ(diff.size(), 0ull);

  // Add more data to histogram.
  histogram1->Add(30);
  histogram1->Add(40);

  diff = monitor.GetDiff();
  ASSERT_EQ(diff.size(), 1ull);
  std::string* header1 = diff[0].GetDict().FindString("header");
  EXPECT_EQ(*header1,
            "Histogram: MonitorHistogram1 recorded 2 samples, mean = 35.0");

  // Add another histogram
  base::HistogramBase* histogram2 = base::Histogram::FactoryGet(
      "MonitorHistogram2", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram2->Add(50);
  diff = monitor.GetDiff();
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
  monitor.StartMonitoring(/*query=*/"MonitorHistogram1");

  base::HistogramBase* histogram2 = base::Histogram::FactoryGet(
      "MonitorHistogram2", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram2->Add(50);
  base::Value::List diff = monitor.GetDiff();
  ASSERT_EQ(diff.size(), 0ull);
}

TEST_F(HistogramsMonitorTest, CaseInsensitiveQuery) {
  base::StatisticsRecorder::ForgetHistogramForTesting("MonitorHistogram1");
  base::StatisticsRecorder::ForgetHistogramForTesting("MonitorHistogram2");

  base::HistogramBase* histogram1 = base::Histogram::FactoryGet(
      "MonitorHistogram1", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram1->Add(30);

  HistogramsMonitor monitor;
  monitor.StartMonitoring(/*query=*/"histogram1");

  base::HistogramBase* histogram2 = base::Histogram::FactoryGet(
      "MonitorHistogram2", 1, 1000, 10, base::HistogramBase::kNoFlags);
  histogram2->Add(50);

  // The query shouldn't match "MonitorHistogram2".
  base::Value::List diff = monitor.GetDiff();
  ASSERT_EQ(diff.size(), 0ull);

  histogram1->Add(10);

  // The query should match "MonitorHistogram1", since it's case insensitive.
  diff = monitor.GetDiff();
  ASSERT_EQ(diff.size(), 1ull);
  std::string* header2 = diff[0].GetDict().FindString("header");
  EXPECT_EQ(*header2,
            "Histogram: MonitorHistogram1 recorded 1 samples, mean = 10.0");
}

}  // namespace content
