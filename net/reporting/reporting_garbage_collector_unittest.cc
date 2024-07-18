// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_garbage_collector.h"

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/mock_timer.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_target_type.h"
#include "net/reporting/reporting_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class ReportingGarbageCollectorTest : public ReportingTestBase {
 protected:
  size_t report_count() {
    std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
    cache()->GetReports(&reports);
    return reports.size();
  }

  const std::optional<base::UnguessableToken> kReportingSource_ =
      base::UnguessableToken::Create();
  const NetworkAnonymizationKey kNak_;
  const IsolationInfo kIsolationInfo_;
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

  cache()->AddReport(std::nullopt, kNak_, kUrl_, kUserAgent_, kGroup_, kType_,
                     base::Value::Dict(), 0, tick_clock()->NowTicks(), 0,
                     ReportingTargetType::kDeveloper);

  EXPECT_TRUE(garbage_collection_timer()->IsRunning());

  garbage_collection_timer()->Fire();

  EXPECT_FALSE(garbage_collection_timer()->IsRunning());
}

TEST_F(ReportingGarbageCollectorTest, Report) {
  cache()->AddReport(std::nullopt, kNak_, kUrl_, kUserAgent_, kGroup_, kType_,
                     base::Value::Dict(), 0, tick_clock()->NowTicks(), 0,
                     ReportingTargetType::kDeveloper);
  garbage_collection_timer()->Fire();

  EXPECT_EQ(1u, report_count());
}

TEST_F(ReportingGarbageCollectorTest, ExpiredReport) {
  cache()->AddReport(std::nullopt, kNak_, kUrl_, kUserAgent_, kGroup_, kType_,
                     base::Value::Dict(), 0, tick_clock()->NowTicks(), 0,
                     ReportingTargetType::kDeveloper);
  tick_clock()->Advance(2 * policy().max_report_age);
  garbage_collection_timer()->Fire();

  EXPECT_EQ(0u, report_count());
}

TEST_F(ReportingGarbageCollectorTest, FailedReport) {
  cache()->AddReport(std::nullopt, kNak_, kUrl_, kUserAgent_, kGroup_, kType_,
                     base::Value::Dict(), 0, tick_clock()->NowTicks(), 0,
                     ReportingTargetType::kDeveloper);

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  for (int i = 0; i < policy().max_report_attempts; i++) {
    cache()->IncrementReportsAttempts(reports);
  }

  garbage_collection_timer()->Fire();

  EXPECT_EQ(0u, report_count());
}

TEST_F(ReportingGarbageCollectorTest, ExpiredSource) {
  ReportingEndpointGroupKey group_key(kNak_, kReportingSource_,
                                      url::Origin::Create(kUrl_), kGroup_,
                                      ReportingTargetType::kDeveloper);
  cache()->SetV1EndpointForTesting(group_key, *kReportingSource_,
                                   kIsolationInfo_, kUrl_);

  // Mark the source as expired. The source should be removed as soon as
  // garbage collection runs, as there are no queued reports for it.
  cache()->SetExpiredSource(*kReportingSource_);

  // Before garbage collection, the endpoint should still exist.
  EXPECT_EQ(1u, cache()->GetReportingSourceCountForTesting());
  EXPECT_TRUE(cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup_));

  // Fire garbage collection. The endpoint configuration should be removed.
  garbage_collection_timer()->Fire();
  EXPECT_EQ(0u, cache()->GetReportingSourceCountForTesting());
  EXPECT_FALSE(cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup_));
}

TEST_F(ReportingGarbageCollectorTest, ExpiredSourceWithPendingReports) {
  ReportingEndpointGroupKey group_key(kNak_, kReportingSource_,
                                      url::Origin::Create(kUrl_), kGroup_,
                                      ReportingTargetType::kDeveloper);
  cache()->SetV1EndpointForTesting(group_key, *kReportingSource_,
                                   kIsolationInfo_, kUrl_);
  cache()->AddReport(kReportingSource_, kNak_, kUrl_, kUserAgent_, kGroup_,
                     kType_, base::Value::Dict(), 0, tick_clock()->NowTicks(),
                     0, ReportingTargetType::kDeveloper);
  // Mark the source as expired. The source data should be removed as soon as
  // all reports are delivered.
  cache()->SetExpiredSource(*kReportingSource_);

  // Even though expired, GC should not delete the source as there is still a
  // queued report.
  garbage_collection_timer()->Fire();
  EXPECT_EQ(1u, report_count());
  EXPECT_EQ(1u, cache()->GetReportingSourceCountForTesting());
  EXPECT_TRUE(cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup_));

  // Deliver report.
  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  cache()->RemoveReports(reports);
  EXPECT_EQ(0u, report_count());

  // Fire garbage collection again. The endpoint configuration should be
  // removed.
  garbage_collection_timer()->Fire();
  EXPECT_EQ(0u, cache()->GetReportingSourceCountForTesting());
  EXPECT_FALSE(cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup_));
}

}  // namespace
}  // namespace net
