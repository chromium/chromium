// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_EVENT_CREATOR_H_
#define NET_NQE_EVENT_CREATOR_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality.h"

namespace net {

class NetLogWithSource;

namespace nqe {

namespace internal {

// Class that adds net log events for network quality estimator.
class NET_EXPORT_PRIVATE EventCreator {
 public:
  explicit EventCreator(NetLogWithSource net_log);
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
  EffectiveConnectionType past_effective_connection_type_;

  //  The network quality when the net log event was last added.
  NetworkQuality past_network_quality_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(EventCreator);
};

}  // namespace internal

}  // namespace nqe

}  // namespace net

#endif  // NET_NQE_EVENT_CREATOR_H_
