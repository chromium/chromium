// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_service.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "net/reporting/mock_persistent_reporting_store.h"
#include "net/reporting/reporting_browsing_data_remover.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_report.h"
#include "net/reporting/reporting_service.h"
#include "net/reporting/reporting_test_util.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  const std::string kUserAgent_ = "Mozilla/1.0";
  const std::string kGroup_ = "group";
  const std::string kType_ = "type";

  ReportingServiceTest() {
    if (GetParam())
      store_ = std::make_unique<MockPersistentReportingStore>();
    else
      store_ = nullptr;

    auto test_context = std::make_unique<TestReportingContext>(
        &clock_, &tick_clock_, ReportingPolicy(), store_.get());
    context_ = test_context.get();

    service_ = ReportingService::CreateForTesting(std::move(test_context));
  }

  // If the store exists, simulate finishing loading the store, which should
  // make the rest of the test run synchronously.
  void FinishLoading(bool load_success) {
    if (store_)
      store_->FinishLoading(load_success);
  }

  MockPersistentReportingStore* store() { return store_.get(); }
  TestReportingContext* context() { return context_; }
  ReportingService* service() { return service_.get(); }

 private:
  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;

  std::unique_ptr<MockPersistentReportingStore> store_;
  TestReportingContext* context_;
  std::unique_ptr<ReportingService> service_;
};

TEST_P(ReportingServiceTest, QueueReport) {
  service()->QueueReport(kUrl_, kUserAgent_, kGroup_, kType_,
                         std::make_unique<base::DictionaryValue>(), 0);
  FinishLoading(true /* load_success */);

  std::vector<const ReportingReport*> reports;
  context()->cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(kUrl_, reports[0]->url);
  EXPECT_EQ(kUserAgent_, reports[0]->user_agent);
  EXPECT_EQ(kGroup_, reports[0]->group);
  EXPECT_EQ(kType_, reports[0]->type);
}

TEST_P(ReportingServiceTest, QueueReportSanitizeUrl) {
  // Same as kUrl_ but with username, password, and fragment.
  GURL url = GURL("https://username:password@origin/path#fragment");
  service()->QueueReport(url, kUserAgent_, kGroup_, kType_,
                         std::make_unique<base::DictionaryValue>(), 0);
  FinishLoading(true /* load_success */);

  std::vector<const ReportingReport*> reports;
  context()->cache()->GetReports(&reports);
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(kUrl_, reports[0]->url);
  EXPECT_EQ(kUserAgent_, reports[0]->user_agent);
  EXPECT_EQ(kGroup_, reports[0]->group);
  EXPECT_EQ(kType_, reports[0]->type);
}

TEST_P(ReportingServiceTest, DontQueueReportInvalidUrl) {
  GURL url = GURL("https://");
  // This does not trigger an attempt to load from the store because the url
  // is immediately rejected as invalid.
  service()->QueueReport(url, kUserAgent_, kGroup_, kType_,
                         std::make_unique<base::DictionaryValue>(), 0);

  std::vector<const ReportingReport*> reports;
  context()->cache()->GetReports(&reports);
  ASSERT_EQ(0u, reports.size());
}

TEST_P(ReportingServiceTest, ProcessHeader) {
  service()->ProcessHeader(kUrl_, "{\"endpoints\":[{\"url\":\"" +
                                      kEndpoint_.spec() +
                                      "\"}],"
                                      "\"group\":\"" +
                                      kGroup_ +
                                      "\","
                                      "\"max_age\":86400}");
  FinishLoading(true /* load_success */);

  EXPECT_EQ(1u, context()->cache()->GetEndpointCount());
}

TEST_P(ReportingServiceTest, ProcessHeader_TooLong) {
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
  service()->ProcessHeader(kUrl_, header_too_long);

  EXPECT_EQ(0u, context()->cache()->GetEndpointCount());
}

TEST_P(ReportingServiceTest, ProcessHeader_TooDeep) {
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
  service()->ProcessHeader(kUrl_, header_too_deep);

  EXPECT_EQ(0u, context()->cache()->GetEndpointCount());
}

TEST_P(ReportingServiceTest, WriteToStore) {
  if (!store())
    return;

  MockPersistentReportingStore::CommandList expected_commands;

  // This first call to any public method triggers a load. The load will block
  // until we call FinishLoading.
  service()->ProcessHeader(kUrl_, "{\"endpoints\":[{\"url\":\"" +
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
  expected_commands.emplace_back(
      CommandType::ADD_REPORTING_ENDPOINT,
      ReportingEndpoint(kOrigin_, kGroup_,
                        ReportingEndpoint::EndpointInfo{kEndpoint_}));
  expected_commands.emplace_back(
      CommandType::ADD_REPORTING_ENDPOINT_GROUP,
      CachedReportingEndpointGroup(
          kOrigin_, kGroup_, OriginSubdomains::DEFAULT /* irrelevant */,
          base::Time() /* irrelevant */, base::Time() /* irrelevant */));
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->ProcessHeader(kUrl2_, "{\"endpoints\":[{\"url\":\"" +
                                       kEndpoint_.spec() +
                                       "\"}],"
                                       "\"group\":\"" +
                                       kGroup_ +
                                       "\","
                                       "\"max_age\":86400}");
  expected_commands.emplace_back(
      CommandType::ADD_REPORTING_ENDPOINT,
      ReportingEndpoint(kOrigin2_, kGroup_,
                        ReportingEndpoint::EndpointInfo{kEndpoint_}));
  expected_commands.emplace_back(
      CommandType::ADD_REPORTING_ENDPOINT_GROUP,
      CachedReportingEndpointGroup(
          kOrigin2_, kGroup_, OriginSubdomains::DEFAULT /* irrelevant */,
          base::Time() /* irrelevant */, base::Time() /* irrelevant */));
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->QueueReport(kUrl_, kUserAgent_, kGroup_, kType_,
                         std::make_unique<base::DictionaryValue>(), 0);
  expected_commands.emplace_back(
      CommandType::UPDATE_REPORTING_ENDPOINT_GROUP_ACCESS_TIME,
      CachedReportingEndpointGroup(
          kOrigin_, kGroup_, OriginSubdomains::DEFAULT /* irrelevant */,
          base::Time() /* irrelevant */, base::Time() /* irrelevant */));
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->RemoveBrowsingData(ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
                                base::BindRepeating([](const GURL& url) {
                                  return url.host() == "origin";
                                }));
  expected_commands.emplace_back(
      CommandType::DELETE_REPORTING_ENDPOINT,
      ReportingEndpoint(kOrigin_, kGroup_,
                        ReportingEndpoint::EndpointInfo{kEndpoint_}));
  expected_commands.emplace_back(
      CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
      CachedReportingEndpointGroup(
          kOrigin_, kGroup_, OriginSubdomains::DEFAULT /* irrelevant */,
          base::Time() /* irrelevant */, base::Time() /* irrelevant */));
  expected_commands.emplace_back(CommandType::FLUSH);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->RemoveAllBrowsingData(
      ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS);
  expected_commands.emplace_back(
      CommandType::DELETE_REPORTING_ENDPOINT,
      ReportingEndpoint(kOrigin2_, kGroup_,
                        ReportingEndpoint::EndpointInfo{kEndpoint_}));
  expected_commands.emplace_back(
      CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
      CachedReportingEndpointGroup(
          kOrigin2_, kGroup_, OriginSubdomains::DEFAULT /* irrelevant */,
          base::Time() /* irrelevant */, base::Time() /* irrelevant */));
  expected_commands.emplace_back(CommandType::FLUSH);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));
}

TEST_P(ReportingServiceTest, WaitUntilLoadFinishesBeforeWritingToStore) {
  if (!store())
    return;

  MockPersistentReportingStore::CommandList expected_commands;

  // This first call to any public method triggers a load. The load will block
  // until we call FinishLoading.
  service()->ProcessHeader(kUrl_, "{\"endpoints\":[{\"url\":\"" +
                                      kEndpoint_.spec() +
                                      "\"}],"
                                      "\"group\":\"" +
                                      kGroup_ +
                                      "\","
                                      "\"max_age\":86400}");
  expected_commands.emplace_back(CommandType::LOAD_REPORTING_CLIENTS);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->ProcessHeader(kUrl2_, "{\"endpoints\":[{\"url\":\"" +
                                       kEndpoint_.spec() +
                                       "\"}],"
                                       "\"group\":\"" +
                                       kGroup_ +
                                       "\","
                                       "\"max_age\":86400}");
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->QueueReport(kUrl_, kUserAgent_, kGroup_, kType_,
                         std::make_unique<base::DictionaryValue>(), 0);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->RemoveBrowsingData(ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS,
                                base::BindRepeating([](const GURL& url) {
                                  return url.host() == "origin";
                                }));
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  service()->RemoveAllBrowsingData(
      ReportingBrowsingDataRemover::DATA_TYPE_CLIENTS);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));

  // Unblock the load. The will let the remaining calls to the service complete
  // without blocking.
  FinishLoading(true /* load_success */);
  expected_commands.emplace_back(
      CommandType::ADD_REPORTING_ENDPOINT,
      ReportingEndpoint(kOrigin_, kGroup_,
                        ReportingEndpoint::EndpointInfo{kEndpoint_}));
  expected_commands.emplace_back(
      CommandType::ADD_REPORTING_ENDPOINT,
      ReportingEndpoint(kOrigin2_, kGroup_,
                        ReportingEndpoint::EndpointInfo{kEndpoint_}));
  expected_commands.emplace_back(
      CommandType::ADD_REPORTING_ENDPOINT_GROUP,
      CachedReportingEndpointGroup(
          kOrigin_, kGroup_, OriginSubdomains::DEFAULT /* irrelevant */,
          base::Time() /* irrelevant */, base::Time() /* irrelevant */));
  expected_commands.emplace_back(
      CommandType::ADD_REPORTING_ENDPOINT_GROUP,
      CachedReportingEndpointGroup(
          kOrigin2_, kGroup_, OriginSubdomains::DEFAULT /* irrelevant */,
          base::Time() /* irrelevant */, base::Time() /* irrelevant */));
  expected_commands.emplace_back(
      CommandType::UPDATE_REPORTING_ENDPOINT_GROUP_ACCESS_TIME,
      CachedReportingEndpointGroup(
          kOrigin_, kGroup_, OriginSubdomains::DEFAULT /* irrelevant */,
          base::Time() /* irrelevant */, base::Time() /* irrelevant */));
  expected_commands.emplace_back(
      CommandType::DELETE_REPORTING_ENDPOINT,
      ReportingEndpoint(kOrigin_, kGroup_,
                        ReportingEndpoint::EndpointInfo{kEndpoint_}));
  expected_commands.emplace_back(
      CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
      CachedReportingEndpointGroup(
          kOrigin_, kGroup_, OriginSubdomains::DEFAULT /* irrelevant */,
          base::Time() /* irrelevant */, base::Time() /* irrelevant */));
  expected_commands.emplace_back(CommandType::FLUSH);
  expected_commands.emplace_back(
      CommandType::DELETE_REPORTING_ENDPOINT,
      ReportingEndpoint(kOrigin2_, kGroup_,
                        ReportingEndpoint::EndpointInfo{kEndpoint_}));
  expected_commands.emplace_back(
      CommandType::DELETE_REPORTING_ENDPOINT_GROUP,
      CachedReportingEndpointGroup(
          kOrigin2_, kGroup_, OriginSubdomains::DEFAULT /* irrelevant */,
          base::Time() /* irrelevant */, base::Time() /* irrelevant */));
  expected_commands.emplace_back(CommandType::FLUSH);
  EXPECT_THAT(store()->GetAllCommands(),
              testing::UnorderedElementsAreArray(expected_commands));
}

INSTANTIATE_TEST_SUITE_P(ReportingServiceStoreTest,
                         ReportingServiceTest,
                         ::testing::Bool());
}  // namespace
}  // namespace net
