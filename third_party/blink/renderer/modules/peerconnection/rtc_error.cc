// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_error.h"

#include <utility>

namespace blink {

// static
RTCError* RTCError::Create(const RTCErrorInit* init, String message) {
  return MakeGarbageCollected<RTCError>(init, std::move(message));
}

RTCError::RTCError(const RTCErrorInit* init, String message)
    : DOMException(0u, "RTCError", std::move(message), String()),
      error_detail_(init->errorDetail()),
      sdp_line_number_(init->hasSdpLineNumber()
                           ? base::Optional<int32_t>(init->sdpLineNumber())
                           : base::nullopt),
      http_request_status_code_(
          init->hasHttpRequestStatusCode()
              ? base::Optional<int32_t>(init->httpRequestStatusCode())
              : base::nullopt),
      sctp_cause_code_(init->hasSctpCauseCode()
                           ? base::Optional<int32_t>(init->sctpCauseCode())
                           : base::nullopt),
      received_alert_(init->hasReceivedAlert()
                          ? base::Optional<uint32_t>(init->receivedAlert())
                          : base::nullopt),
      sent_alert_(init->hasSentAlert()
                      ? base::Optional<uint32_t>(init->sentAlert())
                      : base::nullopt) {}

const String& RTCError::errorDetail() const {
  return error_detail_;
}

int32_t RTCError::sdpLineNumber(bool& is_null) const {
  is_null = !sdp_line_number_;
  return sdp_line_number_ ? *sdp_line_number_ : 0;
}

int32_t RTCError::httpRequestStatusCode(bool& is_null) const {
  is_null = !http_request_status_code_;
  return http_request_status_code_ ? *http_request_status_code_ : 0;
}

int32_t RTCError::sctpCauseCode(bool& is_null) const {
  is_null = !sctp_cause_code_;
  return sctp_cause_code_ ? *sctp_cause_code_ : 0;
}

uint32_t RTCError::receivedAlert(bool& is_null) const {
  is_null = !received_alert_;
  return received_alert_ ? *received_alert_ : 0u;
}

uint32_t RTCError::sentAlert(bool& is_null) const {
  is_null = !sent_alert_;
  return sent_alert_ ? *sent_alert_ : 0u;
}

}  // namespace blink
