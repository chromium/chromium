// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_browsing_data_remover.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_tick_clock.h"
#include "net/base/network_anonymization_key.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_target_type.h"
#include "net/reporting/reporting_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class ReportingBrowsingDataRemoverTest : public ReportingTestBase {
 protected:
  void RemoveBrowsingData(bool remove_reports,
                          bool remove_clients,
                          std::string host) {
    uint64_t data_type_mask = 0;
    if (remove_reports)
      data_type_mask |= ReportingBrowsingDataRemover::DATA_TYPE_REPORTS;
    if (remove_clients)
      data_type_mask |= ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS;

    if (!host.empty()) {
      base::RepeatingCallback<bool(const url::Origin&)> origin_filter =
          base::BindRepeating(&ReportingBrowsingDataRemoverTest::HostIs, host);
      ReportingBrowsingDataRemover::RemoveBrowsingData(cache(), data_type_mask,
                                                       origin_filter);
    } else {
      ReportingBrowsingDataRemover::RemoveAllBrowsingData(cache(),
                                                          data_type_mask);
    }
  }

  // TODO(chlily): Take NAK.
  void AddReport(const GURL& url) {
    cache()->AddReport(std::nullopt, NetworkAnonymizationKey(), url,
                       kUserAgent_, kGroup_, kType_, base::Value::Dict(), 0,
                       tick_clock()->NowTicks(), 0,
                       ReportingTargetType::kDeveloper);
  }

  // TODO(chlily): Take NAK.
  void SetEndpoint(const url::Origin& origin) {
    SetEndpointInCache(
        ReportingEndpointGroupKey(NetworkAnonymizationKey(), origin, kGroup_,
                                  ReportingTargetType::kDeveloper),
        kEndpoint_, base::Time::Now() + base::Days(7));
  }

  static bool HostIs(std::string host, const url::Origin& origin) {
    return origin.host() == host;
  }

  size_t report_count() {
    std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
    cache()->GetReports(&reports);
    return reports.size();
  }

  const GURL kUrl1_ = GURL("https://origin1/path");
  const GURL kUrl2_ = GURL("https://origin2/path");
  const url::Origin kOrigin1_ = url::Origin::Create(kUrl1_);
  const url::Origin kOrigin2_ = url::Origin::Create(kUrl2_);
  const GURL kEndpoint_ = GURL("https://endpoint/");
  const std::string kUserAgent_ = "Mozilla/1.0";
  const std::string kGroup_ = "group";
  const std::string kType_ = "default";
};

TEST_F(ReportingBrowsingDataRemoverTest, RemoveNothing) {
  AddReport(kUrl1_);
  AddReport(kUrl2_);

  SetEndpoint(kOrigin1_);
  SetEndpoint(kOrigin2_);

  RemoveBrowsingData(/* remove_reports= */ false, /* remove_clients= */ false,
                     /* host= */ "");
  EXPECT_EQ(2u, report_count());
  EXPECT_EQ(2u, cache()->GetEndpointCount());
}

TEST_F(ReportingBrowsingDataRemoverTest, RemoveAllReports) {
  AddReport(kUrl1_);
  AddReport(kUrl2_);

  SetEndpoint(kOrigin1_);
  SetEndpoint(kOrigin2_);

  RemoveBrowsingData(/* remove_reports= */ true, /* remove_clients= */ false,
                     /* host= */ "");
  EXPECT_EQ(0u, report_count());
  EXPECT_EQ(2u, cache()->GetEndpointCount());
}

TEST_F(ReportingBrowsingDataRemoverTest, RemoveAllClients) {
  AddReport(kUrl1_);
  AddReport(kUrl2_);

  SetEndpoint(kOrigin1_);
  SetEndpoint(kOrigin2_);

  RemoveBrowsingData(/* remove_reports= */ false, /* remove_clients= */ true,
                     /* host= */ "");
  EXPECT_EQ(2u, report_count());
  EXPECT_EQ(0u, cache()->GetEndpointCount());
}

TEST_F(ReportingBrowsingDataRemoverTest, RemoveAllReportsAndClients) {
  AddReport(kUrl1_);
  AddReport(kUrl2_);

  SetEndpoint(kOrigin1_);
  SetEndpoint(kOrigin2_);

  RemoveBrowsingData(/* remove_reports= */ true, /* remove_clients= */ true,
                     /* host= */ "");
  EXPECT_EQ(0u, report_count());
  EXPECT_EQ(0u, cache()->GetEndpointCount());
}

TEST_F(ReportingBrowsingDataRemoverTest, RemoveSomeReports) {
  AddReport(kUrl1_);
  AddReport(kUrl2_);

  SetEndpoint(kOrigin1_);
  SetEndpoint(kOrigin2_);

  RemoveBrowsingData(/* remove_reports= */ true, /* remove_clients= */ false,
                     /* host= */ kUrl1_.host());
  EXPECT_EQ(2u, cache()->GetEndpointCount());

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(kUrl2_, reports[0]->url);
}

TEST_F(ReportingBrowsingDataRemoverTest, RemoveSomeClients) {
  AddReport(kUrl1_);
  AddReport(kUrl2_);

  SetEndpoint(kOrigin1_);
  SetEndpoint(kOrigin2_);

  RemoveBrowsingData(/* remove_reports= */ false, /* remove_clients= */ true,
                     /* host= */ kUrl1_.host());
  EXPECT_EQ(2u, report_count());
  EXPECT_FALSE(FindEndpointInCache(
      ReportingEndpointGroupKey(NetworkAnonymizationKey(), kOrigin1_, kGroup_,
                                ReportingTargetType::kDeveloper),
      kEndpoint_));
  EXPECT_TRUE(FindEndpointInCache(
      ReportingEndpointGroupKey(NetworkAnonymizationKey(), kOrigin2_, kGroup_,
                                ReportingTargetType::kDeveloper),
      kEndpoint_));
}

}  // namespace
}  // namespace net
