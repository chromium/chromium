// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/p2p/socket_throttler.h"

#include <memory>
#include <utility>

#include "third_party/webrtc/rtc_base/data_rate_limiter.h"
#include "third_party/webrtc/rtc_base/time_utils.h"

namespace network {

namespace {

const int kMaxIceMessageBandwidth = 256 * 1024;

}  // namespace

P2PMessageThrottler::P2PMessageThrottler()
    : rate_limiter_(new rtc::DataRateLimiter(kMaxIceMessageBandwidth, 1.0)) {}

P2PMessageThrottler::~P2PMessageThrottler() {}

void P2PMessageThrottler::SetSendIceBandwidth(int bandwidth_kbps) {
  rate_limiter_ = std::make_unique<rtc::DataRateLimiter>(bandwidth_kbps, 1.0);
}

bool P2PMessageThrottler::DropNextPacket(size_t packet_len) {
  double now = rtc::TimeNanos() / static_cast<double>(rtc::kNumNanosecsPerSec);
  if (!rate_limiter_->CanUse(packet_len, now)) {
    // Exceeding the send rate, this packet should be dropped.
    return true;
  }

  rate_limiter_->Use(packet_len, now);
  return false;
}

}  // namespace network
