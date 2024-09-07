// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transport_processor.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/intercepting_network_controller.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transport.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

RTCRtpTransportProcessor::RTCRtpTransportProcessor(ExecutionContext* context)
    : ExecutionContextClient(context) {}

RTCRtpTransportProcessor::~RTCRtpTransportProcessor() = default;

void RTCRtpTransportProcessor::SetFeedbackProviders(
    Vector<scoped_refptr<FeedbackProvider>> feedback_providers) {
  feedback_providers_ = feedback_providers;
}

webrtc::NetworkControlUpdate RTCRtpTransportProcessor::OnFeedback(
    webrtc::TransportPacketsFeedback feedback) {
  CHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  HeapVector<Member<RTCRtpAck>> acks;
  for (const webrtc::PacketResult& result : feedback.packet_feedbacks) {
    RTCRtpAck* ack = RTCRtpAck::Create();
    // TODO: crbug.com/345101934 - Handle unset (infinite) result.receive_time.
    ack->setRemoteReceiveTimestamp(
        result.receive_time.IsFinite() ? result.receive_time.ms() : 0);
    ack->setAckId(result.sent_packet.sequence_number);
    acks.push_back(ack);
  }
  // TODO: crbug.com/345101934 - Actually fill in a received time & ECN.
  // TODO: crbug.com/345101934 - Handle unset feedback_time.
  // TODO: crbug.com/345101934 - Have a max size for acks_messages_ to prevent
  // unbound growth if JS never calls readReceivedAcks(), and implement stats to
  // tell JS that things were dropped as suggested on
  // https://github.com/w3c/webrtc-rtptransport/pull/42#issuecomment-2142665283.
  acks_messages_.push_back(MakeGarbageCollected<RTCRtpAcks>(
      acks, feedback.feedback_time.IsFinite() ? feedback.feedback_time.ms() : 0,
      /*received_time=*/0, /*explicit_congestion_notification=*/"unset"));

  return webrtc::NetworkControlUpdate();
}

HeapVector<Member<RTCRtpAcks>> RTCRtpTransportProcessor::readReceivedAcks(
    uint32_t maxCount) {
  CHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  HeapVector<Member<RTCRtpAcks>> acks_messages;
  while (acks_messages.size() < maxCount && !acks_messages_.empty()) {
    acks_messages.push_back(acks_messages_.TakeFirst());
  }
  return acks_messages;
}

void RTCRtpTransportProcessor::OnSentPacket(webrtc::SentPacket sp) {
  CHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  sents_.push_back(MakeGarbageCollected<RTCRtpSent>(
      sp.send_time.ms<double>(), sp.sequence_number, sp.size.bytes()));
}

HeapVector<Member<RTCRtpSent>> RTCRtpTransportProcessor::readSentRtp(
    uint32_t maxCount) {
  CHECK(!GetExecutionContext() || GetExecutionContext()->IsContextThread());
  HeapVector<Member<RTCRtpSent>> sents;
  while (sents.size() < maxCount && !sents_.empty()) {
    sents.push_back(sents_.TakeFirst());
  }
  return sents;
}

void RTCRtpTransportProcessor::setCustomMaxBandwidth(
    uint64_t custom_max_bitrate_bps) {
  custom_max_bitrate_bps_ = custom_max_bitrate_bps;

  for (auto& feedback_provider : feedback_providers_) {
    feedback_provider->SetCustomMaxBitrateBps(custom_max_bitrate_bps);
  }
}

void RTCRtpTransportProcessor::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(acks_messages_);
  visitor->Trace(sents_);
}

}  // namespace blink
