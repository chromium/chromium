// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_DESTINATION_THROTTLER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_DESTINATION_THROTTLER_H_

#include <map>

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace net {
class SchemefulSite;
}

namespace attribution_reporting {
class DestinationSet;
}

namespace content {

// DestinationThrottler is a class which manages a rolling time window
// keeping track of unique destinations being registered on source sites.
class CONTENT_EXPORT DestinationThrottler {
 public:
  struct CONTENT_EXPORT Policy {
    int max_total = 200;
    int max_per_reporting_site = 50;
    base::TimeDelta rate_limit_window = base::Minutes(1);

    bool operator==(const Policy&) const;

    [[nodiscard]] bool Validate() const;
  };
  explicit DestinationThrottler(Policy policy);
  ~DestinationThrottler();

  DestinationThrottler(DestinationThrottler&) = delete;
  DestinationThrottler& operator=(DestinationThrottler&) = delete;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Result {
    kAllowed = 0,
    kHitGlobalLimit = 1,
    kHitReportingLimit = 2,
    kHitBothLimits = 3,
    kMaxValue = kHitBothLimits
  };
  // - Returns `kAllowed` if the throttler allowed `destinations` through.
  // - Return `kHitGlobalLimit` if `destinations` are not allowed due to the
  //   global limit of `max_total`.
  // - Returns `kHitReportingLimit` if `destinations` are not allowed due to the
  //   `max_per_reporting_site` limit.
  // - Returns `kHitBothLimits` if `destinations` are not allowed to to both
  //   limits simultaneously.
  //
  // Also updates the internal state of the throttler to track all of the
  // destinations, if allowed.
  [[nodiscard]] Result UpdateAndGetResult(
      const attribution_reporting::DestinationSet& destinations,
      const net::SchemefulSite& source_site,
      const net::SchemefulSite& reporting_site);

  int GetMaxPerReportingSite() const { return policy_.max_per_reporting_site; }

 private:
  void CleanUpOldEntries();

  class SourceSiteData;
  std::map<net::SchemefulSite, SourceSiteData> source_site_data_;
  Policy policy_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_DESTINATION_THROTTLER_H_
