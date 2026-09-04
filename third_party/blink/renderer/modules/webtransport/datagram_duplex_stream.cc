// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/datagram_duplex_stream.h"

#include <algorithm>
#include <cmath>

#include "third_party/blink/renderer/bindings/modules/v8/v8_web_transport_send_options.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport_datagrams_writable.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

WebTransportDatagramsWritable* DatagramDuplexStream::createWritable(
    ScriptState* script_state,
    WebTransportSendOptions* options,
    ExceptionState& exception_state) {
  return web_transport_->CreateDatagramsWritable(script_state, options,
                                                 exception_state);
}

namespace {

bool ValidateAndNormalizeMaxAge(std::optional<double> max_age,
                                const char* error_message,
                                ExceptionState& exception_state,
                                std::optional<double>* normalized_max_age) {
  if (max_age.has_value() &&
      (max_age.value() < 0 || std::isnan(max_age.value()))) {
    exception_state.ThrowRangeError(error_message);
    return false;
  }

  *normalized_max_age = max_age == 0 ? std::nullopt : max_age;
  return true;
}

}  // namespace

void DatagramDuplexStream::setIncomingMaxAge(std::optional<double> max_age,
                                             ExceptionState& exception_state) {
  if (!ValidateAndNormalizeMaxAge(
          max_age, "incomingMaxAge must be non-negative and not NaN",
          exception_state, &incoming_max_age_)) {
    return;
  }
}

void DatagramDuplexStream::setOutgoingMaxAge(std::optional<double> max_age,
                                             ExceptionState& exception_state) {
  if (!ValidateAndNormalizeMaxAge(
          max_age, "outgoingMaxAge must be non-negative and not NaN",
          exception_state, &outgoing_max_age_)) {
    return;
  }

  // WebTransport uses 0.0 to signal "implementation default".
  web_transport_->setDatagramWritableQueueExpirationDuration(
      outgoing_max_age_.value_or(0.0));
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
