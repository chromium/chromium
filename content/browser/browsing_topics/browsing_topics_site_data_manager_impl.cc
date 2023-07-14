// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/browsing_topics_site_data_manager_impl.h"

#include "base/task/lazy_thread_pool_task_runner.h"

namespace content {

namespace {

// The shared-task runner for all browsing topics site data storage operations.
// The backend is a sqlite database and we want to be sure to access it on a
// single thread (to prevent any potential races when a given context is
// destroyed and recreated for the same backing storage.). This uses
// BLOCK_SHUTDOWN as some data deletion operations may be running when the
// browser is closed, and we want to ensure all data is deleted correctly. This
// uses BEST_EFFORT as the database will be queried very infrequently (e.g.
// every week), so some amount of delay is acceptable.
base::LazyThreadPoolSequencedTaskRunner g_storage_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::TaskPriority::BEST_EFFORT,
                         base::MayBlock(),
                         base::TaskShutdownBehavior::BLOCK_SHUTDOWN));

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("BrowsingTopicsSiteData");

}  // namespace

BrowsingTopicsSiteDataManagerImpl::BrowsingTopicsSiteDataManagerImpl(
    const base::FilePath& user_data_directory)
    : storage_(base::SequenceBound<BrowsingTopicsSiteDataStorage>(
          g_storage_task_runner.Get(),
          user_data_directory.Append(kDatabasePath))) {}

BrowsingTopicsSiteDataManagerImpl::~BrowsingTopicsSiteDataManagerImpl() =
    default;

void BrowsingTopicsSiteDataManagerImpl::ExpireDataBefore(base::Time time) {
  storage_.AsyncCall(&BrowsingTopicsSiteDataStorage::ExpireDataBefore)
      .WithArgs(time);
}

void BrowsingTopicsSiteDataManagerImpl::ClearContextDomain(
    const browsing_topics::HashedDomain& hashed_context_domain) {
  storage_.AsyncCall(&BrowsingTopicsSiteDataStorage::ClearContextDomain)
      .WithArgs(hashed_context_domain);
}

void BrowsingTopicsSiteDataManagerImpl::GetBrowsingTopicsApiUsage(
    base::Time begin_time,
    base::Time end_time,
    GetBrowsingTopicsApiUsageCallback callback) {
  storage_.AsyncCall(&BrowsingTopicsSiteDataStorage::GetBrowsingTopicsApiUsage)
      .WithArgs(begin_time, end_time)
      .Then(std::move(callback));
}

void BrowsingTopicsSiteDataManagerImpl::OnBrowsingTopicsApiUsed(
    const browsing_topics::HashedHost& hashed_main_frame_host,
    const browsing_topics::HashedDomain& hashed_context_domain,
    const std::string& context_domain,
    base::Time time) {
  storage_.AsyncCall(&BrowsingTopicsSiteDataStorage::OnBrowsingTopicsApiUsed)
      .WithArgs(hashed_main_frame_host, hashed_context_domain, context_domain,
                time);
}

void BrowsingTopicsSiteDataManagerImpl::
    GetContextDomainsFromHashedContextDomains(
        const std::set<browsing_topics::HashedDomain>& hashed_context_domains,
        BrowsingTopicsSiteDataManager::
            GetContextDomainsFromHashedContextDomainsCallback callback) {
  storage_
      .AsyncCall(&BrowsingTopicsSiteDataStorage::
                     GetContextDomainsFromHashedContextDomains)
      .WithArgs(hashed_context_domains)
      .Then(std::move(callback));
}

}  // namespace content
