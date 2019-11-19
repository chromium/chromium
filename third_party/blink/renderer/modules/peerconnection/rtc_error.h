// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ERROR_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_error_init.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class RTCError final : public DOMException {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RTCError* Create(const RTCErrorInit* init, String message);
  RTCError(const RTCErrorInit* init, String message);

  const String& errorDetail() const;
  int32_t sdpLineNumber(bool& is_null) const;
  int32_t httpRequestStatusCode(bool& is_null) const;
  int32_t sctpCauseCode(bool& is_null) const;
  uint32_t receivedAlert(bool& is_null) const;
  uint32_t sentAlert(bool& is_null) const;

 private:
  // idl enum RTCErrorDetailType.
  String error_detail_;
  base::Optional<int32_t> sdp_line_number_;
  base::Optional<int32_t> http_request_status_code_;
  base::Optional<int32_t> sctp_cause_code_;
  base::Optional<uint32_t> received_alert_;
  base::Optional<uint32_t> sent_alert_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_ERROR_H_
