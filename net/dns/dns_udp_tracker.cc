// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_udp_tracker.h"

#include <algorithm>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/tick_clock.h"

namespace net {

// static
constexpr base::TimeDelta DnsUdpTracker::kMaxAge;

// static
constexpr size_t DnsUdpTracker::kMaxRecordedQueries;

struct DnsUdpTracker::QueryData {
  uint16_t port;
  uint16_t query_id;
  base::TimeTicks time;
};

DnsUdpTracker::DnsUdpTracker() = default;
DnsUdpTracker::~DnsUdpTracker() = default;
DnsUdpTracker::DnsUdpTracker(DnsUdpTracker&&) = default;
DnsUdpTracker& DnsUdpTracker::operator=(DnsUdpTracker&&) = default;

void DnsUdpTracker::RecordQuery(uint16_t port, uint16_t query_id) {
  PurgeOldQueries();

  int reused_port_count = base::checked_cast<int>(std::count_if(
      recent_queries_.cbegin(), recent_queries_.cend(),
      [port](const auto& recent_query) { return port == recent_query.port; }));
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.DNS.DnsTransaction.UDP.ReusedPort.Count",
                              reused_port_count, 1, kMaxRecordedQueries, 50);

  base::TimeTicks now = tick_clock_->NowTicks();
  if (reused_port_count > 0) {
    auto most_recent_match = std::find_if(
        recent_queries_.crbegin(), recent_queries_.crend(),
        [port](const auto& recent_query) { return port == recent_query.port; });
    DCHECK(most_recent_match != recent_queries_.crend());
    UMA_HISTOGRAM_LONG_TIMES(
        "Net.DNS.DnsTransaction.UDP.ReusedPort.MostRecentAge",
        now - most_recent_match->time);
  }

  SaveQuery({port, query_id, now});
}

void DnsUdpTracker::RecordResponseId(uint16_t query_id, uint16_t response_id) {
  PurgeOldQueries();

  // Used in UMA (DNS.UdpIdMismatchStatus). Do not renumber or remove values.
  enum class MismatchStatus {
    kSuccessfulParse = 0,
    kMismatchPreviouslyQueried = 1,
    kMismatchUnknown = 2,
    kMaxValue = kMismatchUnknown,
  };

  MismatchStatus status;
  if (query_id == response_id) {
    status = MismatchStatus::kSuccessfulParse;
  } else {
    auto oldest_matching_id =
        std::find_if(recent_queries_.cbegin(), recent_queries_.cend(),
                     [&](const auto& recent_query) {
                       return response_id == recent_query.query_id;
                     });

    if (oldest_matching_id == recent_queries_.cend()) {
      status = MismatchStatus::kMismatchUnknown;
    } else {
      status = MismatchStatus::kMismatchPreviouslyQueried;
      UMA_HISTOGRAM_LONG_TIMES(
          "Net.DNS.DnsTransaction.UDP.IdMismatch.OldestMatchTime",
          tick_clock_->NowTicks() - oldest_matching_id->time);
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsTransaction.UDP.IdMismatch", status);
}

void DnsUdpTracker::PurgeOldQueries() {
  base::TimeTicks now = tick_clock_->NowTicks();
  while (!recent_queries_.empty() &&
         (now - recent_queries_.front().time) > kMaxAge) {
    recent_queries_.pop_front();
  }
}

void DnsUdpTracker::SaveQuery(QueryData query) {
  if (recent_queries_.size() == kMaxRecordedQueries)
    recent_queries_.pop_front();
  DCHECK_LT(recent_queries_.size(), kMaxRecordedQueries);

  DCHECK(recent_queries_.empty() || query.time >= recent_queries_.back().time);
  recent_queries_.push_back(std::move(query));
}

}  // namespace net
