// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_TRACER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_TRACER_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/webrtc/api/peer_connection_tracer_interface.h"

namespace blink {

class PeerConnectionTracker;
class RTCPeerConnectionHandler;

// Per-PeerConnection adapter that bridges libWebRTC's
// `webrtc::PeerConnectionTracerInterface` callbacks to Chromium's
// `PeerConnectionTracker`, which feeds chrome://webrtc-internals.
//
// One instance is created per `RTCPeerConnectionHandler` and handed to
// libWebRTC via `webrtc::PeerConnectionDependencies::tracer`, which owns it
// for the lifetime of the `webrtc::PeerConnection`.
//
// Threading: every tracer callback fires on the libWebRTC signaling thread.
// Each method serializes its payload there, then posts to the main renderer
// thread where `PeerConnectionTracker::Track*` is invoked - the same shape as
// `RTCPeerConnectionHandler::Observer`.
//
// Local-id timing: `PeerConnectionTracker` keys events by a local id assigned
// in `RegisterPeerConnection`, and nothing fires before that. The tracer
// stays silent while the `webrtc::PeerConnection` is being constructed, and
// every event it does report is triggered by a JS-visible operation, which
// can only happen once `RTCPeerConnectionHandler::Initialize` has returned.
// Should a constructor-time event ever be added, it would still be safe: the
// constructor runs while the main thread is blocked inside
// `PeerConnectionDependencyFactory::CreatePeerConnection`, which `Initialize`
// calls before `RegisterPeerConnection`, so the posted task would run after
// registration.
class RTCPeerConnectionTracerImpl
    : public webrtc::PeerConnectionTracerInterface {
 public:
  RTCPeerConnectionTracerImpl(
      CrossThreadWeakHandle<PeerConnectionTracker> tracker,
      base::WeakPtr<RTCPeerConnectionHandler> handler,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread);
  ~RTCPeerConnectionTracerImpl() override;

  RTCPeerConnectionTracerImpl(const RTCPeerConnectionTracerImpl&) = delete;
  RTCPeerConnectionTracerImpl& operator=(const RTCPeerConnectionTracerImpl&) =
      delete;

  // webrtc::PeerConnectionTracerInterface:
  void OnCreate(const webrtc::PeerConnectionInterface::RTCConfiguration&
                    configuration) override;
  void OnCreateOffer(
      const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options)
      override;
  void OnCreateOfferSuccess(
      const webrtc::SessionDescriptionInterface* description) override;
  void OnCreateOfferFailure(const webrtc::RTCError& error) override;
  void OnCreateAnswer(
      const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options)
      override;
  void OnCreateAnswerSuccess(
      const webrtc::SessionDescriptionInterface* description) override;
  void OnCreateAnswerFailure(const webrtc::RTCError& error) override;
  void OnSetLocalDescription(
      const webrtc::SessionDescriptionInterface* description) override;
  void OnSetLocalDescriptionSuccess(
      const webrtc::SessionDescriptionInterface* description) override;
  void OnSetLocalDescriptionFailure(const webrtc::RTCError& error) override;
  void OnSetRemoteDescription(
      const webrtc::SessionDescriptionInterface* description) override;
  void OnSetRemoteDescriptionSuccess() override;
  void OnSetRemoteDescriptionFailure(const webrtc::RTCError& error) override;
  void OnSetConfiguration(
      const webrtc::PeerConnectionInterface::RTCConfiguration& configuration)
      override;
  void OnRestartIce() override;
  void OnClose() override;
  void OnIceCandidate(const webrtc::IceCandidate& candidate) override;
  void OnAddIceCandidate(const webrtc::IceCandidate& candidate) override;
  void OnAddIceCandidateSuccess() override;
  void OnAddIceCandidateFailure(const webrtc::RTCError& error) override;
  void OnIceCandidateError(absl::string_view address,
                           int port,
                           absl::string_view url,
                           int error_code,
                           absl::string_view error_text) override;
  void OnCreateDataChannel(const webrtc::DataChannelInterface& channel,
                           std::optional<int> id) override;
  void OnDataChannel(const webrtc::DataChannelInterface& channel,
                     std::optional<int> id) override;
  void OnAddTransceiver(webrtc::MediaType media_type,
                        const webrtc::MediaStreamTrackInterface* track,
                        const webrtc::RtpTransceiverInit& init) override;
  void OnAddTrack(const webrtc::MediaStreamTrackInterface& track,
                  const std::vector<std::string>& stream_ids) override;
  void OnTrack(const webrtc::RtpTransceiverInterface& transceiver) override;
  void OnSignalingStateChanged(
      webrtc::PeerConnectionInterface::SignalingState state) override;
  void OnIceConnectionStateChanged(
      webrtc::PeerConnectionInterface::IceConnectionState state) override;
  void OnConnectionStateChanged(
      webrtc::PeerConnectionInterface::PeerConnectionState state) override;
  void OnIceGatheringStateChanged(
      webrtc::PeerConnectionInterface::IceGatheringState state) override;
  void OnNegotiationNeeded() override;

 private:
  // Work to run on the main thread once the tracker and the handler have been
  // resolved. Both pointers are guaranteed non-null when it runs.
  using TrackerTask = CrossThreadOnceFunction<void(PeerConnectionTracker*,
                                                   RTCPeerConnectionHandler*)>;

  // `tracker` is unwrapped from the weak handle by the bind machinery, and is
  // null once the tracker has been collected.
  static void RunTrackerTask(PeerConnectionTracker* tracker,
                             base::WeakPtr<RTCPeerConnectionHandler> handler,
                             TrackerTask task);

  // Hops to the main thread and runs `task` there, dropping the event if
  // either the tracker or the handler is gone by then.
  void PostToTracker(TrackerTask task);

  const CrossThreadWeakHandle<PeerConnectionTracker> tracker_;
  const base::WeakPtr<RTCPeerConnectionHandler> handler_;
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;

  // True while the setLocalDescription() call in flight is the no-argument
  // overload, i.e. `OnSetLocalDescription()` was passed a null description.
  // webrtc-internals labels the implicit form differently, but the interface
  // only distinguishes the two on the way in, not on completion.
  bool set_local_description_is_implicit_
      GUARDED_BY_CONTEXT(signaling_sequence_checker_) = false;

  // The tracer is constructed on the main thread but only ever invoked on the
  // libWebRTC signaling thread.
  SEQUENCE_CHECKER(signaling_sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_TRACER_IMPL_H_
