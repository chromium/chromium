// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_send_packet_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_transport_config.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_received_packet.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class MODULES_EXPORT RtcTransport final
    : public EventTarget,
      public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  using PassKey = base::PassKey<RtcTransport>;
  static RtcTransport* Create(ExecutionContext* context,
                              const RtcTransportConfig* config);

  RtcTransport(PassKey,
               ExecutionContext* context,
               const RtcTransportConfig* config);
  ~RtcTransport() override;

  // ExecutionContextLifecycleObserver implementation
  void ContextDestroyed() final {}

  // EventTarget
  const AtomicString& InterfaceName() const override {
    return event_target_names::kRTCTransport;
  }

  ExecutionContext* GetExecutionContext() const override {
    return ExecutionContextLifecycleObserver::GetExecutionContext();
  }

  HeapVector<Member<RtcReceivedPacket>> getReceivedPackets();
  void sendPackets(HeapVector<Member<RtcSendPacketParameters>> packets);

  // TODO(crbug.com/443019066): Hook up ICE candidate gathering and fire this
  // event.
  DEFINE_ATTRIBUTE_EVENT_LISTENER(icecandidate, kIcecandidate)

  // ScriptWrappable implementation
  void Trace(Visitor* visitor) const override;

 private:
  HeapVector<Member<RtcReceivedPacket>> received_packets_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_H_
