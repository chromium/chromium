// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_RTT_THROUGHPUT_ESTIMATES_OBSERVER_H_
#define NET_NQE_RTT_THROUGHPUT_ESTIMATES_OBSERVER_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net {

// Observes changes in the network quality.
class NET_EXPORT_PRIVATE RTTAndThroughputEstimatesObserver {
 public:
  // Notifies the observer when estimated HTTP RTT, estimated transport RTT or
  // estimated downstream throughput is computed. NetworkQualityEstimator
  // computes the RTT and throughput estimates at regular intervals.
  // Additionally, when there is a change in the connection type of the
  // device, then the estimates are immediately computed.
  //
  // |http_rtt|, |transport_rtt| and |downstream_throughput_kbps| are the
  // computed estimates of the HTTP RTT, transport RTT and downstream
  // throughput (in kilobits per second), respectively. If an estimate of the
  // HTTP or transport RTT is unavailable, it will be set to
  // nqe::internal::InvalidRTT(). If the throughput estimate is unavailable,
  // it will be set to nqe::internal::INVALID_RTT_THROUGHPUT.
  virtual void OnRTTOrThroughputEstimatesComputed(
      base::TimeDelta http_rtt,
      base::TimeDelta transport_rtt,
      int32_t downstream_throughput_kbps) = 0;

  RTTAndThroughputEstimatesObserver(const RTTAndThroughputEstimatesObserver&) =
      delete;
  RTTAndThroughputEstimatesObserver& operator=(
      const RTTAndThroughputEstimatesObserver&) = delete;

  virtual ~RTTAndThroughputEstimatesObserver() = default;

 protected:
  RTTAndThroughputEstimatesObserver() = default;
};

}  // namespace net

#endif  // NET_NQE_RTT_THROUGHPUT_ESTIMATES_OBSERVER_H_
