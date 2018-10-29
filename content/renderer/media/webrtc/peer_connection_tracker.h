// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_PEER_CONNECTION_TRACKER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_PEER_CONNECTION_TRACKER_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "content/common/media/peer_connection_tracker.mojom.h"
#include "content/public/renderer/render_thread_observer.h"
#include "ipc/ipc_platform_file.h"
#include "third_party/blink/public/platform/web_media_stream.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler_client.h"
#include "third_party/blink/public/platform/web_rtc_rtp_transceiver.h"
#include "third_party/blink/public/platform/web_rtc_session_description.h"
#include "third_party/webrtc/api/peerconnectioninterface.h"

namespace blink {
class WebLocalFrame;
class WebMediaConstraints;
class WebRTCAnswerOptions;
class WebRTCICECandidate;
class WebRTCOfferOptions;
class WebRTCSessionDescription;
class WebUserMediaRequest;
}  // namespace blink

namespace webrtc {
class DataChannelInterface;
}  // namespace webrtc

namespace content {
class RTCPeerConnectionHandler;
class RenderThread;

// This class collects data about each peer connection,
// sends it to the browser process, and handles messages
// from the browser process.
class CONTENT_EXPORT PeerConnectionTracker
    : public RenderThreadObserver,
      public base::SupportsWeakPtr<PeerConnectionTracker> {
 public:
  explicit PeerConnectionTracker(
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  PeerConnectionTracker(
      mojom::PeerConnectionTrackerHostAssociatedPtr host,
      scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~PeerConnectionTracker() override;

  enum Source {
    SOURCE_LOCAL,
    SOURCE_REMOTE
  };

  enum Action {
    ACTION_SET_LOCAL_DESCRIPTION,
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

  // RenderThreadObserver implementation.
  bool OnControlMessageReceived(const IPC::Message& message) override;

  //
  // The following methods send an update to the browser process when a
  // PeerConnection update happens. The caller should call the Track* methods
  // after calling RegisterPeerConnection and before calling
  // UnregisterPeerConnection, otherwise the Track* call has no effect.
  //

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
      const blink::WebMediaConstraints& constraints,
      const blink::WebLocalFrame* frame);

  // Sends an update when a PeerConnection has been destroyed.
  virtual void UnregisterPeerConnection(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when createOffer/createAnswer has been called.
  // The |pc_handler| is the handler object associated with the PeerConnection,
  // the |constraints| is the media constraints used to create the offer/answer.
  virtual void TrackCreateOffer(RTCPeerConnectionHandler* pc_handler,
                                const blink::WebRTCOfferOptions& options);
  // TODO(hta): Get rid of the version below.
  virtual void TrackCreateOffer(RTCPeerConnectionHandler* pc_handler,
                                const blink::WebMediaConstraints& options);
  virtual void TrackCreateAnswer(RTCPeerConnectionHandler* pc_handler,
                                 const blink::WebRTCAnswerOptions& options);
  virtual void TrackCreateAnswer(RTCPeerConnectionHandler* pc_handler,
                                 const blink::WebMediaConstraints& constraints);

  // Sends an update when setLocalDescription or setRemoteDescription is called.
  virtual void TrackSetSessionDescription(
      RTCPeerConnectionHandler* pc_handler,
      const std::string& sdp, const std::string& type, Source source);

  // Sends an update when setConfiguration is called.
  virtual void TrackSetConfiguration(
      RTCPeerConnectionHandler* pc_handler,
      const webrtc::PeerConnectionInterface::RTCConfiguration& config);

  // Sends an update when an Ice candidate is added.
  virtual void TrackAddIceCandidate(
      RTCPeerConnectionHandler* pc_handler,
      scoped_refptr<blink::WebRTCICECandidate> candidate,
      Source source,
      bool succeeded);

  // Sends an update when a transceiver is added, modified or removed. This can
  // happen as a result of any of the methods indicated by |reason|.
  // In Plan B: |transceiver| refers to its Sender() or Receiver() depending on
  // ImplementationType(). Example events: "senderAdded", "receiverRemoved".
  // In Plan B: |transceiver| has a fully implemented ImplementationType().
  // Example events: "transceiverAdded", "transceiverModified".
  // See peer_connection_tracker_unittest.cc for expected resulting event
  // strings.
  virtual void TrackAddTransceiver(
      RTCPeerConnectionHandler* pc_handler,
      TransceiverUpdatedReason reason,
      const blink::WebRTCRtpTransceiver& transceiver,
      size_t transceiver_index);
  virtual void TrackModifyTransceiver(
      RTCPeerConnectionHandler* pc_handler,
      TransceiverUpdatedReason reason,
      const blink::WebRTCRtpTransceiver& transceiver,
      size_t transceiver_index);
  // TODO(hbos): When Plan B is removed this is no longer applicable.
  // https://crbug.com/857004
  virtual void TrackRemoveTransceiver(
      RTCPeerConnectionHandler* pc_handler,
      TransceiverUpdatedReason reason,
      const blink::WebRTCRtpTransceiver& transceiver,
      size_t transceiver_index);

  // Sends an update when a DataChannel is created.
  virtual void TrackCreateDataChannel(
      RTCPeerConnectionHandler* pc_handler,
      const webrtc::DataChannelInterface* data_channel, Source source);

  // Sends an update when a PeerConnection has been stopped.
  virtual void TrackStop(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when the signaling state of a PeerConnection has changed.
  virtual void TrackSignalingStateChange(
      RTCPeerConnectionHandler* pc_handler,
      webrtc::PeerConnectionInterface::SignalingState state);

  // Sends an update when the Ice connection state
  // of a PeerConnection has changed.
  virtual void TrackIceConnectionStateChange(
      RTCPeerConnectionHandler* pc_handler,
      webrtc::PeerConnectionInterface::IceConnectionState state);

  // Sends an update when the Ice gathering state
  // of a PeerConnection has changed.
  virtual void TrackIceGatheringStateChange(
      RTCPeerConnectionHandler* pc_handler,
      webrtc::PeerConnectionInterface::IceGatheringState state);

  // Sends an update when the SetSessionDescription or CreateOffer or
  // CreateAnswer callbacks are called.
  virtual void TrackSessionDescriptionCallback(
      RTCPeerConnectionHandler* pc_handler, Action action,
      const std::string& type, const std::string& value);

  // Sends an update when onRenegotiationNeeded is called.
  virtual void TrackOnRenegotiationNeeded(RTCPeerConnectionHandler* pc_handler);

  // Sends an update when getUserMedia is called.
  virtual void TrackGetUserMedia(
      const blink::WebUserMediaRequest& user_media_request);

  // Sends a new fragment on an RtcEventLog.
  virtual void TrackRtcEventLogWrite(RTCPeerConnectionHandler* pc_handler,
                                     const std::string& output);
  // For testing: Override the class that gets posted messages.
  void OverrideSendTargetForTesting(RenderThread* target);

 private:
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
                        const blink::WebRTCRtpTransceiver& transceiver,
                        size_t transceiver_index);

  // IPC Message handler for getting all stats.
  void OnGetAllStats();

  // Called when the browser process reports a suspend event from the OS.
  void OnSuspend();

  // TODO(eladalon): Remove OnStartEventLogFile() and then rename
  // OnStartEventLogOutput() to OnStartEventLog(). https://crbug.com/775415

  // IPC Message handler for starting event log (file).
  void OnStartEventLogFile(int peer_connection_id,
                           IPC::PlatformFileForTransit file);

  // IPC Message handler for starting event log (output).
  void OnStartEventLogOutput(int peer_connection_id);

  // IPC Message handler for stopping event log.
  void OnStopEventLog(int peer_connection_id);

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
                                const std::string& callback_type,
                                const std::string& value);

  RenderThread* SendTarget();
  const mojom::PeerConnectionTrackerHostAssociatedPtr&
  GetPeerConnectionTrackerHost();

  // This map stores the local ID assigned to each RTCPeerConnectionHandler.
  typedef std::map<RTCPeerConnectionHandler*, int> PeerConnectionIdMap;
  PeerConnectionIdMap peer_connection_id_map_;

  // This keeps track of the next available local ID.
  int next_local_id_;
  THREAD_CHECKER(main_thread_);
  RenderThread* send_target_for_test_;
  mojom::PeerConnectionTrackerHostAssociatedPtr
      peer_connection_tracker_host_ptr_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionTracker);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_PEER_CONNECTION_TRACKER_H_
