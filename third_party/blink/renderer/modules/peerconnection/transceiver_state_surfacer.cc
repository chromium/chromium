// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/transceiver_state_surfacer.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/webrtc/api/rtp_transceiver_interface.h"
#include "third_party/webrtc/api/sctp_transport_interface.h"

namespace blink {
namespace {

Vector<webrtc::RtpHeaderExtensionCapability> GetNegotiatedHeaderExtensions(
    const webrtc::RtpTransceiverInterface* webrtc_transceiver) {
  auto std_extensions = webrtc_transceiver->GetNegotiatedHeaderExtensions();
  Vector<webrtc::RtpHeaderExtensionCapability> extensions;
  std::move(std_extensions.begin(), std_extensions.end(),
            std::back_inserter(extensions));
  return extensions;
}

}  // namespace

TransceiverStateSurfacer::TransceiverStateSurfacer(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner)
    : main_task_runner_(std::move(main_task_runner)),
      signaling_task_runner_(std::move(signaling_task_runner)),
      is_initialized_(false) {
  DCHECK(main_task_runner_);
  DCHECK(signaling_task_runner_);
}

TransceiverStateSurfacer::TransceiverStateSurfacer(
    TransceiverStateSurfacer&& other)
    : main_task_runner_(other.main_task_runner_),
      signaling_task_runner_(other.signaling_task_runner_),
      is_initialized_(other.is_initialized_),
      sctp_transport_snapshot_(other.sctp_transport_snapshot_),
      transceiver_states_(std::move(other.transceiver_states_)) {
  // Explicitly null |other|'s task runners for use in destructor.
  other.main_task_runner_ = nullptr;
  other.signaling_task_runner_ = nullptr;
}

TransceiverStateSurfacer::~TransceiverStateSurfacer() {
  // It's OK to not be on the main thread if this object has been moved, in
  // which case |main_task_runner_| is null.
  DCHECK(!main_task_runner_ || main_task_runner_->BelongsToCurrentThread());
}

void TransceiverStateSurfacer::Initialize(
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection,
    scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
        webrtc_transceivers) {
  DCHECK(signaling_task_runner_->BelongsToCurrentThread());
  DCHECK(!is_initialized_);
  DCHECK(native_peer_connection);
  sctp_transport_snapshot_.transport =
      native_peer_connection->GetSctpTransport();
  if (sctp_transport_snapshot_.transport) {
    sctp_transport_snapshot_.sctp_transport_state =
        sctp_transport_snapshot_.transport->Information();
    if (sctp_transport_snapshot_.sctp_transport_state.dtls_transport()) {
      sctp_transport_snapshot_.dtls_transport_state =
          sctp_transport_snapshot_.sctp_transport_state.dtls_transport()
              ->Information();
    }
  }

  for (auto& webrtc_transceiver : webrtc_transceivers) {
    // Create the sender state.
    std::optional<blink::RtpSenderState> sender_state;
    auto webrtc_sender = webrtc_transceiver->sender();
    if (webrtc_sender) {
      std::unique_ptr<blink::WebRtcMediaStreamTrackAdapterMap::AdapterRef>
          sender_track_ref;
      if (webrtc_sender->track()) {
        // The track adapter for this track must already exist for us to obtain
        // it, since this cannot be created from the signaling thread.
        // TODO(hbos): Consider either making it possible to create local track
        // adapters on the signaling thread for initialization on the main
        // thread or wait for Onion Souping to simplify this.
        // https://crbug.com/787254
        sender_track_ref = track_adapter_map->GetLocalTrackAdapter(
            webrtc_sender->track().get());
        CHECK(sender_track_ref);
      }
      sender_state = blink::RtpSenderState(
          main_task_runner_, signaling_task_runner_, webrtc_sender,
          std::move(sender_track_ref), webrtc_sender->stream_ids());
    }
    // Create the receiver state.
    std::optional<blink::RtpReceiverState> receiver_state;
    auto webrtc_receiver = webrtc_transceiver->receiver();
    if (webrtc_receiver) {
      DCHECK(webrtc_receiver->track());
      auto receiver_track_ref =
          track_adapter_map->GetOrCreateRemoteTrackAdapter(
              webrtc_receiver->track().get());
      DCHECK(receiver_track_ref);
      std::vector<std::string> receiver_stream_ids;
      for (auto& stream : webrtc_receiver->streams()) {
        receiver_stream_ids.push_back(stream->id());
      }
      receiver_state = blink::RtpReceiverState(
          main_task_runner_, signaling_task_runner_, webrtc_receiver.get(),
          std::move(receiver_track_ref), std::move(receiver_stream_ids));
    }

    // Create the transceiver state.
    transceiver_states_.emplace_back(
        main_task_runner_, signaling_task_runner_, webrtc_transceiver.get(),
        std::move(sender_state), std::move(receiver_state),
        webrtc_transceiver->mid(), webrtc_transceiver->direction(),
        webrtc_transceiver->current_direction(),
        webrtc_transceiver->fired_direction(),
        GetNegotiatedHeaderExtensions(webrtc_transceiver.get()));
  }
  is_initialized_ = true;
}

blink::WebRTCSctpTransportSnapshot
TransceiverStateSurfacer::SctpTransportSnapshot() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(is_initialized_);
  return sctp_transport_snapshot_;
}

std::vector<blink::RtpTransceiverState>
TransceiverStateSurfacer::ObtainStates() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  DCHECK(is_initialized_);
  for (auto& transceiver_state : transceiver_states_)
    transceiver_state.Initialize();
  return std::move(transceiver_states_);
}

}  // namespace blink
