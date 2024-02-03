// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/datagram_duplex_stream.h"

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

void DatagramDuplexStream::setIncomingHighWaterMark(int high_water_mark) {
  if (high_water_mark >= 0) {
    incoming_high_water_mark_ = high_water_mark;
  }
}

void DatagramDuplexStream::setOutgoingHighWaterMark(int high_water_mark) {
  if (high_water_mark >= 0) {
    outgoing_high_water_mark_ = high_water_mark;
  }
}

}  // namespace blink
