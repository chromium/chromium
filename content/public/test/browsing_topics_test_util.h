// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSING_TOPICS_TEST_UTIL_H_
#define CONTENT_PUBLIC_TEST_BROWSING_TOPICS_TEST_UTIL_H_

#include "base/files/file_path.h"
#include "content/public/browser/browsing_topics_site_data_manager.h"

namespace content {

class BrowsingTopicsSiteDataManagerImpl;

// Synchrnously get all the browsing topics api usage contexts. Entries are
// sorted based on [hashed_context_domain, hashed_main_frame_host, time]
std::vector<browsing_topics::ApiUsageContext> GetBrowsingTopicsApiUsage(
    BrowsingTopicsSiteDataManager* topics_site_data_manager);

// A tester class that allows mocking a query failure (e.g. database error).
class TesterBrowsingTopicsSiteDataManager
    : public BrowsingTopicsSiteDataManager {
 public:
  explicit TesterBrowsingTopicsSiteDataManager(
      const base::FilePath& user_data_directory);

  ~TesterBrowsingTopicsSiteDataManager() override;

  TesterBrowsingTopicsSiteDataManager(
      const TesterBrowsingTopicsSiteDataManager&) = delete;
  TesterBrowsingTopicsSiteDataManager& operator=(
      const TesterBrowsingTopicsSiteDataManager&) = delete;
  TesterBrowsingTopicsSiteDataManager(TesterBrowsingTopicsSiteDataManager&&) =
      delete;
  TesterBrowsingTopicsSiteDataManager& operator=(
      TesterBrowsingTopicsSiteDataManager&&) = delete;

  // Use the default handling from `BrowsingTopicsSiteDataManagerImpl`.
  void ExpireDataBefore(base::Time time) override;

  // Use the default handling from `BrowsingTopicsSiteDataManagerImpl`.
  void ClearContextDomain(
      const browsing_topics::HashedDomain& hashed_context_domain) override;

  // Use the default handling from `BrowsingTopicsSiteDataManagerImpl`.
  void OnBrowsingTopicsApiUsed(
      const browsing_topics::HashedHost& hashed_top_host,
      const base::flat_set<browsing_topics::HashedDomain>&
          hashed_context_domains,
      base::Time time) override;

  void SetQueryFailureOverride() { query_failure_override_ = true; }

  // Return a default/failed `ApiUsageContextQueryResult` if
  // `query_failure_override_` is true; otherwise, sse the default handling from
  // `BrowsingTopicsSiteDataManagerImpl`.
  void GetBrowsingTopicsApiUsage(
      base::Time begin_time,
      base::Time end_time,
      GetBrowsingTopicsApiUsageCallback callback) override;

 private:
  std::unique_ptr<BrowsingTopicsSiteDataManagerImpl> manager_impl_;

  bool query_failure_override_ = false;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSING_TOPICS_TEST_UTIL_H_
