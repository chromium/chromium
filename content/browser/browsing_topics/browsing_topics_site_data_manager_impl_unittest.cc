// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_site_data_manager_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class BrowsingTopicsSiteDataManagerImplTest : public testing::Test {
 public:
  BrowsingTopicsSiteDataManagerImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());

    topics_manager_ =
        std::make_unique<BrowsingTopicsSiteDataManagerImpl>(dir_.GetPath());
  }

 protected:
  base::ScopedTempDir dir_;
  BrowserTaskEnvironment task_environment_;

  std::unique_ptr<BrowsingTopicsSiteDataManagerImpl> topics_manager_;
};

TEST_F(BrowsingTopicsSiteDataManagerImplTest, GetBrowsingTopicsApiUsage) {
  base::Time initial_time = base::Time::Now();

  topics_manager_->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domain=*/browsing_topics::HashedDomain(456),
      /*context_domain=*/"456.com", initial_time);

  size_t query_result_count = 0;

  base::RunLoop get_usage_waiter;

  topics_manager_->GetBrowsingTopicsApiUsage(
      /*begin_time=*/initial_time - base::Seconds(1),
      /*end_time=*/initial_time,
      base::BindLambdaForTesting(
          [&](browsing_topics::ApiUsageContextQueryResult result) {
            // Queries are handled in order. The first callback has to be first
            // invoked.
            EXPECT_EQ(query_result_count, 0u);
            ++query_result_count;

            // No result, as the usage entry's timestamp is outside
            // [begin_time, end_time).
            EXPECT_TRUE(result.success);
            EXPECT_EQ(result.api_usage_contexts.size(), 0u);
          }));

  topics_manager_->GetBrowsingTopicsApiUsage(
      /*begin_time=*/initial_time,
      /*end_time=*/initial_time + base::Seconds(1),
      base::BindLambdaForTesting(
          [&](browsing_topics::ApiUsageContextQueryResult result) {
            // Queries are handled in order. The second callback has to be
            // invoked after the first one.
            EXPECT_EQ(query_result_count, 1u);
            ++query_result_count;

            EXPECT_TRUE(result.success);
            EXPECT_EQ(result.api_usage_contexts.size(), 1u);

            EXPECT_EQ(result.api_usage_contexts[0].hashed_main_frame_host,
                      browsing_topics::HashedHost(123));
            EXPECT_EQ(result.api_usage_contexts[0].hashed_context_domain,
                      browsing_topics::HashedDomain(456));
            EXPECT_EQ(result.api_usage_contexts[0].time, initial_time);

            get_usage_waiter.Quit();
          }));

  get_usage_waiter.Run();

  EXPECT_EQ(query_result_count, 2u);
}

TEST_F(BrowsingTopicsSiteDataManagerImplTest,
       GetContextDomainsFromHashedContextDomains) {
  size_t query_result_count = 0;

  base::RunLoop get_domains_waiter;

  topics_manager_->OnBrowsingTopicsApiUsed(
      /*hashed_main_frame_host=*/browsing_topics::HashedHost(123),
      /*hashed_context_domain=*/browsing_topics::HashedDomain(456),
      /*context_domain=*/"456.com", base::Time::Now());

  topics_manager_->GetContextDomainsFromHashedContextDomains(
      {browsing_topics::HashedDomain(456), browsing_topics::HashedDomain(789)},
      base::BindLambdaForTesting(
          [&](std::map<browsing_topics::HashedDomain, std::string> result) {
            // Queries are handled in order. The first callback has to be first
            // invoked.
            EXPECT_EQ(query_result_count, 0u);
            ++query_result_count;

            std::map<browsing_topics::HashedDomain, std::string>
                expected_result(
                    {{browsing_topics::HashedDomain(456), "456.com"}});
            EXPECT_EQ(result, expected_result);
          }));

  topics_manager_->GetContextDomainsFromHashedContextDomains(
      {browsing_topics::HashedDomain(789)},
      base::BindLambdaForTesting(
          [&](std::map<browsing_topics::HashedDomain, std::string> result) {
            // Queries are handled in order. The second callback has to be
            // invoked after the first one.
            EXPECT_EQ(query_result_count, 1u);
            ++query_result_count;

            std::map<browsing_topics::HashedDomain, std::string>
                expected_result({});
            EXPECT_EQ(result, expected_result);

            get_domains_waiter.Quit();
          }));

  get_domains_waiter.Run();

  EXPECT_EQ(query_result_count, 2u);
}

}  // namespace content
