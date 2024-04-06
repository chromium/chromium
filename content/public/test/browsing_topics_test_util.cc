// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browsing_topics_test_util.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "components/browsing_topics/common/common_types.h"
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

std::map<browsing_topics::HashedDomain, std::string>
GetContextDomainsFromHashedContextDomains(
    content::BrowsingTopicsSiteDataManager* topics_site_data_manager,
    std::set<browsing_topics::HashedDomain> hashed_context_domains) {
  base::RunLoop run_loop;

  std::map<browsing_topics::HashedDomain, std::string> query_result;
  topics_site_data_manager->GetContextDomainsFromHashedContextDomains(
      hashed_context_domains,
      base::BindLambdaForTesting(
          [&](std::map<browsing_topics::HashedDomain, std::string> result) {
            query_result = result;
            run_loop.Quit();
          }));

  run_loop.Run();
  return query_result;
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
    const browsing_topics::HashedDomain& hashed_context_domain,
    const std::string& context_domain,
    base::Time time) {
  manager_impl_->OnBrowsingTopicsApiUsed(hashed_top_host, hashed_context_domain,
                                         context_domain, time);
}

void TesterBrowsingTopicsSiteDataManager::GetBrowsingTopicsApiUsage(
    base::Time begin_time,
    base::Time end_time,
    GetBrowsingTopicsApiUsageCallback callback) {
  auto run_callback_after_delay = base::BindLambdaForTesting(
      [callback = std::move(callback),
       this](browsing_topics::ApiUsageContextQueryResult result) mutable {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindLambdaForTesting([callback = std::move(callback),
                                        result = std::move(result)]() mutable {
              std::move(callback).Run(std::move(result));
            }),
            query_result_delay_);
      });

  if (!query_failure_override_) {
    manager_impl_->GetBrowsingTopicsApiUsage(
        begin_time, end_time, std::move(run_callback_after_delay));
    return;
  }

  std::move(run_callback_after_delay)
      .Run(browsing_topics::ApiUsageContextQueryResult());
}

void TesterBrowsingTopicsSiteDataManager::
    GetContextDomainsFromHashedContextDomains(
        const std::set<browsing_topics::HashedDomain>& hashed_context_domains,
        GetContextDomainsFromHashedContextDomainsCallback callback) {
  manager_impl_->GetContextDomainsFromHashedContextDomains(
      hashed_context_domains, std::move(callback));
}

}  // namespace content
