// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_quality.h"

namespace net::nqe::internal {

base::TimeDelta InvalidRTT() {
  return base::Milliseconds(INVALID_RTT_THROUGHPUT);
}

NetworkQuality::NetworkQuality()
    : NetworkQuality(InvalidRTT(), InvalidRTT(), INVALID_RTT_THROUGHPUT) {
  VerifyValueCorrectness();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NetworkQuality::NetworkQuality(const base::TimeDelta& http_rtt,
                               const base::TimeDelta& transport_rtt,
                               int32_t downstream_throughput_kbps)
    : http_rtt_(http_rtt),
      transport_rtt_(transport_rtt),
      downstream_throughput_kbps_(downstream_throughput_kbps) {
  VerifyValueCorrectness();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NetworkQuality::NetworkQuality(const NetworkQuality& other)
    : NetworkQuality(other.http_rtt_,
                     other.transport_rtt_,
                     other.downstream_throughput_kbps_) {
  VerifyValueCorrectness();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NetworkQuality::~NetworkQuality() = default;

NetworkQuality& NetworkQuality::operator=(const NetworkQuality& other) {
  http_rtt_ = other.http_rtt_;
  transport_rtt_ = other.transport_rtt_;
  downstream_throughput_kbps_ = other.downstream_throughput_kbps_;
  VerifyValueCorrectness();
  DETACH_FROM_SEQUENCE(sequence_checker_);
  return *this;
}

bool NetworkQuality::operator==(const NetworkQuality& other) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return http_rtt_ == other.http_rtt_ &&
         transport_rtt_ == other.transport_rtt_ &&
         downstream_throughput_kbps_ == other.downstream_throughput_kbps_;
}

bool NetworkQuality::IsFaster(const NetworkQuality& other) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return (http_rtt() == InvalidRTT() || other.http_rtt() == InvalidRTT() ||
          http_rtt() <= other.http_rtt()) &&
         (transport_rtt() == InvalidRTT() ||
          other.transport_rtt() == InvalidRTT() ||
          transport_rtt() <= other.transport_rtt()) &&
         (downstream_throughput_kbps() == INVALID_RTT_THROUGHPUT ||
          other.downstream_throughput_kbps() == INVALID_RTT_THROUGHPUT ||
          downstream_throughput_kbps() >= other.downstream_throughput_kbps());
}

void NetworkQuality::VerifyValueCorrectness() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_LE(INVALID_RTT_THROUGHPUT, http_rtt_.InMilliseconds());
  DCHECK_LE(INVALID_RTT_THROUGHPUT, transport_rtt_.InMilliseconds());
  DCHECK_LE(INVALID_RTT_THROUGHPUT, downstream_throughput_kbps_);
}

}  // namespace net::nqe::internal
