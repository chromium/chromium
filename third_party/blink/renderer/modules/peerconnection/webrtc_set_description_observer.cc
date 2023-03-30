// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/webrtc_set_description_observer.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"

namespace blink {

std::unique_ptr<webrtc::SessionDescriptionInterface> CopySessionDescription(
    const webrtc::SessionDescriptionInterface* description) {
  if (!description)
    return nullptr;
  return description->Clone();
}

WebRtcSetDescriptionObserver::States::States()
    : signaling_state(
          webrtc::PeerConnectionInterface::SignalingState::kClosed) {}

WebRtcSetDescriptionObserver::States::States(States&& other)
    : signaling_state(other.signaling_state),
      sctp_transport_state(std::move(other.sctp_transport_state)),
      transceiver_states(std::move(other.transceiver_states)),
      pending_local_description(std::move(other.pending_local_description)),
      current_local_description(std::move(other.current_local_description)),
      pending_remote_description(std::move(other.pending_remote_description)),
      current_remote_description(std::move(other.current_remote_description)) {}

WebRtcSetDescriptionObserver::States::~States() = default;

WebRtcSetDescriptionObserver::States& WebRtcSetDescriptionObserver::States::
operator=(States&& other) {
  signaling_state = other.signaling_state;
  sctp_transport_state = std::move(other.sctp_transport_state);
  transceiver_states = std::move(other.transceiver_states);
  pending_local_description = std::move(other.pending_local_description);
  current_local_description = std::move(other.current_local_description);
  pending_remote_description = std::move(other.pending_remote_description);
  current_remote_description = std::move(other.current_remote_description);
  return *this;
}

WebRtcSetDescriptionObserver::WebRtcSetDescriptionObserver() = default;

WebRtcSetDescriptionObserver::~WebRtcSetDescriptionObserver() = default;

WebRtcSetDescriptionObserverHandlerImpl::
    WebRtcSetDescriptionObserverHandlerImpl(
        scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
        scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
        rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc,
        scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap>
            track_adapter_map,
        scoped_refptr<WebRtcSetDescriptionObserver> observer)
    : main_task_runner_(std::move(main_task_runner)),
      signaling_task_runner_(std::move(signaling_task_runner)),
      pc_(std::move(pc)),
      track_adapter_map_(std::move(track_adapter_map)),
      observer_(std::move(observer)) {}

WebRtcSetDescriptionObserverHandlerImpl::
    ~WebRtcSetDescriptionObserverHandlerImpl() = default;

void WebRtcSetDescriptionObserverHandlerImpl::OnSetDescriptionComplete(
    webrtc::RTCError error) {
  CHECK(signaling_task_runner_->BelongsToCurrentThread());
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>
      receiver_only_transceivers;
  std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> transceivers;
  // Only surface transceiver states if the peer connection is not closed. If
  // the peer connection is closed, the peer connection handler may have been
  // destroyed along with any track adapters that TransceiverStateSurfacer
  // assumes exist. This is treated as a special case due to
  // https://crbug.com/897251.
  if (pc_->signaling_state() != webrtc::PeerConnectionInterface::kClosed) {
    transceivers = pc_->GetTransceivers();
  }
  blink::TransceiverStateSurfacer transceiver_state_surfacer(
      main_task_runner_, signaling_task_runner_);
  transceiver_state_surfacer.Initialize(pc_, track_adapter_map_,
                                        std::move(transceivers));
  std::unique_ptr<webrtc::SessionDescriptionInterface>
      pending_local_description =
          CopySessionDescription(pc_->pending_local_description());
  std::unique_ptr<webrtc::SessionDescriptionInterface>
      current_local_description =
          CopySessionDescription(pc_->current_local_description());
  std::unique_ptr<webrtc::SessionDescriptionInterface>
      pending_remote_description =
          CopySessionDescription(pc_->pending_remote_description());
  std::unique_ptr<webrtc::SessionDescriptionInterface>
      current_remote_description =
          CopySessionDescription(pc_->current_remote_description());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcSetDescriptionObserverHandlerImpl::
                                    OnSetDescriptionCompleteOnMainThread,
                                this, std::move(error), pc_->signaling_state(),
                                std::move(transceiver_state_surfacer),
                                std::move(pending_local_description),
                                std::move(current_local_description),
                                std::move(pending_remote_description),
                                std::move(current_remote_description)));
}

void WebRtcSetDescriptionObserverHandlerImpl::
    OnSetDescriptionCompleteOnMainThread(
        webrtc::RTCError error,
        webrtc::PeerConnectionInterface::SignalingState signaling_state,
        blink::TransceiverStateSurfacer transceiver_state_surfacer,
        std::unique_ptr<webrtc::SessionDescriptionInterface>
            pending_local_description,
        std::unique_ptr<webrtc::SessionDescriptionInterface>
            current_local_description,
        std::unique_ptr<webrtc::SessionDescriptionInterface>
            pending_remote_description,
        std::unique_ptr<webrtc::SessionDescriptionInterface>
            current_remote_description) {
  CHECK(main_task_runner_->BelongsToCurrentThread());
  WebRtcSetDescriptionObserver::States states;
  states.signaling_state = signaling_state;
  states.sctp_transport_state =
      transceiver_state_surfacer.SctpTransportSnapshot();
  states.transceiver_states = transceiver_state_surfacer.ObtainStates();
  states.pending_local_description = std::move(pending_local_description);
  states.current_local_description = std::move(current_local_description);
  states.pending_remote_description = std::move(pending_remote_description);
  states.current_remote_description = std::move(current_remote_description);
  observer_->OnSetDescriptionComplete(std::move(error), std::move(states));
}

scoped_refptr<WebRtcSetLocalDescriptionObserverHandler>
WebRtcSetLocalDescriptionObserverHandler::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc,
    scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    scoped_refptr<WebRtcSetDescriptionObserver> observer) {
  return new rtc::RefCountedObject<WebRtcSetLocalDescriptionObserverHandler>(
      std::move(main_task_runner), std::move(signaling_task_runner),
      std::move(pc), std::move(track_adapter_map), std::move(observer));
}

WebRtcSetLocalDescriptionObserverHandler::
    WebRtcSetLocalDescriptionObserverHandler(
        scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
        scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
        rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc,
        scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap>
            track_adapter_map,
        scoped_refptr<WebRtcSetDescriptionObserver> observer)
    : handler_impl_(
          base::MakeRefCounted<WebRtcSetDescriptionObserverHandlerImpl>(
              std::move(main_task_runner),
              std::move(signaling_task_runner),
              std::move(pc),
              std::move(track_adapter_map),
              std::move(observer))) {}

WebRtcSetLocalDescriptionObserverHandler::
    ~WebRtcSetLocalDescriptionObserverHandler() = default;

void WebRtcSetLocalDescriptionObserverHandler::OnSetLocalDescriptionComplete(
    webrtc::RTCError error) {
  handler_impl_->OnSetDescriptionComplete(std::move(error));
}

scoped_refptr<WebRtcSetRemoteDescriptionObserverHandler>
WebRtcSetRemoteDescriptionObserverHandler::Create(
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc,
    scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_adapter_map,
    scoped_refptr<WebRtcSetDescriptionObserver> observer) {
  return new rtc::RefCountedObject<WebRtcSetRemoteDescriptionObserverHandler>(
      std::move(main_task_runner), std::move(signaling_task_runner),
      std::move(pc), std::move(track_adapter_map), std::move(observer));
}

WebRtcSetRemoteDescriptionObserverHandler::
    WebRtcSetRemoteDescriptionObserverHandler(
        scoped_refptr<base::SingleThreadTaskRunner> main_task_runner,
        scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner,
        rtc::scoped_refptr<webrtc::PeerConnectionInterface> pc,
        scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap>
            track_adapter_map,
        scoped_refptr<WebRtcSetDescriptionObserver> observer)
    : handler_impl_(
          base::MakeRefCounted<WebRtcSetDescriptionObserverHandlerImpl>(
              std::move(main_task_runner),
              std::move(signaling_task_runner),
              std::move(pc),
              std::move(track_adapter_map),
              std::move(observer))) {}

WebRtcSetRemoteDescriptionObserverHandler::
    ~WebRtcSetRemoteDescriptionObserverHandler() = default;

void WebRtcSetRemoteDescriptionObserverHandler::OnSetRemoteDescriptionComplete(
    webrtc::RTCError error) {
  handler_impl_->OnSetDescriptionComplete(std::move(error));
}

}  // namespace blink
