// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_HANDLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_HANDLER_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler_client.h"
#include "third_party/blink/public/platform/web_rtc_stats.h"
#include "third_party/blink/public/platform/web_rtc_stats_request.h"
#include "third_party/blink/public/platform/web_rtc_stats_response.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/media_stream_track_metrics.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_receiver_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_rtp_sender_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/transceiver_state_surfacer.h"
#include "third_party/blink/renderer/modules/peerconnection/webrtc_media_stream_track_adapter_map.h"
#include "third_party/webrtc/api/stats/rtc_stats.h"
#include "third_party/webrtc/api/stats/rtc_stats_collector_callback.h"

namespace blink {
class PeerConnectionDependencyFactory;
class PeerConnectionTracker;
class RTCAnswerOptionsPlatform;
class RTCOfferOptionsPlatform;
class RTCVoidRequest;
class SetLocalDescriptionRequest;
class WebLocalFrame;
class WebRTCLegacyStats;
class WebRTCPeerConnectionHandlerClient;

// Mockable wrapper for blink::WebRTCStatsResponse
class MODULES_EXPORT LocalRTCStatsResponse : public rtc::RefCountInterface {
 public:
  explicit LocalRTCStatsResponse(const blink::WebRTCStatsResponse& impl)
      : impl_(impl) {}

  virtual blink::WebRTCStatsResponse webKitStatsResponse() const;
  virtual void addStats(const blink::WebRTCLegacyStats& stats);

 protected:
  ~LocalRTCStatsResponse() override {}
  // Constructor for creating mocks.
  LocalRTCStatsResponse() {}

 private:
  blink::WebRTCStatsResponse impl_;
};

// Mockable wrapper for blink::WebRTCStatsRequest
class MODULES_EXPORT LocalRTCStatsRequest : public rtc::RefCountInterface {
 public:
  explicit LocalRTCStatsRequest(blink::WebRTCStatsRequest impl);
  // Constructor for testing.
  LocalRTCStatsRequest();

  virtual bool hasSelector() const;
  virtual blink::WebMediaStreamTrack component() const;
  virtual void requestSucceeded(const LocalRTCStatsResponse* response);
  virtual scoped_refptr<LocalRTCStatsResponse> createResponse();

 protected:
  ~LocalRTCStatsRequest() override;

 private:
  blink::WebRTCStatsRequest impl_;
};

// RTCPeerConnectionHandler is a delegate for the RTC PeerConnection API
// messages going between WebKit and native PeerConnection in libjingle. It's
// owned by WebKit.
// WebKit calls all of these methods on the main render thread.
// Callbacks to the webrtc::PeerConnectionObserver implementation also occur on
// the main render thread.
class MODULES_EXPORT RTCPeerConnectionHandler
    : public blink::WebRTCPeerConnectionHandler {
 public:
  RTCPeerConnectionHandler(
      blink::WebRTCPeerConnectionHandlerClient* client,
      blink::PeerConnectionDependencyFactory* dependency_factory,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~RTCPeerConnectionHandler() override;

  void AssociateWithFrame(blink::WebLocalFrame* frame) override;

  // Initialize method only used for unit test.
  bool InitializeForTest(
      const webrtc::PeerConnectionInterface::RTCConfiguration&
          server_configuration,
      const blink::WebMediaConstraints& options,
      const base::WeakPtr<PeerConnectionTracker>& peer_connection_tracker);

  // blink::WebRTCPeerConnectionHandler implementation
  bool Initialize(const webrtc::PeerConnectionInterface::RTCConfiguration&
                      server_configuration,
                  const blink::WebMediaConstraints& options) override;

  blink::WebVector<std::unique_ptr<blink::WebRTCRtpTransceiver>> CreateOffer(
      blink::RTCSessionDescriptionRequest* request,
      const blink::WebMediaConstraints& options) override;
  blink::WebVector<std::unique_ptr<blink::WebRTCRtpTransceiver>> CreateOffer(
      blink::RTCSessionDescriptionRequest* request,
      blink::RTCOfferOptionsPlatform* options) override;

  void CreateAnswer(blink::RTCSessionDescriptionRequest* request,
                    const blink::WebMediaConstraints& options) override;
  void CreateAnswer(blink::RTCSessionDescriptionRequest* request,
                    blink::RTCAnswerOptionsPlatform* options) override;

  void SetLocalDescription(blink::RTCVoidRequest* request) override;
  void SetLocalDescription(
      blink::RTCVoidRequest* request,
      const blink::WebRTCSessionDescription& description) override;
  void SetRemoteDescription(
      blink::RTCVoidRequest* request,
      const blink::WebRTCSessionDescription& description) override;

  blink::WebRTCSessionDescription LocalDescription() override;
  blink::WebRTCSessionDescription RemoteDescription() override;
  blink::WebRTCSessionDescription CurrentLocalDescription() override;
  blink::WebRTCSessionDescription CurrentRemoteDescription() override;
  blink::WebRTCSessionDescription PendingLocalDescription() override;
  blink::WebRTCSessionDescription PendingRemoteDescription() override;

  const webrtc::PeerConnectionInterface::RTCConfiguration& GetConfiguration()
      const override;
  webrtc::RTCErrorType SetConfiguration(
      const webrtc::PeerConnectionInterface::RTCConfiguration& configuration)
      override;
  void AddICECandidate(
      blink::RTCVoidRequest* request,
      scoped_refptr<blink::WebRTCICECandidate> candidate) override;
  void RestartIce() override;

  void GetStats(const blink::WebRTCStatsRequest& request) override;
  void GetStats(blink::WebRTCStatsReportCallback callback,
                const blink::WebVector<webrtc::NonStandardGroupId>&
                    exposed_group_ids) override;
  webrtc::RTCErrorOr<std::unique_ptr<blink::WebRTCRtpTransceiver>>
  AddTransceiverWithTrack(const blink::WebMediaStreamTrack& web_track,
                          const webrtc::RtpTransceiverInit& init) override;
  webrtc::RTCErrorOr<std::unique_ptr<blink::WebRTCRtpTransceiver>>
  AddTransceiverWithKind(std::string kind,
                         const webrtc::RtpTransceiverInit& init) override;
  webrtc::RTCErrorOr<std::unique_ptr<blink::WebRTCRtpTransceiver>> AddTrack(
      const blink::WebMediaStreamTrack& web_track,
      const blink::WebVector<blink::WebMediaStream>& web_streams) override;
  webrtc::RTCErrorOr<std::unique_ptr<blink::WebRTCRtpTransceiver>> RemoveTrack(
      blink::RTCRtpSenderPlatform* web_sender) override;

  scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
      const blink::WebString& label,
      const blink::WebRTCDataChannelInit& init) override;
  void Stop() override;
  webrtc::PeerConnectionInterface* NativePeerConnection() override;
  void RunSynchronousOnceClosureOnSignalingThread(
      base::OnceClosure closure,
      const char* trace_event_name) override;
  void RunSynchronousRepeatingClosureOnSignalingThread(
      const base::RepeatingClosure& closure,
      const char* trace_event_name) override;

  void TrackIceConnectionStateChange(
      WebRTCPeerConnectionHandler::IceConnectionStateVersion version,
      webrtc::PeerConnectionInterface::IceConnectionState state) override;

  // Delegate functions to allow for mocking of WebKit interfaces.
  // getStats takes ownership of request parameter.
  virtual void getStats(const scoped_refptr<LocalRTCStatsRequest>& request);

  // Asynchronously calls native_peer_connection_->getStats on the signaling
  // thread.
  void GetStandardStatsForTracker(
      scoped_refptr<webrtc::RTCStatsCollectorCallback> observer);
  void GetStats(webrtc::StatsObserver* observer,
                webrtc::PeerConnectionInterface::StatsOutputLevel level,
                rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> selector);

  // Tells the |client_| to close RTCPeerConnection.
  // Make it virtual for testing purpose.
  virtual void CloseClientPeerConnection();

  // Start recording an event log.
  void StartEventLog(int output_period_ms);
  // Stop recording an event log.
  void StopEventLog();

  // WebRTC event log fragments sent back from PeerConnection land here.
  void OnWebRtcEventLogWrite(const String& output);

 protected:
  webrtc::PeerConnectionInterface* native_peer_connection() {
    return native_peer_connection_.get();
  }

  class Observer;
  friend class Observer;
  class WebRtcSetDescriptionObserverImpl;
  friend class WebRtcSetDescriptionObserverImpl;
  class SetLocalDescriptionRequest;
  friend class SetLocalDescriptionRequest;

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state);
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);
  void OnStandardizedIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);
  void OnConnectionChange(
      webrtc::PeerConnectionInterface::PeerConnectionState new_state);
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state);
  void OnRenegotiationNeeded();
  void OnAddReceiverPlanB(blink::RtpReceiverState receiver_state);
  void OnRemoveReceiverPlanB(uintptr_t receiver_id);
  void OnModifySctpTransport(blink::WebRTCSctpTransportSnapshot state);
  void OnModifyTransceivers(
      std::vector<blink::RtpTransceiverState> transceiver_states,
      bool is_remote_description);
  void OnDataChannel(scoped_refptr<webrtc::DataChannelInterface> channel);
  void OnIceCandidate(const String& sdp,
                      const String& sdp_mid,
                      int sdp_mline_index,
                      int component,
                      int address_family);
  void OnIceCandidateError(const String& host_candidate,
                           const String& url,
                           int error_code,
                           const String& error_text);
  void OnInterestingUsage(int usage_pattern);

 private:
  // Record info about the first SessionDescription from the local and
  // remote side to record UMA stats once both are set.
  struct FirstSessionDescription {
    explicit FirstSessionDescription(
        const webrtc::SessionDescriptionInterface* desc);

    bool audio = false;
    bool video = false;
    // If audio or video will use RTCP-mux (if there is no audio or
    // video, then false).
    bool rtcp_mux = false;
  };

  webrtc::SessionDescriptionInterface* CreateNativeSessionDescription(
      const String& sdp,
      const String& type,
      webrtc::SdpParseError* error);

  blink::WebRTCSessionDescription GetWebRTCSessionDescriptionOnSignalingThread(
      base::OnceCallback<const webrtc::SessionDescriptionInterface*()>
          description_cb,
      const char* log_text);

  // Report to UMA whether an IceConnectionState has occurred. It only records
  // the first occurrence of a given state.
  void ReportICEState(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);

  // Reset UMA related members to the initial state. This is invoked at the
  // constructor as well as after Ice Restart.
  void ResetUMAStats();

  void ReportFirstSessionDescriptions(const FirstSessionDescription& local,
                                      const FirstSessionDescription& remote);

  void AddTransceiverWithTrackOnSignalingThread(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track,
      webrtc::RtpTransceiverInit init,
      blink::TransceiverStateSurfacer* transceiver_state_surfacer,
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>*
          error_or_transceiver);
  void AddTransceiverWithMediaTypeOnSignalingThread(
      cricket::MediaType media_type,
      webrtc::RtpTransceiverInit init,
      blink::TransceiverStateSurfacer* transceiver_state_surfacer,
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>>*
          error_or_transceiver);
  void AddTrackOnSignalingThread(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
      std::vector<std::string> stream_ids,
      blink::TransceiverStateSurfacer* transceiver_state_surfacer,
      webrtc::RTCErrorOr<rtc::scoped_refptr<webrtc::RtpSenderInterface>>*
          error_or_sender);
  bool RemoveTrackPlanB(blink::RTCRtpSenderPlatform* web_sender);
  webrtc::RTCErrorOr<std::unique_ptr<blink::WebRTCRtpTransceiver>>
  RemoveTrackUnifiedPlan(blink::RTCRtpSenderPlatform* web_sender);
  void RemoveTrackUnifiedPlanOnSignalingThread(
      rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
      blink::TransceiverStateSurfacer* transceiver_state_surfacer,
      bool* result);
  std::vector<std::unique_ptr<blink::WebRTCRtpTransceiver>> CreateOfferInternal(
      blink::RTCSessionDescriptionRequest* request,
      webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options);
  void CreateOfferOnSignalingThread(
      webrtc::CreateSessionDescriptionObserver* observer,
      webrtc::PeerConnectionInterface::RTCOfferAnswerOptions offer_options,
      blink::TransceiverStateSurfacer* transceiver_state_surfacer);
  std::vector<std::unique_ptr<blink::RTCRtpSenderImpl>>::iterator FindSender(
      uintptr_t id);
  std::vector<std::unique_ptr<blink::RTCRtpReceiverImpl>>::iterator
  FindReceiver(uintptr_t id);
  std::vector<std::unique_ptr<blink::RTCRtpTransceiverImpl>>::iterator
  FindTransceiver(uintptr_t id);
  // For full transceiver implementations, returns the index of
  // |rtp_transceivers_| that correspond to |web_transceiver|.
  // For sender-only transceiver implementations, returns the index of
  // |rtp_senders_| that correspond to |web_transceiver.Sender()|.
  // For receiver-only transceiver implementations, returns the index of
  // |rtp_receivers_| that correspond to |web_transceiver.Receiver()|.
  // NOTREACHED()-crashes if no correspondent is found.
  size_t GetTransceiverIndex(
      const blink::WebRTCRtpTransceiver& web_transceiver);
  std::unique_ptr<blink::RTCRtpTransceiverImpl> CreateOrUpdateTransceiver(
      blink::RtpTransceiverState transceiver_state,
      blink::TransceiverStateUpdateMode update_mode);

  scoped_refptr<base::SingleThreadTaskRunner> signaling_thread() const;

  // Initialize() is never expected to be called more than once, even if the
  // first call fails.
  bool initialize_called_;

  // |client_| is a weak pointer to the blink object (blink::RTCPeerConnection)
  // that owns this object.
  // It is valid for the lifetime of this object.
  blink::WebRTCPeerConnectionHandlerClient* const client_;
  // True if this PeerConnection has been closed.
  // After the PeerConnection has been closed, this object may no longer
  // forward callbacks to blink.
  bool is_closed_;

  // Transition from kHaveLocalOffer to kHaveRemoteOffer indicates implicit
  // rollback in which case we need to also make visiting of kStable observable.
  webrtc::PeerConnectionInterface::SignalingState previous_signaling_state_ =
      webrtc::PeerConnectionInterface::kStable;

  // |dependency_factory_| is a raw pointer, and is valid for the lifetime of
  // RenderThreadImpl.
  blink::PeerConnectionDependencyFactory* const dependency_factory_;

  blink::WebLocalFrame* frame_ = nullptr;

  // Map and owners of track adapters. Every track that is in use by the peer
  // connection has an associated blink and webrtc layer representation of it.
  // The map keeps track of the relationship between
  // |blink::WebMediaStreamTrack|s and |webrtc::MediaStreamTrackInterface|s.
  // Track adapters are created on the fly when a component (such as a stream)
  // needs to reference it, and automatically disposed when there are no longer
  // any components referencing it.
  scoped_refptr<blink::WebRtcMediaStreamTrackAdapterMap> track_adapter_map_;
  // In Plan B, senders and receivers are added or removed independently of one
  // another. In Unified Plan, senders and receivers are created in pairs as
  // transceivers. Transceivers may become inactive, but are never removed.
  // TODO(hbos): Implement transceiver behaviors. https://crbug.com/777617
  // Blink layer correspondents of |webrtc::RtpSenderInterface|.
  std::vector<std::unique_ptr<blink::RTCRtpSenderImpl>> rtp_senders_;
  // Blink layer correspondents of |webrtc::RtpReceiverInterface|.
  std::vector<std::unique_ptr<blink::RTCRtpReceiverImpl>> rtp_receivers_;
  // Blink layer correspondents of |webrtc::RtpTransceiverInterface|.
  std::vector<std::unique_ptr<blink::RTCRtpTransceiverImpl>> rtp_transceivers_;
  // A snapshot of transceiver ids taken before the last transition, used to
  // detect any removals during rollback.
  blink::WebVector<uintptr_t> previous_transceiver_ids_;

  base::WeakPtr<PeerConnectionTracker> peer_connection_tracker_;

  MediaStreamTrackMetrics track_metrics_;

  // Counter for a UMA stat reported at destruction time.
  int num_data_channels_created_ = 0;

  // Counter for number of IPv4 and IPv6 local candidates.
  int num_local_candidates_ipv4_ = 0;
  int num_local_candidates_ipv6_ = 0;

  // To make sure the observers are released after native_peer_connection_,
  // they have to come first.
  scoped_refptr<Observer> peer_connection_observer_;

  // |native_peer_connection_| is the libjingle native PeerConnection object.
  scoped_refptr<webrtc::PeerConnectionInterface> native_peer_connection_;

  // The last applied configuration. Used so that the constraints
  // used when constructing the PeerConnection carry over when
  // SetConfiguration is called.
  webrtc::PeerConnectionInterface::RTCConfiguration configuration_;

  // Record info about the first SessionDescription from the local and
  // remote side to record UMA stats once both are set.  We only check
  // for the first offer or answer.  "pranswer"s and "unknown"s (from
  // unit tests) are ignored.
  std::unique_ptr<FirstSessionDescription> first_local_description_;
  std::unique_ptr<FirstSessionDescription> first_remote_description_;

  base::TimeTicks ice_connection_checking_start_;

  // Track which ICE Connection state that this PeerConnection has gone through.
  bool ice_state_seen_[webrtc::PeerConnectionInterface::kIceConnectionMax] = {};

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::WeakPtrFactory<RTCPeerConnectionHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RTCPeerConnectionHandler);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_PEER_CONNECTION_HANDLER_H_
