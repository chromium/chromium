// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_TRACKER_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/peerconnection/peer_connection_tracker.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_peer_connection_handler_client.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_rtp_transceiver_platform.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_session_description_platform.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/peer_connection_interface.h"

namespace webrtc {
class DataChannelInterface;
}  // namespace webrtc

namespace blink {
class LocalFrame;
class MediaConstraints;
class MockPeerConnectionTracker;
class PeerConnectionTrackerTest;
class RTCAnswerOptionsPlatform;
class RTCIceCandidatePlatform;
class RTCOfferOptionsPlatform;
class RTCPeerConnectionHandler;
class UserMediaRequest;
class WebLocalFrame;

// This class collects data about each peer connection,
// sends it to the browser process, and handles messages
// from the browser process.
class MODULES_EXPORT PeerConnectionTracker
    : public GarbageCollected<PeerConnectionTracker>,
      public Supplement<LocalDOMWindow>,
      public blink::mojom::blink::PeerConnectionManager {
 public:
  static const char kSupplementName[];

  static PeerConnectionTracker& From(LocalDOMWindow& window);
  static PeerConnectionTracker* From(LocalFrame& frame);
  static PeerConnectionTracker* From(WebLocalFrame& frame);
  PeerConnectionTracker(
      LocalDOMWindow& window,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      base::PassKey<PeerConnectionTracker>);
  ~PeerConnectionTracker() override;

  // Ctors for tests.
  PeerConnectionTracker(
      mojo::Remote<mojom::blink::PeerConnectionTrackerHost> host,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      base::PassKey<PeerConnectionTrackerTest> key)
      : PeerConnectionTracker(std::move(host), main_thread_task_runner) {}
  PeerConnectionTracker(
      mojo::Remote<mojom::blink::PeerConnectionTrackerHost> host,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner,
      base::PassKey<MockPeerConnectionTracker> key)
      : PeerConnectionTracker(std::move(host), main_thread_task_runner) {}

  static void BindToFrame(
      LocalFrame* frame,
      mojo::PendingReceiver<mojom::blink::PeerConnectionManager> receiver);

  enum Source { SOURCE_LOCAL, SOURCE_REMOTE };

  enum Action {
    ACTION_SET_LOCAL_DESCRIPTION,
    ACTION_SET_LOCAL_DESCRIPTION_IMPLICIT,
    ACTION_SET_REMOTE_DESCRIPTION,
    ACTION_CREATE_OFFER,
    ACTION_CREATE_ANSWER
  };

  // In Plan B: "Transceiver" refers to RTCRtpSender or RTCRtpReceiver.
  // In Unified Plan: "Transceiver" refers to RTCRtpTransceiver.
  enum class TransceiverUpdatedReason {
    kAddTransceiver,
    kAddTrack,
    kRemoveTrack,
    kSetLocalDescription,
    kSetRemoteDescription,
  };

  // The following methods send an update to the browser process when a
  // PeerConnection update happens. The caller should call the Track* methods
  // after calling RegisterPeerConnection and before calling
  // UnregisterPeerConnection, otherwise the Track* call has no effect.

  // Sends an update when a PeerConnection has been created in Javascript. This
  // should be called once and only once for each PeerConnection. The
  // |pc_handler| is the handler object associated with the PeerConnection, the
  // |servers| are the server configurations used to establish the connection,
  // the |constraints| are the media constraints used to initialize the
  // PeerConnection, the |frame| is the WebLocalFrame object representing the
  // page in which the PeerConnection is created.
  void RegisterPeerConnection(
      RTCPeerConnectionHandler* pc_handler,
      const webrtc::PeerConnectionInterface::RTCConfiguration& config,
      const MediaConstraints& constraints,
      const blink::WebLocalFrame* frame);

  // Sends an update when a PeerConnection has been destroyed.
  virtual void UnregisterPeerConnection(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when createOffer/createAnswer has been called.
  // The |pc_handler| is the handler object associated with the PeerConnection,
  // the |constraints| is the media constraints used to create the offer/answer.
  virtual void TrackCreateOffer(RTCPeerConnectionHandler* pc_handler,
                                RTCOfferOptionsPlatform* options);
  // TODO(hta): Get rid of the version below.
  virtual void TrackCreateOffer(RTCPeerConnectionHandler* pc_handler,
                                const MediaConstraints& options);
  virtual void TrackCreateAnswer(RTCPeerConnectionHandler* pc_handler,
                                 blink::RTCAnswerOptionsPlatform* options);
  virtual void TrackCreateAnswer(RTCPeerConnectionHandler* pc_handler,
                                 const MediaConstraints& constraints);

  // Sends an update when setLocalDescription or setRemoteDescription is called.
  virtual void TrackSetSessionDescription(RTCPeerConnectionHandler* pc_handler,
                                          const String& sdp,
                                          const String& type,
                                          Source source);
  virtual void TrackSetSessionDescriptionImplicit(
      RTCPeerConnectionHandler* pc_handler);

  // Sends an update when setConfiguration is called.
  virtual void TrackSetConfiguration(
      RTCPeerConnectionHandler* pc_handler,
      const webrtc::PeerConnectionInterface::RTCConfiguration& config);

  // Sends an update when an Ice candidate is added.
  virtual void TrackAddIceCandidate(RTCPeerConnectionHandler* pc_handler,
                                    RTCIceCandidatePlatform* candidate,
                                    Source source,
                                    bool succeeded);
  // Sends an update when an Ice candidate error is receiver.
  virtual void TrackIceCandidateError(RTCPeerConnectionHandler* pc_handler,
                                      const String& address,
                                      absl::optional<uint16_t> port,
                                      const String& host_candidate,
                                      const String& url,
                                      int error_code,
                                      const String& error_text);

  // Sends an update when a transceiver is added, modified or removed. This can
  // happen as a result of any of the methods indicated by |reason|.
  // In Plan B: |transceiver| refers to its Sender() or Receiver() depending on
  // ImplementationType(). Example events: "senderAdded", "receiverRemoved".
  // In Plan B: |transceiver| has a fully implemented ImplementationType().
  // Example events: "transceiverAdded", "transceiverModified".
  // See peer_connection_tracker_unittest.cc for expected resulting event
  // strings.
  virtual void TrackAddTransceiver(RTCPeerConnectionHandler* pc_handler,
                                   TransceiverUpdatedReason reason,
                                   const RTCRtpTransceiverPlatform& transceiver,
                                   size_t transceiver_index);
  virtual void TrackModifyTransceiver(
      RTCPeerConnectionHandler* pc_handler,
      TransceiverUpdatedReason reason,
      const RTCRtpTransceiverPlatform& transceiver,
      size_t transceiver_index);
  // TODO(hbos): When Plan B is removed this is no longer applicable.
  // https://crbug.com/857004
  virtual void TrackRemoveTransceiver(
      RTCPeerConnectionHandler* pc_handler,
      TransceiverUpdatedReason reason,
      const RTCRtpTransceiverPlatform& transceiver,
      size_t transceiver_index);

  // Sends an update when a DataChannel is created.
  virtual void TrackCreateDataChannel(
      RTCPeerConnectionHandler* pc_handler,
      const webrtc::DataChannelInterface* data_channel,
      Source source);

  // Sends an update when a PeerConnection has been stopped.
  virtual void TrackStop(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when the signaling state of a PeerConnection has changed.
  virtual void TrackSignalingStateChange(
      RTCPeerConnectionHandler* pc_handler,
      webrtc::PeerConnectionInterface::SignalingState state);

  // Sends an update when the ICE connection state of a PeerConnection has
  // changed. There's a legacy and non-legacy version. The non-legacy version
  // reflects the blink::RTCPeerConnection::iceConnectionState.
  //
  // "Legacy" usage: In Unifed Plan, TrackLegacyIceConnectionStateChange() is
  // used to report the webrtc::PeerConnection layer implementation of the
  // state, which might not always be the same as the
  // blink::RTCPeerConnection::iceConnectionState reported with
  // TrackIceConnectionStateChange(). In Plan B, the webrtc::PeerConnection
  // layer implementation is the only iceConnectionState version, and
  // TrackLegacyIceConnectionStateChange() is not applicable.
  virtual void TrackLegacyIceConnectionStateChange(
      RTCPeerConnectionHandler* pc_handler,
      webrtc::PeerConnectionInterface::IceConnectionState state);
  virtual void TrackIceConnectionStateChange(
      RTCPeerConnectionHandler* pc_handler,
      webrtc::PeerConnectionInterface::IceConnectionState state);

  // Sends an update when the connection state
  // of a PeerConnection has changed.
  virtual void TrackConnectionStateChange(
      RTCPeerConnectionHandler* pc_handler,
      webrtc::PeerConnectionInterface::PeerConnectionState state);

  // Sends an update when the Ice gathering state
  // of a PeerConnection has changed.
  virtual void TrackIceGatheringStateChange(
      RTCPeerConnectionHandler* pc_handler,
      webrtc::PeerConnectionInterface::IceGatheringState state);

  // Sends an update when the SetSessionDescription or CreateOffer or
  // CreateAnswer callbacks are called.
  virtual void TrackSessionDescriptionCallback(
      RTCPeerConnectionHandler* pc_handler,
      Action action,
      const String& type,
      const String& value);

  // Sends an update when the session description's ID is set.
  virtual void TrackSessionId(RTCPeerConnectionHandler* pc_handler,
                              const String& session_id);

  // Sends an update when onRenegotiationNeeded is called.
  virtual void TrackOnRenegotiationNeeded(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when getUserMedia is called.
  virtual void TrackGetUserMedia(UserMediaRequest* user_media_request);

  // Sends a new fragment on an RtcEventLog.
  virtual void TrackRtcEventLogWrite(RTCPeerConnectionHandler* pc_handler,
                                     const WTF::Vector<uint8_t>& output);

 private:
  FRIEND_TEST_ALL_PREFIXES(PeerConnectionTrackerTest, OnSuspend);
  FRIEND_TEST_ALL_PREFIXES(PeerConnectionTrackerTest, OnThermalStateChange);
  FRIEND_TEST_ALL_PREFIXES(PeerConnectionTrackerTest,
                           ReportInitialThermalState);

  PeerConnectionTracker(
      mojo::Remote<mojom::blink::PeerConnectionTrackerHost> host,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);

  void Bind(mojo::PendingReceiver<blink::mojom::blink::PeerConnectionManager>
                receiver);

  // Assign a local ID to a peer connection so that the browser process can
  // uniquely identify a peer connection in the renderer process.
  // The return value will always be positive.
  int GetNextLocalID();

  // Looks up a handler in our map and if found, returns its ID. If the handler
  // is not registered, the return value will be -1.
  int GetLocalIDForHandler(RTCPeerConnectionHandler* handler) const;

  void TrackTransceiver(const char* callback_type_ending,
                        RTCPeerConnectionHandler* pc_handler,
                        PeerConnectionTracker::TransceiverUpdatedReason reason,
                        const RTCRtpTransceiverPlatform& transceiver,
                        size_t transceiver_index);

  // PeerConnectionTracker implementation.
  void OnSuspend() override;
  void OnThermalStateChange(
      mojom::blink::DeviceThermalState thermal_state) override;
  void StartEventLog(int peer_connection_local_id,
                     int output_period_ms) override;
  void StopEventLog(int peer_connection_local_id) override;
  void GetStandardStats() override;
  void GetLegacyStats() override;

  // Called to deliver an update to the host (PeerConnectionTrackerHost).
  // |local_id| - The id of the registered RTCPeerConnectionHandler.
  //              Using an id instead of the hander pointer is done on purpose
  //              to force doing the lookup before building the callback data
  //              in case the handler isn't registered.
  // |callback_type| - A string, most often static, that represents the type
  //                   of operation that the data stored in |value| comes from.
  //                   E.g. "createOffer", "createAnswer",
  //                   "setRemoteDescription" etc.
  // |value| - A json serialized string containing all the information for the
  //           update event.
  void SendPeerConnectionUpdate(int local_id,
                                const String& callback_type,
                                const String& value);

  void AddStandardStats(int lid, base::Value value);
  void AddLegacyStats(int lid, base::Value value);

  // This map stores the local ID assigned to each RTCPeerConnectionHandler.
  typedef WTF::HashMap<RTCPeerConnectionHandler*, int> PeerConnectionLocalIdMap;
  PeerConnectionLocalIdMap peer_connection_local_id_map_;
  mojom::blink::DeviceThermalState current_thermal_state_ =
      mojom::blink::DeviceThermalState::kUnknown;

  THREAD_CHECKER(main_thread_);
  mojo::Remote<blink::mojom::blink::PeerConnectionTrackerHost>
      peer_connection_tracker_host_;
  mojo::Receiver<blink::mojom::blink::PeerConnectionManager> receiver_{this};

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionTracker);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_TRACKER_H_
