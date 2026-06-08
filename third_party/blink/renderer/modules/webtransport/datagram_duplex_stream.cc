// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/datagram_duplex_stream.h"

#include <algorithm>

namespace blink {

void DatagramDuplexStream::setIncomingMaxAge(std::optional<double> max_age) {
  if (!max_age.has_value() || max_age.value() > 0) {
    incoming_max_age_ = max_age;
  }
}

void DatagramDuplexStream::setOutgoingMaxAge(std::optional<double> max_age) {
  if (!max_age.has_value() || max_age.value() > 0) {
    outgoing_max_age_ = max_age;

    // WebTransport uses 0.0 to signal "implementation default".
    web_transport_->setDatagramWritableQueueExpirationDuration(
        max_age.value_or(0.0));
  }
}

void DatagramDuplexStream::setIncomingMaxBufferedDatagrams(uint32_t value) {
  incoming_max_buffered_datagrams_ =
      std::max(value, kMinimumMaxBufferedDatagrams);
}

void DatagramDuplexStream::setOutgoingMaxBufferedDatagrams(uint32_t value) {
  outgoing_max_buffered_datagrams_ =
      std::max(value, kMinimumMaxBufferedDatagrams);
}

void DatagramDuplexStream::setIncomingHighWaterMark(int32_t value) {
  setIncomingMaxBufferedDatagrams(base::saturated_cast<uint32_t>(value));
}

void DatagramDuplexStream::setOutgoingHighWaterMark(int32_t value) {
  setOutgoingMaxBufferedDatagrams(base::saturated_cast<uint32_t>(value));
}

}  // namespace blink
