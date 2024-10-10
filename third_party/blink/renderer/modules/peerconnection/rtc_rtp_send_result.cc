// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_send_result.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_rtp_unsent_reason.h"

namespace blink {

RTCRtpSent* RTCRtpSendResult::sent() {
  // TODO(crbug.com/345101934): Implement me.
  return nullptr;
}

std::optional<V8RTCRtpUnsentReason> RTCRtpSendResult::unsent() {
  // TODO(crbug.com/345101934): Implement me.
  return std::nullopt;
}

}  // namespace blink
