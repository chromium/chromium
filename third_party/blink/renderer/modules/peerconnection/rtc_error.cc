// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_error.h"

#include <utility>

#include "base/notreached.h"

namespace blink {

namespace {

V8RTCErrorDetailType::Enum RTCErrorDetailToEnum(
    webrtc::RTCErrorDetailType detail) {
  switch (detail) {
    case webrtc::RTCErrorDetailType::NONE:
      return V8RTCErrorDetailType::Enum::kNoInfo;
    case webrtc::RTCErrorDetailType::DATA_CHANNEL_FAILURE:
      return V8RTCErrorDetailType::Enum::kDataChannelFailure;
    case webrtc::RTCErrorDetailType::DTLS_FAILURE:
      return V8RTCErrorDetailType::Enum::kDtlsFailure;
    case webrtc::RTCErrorDetailType::FINGERPRINT_FAILURE:
      return V8RTCErrorDetailType::Enum::kFingerprintFailure;
    case webrtc::RTCErrorDetailType::SCTP_FAILURE:
      return V8RTCErrorDetailType::Enum::kSctpFailure;
    case webrtc::RTCErrorDetailType::SDP_SYNTAX_ERROR:
      return V8RTCErrorDetailType::Enum::kSdpSyntaxError;
    case webrtc::RTCErrorDetailType::HARDWARE_ENCODER_NOT_AVAILABLE:
      return V8RTCErrorDetailType::Enum::kHardwareEncoderNotAvailable;
    case webrtc::RTCErrorDetailType::HARDWARE_ENCODER_ERROR:
      return V8RTCErrorDetailType::Enum::kHardwareEncoderError;
    default:
      // Included to ease introduction of new errors at the webrtc layer.
      NOTREACHED();
  }
}
}  // namespace

// static
RTCError* RTCError::Create(const RTCErrorInit* init, String message) {
  return MakeGarbageCollected<RTCError>(init, std::move(message));
}

RTCError::RTCError(const RTCErrorInit* init, String message)
    : DOMException(DOMExceptionCode::kOperationError, std::move(message)),
      error_detail_(init->errorDetail().AsEnum()),
      sdp_line_number_(init->hasSdpLineNumber()
                           ? std::optional<int32_t>(init->sdpLineNumber())
                           : std::nullopt),
      http_request_status_code_(
          init->hasHttpRequestStatusCode()
              ? std::optional<int32_t>(init->httpRequestStatusCode())
              : std::nullopt),
      sctp_cause_code_(init->hasSctpCauseCode()
                           ? std::optional<int32_t>(init->sctpCauseCode())
                           : std::nullopt),
      received_alert_(init->hasReceivedAlert()
                          ? std::optional<uint32_t>(init->receivedAlert())
                          : std::nullopt),
      sent_alert_(init->hasSentAlert()
                      ? std::optional<uint32_t>(init->sentAlert())
                      : std::nullopt) {}

RTCError::RTCError(webrtc::RTCError err)
    : DOMException(DOMExceptionCode::kOperationError, err.message()),
      error_detail_(RTCErrorDetailToEnum(err.error_detail())),
      sctp_cause_code_(err.sctp_cause_code()
                           ? std::optional<int32_t>(*err.sctp_cause_code())
                           : std::nullopt) {}

V8RTCErrorDetailType RTCError::errorDetail() const {
  return V8RTCErrorDetailType(error_detail_);
}

}  // namespace blink
