// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_TRANSCEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_TRANSCEIVER_H_

#include "base/optional.h"
#include "third_party/blink/public/platform/web_rtc_rtp_transceiver.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_codec_capability.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"

namespace blink {

class RTCPeerConnection;
class RTCRtpReceiver;
class RTCRtpSender;

webrtc::RtpTransceiverInit ToRtpTransceiverInit(const RTCRtpTransceiverInit*);

class RTCRtpTransceiver final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  RTCRtpTransceiver(RTCPeerConnection* pc,
                    std::unique_ptr<WebRTCRtpTransceiver>,
                    RTCRtpSender*,
                    RTCRtpReceiver*);

  // rtc_rtp_transceiver.idl
  String mid() const;
  RTCRtpSender* sender() const;
  RTCRtpReceiver* receiver() const;
  bool stopped() const;
  // Enum type RTCRtpTransceiverDirection
  String direction() const;
  void setDirection(String direction, ExceptionState&);
  String currentDirection() const;
  void setCodecPreferences(
      const HeapVector<Member<RTCRtpCodecCapability>>& codecs,
      ExceptionState& exception_state);

  // Updates the transceiver attributes by fetching values from
  // |web_transceiver_|. This is made an explicit operation (rather than
  // fetching the values from the |web_transceivers_| directly every time an
  // attribute is read) to make it possible to look at before/after snapshots of
  // the attributes when a session description has been applied. This is
  // necessary in order for blink to know when to process the addition/removal
  // of remote tracks:
  // https://w3c.github.io/webrtc-pc/#set-the-rtcsessiondescription.
  void UpdateMembers();
  void OnPeerConnectionClosed();

  WebRTCRtpTransceiver* web_transceiver() const;
  base::Optional<webrtc::RtpTransceiverDirection> fired_direction() const;
  bool DirectionHasSend() const;
  bool DirectionHasRecv() const;
  bool FiredDirectionHasRecv() const;

  void Trace(Visitor*) override;

 private:
  Member<RTCPeerConnection> pc_;
  std::unique_ptr<WebRTCRtpTransceiver> web_transceiver_;
  Member<RTCRtpSender> sender_;
  Member<RTCRtpReceiver> receiver_;
  bool stopped_;
  String direction_;
  String current_direction_;
  base::Optional<webrtc::RtpTransceiverDirection> fired_direction_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_RTP_TRANSCEIVER_H_
