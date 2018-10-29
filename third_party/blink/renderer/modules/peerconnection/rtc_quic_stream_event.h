// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_STREAM_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_STREAM_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"

namespace blink {

class RTCQuicStream;
class RTCQuicStreamEventInit;

class MODULES_EXPORT RTCQuicStreamEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static RTCQuicStreamEvent* Create(RTCQuicStream* stream);
  static RTCQuicStreamEvent* Create(const AtomicString& type,
                                    const RTCQuicStreamEventInit& init);

  ~RTCQuicStreamEvent() override;

  // rtc_quic_stream_event.idl
  RTCQuicStream* stream() const;

  // Event overrides.
  const AtomicString& InterfaceName() const override;

  // For garbage collection.
  void Trace(blink::Visitor*) override;

 private:
  RTCQuicStreamEvent(RTCQuicStream* stream);
  RTCQuicStreamEvent(const AtomicString& type,
                     const RTCQuicStreamEventInit& init);

  Member<RTCQuicStream> stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_STREAM_EVENT_H_
