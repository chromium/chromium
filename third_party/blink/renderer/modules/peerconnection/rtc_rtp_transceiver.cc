// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transceiver.h"

#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

String TransceiverDirectionToString(
    const webrtc::RtpTransceiverDirection& direction) {
  switch (direction) {
    case webrtc::RtpTransceiverDirection::kSendRecv:
      return "sendrecv";
    case webrtc::RtpTransceiverDirection::kSendOnly:
      return "sendonly";
    case webrtc::RtpTransceiverDirection::kRecvOnly:
      return "recvonly";
    case webrtc::RtpTransceiverDirection::kInactive:
      return "inactive";
  }
}

String OptionalTransceiverDirectionToString(
    const base::Optional<webrtc::RtpTransceiverDirection>& direction) {
  return direction ? TransceiverDirectionToString(*direction)
                   : String();  // null
}

bool TransceiverDirectionFromString(
    const String& direction_string,
    base::Optional<webrtc::RtpTransceiverDirection>* direction_out) {
  if (!direction_string) {
    *direction_out = base::nullopt;
    return true;
  }
  if (direction_string == "sendrecv") {
    *direction_out = webrtc::RtpTransceiverDirection::kSendRecv;
    return true;
  }
  if (direction_string == "sendonly") {
    *direction_out = webrtc::RtpTransceiverDirection::kSendOnly;
    return true;
  }
  if (direction_string == "recvonly") {
    *direction_out = webrtc::RtpTransceiverDirection::kRecvOnly;
    return true;
  }
  if (direction_string == "inactive") {
    *direction_out = webrtc::RtpTransceiverDirection::kInactive;
    return true;
  }
  return false;
}

}  // namespace

webrtc::RtpTransceiverInit ToRtpTransceiverInit(
    const RTCRtpTransceiverInit& init) {
  webrtc::RtpTransceiverInit webrtc_init;
  base::Optional<webrtc::RtpTransceiverDirection> direction;
  if (init.hasDirection() &&
      TransceiverDirectionFromString(init.direction(), &direction) &&
      direction) {
    webrtc_init.direction = *direction;
  }
  DCHECK(init.hasStreams());
  for (const auto& stream : init.streams()) {
    webrtc_init.stream_ids.push_back(stream->id().Utf8().data());
  }
  DCHECK(init.hasSendEncodings());
  for (const auto& encoding : init.sendEncodings()) {
    webrtc_init.send_encodings.push_back(ToRtpEncodingParameters(encoding));
  }
  return webrtc_init;
}

RTCRtpTransceiver::RTCRtpTransceiver(
    RTCPeerConnection* pc,
    std::unique_ptr<WebRTCRtpTransceiver> web_transceiver,
    RTCRtpSender* sender,
    RTCRtpReceiver* receiver)
    : pc_(pc),
      web_transceiver_(std::move(web_transceiver)),
      sender_(sender),
      receiver_(receiver),
      fired_direction_(base::nullopt) {
  DCHECK(pc_);
  DCHECK(web_transceiver_);
  DCHECK(sender_);
  DCHECK(receiver_);
  UpdateMembers();
}

String RTCRtpTransceiver::mid() const {
  return web_transceiver_->Mid();
}

RTCRtpSender* RTCRtpTransceiver::sender() const {
  return sender_;
}

RTCRtpReceiver* RTCRtpTransceiver::receiver() const {
  return receiver_;
}

bool RTCRtpTransceiver::stopped() const {
  return stopped_;
}

String RTCRtpTransceiver::direction() const {
  return direction_;
}

void RTCRtpTransceiver::setDirection(String direction,
                                     ExceptionState& exception_state) {
  base::Optional<webrtc::RtpTransceiverDirection> webrtc_direction;
  if (!TransceiverDirectionFromString(direction, &webrtc_direction) ||
      !webrtc_direction) {
    exception_state.ThrowTypeError("Invalid RTCRtpTransceiverDirection.");
    return;
  }
  if (pc_->IsClosed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The peer connection is closed.");
    return;
  }
  if (stopped_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The transceiver is stopped.");
    return;
  }
  web_transceiver_->SetDirection(*webrtc_direction);
  UpdateMembers();
}

String RTCRtpTransceiver::currentDirection() const {
  return current_direction_;
}

void RTCRtpTransceiver::UpdateMembers() {
  stopped_ = web_transceiver_->Stopped();
  direction_ = TransceiverDirectionToString(web_transceiver_->Direction());
  current_direction_ = OptionalTransceiverDirectionToString(
      web_transceiver_->CurrentDirection());
  fired_direction_ = web_transceiver_->FiredDirection();
}

void RTCRtpTransceiver::OnPeerConnectionClosed() {
  receiver_->track()->Component()->Source()->SetReadyState(
      MediaStreamSource::kReadyStateMuted);
  stopped_ = true;
  current_direction_ = String();  // null
}

WebRTCRtpTransceiver* RTCRtpTransceiver::web_transceiver() const {
  return web_transceiver_.get();
}

base::Optional<webrtc::RtpTransceiverDirection>
RTCRtpTransceiver::fired_direction() const {
  return fired_direction_;
}

bool RTCRtpTransceiver::DirectionHasSend() const {
  auto direction = web_transceiver_->Direction();
  return direction == webrtc::RtpTransceiverDirection::kSendRecv ||
         direction == webrtc::RtpTransceiverDirection::kSendOnly;
}

bool RTCRtpTransceiver::DirectionHasRecv() const {
  auto direction = web_transceiver_->Direction();
  return direction == webrtc::RtpTransceiverDirection::kSendRecv ||
         direction == webrtc::RtpTransceiverDirection::kRecvOnly;
}

bool RTCRtpTransceiver::FiredDirectionHasRecv() const {
  return fired_direction_ &&
         (*fired_direction_ == webrtc::RtpTransceiverDirection::kSendRecv ||
          *fired_direction_ == webrtc::RtpTransceiverDirection::kRecvOnly);
}

void RTCRtpTransceiver::Trace(Visitor* visitor) {
  visitor->Trace(pc_);
  visitor->Trace(sender_);
  visitor->Trace(receiver_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
