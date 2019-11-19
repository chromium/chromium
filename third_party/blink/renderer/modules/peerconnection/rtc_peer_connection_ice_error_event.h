// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_ICE_ERROR_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_ICE_ERROR_EVENT_H_

#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class RTCPeerConnectionIceErrorEventInit;

class MODULES_EXPORT RTCPeerConnectionIceErrorEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RTCPeerConnectionIceErrorEvent(const String& host_candidate,
                                 const String& url,
                                 uint16_t error_code,
                                 const String& error_text);

  RTCPeerConnectionIceErrorEvent(const AtomicString& type,
                                 const RTCPeerConnectionIceErrorEventInit*);
  ~RTCPeerConnectionIceErrorEvent() override;

  static RTCPeerConnectionIceErrorEvent* Create(const String& host_candidate,
                                                const String& url,
                                                int error_code,
                                                const String& error_text);

  static RTCPeerConnectionIceErrorEvent* Create(
      const AtomicString& type,
      const RTCPeerConnectionIceErrorEventInit*);

  String hostCandidate() const;
  String url() const;
  uint16_t errorCode() const;
  String errorText() const;
  const AtomicString& InterfaceName() const override;

  void Trace(blink::Visitor*) override;

 private:
  String host_candidate_;
  String url_;
  uint16_t error_code_;
  String error_text_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_ICE_ERROR_EVENT_H_
