// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_UDP_TRACKER_H_
#define NET_DNS_DNS_UDP_TRACKER_H_

#include <stdint.h>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "net/base/net_export.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

// Data tracker for DNS UDP and its usage of local ports. Intended to be owned
// by a DnsSession and thus keep track of the data session-wide. Responsible for
// related metrics and used to inform behavior based on the stored data.
//
// TODO(ericorth@chromium.org): Add methods to access the stored data or
// conclusions about it.
class NET_EXPORT_PRIVATE DnsUdpTracker {
 public:
  static constexpr base::TimeDelta kMaxAge = base::Minutes(10);
  static constexpr size_t kMaxRecordedQueries = 256;

  // How recently an ID needs to be recorded in a recent query to be considered
  // "recognized".
  static constexpr base::TimeDelta kMaxRecognizedIdAge = base::Seconds(15);

  // Numbers of ID mismatches required to set the |low_entropy_| flag. Also
  // serves as the max number of mismatches to be recorded, as no more entries
  // are recorded after setting the flag.
  static constexpr size_t kUnrecognizedIdMismatchThreshold = 8;
  static constexpr size_t kRecognizedIdMismatchThreshold = 128;

  // Number of reuses of the same port required to set the |low_entropy_| flag.
  static constexpr int kPortReuseThreshold = 2;

  DnsUdpTracker();
  ~DnsUdpTracker();

  DnsUdpTracker(DnsUdpTracker&&);
  DnsUdpTracker& operator=(DnsUdpTracker&&);

  void RecordQuery(uint16_t port, uint16_t query_id);
  void RecordResponseId(uint16_t query_id, uint16_t response_id);
  void RecordConnectionError(int connection_error);

  // If true, the entropy from random UDP port and DNS ID has been detected to
  // potentially be low, e.g. due to exhaustion of the port pool or mismatches
  // on IDs.
  bool low_entropy() const { return low_entropy_; }

  void set_tick_clock_for_testing(base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  struct QueryData;

  void PurgeOldRecords();
  void SaveQuery(QueryData query);
  void SaveIdMismatch(uint16_t it);

  bool low_entropy_ = false;
  base::circular_deque<QueryData> recent_queries_;

  // Times of recent ID mismatches, separated by whether or not the ID was
  // recognized from recent queries.
  base::circular_deque<base::TimeTicks> recent_unrecognized_id_hits_;
  base::circular_deque<base::TimeTicks> recent_recognized_id_hits_;

  raw_ptr<const base::TickClock> tick_clock_ =
      base::DefaultTickClock::GetInstance();
};

}  // namespace net

#endif  // NET_DNS_DNS_UDP_TRACKER_H_
