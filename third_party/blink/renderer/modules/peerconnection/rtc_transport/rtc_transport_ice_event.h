// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_ICE_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_ICE_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_ice_candidate.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class MODULES_EXPORT RtcTransportIceEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit RtcTransportIceEvent(RtcTransportIceCandidate*);
  ~RtcTransportIceEvent() override = default;

  RtcTransportIceCandidate* candidate() const;

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) const override;

 private:
  Member<RtcTransportIceCandidate> candidate_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_RTC_TRANSPORT_ICE_EVENT_H_
