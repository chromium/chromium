// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/highest_pmf_reporter.h"

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class MockHighestPmfReporter : public HighestPmfReporter {
 public:
  MockHighestPmfReporter(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner_for_testing,
      const base::TickClock* clock)
      : HighestPmfReporter(task_runner_for_testing, clock),
        navigation_started_(false) {}
  ~MockHighestPmfReporter() override = default;

  void NotifyNavigationStart() { navigation_started_ = true; }

  const std::vector<double>& GetReportedHighestPmf() const {
    return reported_highest_pmf_;
  }

  const std::vector<double>& GetReportedPeakRss() const {
    return reported_peak_rss_;
  }

  const std::vector<unsigned>& GetReportedWebpageCount() const {
    return reported_webpage_count_;
  }

  int GetReportCount() const { return report_count_; }

 private:
  void ReportMetrics() override {
    reported_highest_pmf_.push_back(current_highest_pmf_);
    reported_peak_rss_.push_back(peak_resident_bytes_at_current_highest_pmf_);
    reported_webpage_count_.push_back(webpage_counts_at_current_highest_pmf_);
  }

  bool HasNavigationAlreadyStarted() const override {
    return navigation_started_;
  }

  std::vector<double> reported_highest_pmf_;
  std::vector<double> reported_peak_rss_;
  std::vector<unsigned> reported_webpage_count_;
  bool navigation_started_;
};

namespace peak_memory_reporter_test {

using testing::_;

// Mock that allows setting mock memory usage.
class MockMemoryUsageMonitor : public MemoryUsageMonitor {
 public:
  MockMemoryUsageMonitor(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner_for_testing,
      const base::TickClock* clock)
      : MemoryUsageMonitor(task_runner_for_testing, clock) {
    memset(&mock_memory_usage_, 0, sizeof(mock_memory_usage_));
  }
  ~MockMemoryUsageMonitor() override = default;

  MemoryUsage GetCurrentMemoryUsage() override { return mock_memory_usage_; }

  void SetPrivateFootprintBytes(double private_footprint_bytes) {
    mock_memory_usage_.private_footprint_bytes = private_footprint_bytes;
  }

  void SetPeakResidentBytes(double peak_resident_bytes) {
    mock_memory_usage_.peak_resident_bytes = peak_resident_bytes;
  }

  // Insert fake NonOrdinaryPage into Page::OrdinaryPages().
  void SetOrdinaryPageCount(unsigned page_count) {
    DCHECK_GT(page_count, 0U);
    while (dummy_pages_.size() < page_count) {
      Page* page = CreateDummyPage();
      DCHECK(page);
      dummy_pages_.push_back(page);
    }
    if (Page::OrdinaryPages().size() > 1U) {
      for (unsigned i = 0; i < dummy_pages_.size(); i++) {
        if (Page::OrdinaryPages().Contains(dummy_pages_.at(i).Get()))
          Page::OrdinaryPages().erase(dummy_pages_.at(i).Get());
      }
    }
    DCHECK_EQ(Page::OrdinaryPages().size(), 1U);

    std::vector<Persistent<Page>>::iterator it = dummy_pages_.begin();
    while (Page::OrdinaryPages().size() < page_count) {
      DCHECK(it != dummy_pages_.end());
      Page::OrdinaryPages().insert(it->Get());
      it++;
    }
  }

 private:
  MockMemoryUsageMonitor() = delete;

  Page* CreateDummyPage() {
    Page::PageClients page_clients;
    FillWithEmptyClients(page_clients);
    return Page::CreateNonOrdinary(page_clients);
  }

  MemoryUsage mock_memory_usage_;
  std::vector<Persistent<Page>> dummy_pages_;
};

class HighestPmfReporterTest : public PageTestBase {
 public:
  HighestPmfReporterTest() = default;

  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
    memory_usage_monitor_.reset(new MockMemoryUsageMonitor(
        test_task_runner_, test_task_runner_->GetMockTickClock()));
    MemoryUsageMonitor::SetInstanceForTesting(memory_usage_monitor_.get());
    reporter_.reset(new MockHighestPmfReporter(
        test_task_runner_, test_task_runner_->GetMockTickClock()));
    PageTestBase::SetUp();
  }

  void TearDown() override {
    PageTestBase::TearDown();
    MemoryUsageMonitor::SetInstanceForTesting(nullptr);
    memory_usage_monitor_.reset();
    reporter_.reset();
  }

  void AdvanceClock(base::TimeDelta delta) {
    test_task_runner_->FastForwardBy(delta);
  }

  void AdvanceClockTo(base::TimeTicks time) {
    base::TimeDelta delta = time - NowTicks();
    if (delta.is_zero())
      return;
    AdvanceClock(delta);
  }

  base::TimeTicks NowTicks() const {
    return test_task_runner_->GetMockTickClock()->NowTicks();
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  std::unique_ptr<MockMemoryUsageMonitor> memory_usage_monitor_;
  std::unique_ptr<MockHighestPmfReporter> reporter_;
};

TEST_F(HighestPmfReporterTest, ReportNoMetricBeforeNavigationStart) {
  EXPECT_TRUE(memory_usage_monitor_->TimerIsActive());
  Page::OrdinaryPages().insert(&GetPage());

  memory_usage_monitor_->SetPrivateFootprintBytes(1000.0);
  AdvanceClock(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(0, reporter_->GetReportCount());
  EXPECT_EQ(0U, reporter_->GetReportedHighestPmf().size());
  EXPECT_EQ(0U, reporter_->GetReportedPeakRss().size());
}

TEST_F(HighestPmfReporterTest, ReportMetric) {
  EXPECT_TRUE(memory_usage_monitor_->TimerIsActive());
  Page::OrdinaryPages().insert(&GetPage());
  AdvanceClock(base::TimeDelta::FromSeconds(1));

  // PMF, PeakRSS and PageCount at specified TimeSinceNavigation.
  static const struct {
    base::TimeDelta time_since_navigation;
    double pmf;
    double peak_rss;
    unsigned page_count;
  } time_pmf_rss_table[] = {
      {base::TimeDelta::FromMinutes(0), 1000.0, 1200.0, 1},
      {base::TimeDelta::FromMinutes(1), 750.0, 900.0, 1},
      {base::TimeDelta::FromSeconds(80), 750.0, 1000.0, 4},   // t=1min 20sec
      {base::TimeDelta::FromSeconds(90), 1100.0, 1500.0, 2},  // t=1min 30sec
      {base::TimeDelta::FromMinutes(2), 900.0, 1000.0, 1},
      {base::TimeDelta::FromMinutes(4), 900.0, 1000.0, 1},
      {base::TimeDelta::FromMinutes(5), 1500.0, 2000.0, 3},
      {base::TimeDelta::FromMinutes(7), 800.0, 900.0, 1},
      {base::TimeDelta::FromMinutes(8), 900.0, 1000.0, 1},
      {base::TimeDelta::FromMinutes(16), 900.0, 1000.0, 1},
  };

  base::TimeTicks navigation_start_time = NowTicks();
  reporter_->NotifyNavigationStart();

  for (const auto& item : time_pmf_rss_table) {
    AdvanceClockTo(navigation_start_time + item.time_since_navigation);
    // PMF, PeakRSS, Webpage count are captured at next OnMemoryPing.
    memory_usage_monitor_->SetPrivateFootprintBytes(item.pmf);
    memory_usage_monitor_->SetPeakResidentBytes(item.peak_rss);
    memory_usage_monitor_->SetOrdinaryPageCount(item.page_count);
  }
  AdvanceClockTo(navigation_start_time + base::TimeDelta::FromMinutes(17));

  EXPECT_EQ(4, reporter_->GetReportCount());
  EXPECT_EQ(4U, reporter_->GetReportedHighestPmf().size());
  EXPECT_NEAR(1100.0, reporter_->GetReportedHighestPmf().at(0), 0.001);
  EXPECT_NEAR(900.0, reporter_->GetReportedHighestPmf().at(1), 0.001);
  EXPECT_NEAR(1500.0, reporter_->GetReportedHighestPmf().at(2), 0.001);
  EXPECT_NEAR(900.0, reporter_->GetReportedHighestPmf().at(3), 0.001);

  EXPECT_EQ(4U, reporter_->GetReportedPeakRss().size());
  EXPECT_NEAR(1500.0, reporter_->GetReportedPeakRss().at(0), 0.001);
  EXPECT_NEAR(1000.0, reporter_->GetReportedPeakRss().at(1), 0.001);
  EXPECT_NEAR(2000.0, reporter_->GetReportedPeakRss().at(2), 0.001);
  EXPECT_NEAR(1000.0, reporter_->GetReportedPeakRss().at(3), 0.001);

  EXPECT_EQ(4U, reporter_->GetReportedWebpageCount().size());
  EXPECT_EQ(2U, reporter_->GetReportedWebpageCount().at(0));
  EXPECT_EQ(1U, reporter_->GetReportedWebpageCount().at(1));
  EXPECT_EQ(3U, reporter_->GetReportedWebpageCount().at(2));
  EXPECT_EQ(1U, reporter_->GetReportedWebpageCount().at(3));
}

TEST_F(HighestPmfReporterTest, TestReportTiming) {
  EXPECT_TRUE(memory_usage_monitor_->TimerIsActive());
  Page::OrdinaryPages().insert(&GetPage());

  memory_usage_monitor_->SetPrivateFootprintBytes(1000.0);

  base::TimeTicks navigation_start_time = NowTicks();
  reporter_->NotifyNavigationStart();
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  // Now ReportMetrics task is posted with 2minutes delay.
  // The task will be executed at "navigation_start_time + 2min + 1sec."

  EXPECT_EQ(0, reporter_->GetReportCount());
  AdvanceClockTo(navigation_start_time + base::TimeDelta::FromMinutes(2));
  EXPECT_EQ(0, reporter_->GetReportCount());
  // ReportMetrics task is executed and next ReportMetrics task is posted.
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, reporter_->GetReportCount());

  AdvanceClockTo(navigation_start_time + base::TimeDelta::FromMinutes(4));
  EXPECT_EQ(1, reporter_->GetReportCount());
  // ReportMetrics task is executed and next ReportMetrics task is posted.
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(2, reporter_->GetReportCount());

  AdvanceClockTo(navigation_start_time + base::TimeDelta::FromMinutes(8));
  EXPECT_EQ(2, reporter_->GetReportCount());
  // ReportMetrics task is executed and next ReportMetrics task is posted.
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(3, reporter_->GetReportCount());

  AdvanceClockTo(navigation_start_time + base::TimeDelta::FromMinutes(16));
  EXPECT_EQ(3, reporter_->GetReportCount());
  // ReportMetrics task is executed and next ReportMetrics task is posted.
  AdvanceClock(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(4, reporter_->GetReportCount());
}

}  // namespace peak_memory_reporter_test
}  // namespace blink
