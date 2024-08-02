// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_INTERCEPTING_NETWORK_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_INTERCEPTING_NETWORK_CONTROLLER_H_

#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_transport.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/transport/network_control.h"

namespace blink {

// Implementation of NetworkControllerInterface intercepting calls to methods
// we're interested in, forwarding the rest on to a supplied
// fallback_controller instance.
class InterceptingNetworkController
    : public webrtc::NetworkControllerInterface {
 public:
  explicit InterceptingNetworkController(
      std::unique_ptr<webrtc::NetworkControllerInterface> fallback_controller,
      CrossThreadWeakHandle<RTCRtpTransport> rtp_transport_handle,
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : fallback_controller_(std::move(fallback_controller)),
        feedback_receiver_(
            base::MakeRefCounted<FeedbackReceiver>(rtp_transport_handle,
                                                   std::move(task_runner))) {}

  // Called when network availability changes.
  webrtc::NetworkControlUpdate OnNetworkAvailability(
      webrtc::NetworkAvailability na) override {
    return fallback_controller_->OnNetworkAvailability(na);
  }
  // Called when the receiving or sending endpoint changes address.
  webrtc::NetworkControlUpdate OnNetworkRouteChange(
      webrtc::NetworkRouteChange nrc) override {
    return fallback_controller_->OnNetworkRouteChange(nrc);
  }
  // Called periodically with a periodicy as specified by
  // NetworkControllerFactoryInterface::GetProcessInterval.
  webrtc::NetworkControlUpdate OnProcessInterval(
      webrtc::ProcessInterval pi) override {
    return fallback_controller_->OnProcessInterval(pi);
  }
  // Called when remotely calculated bitrate is received.
  webrtc::NetworkControlUpdate OnRemoteBitrateReport(
      webrtc::RemoteBitrateReport rbr) override {
    return fallback_controller_->OnRemoteBitrateReport(rbr);
  }
  // Called round trip time has been calculated by protocol specific mechanisms.
  webrtc::NetworkControlUpdate OnRoundTripTimeUpdate(
      webrtc::RoundTripTimeUpdate rttu) override {
    return fallback_controller_->OnRoundTripTimeUpdate(rttu);
  }
  // Called when a packet is sent on the network.
  webrtc::NetworkControlUpdate OnSentPacket(webrtc::SentPacket sp) override {
    feedback_receiver_->OnSentPacket(sp);
    return fallback_controller_->OnSentPacket(sp);
  }
  // Called when a packet is received from the remote client.
  webrtc::NetworkControlUpdate OnReceivedPacket(
      webrtc::ReceivedPacket rp) override {
    return fallback_controller_->OnReceivedPacket(rp);
  }
  // Called when the stream specific configuration has been updated.
  webrtc::NetworkControlUpdate OnStreamsConfig(
      webrtc::StreamsConfig sc) override {
    return fallback_controller_->OnStreamsConfig(sc);
  }
  // Called when target transfer rate constraints has been changed.
  webrtc::NetworkControlUpdate OnTargetRateConstraints(
      webrtc::TargetRateConstraints trc) override {
    return fallback_controller_->OnTargetRateConstraints(trc);
  }
  // Called when a protocol specific calculation of packet loss has been made.
  webrtc::NetworkControlUpdate OnTransportLossReport(
      webrtc::TransportLossReport tlr) override {
    return fallback_controller_->OnTransportLossReport(tlr);
  }
  // Called with per packet feedback regarding receive time.
  webrtc::NetworkControlUpdate OnTransportPacketsFeedback(
      webrtc::TransportPacketsFeedback tpf) override {
    feedback_receiver_->OnFeedback(tpf);
    return fallback_controller_->OnTransportPacketsFeedback(tpf);
  }
  // Called with network state estimate updates.
  webrtc::NetworkControlUpdate OnNetworkStateEstimate(
      webrtc::NetworkStateEstimate nse) override {
    return fallback_controller_->OnNetworkStateEstimate(nse);
  }

 private:
  class FeedbackReceiver : public WTF::ThreadSafeRefCounted<FeedbackReceiver> {
   public:
    FeedbackReceiver(
        CrossThreadWeakHandle<RTCRtpTransport> rtp_transport_handle,
        scoped_refptr<base::SequencedTaskRunner> task_runner);

    void OnFeedback(webrtc::TransportPacketsFeedback feedback);

    void OnSentPacket(webrtc::SentPacket sp);

   private:
    void OnFeedbackOnDestinationTaskRunner(
        webrtc::TransportPacketsFeedback feedback,
        RTCRtpTransport* rtp_transport);
    void OnSentPacketOnDestinationTaskRunner(webrtc::SentPacket sp,
                                             RTCRtpTransport* rtp_transport);

    // Store just a CrossThreadWeakHandle pointing at an RTCRtpTransport, as
    // we're constructed on a WebRTC thread, only unwrapping in tasks posted to
    // the blink task runner which owns the RTCRtpTransport object.
    const CrossThreadWeakHandle<RTCRtpTransport> rtp_transport_handle_;
    const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  };

  const std::unique_ptr<webrtc::NetworkControllerInterface>
      fallback_controller_;
  const scoped_refptr<FeedbackReceiver> feedback_receiver_;
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_INTERCEPTING_NETWORK_CONTROLLER_H_
