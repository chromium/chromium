// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_EVENT_CREATOR_H_
#define NET_NQE_EVENT_CREATOR_H_

#include <stdint.h>

#include "base/sequence_checker.h"
#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality.h"

namespace net {

class NetLogWithSource;

namespace nqe::internal {

// Class that adds net log events for network quality estimator.
class NET_EXPORT_PRIVATE EventCreator {
 public:
  explicit EventCreator(NetLogWithSource net_log);

  EventCreator(const EventCreator&) = delete;
  EventCreator& operator=(const EventCreator&) = delete;

  ~EventCreator();

  // May add network quality changed event to the net-internals log if there
  // is a change in the effective connection type, or if there is a meaningful
  // change in the values of HTTP RTT, transport RTT or bandwidth.
  // |effective_connection_type| is the current effective connection type.
  // |network_quality| is the current network quality.
  void MaybeAddNetworkQualityChangedEventToNetLog(
      EffectiveConnectionType effective_connection_type,
      const NetworkQuality& network_quality);

 private:
  NetLogWithSource net_log_;

  // The effective connection type when the net log event was last added.
  EffectiveConnectionType past_effective_connection_type_ =
      EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  //  The network quality when the net log event was last added.
  NetworkQuality past_network_quality_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace nqe::internal

}  // namespace net

#endif  // NET_NQE_EVENT_CREATOR_H_
