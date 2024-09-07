// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/intercepting_network_controller.h"

#include "third_party/blink/renderer/modules/peerconnection/adapters/web_rtc_cross_thread_copier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

webrtc::TargetTransferRate CreateTargetTransferRate(
    webrtc::Timestamp at_time,
    uint64_t custom_max_bitrate_bps) {
  webrtc::DataRate data_rate =
      webrtc::DataRate::BitsPerSec(custom_max_bitrate_bps);

  webrtc::NetworkEstimate network_estimate;
  network_estimate.at_time = at_time;
  network_estimate.bandwidth = data_rate;

  // This RTT is used within libwebrtc to configure FEC.
  // TODO(crbug.com/345101934): Supply it from a different RTT estimator, or add
  // it to the JS interface.
  network_estimate.round_trip_time = webrtc::TimeDelta::Millis(0);
  // The bwe_period field is deprecated in webrtc, having been replaced by
  // stable_target_rate, but must still be set.
  network_estimate.bwe_period = webrtc::TimeDelta::Millis(0);
  return {
      .at_time = at_time,
      .network_estimate = network_estimate,
      .target_rate = data_rate,
      .stable_target_rate = data_rate,
  };
}

webrtc::NetworkControlUpdate OverwriteTargetRate(
    webrtc::NetworkControlUpdate fallback_update,
    std::optional<uint64_t> custom_max_bitrate_bps) {
  if (!fallback_update.target_rate || !custom_max_bitrate_bps) {
    return fallback_update;
  }
  webrtc::TargetTransferRate target_transfer_rate = CreateTargetTransferRate(
      fallback_update.target_rate->at_time, *custom_max_bitrate_bps);

  fallback_update.target_rate = target_transfer_rate;
  return fallback_update;
}

}  // namespace

InterceptingNetworkController::InterceptingNetworkController(
    std::unique_ptr<webrtc::NetworkControllerInterface> fallback_controller,
    CrossThreadWeakHandle<RTCRtpTransport> rtp_transport_handle,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : fallback_controller_(std::move(fallback_controller)),
      feedback_provider_(base::MakeRefCounted<FeedbackProviderImpl>()) {
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          &RTCRtpTransport::RegisterFeedbackProvider,
          MakeUnwrappingCrossThreadWeakHandle(rtp_transport_handle),
          feedback_provider_));
}

webrtc::NetworkControlUpdate
InterceptingNetworkController::OnNetworkAvailability(
    webrtc::NetworkAvailability na) {
  return OverwriteTargetRate(fallback_controller_->OnNetworkAvailability(na),
                             feedback_provider_->CustomMaxBitrateBps());
}

webrtc::NetworkControlUpdate
InterceptingNetworkController::OnNetworkRouteChange(
    webrtc::NetworkRouteChange nrc) {
  return OverwriteTargetRate(fallback_controller_->OnNetworkRouteChange(nrc),
                             feedback_provider_->CustomMaxBitrateBps());
}

webrtc::NetworkControlUpdate InterceptingNetworkController::OnProcessInterval(
    webrtc::ProcessInterval pi) {
  webrtc::NetworkControlUpdate fallback_update =
      fallback_controller_->OnProcessInterval(pi);

  if (!feedback_provider_->CustomMaxBitrateBps()) {
    return fallback_update;
  }
  webrtc::TargetTransferRate target_rate = CreateTargetTransferRate(
      pi.at_time, *feedback_provider_->CustomMaxBitrateBps());
  webrtc::NetworkControlUpdate update;
  update.target_rate = target_rate;
  return update;
}

webrtc::NetworkControlUpdate
InterceptingNetworkController::OnRemoteBitrateReport(
    webrtc::RemoteBitrateReport rbr) {
  return OverwriteTargetRate(fallback_controller_->OnRemoteBitrateReport(rbr),
                             feedback_provider_->CustomMaxBitrateBps());
}

webrtc::NetworkControlUpdate
InterceptingNetworkController::OnRoundTripTimeUpdate(
    webrtc::RoundTripTimeUpdate rttu) {
  return OverwriteTargetRate(fallback_controller_->OnRoundTripTimeUpdate(rttu),
                             feedback_provider_->CustomMaxBitrateBps());
}

webrtc::NetworkControlUpdate InterceptingNetworkController::OnSentPacket(
    webrtc::SentPacket sp) {
  feedback_provider_->OnSentPacket(sp);
  return OverwriteTargetRate(fallback_controller_->OnSentPacket(sp),
                             feedback_provider_->CustomMaxBitrateBps());
}

webrtc::NetworkControlUpdate InterceptingNetworkController::OnReceivedPacket(
    webrtc::ReceivedPacket rp) {
  return OverwriteTargetRate(fallback_controller_->OnReceivedPacket(rp),
                             feedback_provider_->CustomMaxBitrateBps());
}

webrtc::NetworkControlUpdate InterceptingNetworkController::OnStreamsConfig(
    webrtc::StreamsConfig sc) {
  return OverwriteTargetRate(fallback_controller_->OnStreamsConfig(sc),
                             feedback_provider_->CustomMaxBitrateBps());
}

webrtc::NetworkControlUpdate
InterceptingNetworkController::OnTargetRateConstraints(
    webrtc::TargetRateConstraints trc) {
  return OverwriteTargetRate(fallback_controller_->OnTargetRateConstraints(trc),
                             feedback_provider_->CustomMaxBitrateBps());
}

webrtc::NetworkControlUpdate
InterceptingNetworkController::OnTransportLossReport(
    webrtc::TransportLossReport tlr) {
  return OverwriteTargetRate(fallback_controller_->OnTransportLossReport(tlr),
                             feedback_provider_->CustomMaxBitrateBps());
}

webrtc::NetworkControlUpdate
InterceptingNetworkController::OnTransportPacketsFeedback(
    webrtc::TransportPacketsFeedback tpf) {
  feedback_provider_->OnFeedback(tpf);
  return OverwriteTargetRate(
      fallback_controller_->OnTransportPacketsFeedback(tpf),
      feedback_provider_->CustomMaxBitrateBps());
}

webrtc::NetworkControlUpdate
InterceptingNetworkController::OnNetworkStateEstimate(
    webrtc::NetworkStateEstimate nse) {
  return OverwriteTargetRate(fallback_controller_->OnNetworkStateEstimate(nse),
                             feedback_provider_->CustomMaxBitrateBps());
}

void InterceptingNetworkController::FeedbackProviderImpl::OnFeedback(
    webrtc::TransportPacketsFeedback feedback) {
  // Called on a WebRTC thread.
  base::AutoLock mutex(processor_lock_);
  // TODO(crbug.com/345101934): Consider buffering these until the
  // processor_handle has been created and then replaying them.
  if (!rtp_transport_processor_handle_) {
    return;
  }
  CHECK(!rtp_transport_processor_task_runner_->RunsTasksInCurrentSequence());
  PostCrossThreadTask(
      *rtp_transport_processor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&InterceptingNetworkController::FeedbackProviderImpl::
                              OnFeedbackOnDestinationTaskRunner,
                          WrapRefCounted(this), feedback,
                          MakeUnwrappingCrossThreadWeakHandle(
                              *rtp_transport_processor_handle_)));
}

void InterceptingNetworkController::FeedbackProviderImpl::
    OnFeedbackOnDestinationTaskRunner(
        webrtc::TransportPacketsFeedback feedback,
        RTCRtpTransportProcessor* rtp_transport_processor) {
  // Runs on the task runner  matching the JS thread of rtp_transport_processor.
  if (rtp_transport_processor) {
    rtp_transport_processor->OnFeedback(feedback);
  }
}

void InterceptingNetworkController::FeedbackProviderImpl::OnSentPacket(
    webrtc::SentPacket sp) {
  // Called on a WebRTC thread.
  base::AutoLock mutex(processor_lock_);
  // TODO(crbug.com/345101934): Consider buffering these until the
  // processor_handle has been created and then replaying them
  if (!rtp_transport_processor_handle_) {
    return;
  }
  CHECK(!rtp_transport_processor_task_runner_->RunsTasksInCurrentSequence());
  PostCrossThreadTask(
      *rtp_transport_processor_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&InterceptingNetworkController::FeedbackProviderImpl::
                              OnSentPacketOnDestinationTaskRunner,
                          WrapRefCounted(this), sp,
                          MakeUnwrappingCrossThreadWeakHandle(
                              *rtp_transport_processor_handle_)));
}

void InterceptingNetworkController::FeedbackProviderImpl::
    OnSentPacketOnDestinationTaskRunner(
        webrtc::SentPacket sp,
        RTCRtpTransportProcessor* rtp_transport_processor) {
  // Runs on the task runner matching the JS thread of rtp_transport_processor
  // ie a worker.
  if (rtp_transport_processor) {
    rtp_transport_processor->OnSentPacket(sp);
  }
}

void InterceptingNetworkController::FeedbackProviderImpl::SetProcessor(
    CrossThreadWeakHandle<RTCRtpTransportProcessor>
        rtp_transport_processor_handle,
    scoped_refptr<base::SequencedTaskRunner>
        rtp_transport_processor_task_runner) {
  // Called on the main JS thread owning the RTCRtpTransport instance.
  base::AutoLock mutex(processor_lock_);
  rtp_transport_processor_handle_.emplace(
      std::move(rtp_transport_processor_handle));
  rtp_transport_processor_task_runner_ =
      std::move(rtp_transport_processor_task_runner);
}

}  // namespace blink
