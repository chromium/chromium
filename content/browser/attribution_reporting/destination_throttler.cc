// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/destination_throttler.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "components/attribution_reporting/destination_set.h"
#include "net/base/schemeful_site.h"

namespace content {

namespace {

// Needed in order to keep iterators in a set. Compare using the underlying
// pointer which will be cheaper than comparing SchemefulSites.
struct ptr_less {
  template <typename I>
  bool operator()(const I& lh, const I& rh) const {
    return &*lh < &*rh;
  }
};

}  // namespace

// This class maintains a set of destination sites, along with a list of
// (possibly overlapping) subsets keyed by reporting sites. Each destination is
// tagged with when it was last used within the source site, so that a rolling
// window of unique destinations can be enforced.
//
// Individual subsets do not keep a last update time as they are strict subsets
// of the overall set of destinations.
class DestinationThrottler::SourceSiteData {
 public:
  using DestinationMap = base::LRUCache<net::SchemefulSite, base::TimeTicks>;
  using DestinationMapIt = typename DestinationMap::iterator;

  // Use a flat map for the subsets. This should be efficient for small sets
  // even when doing lots of O(n) insertions and deletions. Consider a different
  // data structure if subsets will grow > ~100 entries. With a more complex
  // indexing approach we could implement this with a `max_per_reporting_site`
  // sized bitset which will be very competitive from a memory overhead
  // standpoint.
  using DestinationSubset = base::flat_set<DestinationMapIt, ptr_less>;

  explicit SourceSiteData(const DestinationThrottler::Policy& policy)
      : destinations_(policy.max_total) {}
  ~SourceSiteData() = default;

  SourceSiteData(SourceSiteData&&) noexcept = default;
  SourceSiteData& operator=(SourceSiteData&&) noexcept = default;

  Result UpdateAndGetResult(
      const attribution_reporting::DestinationSet& destinations,
      const net::SchemefulSite& reporting_site,
      const Policy& policy) {
    base::TimeTicks now = base::TimeTicks::Now();
    EvictEntriesOlderThan(now - policy.rate_limit_window);
    auto& reporting_set = reporting_destinations_[reporting_site];

    // First detect whether we have capacity for _all_ the destinations.
    Result throttle_result = HasCapacity(destinations, reporting_set, policy);
    if (throttle_result != Result::kAllowed) {
      return throttle_result;
    }

    // Mutate the data structure only after guaranteeing capacity to avoid
    // having to rewind on failure.
    for (const net::SchemefulSite& dest : destinations.destinations()) {
      auto it = destinations_.Get(dest);
      if (it == destinations_.end()) {
        it = destinations_.Put(dest, now);
      } else {
        it->second = now;
      }
      reporting_set.insert(it);
    }
    return throttle_result;
  }

  bool AllEntriesOlderThan(base::TimeTicks time) const {
    return destinations_.empty() || destinations_.begin()->second < time;
  }

 private:
  Result HasCapacity(const attribution_reporting::DestinationSet& destinations,
                     const DestinationSubset& reporting_set,
                     const Policy& policy) {
    int all_capacity = policy.max_total - destinations_.size();
    int reporting_capacity =
        policy.max_per_reporting_site - reporting_set.size();
    for (const net::SchemefulSite& dest : destinations.destinations()) {
      const auto& it = destinations_.Peek(dest);
      if (it == destinations_.end()) {
        all_capacity--;
        reporting_capacity--;
      } else if (!base::Contains(reporting_set, it)) {
        reporting_capacity--;
      }
    }
    if (all_capacity >= 0 && reporting_capacity >= 0) {
      return Result::kAllowed;
    }
    if (all_capacity < 0 && reporting_capacity < 0) {
      return Result::kHitBothLimits;
    }
    if (all_capacity < 0) {
      return Result::kHitGlobalLimit;
    }
    return Result::kHitReportingLimit;
  }

  void EvictEntriesOlderThan(base::TimeTicks time) {
    while (!destinations_.empty()) {
      // Don't use a reverse iterator because the subsets index on the forward
      // iterator.
      auto it = --destinations_.end();
      if (it->second >= time) {
        return;
      }
      for (auto& set : reporting_destinations_) {
        set.second.erase(it);
      }
      destinations_.Erase(it);
    }
  }

  DestinationMap destinations_;
  std::map<net::SchemefulSite, DestinationSubset> reporting_destinations_;
};

bool DestinationThrottler::Policy::Validate() const {
  if (max_per_reporting_site <= 0) {
    return false;
  }

  if (max_total < max_per_reporting_site) {
    return false;
  }

  if (!rate_limit_window.is_positive()) {
    return false;
  }

  return true;
}

bool DestinationThrottler::Policy::operator==(const Policy& other) const {
  return max_total == other.max_total &&
         max_per_reporting_site == other.max_per_reporting_site &&
         rate_limit_window == other.rate_limit_window;
}

DestinationThrottler::DestinationThrottler(Policy policy)
    : policy_(std::move(policy)) {}

DestinationThrottler::~DestinationThrottler() = default;

DestinationThrottler::Result DestinationThrottler::UpdateAndGetResult(
    const attribution_reporting::DestinationSet& destinations,
    const net::SchemefulSite& source_site,
    const net::SchemefulSite& reporting_site) {
  CleanUpOldEntries();
  auto it = source_site_data_.try_emplace(source_site, policy_);
  return it.first->second.UpdateAndGetResult(destinations, reporting_site,
                                             policy_);
}

void DestinationThrottler::CleanUpOldEntries() {
  base::TimeTicks old_time = base::TimeTicks::Now() - policy_.rate_limit_window;
  base::EraseIf(source_site_data_, [old_time](auto& it) {
    return it.second.AllEntriesOlderThan(old_time);
  });
}

}  // namespace content
