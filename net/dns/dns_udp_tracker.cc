// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_udp_tracker.h"

#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/time/tick_clock.h"
#include "net/base/net_errors.h"

namespace net {

namespace {
// Used in UMA (DNS.UdpLowEntropyReason). Do not renumber or remove values.
enum class LowEntropyReason {
  kPortReuse = 0,
  kRecognizedIdMismatch = 1,
  kUnrecognizedIdMismatch = 2,
  kSocketLimitExhaustion = 3,
  kMaxValue = kSocketLimitExhaustion,
};

void RecordLowEntropyUma(LowEntropyReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Net.DNS.DnsTransaction.UDP.LowEntropyReason",
                            reason);
}

}  // namespace

// static
constexpr base::TimeDelta DnsUdpTracker::kMaxAge;

// static
constexpr size_t DnsUdpTracker::kMaxRecordedQueries;

// static
constexpr base::TimeDelta DnsUdpTracker::kMaxRecognizedIdAge;

// static
constexpr size_t DnsUdpTracker::kUnrecognizedIdMismatchThreshold;

// static
constexpr size_t DnsUdpTracker::kRecognizedIdMismatchThreshold;

// static
constexpr int DnsUdpTracker::kPortReuseThreshold;

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
  PurgeOldRecords();

  int reused_port_count = base::checked_cast<int>(
      base::ranges::count(recent_queries_, port, &QueryData::port));

  if (reused_port_count >= kPortReuseThreshold && !low_entropy_) {
    low_entropy_ = true;
    RecordLowEntropyUma(LowEntropyReason::kPortReuse);
  }

  SaveQuery({port, query_id, tick_clock_->NowTicks()});
}

void DnsUdpTracker::RecordResponseId(uint16_t query_id, uint16_t response_id) {
  PurgeOldRecords();

  if (query_id != response_id) {
    SaveIdMismatch(response_id);
  }
}

void DnsUdpTracker::RecordConnectionError(int connection_error) {
  if (!low_entropy_ && connection_error == ERR_INSUFFICIENT_RESOURCES) {
    // On UDP connection, this error signifies that the process is using an
    // unreasonably large number of UDP sockets, potentially a deliberate
    // attack to reduce DNS port entropy.
    low_entropy_ = true;
    RecordLowEntropyUma(LowEntropyReason::kSocketLimitExhaustion);
  }
}

void DnsUdpTracker::PurgeOldRecords() {
  base::TimeTicks now = tick_clock_->NowTicks();

  while (!recent_queries_.empty() &&
         (now - recent_queries_.front().time) > kMaxAge) {
    recent_queries_.pop_front();
  }
  while (!recent_unrecognized_id_hits_.empty() &&
         now - recent_unrecognized_id_hits_.front() > kMaxAge) {
    recent_unrecognized_id_hits_.pop_front();
  }
  while (!recent_recognized_id_hits_.empty() &&
         now - recent_recognized_id_hits_.front() > kMaxAge) {
    recent_recognized_id_hits_.pop_front();
  }
}

void DnsUdpTracker::SaveQuery(QueryData query) {
  if (recent_queries_.size() == kMaxRecordedQueries)
    recent_queries_.pop_front();
  DCHECK_LT(recent_queries_.size(), kMaxRecordedQueries);

  DCHECK(recent_queries_.empty() || query.time >= recent_queries_.back().time);
  recent_queries_.push_back(std::move(query));
}

void DnsUdpTracker::SaveIdMismatch(uint16_t id) {
  // No need to track mismatches if already flagged for low entropy.
  if (low_entropy_)
    return;

  base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeTicks time_cutoff = now - kMaxRecognizedIdAge;
  bool is_recognized =
      base::ranges::any_of(recent_queries_, [&](const auto& recent_query) {
        return recent_query.query_id == id && recent_query.time >= time_cutoff;
      });

  if (is_recognized) {
    DCHECK_LT(recent_recognized_id_hits_.size(),
              kRecognizedIdMismatchThreshold);
    if (recent_recognized_id_hits_.size() ==
        kRecognizedIdMismatchThreshold - 1) {
      low_entropy_ = true;
      RecordLowEntropyUma(LowEntropyReason::kRecognizedIdMismatch);
      return;
    }

    DCHECK(recent_recognized_id_hits_.empty() ||
           now >= recent_recognized_id_hits_.back());
    recent_recognized_id_hits_.push_back(now);
  } else {
    DCHECK_LT(recent_unrecognized_id_hits_.size(),
              kUnrecognizedIdMismatchThreshold);
    if (recent_unrecognized_id_hits_.size() ==
        kUnrecognizedIdMismatchThreshold - 1) {
      low_entropy_ = true;
      RecordLowEntropyUma(LowEntropyReason::kUnrecognizedIdMismatch);
      return;
    }

    DCHECK(recent_unrecognized_id_hits_.empty() ||
           now >= recent_unrecognized_id_hits_.back());
    recent_unrecognized_id_hits_.push_back(now);
  }
}

}  // namespace net
