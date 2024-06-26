// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_send_stream.h"

namespace blink {

ScriptPromise<RTCRtpSendResult> RTCRtpSendStream::sendRtp(
    ScriptState* script_state,
    RTCRtpPacketInit* packet,
    RTCRtpSendOptions* options) {
  // TODO(crbug.com/345101934): Implement me :D
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<RTCRtpSendResult>>(
          script_state);
  return resolver->Promise();
}

}  // namespace blink
