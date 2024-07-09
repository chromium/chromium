// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transport.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/modules/peerconnection/intercepting_network_controller.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

class FeedbackReceiverImpl : public FeedbackReceiver {
 public:
  FeedbackReceiverImpl(RTCRtpTransport* rtc_rtp_transport,
                       scoped_refptr<base::SequencedTaskRunner> task_runner)
      : rtc_rtp_transport_(rtc_rtp_transport),
        task_runner_(std::move(task_runner)) {}

  void OnFeedback(webrtc::TransportPacketsFeedback feedback) override {
    // Called on a WebRTC thread.
    CHECK(!task_runner_->RunsTasksInCurrentSequence());
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &FeedbackReceiverImpl::OnFeedbackOnDestinationTaskRunner,
            WrapRefCounted(this), feedback));
  }

  void OnFeedbackOnDestinationTaskRunner(
      webrtc::TransportPacketsFeedback feedback) {
    CHECK(task_runner_->RunsTasksInCurrentSequence());
    if (rtc_rtp_transport_) {
      rtc_rtp_transport_->OnFeedback(feedback);
    }
  }

  void OnSentPacket(webrtc::SentPacket sp) override {
    // Called on a WebRTC thread.
    CHECK(!task_runner_->RunsTasksInCurrentSequence());
    PostCrossThreadTask(
        *task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &FeedbackReceiverImpl::OnSentPacketOnDestinationTaskRunner,
            WrapRefCounted(this), sp));
  }

  void OnSentPacketOnDestinationTaskRunner(webrtc::SentPacket sp) {
    CHECK(task_runner_->RunsTasksInCurrentSequence());
    if (rtc_rtp_transport_) {
      rtc_rtp_transport_->OnSentPacket(sp);
    }
  }

 private:
  WeakPersistent<RTCRtpTransport> rtc_rtp_transport_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace

void RTCRtpTransport::Register(webrtc::NetworkControllerInterface* controller) {
  InterceptingNetworkController* intercepting_controller =
      static_cast<InterceptingNetworkController*>(controller);
  intercepting_controller->SetFeedbackReceiver(
      base::MakeRefCounted<FeedbackReceiverImpl>(
          this, To<LocalDOMWindow>(GetExecutionContext())
                    ->GetTaskRunner(TaskType::kInternalMedia)));
}

webrtc::NetworkControlUpdate RTCRtpTransport::OnFeedback(
    webrtc::TransportPacketsFeedback feedback) {
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

HeapVector<Member<RTCRtpAcks>> RTCRtpTransport::readReceivedAcks(
    uint32_t maxCount) {
  HeapVector<Member<RTCRtpAcks>> acks_messages;
  if (acks_messages_.size() <= maxCount) {
    std::swap(acks_messages, acks_messages_);
  } else {
    auto begin = acks_messages_.begin();
    acks_messages.AppendRange(begin, begin + maxCount);
    acks_messages_.erase(begin, begin + maxCount);
  }
  return acks_messages;
}

void RTCRtpTransport::OnSentPacket(webrtc::SentPacket sp) {
  sents_.push_back(MakeGarbageCollected<RTCRtpSent>(
      sp.send_time.ms<double>(), sp.sequence_number, sp.size.bytes()));
}

HeapVector<Member<RTCRtpSent>> RTCRtpTransport::readSentRtp(uint32_t maxCount) {
  HeapVector<Member<RTCRtpSent>> sents;
  if (sents_.size() <= maxCount) {
    std::swap(sents, sents_);
  } else {
    auto begin = sents_.begin();
    sents.AppendRange(begin, begin + maxCount);
    sents_.erase(begin, begin + maxCount);
  }
  return sents;
}

void RTCRtpTransport::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(acks_messages_);
  visitor->Trace(sents_);
}

}  // namespace blink
