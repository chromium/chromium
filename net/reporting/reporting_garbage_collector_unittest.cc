// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_garbage_collector.h"

#include <string>

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {
namespace {

class ReportingGarbageCollectorTest : public ReportingTestBase {
 protected:
  size_t report_count() {
    std::vector<const ReportingReport*> reports;
    cache()->GetReports(&reports);
    return reports.size();
  }

  const absl::optional<base::UnguessableToken> kReportingSource_ =
      absl::nullopt;
  const NetworkIsolationKey kNik_;
  const GURL kUrl_ = GURL("https://origin/path");
  const std::string kUserAgent_ = "Mozilla/1.0";
  const std::string kGroup_ = "group";
  const std::string kType_ = "default";
};

// Make sure the garbage collector is actually present in the context.
TEST_F(ReportingGarbageCollectorTest, Created) {
  EXPECT_NE(nullptr, garbage_collector());
}

// Make sure that the garbage collection timer is started and stopped correctly.
TEST_F(ReportingGarbageCollectorTest, Timer) {
  EXPECT_FALSE(garbage_collection_timer()->IsRunning());

  cache()->AddReport(kReportingSource_, kNik_, kUrl_, kUserAgent_, kGroup_,
                     kType_, std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  EXPECT_TRUE(garbage_collection_timer()->IsRunning());

  garbage_collection_timer()->Fire();

  EXPECT_FALSE(garbage_collection_timer()->IsRunning());
}

TEST_F(ReportingGarbageCollectorTest, Report) {
  cache()->AddReport(kReportingSource_, kNik_, kUrl_, kUserAgent_, kGroup_,
                     kType_, std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);
  garbage_collection_timer()->Fire();

  EXPECT_EQ(1u, report_count());
}

TEST_F(ReportingGarbageCollectorTest, ExpiredReport) {
  cache()->AddReport(kReportingSource_, kNik_, kUrl_, kUserAgent_, kGroup_,
                     kType_, std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);
  tick_clock()->Advance(2 * policy().max_report_age);
  garbage_collection_timer()->Fire();

  EXPECT_EQ(0u, report_count());
}

TEST_F(ReportingGarbageCollectorTest, FailedReport) {
  cache()->AddReport(kReportingSource_, kNik_, kUrl_, kUserAgent_, kGroup_,
                     kType_, std::make_unique<base::DictionaryValue>(), 0,
                     tick_clock()->NowTicks(), 0);

  std::vector<const ReportingReport*> reports;
  cache()->GetReports(&reports);
  for (int i = 0; i < policy().max_report_attempts; i++) {
    cache()->IncrementReportsAttempts(reports);
  }

  garbage_collection_timer()->Fire();

  EXPECT_EQ(0u, report_count());
}

}  // namespace
}  // namespace net
