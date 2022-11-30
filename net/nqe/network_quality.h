// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_NETWORK_QUALITY_H_
#define NET_NQE_NETWORK_QUALITY_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "net/base/net_export.h"

namespace net::nqe::internal {

// RTT and throughput values are set to |INVALID_RTT_THROUGHPUT| if a valid
// value is unavailable.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.net
enum RttThroughputValues {
  // Invalid value.
  INVALID_RTT_THROUGHPUT = -1,
};

// Returns the RTT value to be used when the valid RTT is unavailable. Readers
// should discard RTT if it is set to the value returned by |InvalidRTT()|.
// TODO(tbansal): Remove this method, and replace all calls by
// |INVALID_RTT_THROUGHPUT|.
NET_EXPORT_PRIVATE base::TimeDelta InvalidRTT();

// NetworkQuality is used to cache the quality of a network connection.
class NET_EXPORT_PRIVATE NetworkQuality {
 public:
  NetworkQuality();
  // |http_rtt| is the estimate of the round trip time at the HTTP layer.
  // |transport_rtt| is the estimate of the round trip time at the transport
  // layer. |downstream_throughput_kbps| is the estimate of the downstream
  // throughput in kilobits per second.
  NetworkQuality(const base::TimeDelta& http_rtt,
                 const base::TimeDelta& transport_rtt,
                 int32_t downstream_throughput_kbps);
  NetworkQuality(const NetworkQuality& other);
  ~NetworkQuality();

  NetworkQuality& operator=(const NetworkQuality& other);

  bool operator==(const NetworkQuality& other) const;

  // Returns true if |this| is at least as fast as |other| for all parameters
  // (HTTP RTT, transport RTT etc.)
  bool IsFaster(const NetworkQuality& other) const;

  // Returns the estimate of the round trip time at the HTTP layer.
  const base::TimeDelta& http_rtt() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return http_rtt_;
  }

  void set_http_rtt(base::TimeDelta http_rtt) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    http_rtt_ = http_rtt;
    DCHECK_LE(INVALID_RTT_THROUGHPUT, http_rtt_.InMilliseconds());
  }

  // Returns the estimate of the round trip time at the transport layer.
  const base::TimeDelta& transport_rtt() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return transport_rtt_;
  }

  void set_transport_rtt(base::TimeDelta transport_rtt) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    transport_rtt_ = transport_rtt;
    DCHECK_LE(INVALID_RTT_THROUGHPUT, transport_rtt_.InMilliseconds());
  }

  // Returns the estimate of the downstream throughput in Kbps (Kilobits per
  // second).
  int32_t downstream_throughput_kbps() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return downstream_throughput_kbps_;
  }

  void set_downstream_throughput_kbps(int32_t downstream_throughput_kbps) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    downstream_throughput_kbps_ = downstream_throughput_kbps;
    DCHECK_LE(INVALID_RTT_THROUGHPUT, downstream_throughput_kbps_);
  }

 private:
  // Verifies that the value of network quality is within the expected range.
  void VerifyValueCorrectness() const;

  // Estimated round trip time at the HTTP layer.
  base::TimeDelta http_rtt_;

  // Estimated round trip time at the transport layer.
  base::TimeDelta transport_rtt_;

  // Estimated downstream throughput in kilobits per second.
  int32_t downstream_throughput_kbps_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace net::nqe::internal

#endif  // NET_NQE_NETWORK_QUALITY_H_
