// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_UDP_TRACKER_H_
#define NET_DNS_DNS_UDP_TRACKER_H_

#include <stdint.h>

#include "base/containers/circular_deque.h"
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
  static constexpr base::TimeDelta kMaxAge = base::TimeDelta::FromMinutes(10);
  static constexpr size_t kMaxRecordedQueries = 256;

  DnsUdpTracker();
  ~DnsUdpTracker();

  DnsUdpTracker(DnsUdpTracker&&);
  DnsUdpTracker& operator=(DnsUdpTracker&&);

  void RecordQuery(uint16_t port, uint16_t query_id);
  void RecordResponseId(uint16_t query_id, uint16_t response_id);

  void set_tick_clock_for_testing(base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  struct QueryData;

  void PurgeOldQueries();
  void SaveQuery(QueryData query);

  base::circular_deque<QueryData> recent_queries_;

  const base::TickClock* tick_clock_ = base::DefaultTickClock::GetInstance();
};

}  // namespace net

#endif  // NET_DNS_DNS_UDP_TRACKER_H_
