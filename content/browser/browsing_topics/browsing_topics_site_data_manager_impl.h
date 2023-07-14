// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_SITE_DATA_MANAGER_IMPL_H_
#define CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_SITE_DATA_MANAGER_IMPL_H_

#include "content/public/browser/browsing_topics_site_data_manager.h"

#include "base/threading/sequence_bound.h"
#include "content/browser/browsing_topics/browsing_topics_site_data_storage.h"

namespace content {

// UI thread class that manages the lifetime of the underlying browsing topics
// site data storage. Owned by the storage partition.
class CONTENT_EXPORT BrowsingTopicsSiteDataManagerImpl
    : public BrowsingTopicsSiteDataManager {
 public:
  explicit BrowsingTopicsSiteDataManagerImpl(
      const base::FilePath& user_data_directory);

  BrowsingTopicsSiteDataManagerImpl(const BrowsingTopicsSiteDataManagerImpl&) =
      delete;
  BrowsingTopicsSiteDataManagerImpl& operator=(
      const BrowsingTopicsSiteDataManagerImpl&) = delete;
  BrowsingTopicsSiteDataManagerImpl(BrowsingTopicsSiteDataManagerImpl&&) =
      delete;
  BrowsingTopicsSiteDataManagerImpl& operator=(
      BrowsingTopicsSiteDataManagerImpl&&) = delete;

  ~BrowsingTopicsSiteDataManagerImpl() override;

  void ExpireDataBefore(base::Time time) override;

  void ClearContextDomain(
      const browsing_topics::HashedDomain& hashed_context_domain) override;

  void GetBrowsingTopicsApiUsage(
      base::Time begin_time,
      base::Time end_time,
      GetBrowsingTopicsApiUsageCallback callback) override;

  void OnBrowsingTopicsApiUsed(
      const browsing_topics::HashedHost& hashed_main_frame_host,
      const browsing_topics::HashedDomain& hashed_context_domain,
      const std::string& context_domain,
      base::Time time) override;

  void GetContextDomainsFromHashedContextDomains(
      const std::set<browsing_topics::HashedDomain>& hashed_context_domains,
      GetContextDomainsFromHashedContextDomainsCallback callback) override;

 private:
  base::SequenceBound<BrowsingTopicsSiteDataStorage> storage_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSING_TOPICS_BROWSING_TOPICS_SITE_DATA_MANAGER_IMPL_H_
