// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BROWSING_TOPICS_SITE_DATA_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_BROWSING_TOPICS_SITE_DATA_MANAGER_H_

#include <map>
#include <set>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "components/browsing_topics/common/common_types.h"
#include "content/common/content_export.h"

namespace content {

// Interface for storing and accessing the browsing topics site data used to
// calculate web-visible topics.
class CONTENT_EXPORT BrowsingTopicsSiteDataManager {
 public:
  using GetBrowsingTopicsApiUsageCallback =
      base::OnceCallback<void(browsing_topics::ApiUsageContextQueryResult)>;
  using GetContextDomainsFromHashedContextDomainsCallback =
      base::OnceCallback<void(
          std::map<browsing_topics::HashedDomain, std::string>)>;

  virtual ~BrowsingTopicsSiteDataManager() = default;

  // Expire all data before the given time.
  virtual void ExpireDataBefore(base::Time time) = 0;

  // Clear per-context-domain data.
  virtual void ClearContextDomain(
      const browsing_topics::HashedDomain& hashed_context_domain) = 0;

  // Get all browsing topics `ApiUsageContext` with its `last_usage_time` within
  // [`begin_time`, `end_time`). Note that it's possible for a usage to occur
  // within the specified time range, and a more recent usage has renewed its
  // `last_usage_time`, so that the corresponding context is not retrieved in
  // this query. In practice, this method will be called with
  // `end_time` being very close to the current time, so the amount of missed
  // data should be negligible. This query also deletes all data with
  // last_usage_time (non-inclusive) less than `begin_time`.
  virtual void GetBrowsingTopicsApiUsage(
      base::Time begin_time,
      base::Time end_time,
      GetBrowsingTopicsApiUsageCallback callback) = 0;

  // Persist the browsing topics api usage context to storage. Called when the
  // usage is detected in a context on a page.
  virtual void OnBrowsingTopicsApiUsed(
      const browsing_topics::HashedHost& hashed_main_frame_host,
      const browsing_topics::HashedDomain& hashed_context_domain,
      const std::string& context_domain,
      base::Time time) = 0;

  // For each hashed context domain, get the stored unhashed version. Only
  // hashed domains for which there is a corresponding unhashed domain will be
  // included in the output.
  virtual void GetContextDomainsFromHashedContextDomains(
      const std::set<browsing_topics::HashedDomain>& hashed_context_domains,
      GetContextDomainsFromHashedContextDomainsCallback callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BROWSING_TOPICS_SITE_DATA_MANAGER_H_
