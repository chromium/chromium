// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_TRANSPORT_H_

#include "base/memory/weak_ptr.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_acks.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/webrtc/api/transport/network_control.h"

namespace blink {

class MODULES_EXPORT RTCRtpTransport : public ScriptWrappable,
                                       public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit RTCRtpTransport(ExecutionContext* context)
      : ExecutionContextClient(context) {}

  void Register(webrtc::NetworkControllerInterface* controller);
  webrtc::NetworkControlUpdate OnFeedback(
      webrtc::TransportPacketsFeedback feedback);

  HeapVector<Member<RTCRtpAcks>> readReceivedAcks(uint32_t maxCount);

  void Trace(Visitor* visitor) const override;

  HeapVector<Member<RTCRtpAcks>> acks_messages_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_TRANSPORT_H_
