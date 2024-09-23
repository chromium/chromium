// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sent.h"

namespace blink {

double RTCRtpSent::time() {
  return time_;
}
uint64_t RTCRtpSent::ackId() {
  return ackId_;
}
uint64_t RTCRtpSent::size() {
  return size_;
}

}  // namespace blink
