// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_network_change_observer.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "net/base/network_change_notifier.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_target_type.h"
#include "net/reporting/reporting_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class ReportingNetworkChangeObserverTest : public ReportingTestBase {
 protected:
  void SimulateNetworkChange() {
    // TODO: Need to SetTestNotificationsOnly(true) to keep things from flaking,
    // but have to figure out how to do that before NCN is created or how to
    // recreate NCN.
    NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        NetworkChangeNotifier::CONNECTION_NONE);
    base::RunLoop().RunUntilIdle();
    NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
        NetworkChangeNotifier::CONNECTION_WIFI);
    base::RunLoop().RunUntilIdle();
  }

  void SetEndpoint() {
    ASSERT_TRUE(SetEndpointInCache(kGroupKey_, kEndpoint_,
                                   base::Time::Now() + base::Days(7)));
  }

  size_t report_count() {
    std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
    cache()->GetReports(&reports);
    return reports.size();
  }

  const std::optional<base::UnguessableToken> kReportingSource_ = std::nullopt;
  const NetworkAnonymizationKey kNak_;
  const GURL kUrl_ = GURL("https://origin/path");
  const url::Origin kOrigin_ = url::Origin::Create(kUrl_);
  const GURL kEndpoint_ = GURL("https://endpoint/");
  const std::string kUserAgent_ = "Mozilla/1.0";
  const std::string kGroup_ = "group";
  const std::string kType_ = "default";
  const ReportingEndpointGroupKey kGroupKey_ =
      ReportingEndpointGroupKey(NetworkAnonymizationKey(),
                                kOrigin_,
                                kGroup_,
                                ReportingTargetType::kDeveloper);
};

TEST_F(ReportingNetworkChangeObserverTest, ClearNothing) {
  ReportingPolicy new_policy = policy();
  new_policy.persist_reports_across_network_changes = true;
  new_policy.persist_clients_across_network_changes = true;
  UsePolicy(new_policy);

  cache()->AddReport(kReportingSource_, kNak_, kUrl_, kUserAgent_, kGroup_,
                     kType_, base::Value::Dict(), 0, tick_clock()->NowTicks(),
                     0, ReportingTargetType::kDeveloper);
  SetEndpoint();
  ASSERT_EQ(1u, report_count());
  ASSERT_EQ(1u, cache()->GetEndpointCount());

  SimulateNetworkChange();

  EXPECT_EQ(1u, report_count());
  EXPECT_EQ(1u, cache()->GetEndpointCount());
}

TEST_F(ReportingNetworkChangeObserverTest, ClearReports) {
  ReportingPolicy new_policy = policy();
  new_policy.persist_reports_across_network_changes = false;
  new_policy.persist_clients_across_network_changes = true;
  UsePolicy(new_policy);

  cache()->AddReport(kReportingSource_, kNak_, kUrl_, kUserAgent_, kGroup_,
                     kType_, base::Value::Dict(), 0, tick_clock()->NowTicks(),
                     0, ReportingTargetType::kDeveloper);
  SetEndpoint();
  ASSERT_EQ(1u, report_count());
  ASSERT_EQ(1u, cache()->GetEndpointCount());

  SimulateNetworkChange();

  EXPECT_EQ(0u, report_count());
  EXPECT_EQ(1u, cache()->GetEndpointCount());
}

TEST_F(ReportingNetworkChangeObserverTest, ClearClients) {
  ReportingPolicy new_policy = policy();
  new_policy.persist_reports_across_network_changes = true;
  new_policy.persist_clients_across_network_changes = false;
  UsePolicy(new_policy);

  cache()->AddReport(kReportingSource_, kNak_, kUrl_, kUserAgent_, kGroup_,
                     kType_, base::Value::Dict(), 0, tick_clock()->NowTicks(),
                     0, ReportingTargetType::kDeveloper);
  SetEndpoint();
  ASSERT_EQ(1u, report_count());
  ASSERT_EQ(1u, cache()->GetEndpointCount());

  SimulateNetworkChange();

  EXPECT_EQ(1u, report_count());
  EXPECT_EQ(0u, cache()->GetEndpointCount());
}

TEST_F(ReportingNetworkChangeObserverTest, ClearReportsAndClients) {
  ReportingPolicy new_policy = policy();
  new_policy.persist_reports_across_network_changes = false;
  new_policy.persist_clients_across_network_changes = false;
  UsePolicy(new_policy);

  cache()->AddReport(kReportingSource_, kNak_, kUrl_, kUserAgent_, kGroup_,
                     kType_, base::Value::Dict(), 0, tick_clock()->NowTicks(),
                     0, ReportingTargetType::kDeveloper);
  SetEndpoint();
  ASSERT_EQ(1u, report_count());
  ASSERT_EQ(1u, cache()->GetEndpointCount());

  SimulateNetworkChange();

  EXPECT_EQ(0u, report_count());
  EXPECT_EQ(0u, cache()->GetEndpointCount());
}

}  // namespace
}  // namespace net
