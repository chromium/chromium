// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browsing_topics_test_util.h"

#include "base/test/bind.h"
#include "content/browser/browsing_topics/browsing_topics_site_data_manager_impl.h"

namespace content {

std::vector<browsing_topics::ApiUsageContext> GetBrowsingTopicsApiUsage(
    content::BrowsingTopicsSiteDataManager* topics_site_data_manager) {
  browsing_topics::ApiUsageContextQueryResult query_result;

  base::RunLoop run_loop;
  topics_site_data_manager->GetBrowsingTopicsApiUsage(
      base::Time(), base::Time::Now() + base::Days(1),
      base::BindLambdaForTesting(
          [&](browsing_topics::ApiUsageContextQueryResult result) {
            query_result = std::move(result);
            run_loop.Quit();
          }));

  run_loop.Run();

  DCHECK(query_result.success);

  std::vector<browsing_topics::ApiUsageContext> api_usage_contexts =
      std::move(query_result.api_usage_contexts);

  std::sort(api_usage_contexts.begin(), api_usage_contexts.end(),
            [](auto& left, auto& right) {
              return left.hashed_context_domain != right.hashed_context_domain
                         ? left.hashed_context_domain <
                               right.hashed_context_domain
                         : (left.hashed_main_frame_host !=
                                    right.hashed_main_frame_host
                                ? left.hashed_main_frame_host <
                                      right.hashed_main_frame_host
                                : left.time < right.time);
            });

  return api_usage_contexts;
}

TesterBrowsingTopicsSiteDataManager::TesterBrowsingTopicsSiteDataManager(
    const base::FilePath& user_data_directory)
    : manager_impl_(
          new BrowsingTopicsSiteDataManagerImpl(user_data_directory)) {}

void TesterBrowsingTopicsSiteDataManager::ExpireDataBefore(base::Time time) {
  manager_impl_->ExpireDataBefore(time);
}

void TesterBrowsingTopicsSiteDataManager::ClearContextDomain(
    const browsing_topics::HashedDomain& hashed_context_domain) {
  manager_impl_->ClearContextDomain(hashed_context_domain);
}

TesterBrowsingTopicsSiteDataManager::~TesterBrowsingTopicsSiteDataManager() =
    default;

void TesterBrowsingTopicsSiteDataManager::OnBrowsingTopicsApiUsed(
    const browsing_topics::HashedHost& hashed_top_host,
    const base::flat_set<browsing_topics::HashedDomain>& hashed_context_domains,
    base::Time time) {
  manager_impl_->OnBrowsingTopicsApiUsed(hashed_top_host,
                                         hashed_context_domains, time);
}

void TesterBrowsingTopicsSiteDataManager::GetBrowsingTopicsApiUsage(
    base::Time begin_time,
    base::Time end_time,
    GetBrowsingTopicsApiUsageCallback callback) {
  if (!query_failure_override_) {
    manager_impl_->GetBrowsingTopicsApiUsage(begin_time, end_time,
                                             std::move(callback));
    return;
  }

  std::move(callback).Run(browsing_topics::ApiUsageContextQueryResult());
}

}  // namespace content
