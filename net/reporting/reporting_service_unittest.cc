// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_service.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "net/reporting/mock_persistent_reporting_store.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_service.h"
#include "net/reporting/reporting_target_type.h"
#include "net/reporting/reporting_test_util.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

using CommandType = MockPersistentReportingStore::Command::Type;

// The tests are parametrized on a boolean value which represents whether to use
// a MockPersistentReportingStore (if false, no store is used).
class ReportingServiceTest : public ::testing::TestWithParam<bool>,
                             public WithTaskEnvironment {
 protected:
  const GURL kUrl_ = GURL("https://origin/path");
  const GURL kUrl2_ = GURL("https://origin2/path");
  const url::Origin kOrigin_ = url::Origin::Create(kUrl_);
  const url::Origin kOrigin2_ = url::Origin::Create(kUrl2_);
  const GURL kEndpoint_ = GURL("https://endpoint/");
  const GURL kEndpoint2_ = GURL("https://endpoint2/");
  const std::string kUserAgent_ = "Mozilla/1.0";
  const std::string kGroup_ = "group";
  const std::string kGroup2_ = "group2";
  const std::string kType_ = "type";
  const std::optional<base::UnguessableToken> kReportingSource_ =
      base::UnguessableToken::Create();
  const NetworkAnonymizationKey kNak_ =
      NetworkAnonymizationKey::CreateSameSite(SchemefulSite(kOrigin_));
  const NetworkAnonymizationKey kNak2_ =
      NetworkAnonymizationKey::CreateSameSite(SchemefulSite(kOrigin2_));
  const ReportingEndpointGroupKey kGroupKey_ =
      ReportingEndpointGroupKey(kNak_,
                                kOrigin_,
                                kGroup_,
                                ReportingTargetType::kDeveloper);
  const ReportingEndpointGroupKey kGroupKey2_ =
      ReportingEndpointGroupKey(kNak2_,
                                kOrigin2_,
                                kGroup_,
                                ReportingTargetType::kDeveloper);
  const IsolationInfo kIsolationInfo_ =
      IsolationInfo::Create(IsolationInfo::RequestType::kOther,
                            kOrigin_,
                            kOrigin_,
                            SiteForCookies::FromOrigin(kOrigin_));

  ReportingServiceTest() {
    feature_list_.InitAndEnableFeature(
        features::kPartitionConnectionsByNetworkIsolationKey);
    Init();
  }

  // Initializes, or re-initializes, |service_| and its dependencies.
  void Init() {
    // Must destroy old service, if there is one, before destroying old store.
    // Need to clear `context_` first, since it points to an object owned by the
    // service.
    context_ = nullptr;
    service_.reset();

    if (GetParam()) {
      store_ = std::make_unique<MockPersistentReportingStore>();
    } else {
      store_ = nullptr;
    }

    auto test_context = std::make_unique<TestReportingContext>(
        &clock_, &tick_clock_, ReportingPolicy(), store_.get());
    context_ = test_context.get();

    service_ = ReportingService::CreateForTesting(std::move(test_context));
  }

  // If the store exists, simulate finishing loading the store, which should
  // make the rest of the test run synchronously.
  void FinishLoading(bool load_success) {
    if (store_) {
      store_->FinishLoading(load_success);
    }
  }

  MockPersistentReportingStore* store() { return store_.get(); }
  TestReportingContext* context() { return context_; }
  ReportingService* service() { return service_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;

  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;

  std::unique_ptr<MockPersistentReportingStore> store_;
  std::unique_ptr<ReportingService> service_;
  raw_ptr<TestReportingContext> context_ = nullptr;
};

TEST_P(ReportingServiceTest, QueueReport) {
  service()->QueueReport(kUrl_, kReportingSource_, kNak_, kUserAgent_, kGroup_,
                         kType_, base::Value::Dict(), 0,
                         ReportingTargetType::kDeveloper);
  FinishLoading(true /* load_success */);

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  context()->cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(kUrl_, reports[0]->url);
  EXPECT_EQ(kNak_, reports[0]->network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports[0]->user_agent);
  EXPECT_EQ(kGroup_, reports[0]->group);
  EXPECT_EQ(kType_, reports[0]->type);
  EXPECT_EQ(ReportingTargetType::kDeveloper, reports[0]->target_type);
}

TEST_P(ReportingServiceTest, QueueEnterpriseReport) {
  service()->QueueReport(kUrl_, kReportingSource_, kNak_, kUserAgent_, kGroup_,
                         kType_, base::Value::Dict(), 0,
                         ReportingTargetType::kEnterprise);
  FinishLoading(true /* load_success */);

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  context()->cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(kUrl_, reports[0]->url);
  EXPECT_EQ(kNak_, reports[0]->network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports[0]->user_agent);
  EXPECT_EQ(kGroup_, reports[0]->group);
  EXPECT_EQ(kType_, reports[0]->type);
  EXPECT_EQ(ReportingTargetType::kEnterprise, reports[0]->target_type);
}

TEST_P(ReportingServiceTest, QueueReportSanitizeUrl) {
  // Same as kUrl_ but with username, password, and fragment.
  GURL url = GURL("https://username:password@origin/path#fragment");
  service()->QueueReport(url, kReportingSource_, kNak_, kUserAgent_, kGroup_,
                         kType_, base::Value::Dict(), 0,
                         ReportingTargetType::kDeveloper);
  FinishLoading(true /* load_success */);

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  context()->cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(kUrl_, reports[0]->url);
  EXPECT_EQ(kNak_, reports[0]->network_anonymization_key);
  EXPECT_EQ(kUserAgent_, reports[0]->user_agent);
  EXPECT_EQ(kGroup_, reports[0]->group);
  EXPECT_EQ(kType_, reports[0]->type);
}

TEST_P(ReportingServiceTest, DontQueueReportInvalidUrl) {
  GURL url = GURL("https://");
  // This does not trigger an attempt to load from the store because the url
  // is immediately rejected as invalid.
  service()->QueueReport(url, kReportingSource_, kNak_, kUserAgent_, kGroup_,
                         kType_, base::Value::Dict(), 0,
                         ReportingTargetType::kDeveloper);

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  context()->cache()->GetReports(&reports);
  ASSERT_EQ(0u, reports.size());
}

TEST_P(ReportingServiceTest, QueueReportNetworkIsolationKeyDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  // Re-create the store, so it reads the new feature value.
  Init();

  service()->QueueReport(kUrl_, kReportingSource_, kNak_, kUserAgent_, kGroup_,
                         kType_, base::Value::Dict(), 0,
                         ReportingTargetType::kDeveloper);
  FinishLoading(true /* load_success */);

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  context()->cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());

  // NetworkAnonymizationKey should be empty, instead of kNak_;
  EXPECT_EQ(NetworkAnonymizationKey(), reports[0]->network_anonymization_key);
  EXPECT_NE(kNak_, reports[0]->network_anonymization_key);

  EXPECT_EQ(kUrl_, reports[0]->url);
  EXPECT_EQ(kUserAgent_, reports[0]->user_agent);
  EXPECT_EQ(kGroup_, reports[0]->group);
  EXPECT_EQ(kType_, reports[0]->type);
}

TEST_P(ReportingServiceTest, ProcessReportToHeader) {
  service()->ProcessReportToHeader(kOrigin_, kNak_,
                                   "{\"endpoints\":[{\"url\":\"" +
                                       kEndpoint_.spec() +
                                       "\"}],"
                                       "\"group\":\"" +
                                       kGroup_ +
                                       "\","
                                       "\"max_age\":86400}");
  FinishLoading(true /* load_success */);

  EXPECT_EQ(1u, context()->cache()->GetEndpointCount());
  EXPECT_TRUE(context()->cache()->GetEndpointForTesting(
      ReportingEndpointGroupKey(kNak_, kOrigin_, kGroup_,
                                ReportingTargetType::kDeveloper),
      kEndpoint_));
}

TEST_P(ReportingServiceTest, ProcessReportingEndpointsHeader) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kDocumentReporting);
  auto parsed_header =
      ParseReportingEndpoints(kGroup_ + "=\"" + kEndpoint_.spec() + "\"");
  ASSERT_TRUE(parsed_header.has_value());
  service()->SetDocumentReportingEndpoints(*kReportingSource_, kOrigin_,
                                           kIsolationInfo_, *parsed_header);
  FinishLoading(true /* load_success */);

  // Endpoint should not be part of the persistent store.
  EXPECT_EQ(0u, context()->cache()->GetEndpointCount());
  // Endpoint should be associated with the reporting source.
  ReportingEndpoint cached_endpoint =
      context()->cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup_);
  EXPECT_TRUE(cached_endpoint);

  // Ensure that the NAK is stored properly with the endpoint group.
  EXPECT_FALSE(cached_endpoint.group_key.network_anonymization_key.IsEmpty());
}

TEST_P(ReportingServiceTest,
       ProcessReportingEndpointsHeaderNetworkIsolationKeyDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {net::features::kDocumentReporting},
      {features::kPartitionConnectionsByNetworkIsolationKey});

  // Re-create the store, so it reads the new feature value.
  Init();

  auto parsed_header =
      ParseReportingEndpoints(kGroup_ + "=\"" + kEndpoint_.spec() + "\"");
  ASSERT_TRUE(parsed_header.has_value());
  service()->SetDocumentReportingEndpoints(*kReportingSource_, kOrigin_,
                                           kIsolationInfo_, *parsed_header);
  FinishLoading(true /* load_success */);

  // Endpoint should not be part of the persistent store.
  EXPECT_EQ(0u, context()->cache()->GetEndpointCount());
  // Endpoint should be associated with the reporting source.
  ReportingEndpoint cached_endpoint =
      context()->cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup_);
  EXPECT_TRUE(cached_endpoint);

  // When isolation is disabled, cached endpoints should have a null NAK.
  EXPECT_TRUE(cached_endpoint.group_key.network_anonymization_key.IsEmpty());
}

TEST_P(ReportingServiceTest, SendReportsAndRemoveSource) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kDocumentReporting);
  auto parsed_header =
      ParseReportingEndpoints(kGroup_ + "=\"" + kEndpoint_.spec() + "\", " +
                              kGroup2_ + "=\"" + kEndpoint2_.spec() + "\"");
  ASSERT_TRUE(parsed_header.has_value());
  service()->SetDocumentReportingEndpoints(*kReportingSource_, kOrigin_,
                                           kIsolationInfo_, *parsed_header);
  // This report should be sent immediately, starting the delivery agent timer.
  service()->QueueReport(kUrl_, kReportingSource_, kNak_, kUserAgent_, kGroup_,
                         kType_, base::Value::Dict(), 0,
                         ReportingTargetType::kDeveloper);

  FinishLoading(true /* load_success */);

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  context()->cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(0u, context()->cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));

  // Now simulate the source being destroyed.
  service()->SendReportsAndRemoveSource(*kReportingSource_);

  // There should be no queued reports, but the previously sent report should
  // still be pending.
  EXPECT_EQ(0u, context()->cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));
  EXPECT_EQ(1u, context()->cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::PENDING));
  // Source should be marked as expired.
  ASSERT_TRUE(
      context()->cache()->GetExpiredSources().contains(*kReportingSource_));
}

// Flaky in ChromeOS: crbug.com/1356127
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SendReportsAndRemoveSourceWithPendingReports \
  DISABLED_SendReportsAndRemoveSourceWithPendingReports
#else
#define MAYBE_SendReportsAndRemoveSourceWithPendingReports \
  SendReportsAndRemoveSourceWithPendingReports
#endif
TEST_P(ReportingServiceTest,
       MAYBE_SendReportsAndRemoveSourceWithPendingReports) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kDocumentReporting);
  auto parsed_header =
      ParseReportingEndpoints(kGroup_ + "=\"" + kEndpoint_.spec() + "\", " +
                              kGroup2_ + "=\"" + kEndpoint2_.spec() + "\"");
  ASSERT_TRUE(parsed_header.has_value());
  service()->SetDocumentReportingEndpoints(*kReportingSource_, kOrigin_,
                                           kIsolationInfo_, *parsed_header);
  // This report should be sent immediately, starting the delivery agent timer.
  service()->QueueReport(kUrl_, kReportingSource_, kNak_, kUserAgent_, kGroup_,
                         kType_, base::Value::Dict(), 0,
                         ReportingTargetType::kDeveloper);

  FinishLoading(true /* load_success */);

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> reports;
  context()->cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(0u, context()->cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));
  EXPECT_EQ(1u, context()->cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::PENDING));

  // Queue another report, which should remain queued.
  service()->QueueReport(kUrl_, kReportingSource_, kNak_, kUserAgent_, kGroup_,
                         kType_, base::Value::Dict(), 0,
                         ReportingTargetType::kDeveloper);
  EXPECT_EQ(1u, context()->cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));
  EXPECT_EQ(1u, context()->cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::PENDING));

  // Now simulate the source being destroyed.
  service()->SendReportsAndRemoveSource(*kReportingSource_);

  // The report should still be queued, while the source should be marked as
  // expired. (The original report is still pending.)
  EXPECT_EQ(1u, context()->cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::QUEUED));
  EXPECT_EQ(1u, context()->cache()->GetReportCountWithStatusForTesting(
                    ReportingReport::Status::PENDING));
  ASSERT_TRUE(
      context()->cache()->GetExpiredSources().contains(kReportingSource_));
}

TEST_P(ReportingServiceTest, ProcessReportingEndpointsHeaderPathAbsolute) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(net::features::kDocumentReporting);
  auto parsed_header = ParseReportingEndpoints(kGroup_ + "=\"/path-absolute\"");
  ASSERT_TRUE(parsed_header.has_value());
  service()->SetDocumentReportingEndpoints(*kReportingSource_, kOrigin_,
                                           kIsolationInfo_, *parsed_header);
  FinishLoading(true /* load_success */);

  // Endpoint should not be part of the persistent store.
  EXPECT_EQ(0u, context()->cache()->GetEndpointCount());
  // Endpoint should be associated with the reporting source.
  ReportingEndpoint endpoint =
      context()->cache()->GetV1EndpointForTesting(*kReportingSource_, kGroup_);
  EXPECT_TRUE(endpoint);
  // Endpoint should have the correct path.
  EXPECT_EQ(kUrl_.Resolve("/path-absolute"), endpoint.info.url);
}

TEST_P(ReportingServiceTest, ProcessReportToHeaderPathAbsolute) {
  service()->ProcessReportToHeader(
      kOrigin_, kNak_,
      "{\"endpoints\":[{\"url\":\"/path-absolute\"}],"
      "\"group\":\"" +
          kGroup_ +
          "\","
          "\"max_age\":86400}");
  FinishLoading(true /* load_success */);

  EXPECT_EQ(1u, context()->cache()->GetEndpointCount());
}

TEST_P(ReportingServiceTest, ProcessReportToHeader_TooLong) {
  const std::string header_too_long =
      "{\"endpoints\":[{\"url\":\"" + kEndpoint_.spec() +
      "\"}],"
      "\"group\":\"" +
      kGroup_ +
      "\","
      "\"max_age\":86400," +
      "\"junk\":\"" + std::string(32 * 1024, 'a') + "\"}";
  // This does not trigger an attempt to load from the store because the header
  // is immediately rejected as invalid.
  service()->ProcessReportToHeader(kOrigin_, kNak_, header_too_long);

  EXPECT_EQ(0u, context()->cache()->GetEndpointCount());
}

TEST_P(ReportingServiceTest, ProcessReportToHeader_TooDeep) {
  const std::string header_too_deep = "{\"endpoints\":[{\"url\":\"" +
                                      kEndpoint_.spec() +
                                      "\"}],"
                                      "\"group\":\"" +
                                      kGroup_ +
                                      "\","
                                      "\"max_age\":86400," +
                                      "\"junk\":[[[[[[[[[[]]]]]]]]]]}";
  // This does not trigger an attempt to load from the store because the header
  // is immediately rejected as invalid.
  service()->ProcessReportToHeader(kOrigin_, kNak_, header_too_deep);

  EXPECT_EQ(0u, context()->cache()->GetEndpointCount());
}

TEST_P(ReportingServiceTest, ProcessReportToHeaderNetworkIsolationKeyDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPartitionConnectionsByNetworkIsolationKey);

  // Re-create the store, so it reads the new feature value.
  Init();

  service()->ProcessReportToHeader(kOrigin_, kNak_,
                                   "{\"endpoints\":[{\"url\":\"" +
                                       kEndpoint_.spec() +
                                       "\"}],"
                                       "\"group\":\"" +
                                       kGroup_ +
                                       "\","
                                       "\"max_age\":86400}");
  FinishLoading(true /* load_success */);

  EXPECT_EQ(1u, context()->cache()->GetEndpointCount());
  EXPECT_FALSE(context()->cache()->GetEndpointForTesting(
      ReportingEndpointGroupKey(kNak_, kOrigin_, kGroup_,
                                ReportingTargetType::kDeveloper),
      kEndpoint_));
  EXPECT_TRUE(context()->cache()->GetEndpointForTesting(
      ReportingEndpointGroupKey(NetworkAnonymizationKey(), kOrigin_, kGroup_,
                                ReportingTargetType::kDeveloper),
      kEndpoint_));
}

TEST_P(ReportingServiceTest, WriteToStore) {
  if (!store()) {
    return;
  }

  MockPersistentReportingStore::CommandList expected_commands;

  // This first call to any public method triggers a load. The load will block
  // until we call FinishLoading.
  service()->ProcessReportToHeader(kOrigin_, kNak_,
                                   "{\"endpoints\":[{\"url\":\"" +
                                       kEndpoint_.spec() +
                                       "\"}],"
                                       "\"group\":\"" +
                                       kGroup_ +
                                       "\","
                                       "\"max_age\":86400}");
  expected_commands.emplace_back(CommandType::LOAD_REPORTING_CLIENTS);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  // Unblock the load. The will let the remaining calls to the service complete
  // without blocking.
  FinishLoading(true /* load_success */);
  expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                 kGroupKey_, kEndpoint_);
  expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                 kGroupKey_);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->ProcessReportToHeader(kOrigin2_, kNak2_,
                                   "{\"endpoints\":[{\"url\":\"" +
                                       kEndpoint_.spec() +
                                       "\"}],"
                                       "\"group\":\"" +
                                       kGroup_ +
                                       "\","
                                       "\"max_age\":86400}");
  expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                 kGroupKey2_, kEndpoint_);
  expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                 kGroupKey2_);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->QueueReport(kUrl_, kReportingSource_, kNak_, kUserAgent_, kGroup_,
                         kType_, base::Value::Dict(), 0,
                         ReportingTargetType::kDeveloper);
  expected_commands.emplace_back(
      CommandType::UPDATE_REPORTING_ENDPOINT_GROUP_ACCESS_TIME, kGroupKey_);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->RemoveBrowsingData(
      ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
      base::BindRepeating(
          [](const url::Origin& origin) { return origin.host() == "origin"; }));
  expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                 kGroupKey_, kEndpoint_);
  expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                 kGroupKey_);
  expected_commands.emplace_back(CommandType::FLUSH);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->RemoveAllBrowsingData(
      ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS);
  expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                 kGroupKey2_, kEndpoint_);
  expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                 kGroupKey2_);
  expected_commands.emplace_back(CommandType::FLUSH);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));
}

TEST_P(ReportingServiceTest, WaitUntilLoadFinishesBeforeWritingToStore) {
  if (!store()) {
    return;
  }

  MockPersistentReportingStore::CommandList expected_commands;

  // This first call to any public method triggers a load. The load will block
  // until we call FinishLoading.
  service()->ProcessReportToHeader(kOrigin_, kNak_,
                                   "{\"endpoints\":[{\"url\":\"" +
                                       kEndpoint_.spec() +
                                       "\"}],"
                                       "\"group\":\"" +
                                       kGroup_ +
                                       "\","
                                       "\"max_age\":86400}");
  expected_commands.emplace_back(CommandType::LOAD_REPORTING_CLIENTS);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->ProcessReportToHeader(kOrigin2_, kNak2_,
                                   "{\"endpoints\":[{\"url\":\"" +
                                       kEndpoint_.spec() +
                                       "\"}],"
                                       "\"group\":\"" +
                                       kGroup_ +
                                       "\","
                                       "\"max_age\":86400}");
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->QueueReport(kUrl_, kReportingSource_, kNak_, kUserAgent_, kGroup_,
                         kType_, base::Value::Dict(), 0,
                         ReportingTargetType::kDeveloper);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->RemoveBrowsingData(
      ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
      base::BindRepeating(
          [](const url::Origin& origin) { return origin.host() == "origin"; }));
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->RemoveAllBrowsingData(
      ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  // Unblock the load. The will let the remaining calls to the service complete
  // without blocking.
  FinishLoading(true /* load_success */);
  expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                 kGroupKey_, kEndpoint_);
  expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT,
                                 kGroupKey2_, kEndpoint_);
  expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                 kGroupKey_);
  expected_commands.emplace_back(CommandType::ADD_REPORTING_ENDPOINT_GROUP,
                                 kGroupKey2_);
  expected_commands.emplace_back(
      CommandType::UPDATE_REPORTING_ENDPOINT_GROUP_ACCESS_TIME, kGroupKey_);
  expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                 kGroupKey_, kEndpoint_);
  expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                 kGroupKey_);
  expected_commands.emplace_back(CommandType::FLUSH);
  expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT,
                                 kGroupKey2_, kEndpoint_);
  expected_commands.emplace_back(CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
                                 kGroupKey2_);
  expected_commands.emplace_back(CommandType::FLUSH);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));
}

TEST_P(ReportingServiceTest,
       SetEnterpriseReportingEndpointsWithFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);
  EXPECT_EQ(0u, context()->cache()->GetEnterpriseEndpointsForTesting().size());
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };

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

  service()->SetEnterpriseReportingEndpoints(test_enterprise_endpoints);
  EXPECT_EQ(expected_enterprise_endpoints,
            context()->cache()->GetEnterpriseEndpointsForTesting());
}

TEST_P(ReportingServiceTest,
       SetEnterpriseReportingEndpointsWithFeatureDisabled) {
  EXPECT_EQ(0u, context()->cache()->GetEnterpriseEndpointsForTesting().size());
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };

  service()->SetEnterpriseReportingEndpoints(test_enterprise_endpoints);
  EXPECT_EQ(0u, context()->cache()->GetEnterpriseEndpointsForTesting().size());
}

TEST_P(ReportingServiceTest, ReportingServiceConstructionWithFeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };

  EXPECT_EQ(0u, service()
                    ->GetContextForTesting()
                    ->cache()
                    ->GetEnterpriseEndpointsForTesting()
                    .size());
  std::unique_ptr<URLRequestContext> url_request_context =
      CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<ReportingService> reporting_service_ptr =
      ReportingService::Create(ReportingPolicy(), url_request_context.get(),
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
            reporting_service_ptr->GetContextForTesting()
                ->cache()
                ->GetEnterpriseEndpointsForTesting());
}

TEST_P(ReportingServiceTest, ReportingServiceConstructionWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      net::features::kReportingApiEnableEnterpriseCookieIssues);
  base::flat_map<std::string, GURL> test_enterprise_endpoints{
      {"endpoint-1", GURL("https://example.com/reports")},
      {"endpoint-2", GURL("https://reporting.example/cookie-issues")},
      {"endpoint-3", GURL("https://report-collector.example")},
  };

  EXPECT_EQ(0u, service()
                    ->GetContextForTesting()
                    ->cache()
                    ->GetEnterpriseEndpointsForTesting()
                    .size());
  std::unique_ptr<URLRequestContext> url_request_context =
      CreateTestURLRequestContextBuilder()->Build();
  std::unique_ptr<ReportingService> reporting_service_ptr =
      ReportingService::Create(ReportingPolicy(), url_request_context.get(),
                               store(), test_enterprise_endpoints);

  EXPECT_EQ(0u, reporting_service_ptr->GetContextForTesting()
                    ->cache()
                    ->GetEnterpriseEndpointsForTesting()
                    .size());
}

INSTANTIATE_TEST_SUITE_P(ReportingServiceStoreTest,
                         ReportingServiceTest,
                         ::testing::Bool());
}  // namespace
}  // namespace net
