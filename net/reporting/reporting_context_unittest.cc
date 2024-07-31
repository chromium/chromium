// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_context.h"

#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/reporting/mock_persistent_reporting_store.h"
#include "net/reporting/reporting_test_util.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace {

// The tests are parametrized on a boolean value which represents whether to use
// a MockPersistentReportingStore or not.
class ReportingContextTest : public ReportingTestBase,
                             public ::testing::WithParamInterface<bool> {
 protected:
  ReportingContextTest() {
    feature_list_.InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
    std::unique_ptr<MockPersistentReportingStore> store;
    if (GetParam()) {
      store = std::make_unique<MockPersistentReportingStore>();
    }
    store_ = store.get();
    UseStore(std::move(store));
  }

  MockPersistentReportingStore* store() { return store_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockPersistentReportingStore> store_;
};

TEST_P(ReportingContextTest, ReportingContextConstructionWithFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };
  EXPECT_EQ(0u, cache()->GetEnterpriseEndpointsForTesting().size());
  std::unique_ptr<URLRequestContext> url_request_context =
      CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<ReportingContext> reporting_context_ptr =
      ReportingContext::Create(ReportingPolicy(), url_request_context.get(),
                               store(), test_enterprise_endpoints);

  std::vector<ReportingEndpoint> expected_enterprise_endpoints = {
      {ReportingEndpointGroupKey(NetworkAnonymizationKey(),
                                 /*reporting_source=*/std::nullopt,
                                 /*origin=*/std::nullopt, "endpoint-1",
                                 ReportingTargetType::kEnterprise),
       {.url = GURL("https://example.com/reports")}},
      {ReportingEndpointGroupKey(NetworkAnonymizationKey(),
                                 /*reporting_source=*/std::nullopt,
                                 /*origin=*/std::nullopt, "endpoint-2",
                                 ReportingTargetType::kEnterprise),
       {.url = GURL("https://reporting.example/cookie-issues")}},
      {ReportingEndpointGroupKey(NetworkAnonymizationKey(),
                                 /*reporting_source=*/std::nullopt,
                                 /*origin=*/std::nullopt, "endpoint-3",
                                 ReportingTargetType::kEnterprise),
       {.url = GURL("https://report-collector.example")}}};

  EXPECT_EQ(expected_enterprise_endpoints,
            reporting_context_ptr->cache()->GetEnterpriseEndpointsForTesting());
}

TEST_P(ReportingContextTest, ReportingContextConstructionWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };
  EXPECT_EQ(0u, cache()->GetEnterpriseEndpointsForTesting().size());
  std::unique_ptr<URLRequestContext> url_request_context =
      CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<ReportingContext> reporting_context_ptr =
      ReportingContext::Create(ReportingPolicy(), url_request_context.get(),
                               store(), test_enterprise_endpoints);

  EXPECT_EQ(0u, reporting_context_ptr->cache()
                    ->GetEnterpriseEndpointsForTesting()
                    .size());
}

INSTANTIATE_TEST_SUITE_P(ReportingContextStoreTest,
                         ReportingContextTest,
                         ::testing::Bool());
}  // namespace
}  // namespace net
