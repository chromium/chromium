// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_
#define REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "crypto/hmac.h"
#include "remoting/base/constants.h"
#include "remoting/base/session_options.h"
#include "remoting/protocol/network_settings.h"
#include "remoting/protocol/peer_connection_controls.h"
#include "remoting/protocol/port_allocator.h"
#include "remoting/protocol/port_allocator_factory.h"
#include "remoting/protocol/session_options_provider.h"
#include "remoting/protocol/transport.h"
#include "remoting/protocol/webrtc_data_stream_adapter.h"
#include "remoting/protocol/webrtc_event_log_data.h"
#include "remoting/signaling/signal_strategy.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/api/video_codecs/video_encoder_factory.h"

namespace base {
class Watchdog;
}  // namespace base

namespace remoting::protocol {

class TransportContext;
class MessagePipe;
class WebrtcAudioModule;

class WebrtcTransport : public Transport,
                        public SessionOptionsProvider,
                        public PeerConnectionControls {
 public:
  class EventHandler {
   public:
    virtual ~EventHandler() = default;

    // Called after |peer_connection| has been created but before handshake. The
    // handler should create data channels and media streams. Renegotiation will
    // be required in two cases after this method returns:
    //   1. When the first data channel is created, if it wasn't created by this
    //      event handler.
    //   2. Whenever a media stream is added or removed.
    virtual void OnWebrtcTransportConnecting() = 0;

    // Called when the transport is connected.
    virtual void OnWebrtcTransportConnected() = 0;

    // Called when there is an error connecting the session.
    virtual void OnWebrtcTransportError(ErrorCode error) = 0;

    // Called when the transport protocol has been changed. Note that this might
    // be called before the channels become ready.
    virtual void OnWebrtcTransportProtocolChanged() = 0;

    // Called when a new data channel is created by the peer.
    virtual void OnWebrtcTransportIncomingDataChannel(
        const std::string& name,
        std::unique_ptr<MessagePipe> pipe) = 0;

    // Called when an incoming media stream is added or removed.
    virtual void OnWebrtcTransportMediaStreamAdded(
        rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) = 0;
    virtual void OnWebrtcTransportMediaStreamRemoved(
        rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) = 0;

    // Called when the transport route changes (for example, from relayed to
    // direct connection). Also called on initial connection.
    virtual void OnWebrtcTransportRouteChanged(const TransportRoute& route) = 0;
  };

  // |video_encoder_factory| can be nullptr if the connection is not used for
  // sending video.
  WebrtcTransport(
      rtc::Thread* worker_thread,
      scoped_refptr<TransportContext> transport_context,
      std::unique_ptr<webrtc::VideoEncoderFactory> video_encoder_factory,
      EventHandler* event_handler);

  WebrtcTransport(const WebrtcTransport&) = delete;
  WebrtcTransport& operator=(const WebrtcTransport&) = delete;

  ~WebrtcTransport() override;

  webrtc::PeerConnectionInterface* peer_connection();
  webrtc::PeerConnectionFactoryInterface* peer_connection_factory();
  WebrtcAudioModule* audio_module();
  WebrtcEventLogData* rtc_event_log() { return &rtc_event_log_; }

  // Creates outgoing data channel. The channel is created in CONNECTING state.
  // The caller must wait for OnMessagePipeOpen() notification before sending
  // any messages.
  std::unique_ptr<MessagePipe> CreateOutgoingChannel(const std::string& name);

  // Applies network settings. This can be called after Start(), but negotiation
  // will not start until the network settings are applied.
  void ApplyNetworkSettings(const NetworkSettings& network_settings);

  // Transport implementations.
  void Start(Authenticator* authenticator,
             SendTransportInfoCallback send_transport_info_callback) override;
  bool ProcessTransportInfo(jingle_xmpp::XmlElement* transport_info) override;

  // SessionOptionsProvider implementations.
  const SessionOptions& session_options() const override;

  // PeerConnectionControls implementations.
  void SetPreferredBitrates(std::optional<int> min_bitrate_bps,
                            std::optional<int> max_bitrate_bps) override;
  void RequestIceRestart() override;
  void RequestSdpRestart() override;

  void Close(ErrorCode error);

  void ApplySessionOptions(const SessionOptions& options);

  // Called when a new audio transceiver has been created by the PeerConnection.
  void OnAudioTransceiverCreated(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);

  // Called when a new video transceiver has been created by the PeerConnection.
  void OnVideoTransceiverCreated(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver);

  // Transport layer protocol used to connect to the relay server or the peer.
  // Possible values are those defined in the protocol and relayProtocol fields
  // in the RTCIceCandidateStats dictionary.  Empty if the protocol is not known
  // yet, "api-error" if failed to get the current protocol.
  const std::string& transport_protocol() const { return transport_protocol_; }

  // Since WebRTC uses its own threads, it is difficult to control its behavior
  // using the standard Chromium threading test classes.  For higher-level tests
  // which do not want to mock out WebRTC, we provide this mechanism to allow
  // for polling faster (which should mean the teardown work completing faster)
  // or to zero out the interval and prevent hangs due to PostDelayedTask.
  static void SetDataChannelPollingIntervalForTests(
      base::TimeDelta data_channel_state_polling_interval);

  // Replaces the watchdog that monitors the thread join process when the peer
  // connection is being torn down.
  void SetThreadJoinWatchdogForTests(std::unique_ptr<base::Watchdog> watchdog);

  // Sets a callback to be executed before disarming the thread join watchdog.
  // Only used for testing.
  void SetBeforeDisarmThreadJoinWatchdogCallbackForTests(base::OnceClosure cb);

 private:
  // PeerConnectionWrapper is responsible for PeerConnection creation,
  // ownership. It passes all events to the corresponding methods below. This is
  // necessary to make it possible to close and destroy PeerConnection
  // asynchronously, as it may be on stack when the transport is destroyed.
  class PeerConnectionWrapper;
  friend class PeerConnectionWrapper;

  void OnLocalSessionDescriptionCreated(
      std::unique_ptr<webrtc::SessionDescriptionInterface> description,
      const std::string& error);
  void OnLocalDescriptionSet(bool success, const std::string& error);
  void OnRemoteDescriptionSet(bool send_answer,
                              bool success,
                              const std::string& error);
  void SendAnswer();
  void OnCloseAfterDisconnectTimeout();

  // PeerConnection event handlers, called by PeerConnectionWrapper.
  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state);
  void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
  void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream);
  void OnDataChannel(
      rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel);
  void OnRenegotiationNeeded();
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state);
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state);
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
  void OnIceSelectedCandidatePairChanged(
      const cricket::CandidatePairChangeEvent& event);
  void OnStatsDelivered(
      const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report);

  // Returns the min (first element) and max (second element) bitrate for this
  // connection, taking into account any relay bitrate cap and client overrides.
  // The default range is [0, default max bitrate]. Client overrides that go
  // beyond this bound or exceed the relay server's max bitrate will be ignored.
  std::tuple<int, int> BitratesForConnection();

  // Sets the min/max bitrate (using the preferred bitrate members) on the peer
  // connection and each video RtpSender.
  void UpdateBitrates();

  // Sets bitrates on the PeerConnection.
  // Called after SetRemoteDescription(), but also called if the relay status
  // changes.
  void SetPeerConnectionBitrates(int min_bitrate_bps, int max_bitrate_bps);

  // Sets bitrates on the (video) sender. Called when a video sender is created,
  // but also called if the relay status changes.
  void SetSenderBitrates(rtc::scoped_refptr<webrtc::RtpSenderInterface> sender,
                         int min_bitrate_bps,
                         int max_bitrate_bps);

  void RequestRtcStats();
  void RequestNegotiation();
  void SendOffer();
  void EnsurePendingTransportInfoMessage();
  void SendTransportInfo();
  void AddPendingCandidatesIfPossible();

  // Closes the PeerConnection after |control_data_channel| and
  // |event_data_channel| have closed.  Note that |peer_connection_wrapper| is
  // always destroyed asynchronously to allow the callstack to unwind first.
  static void ClosePeerConnection(
      rtc::scoped_refptr<webrtc::DataChannelInterface> control_data_channel,
      rtc::scoped_refptr<webrtc::DataChannelInterface> event_data_channel,
      std::unique_ptr<PeerConnectionWrapper> peer_connection_wrapper,
      base::Time start_time);

  void StartRtcEventLogging();
  void StopRtcEventLogging();

  scoped_refptr<TransportContext> transport_context_;
  raw_ptr<EventHandler> event_handler_ = nullptr;
  SendTransportInfoCallback send_transport_info_callback_;

  crypto::HMAC handshake_hmac_;

  std::unique_ptr<PeerConnectionWrapper> peer_connection_wrapper_;

  bool negotiation_pending_ = false;

  bool connected_ = false;

  std::optional<bool> connection_relayed_;

  std::string transport_protocol_;

  bool want_ice_restart_ = false;

  std::unique_ptr<jingle_xmpp::XmlElement> pending_transport_info_message_;
  base::OneShotTimer transport_info_timer_;
  // Timer that closes the transport after the ICE connection has become
  // disconnected for the specified timeout.
  base::OneShotTimer close_after_disconnect_timer_;

  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
      pending_incoming_candidates_;

  std::string preferred_video_codec_;

  SessionOptions session_options_;

  // Track the data channels so we can make sure they are closed before we
  // close the peer connection.  This prevents RTCErrors being thrown on the
  // other side of the WebRTC connection.
  rtc::scoped_refptr<webrtc::DataChannelInterface> control_data_channel_;
  rtc::scoped_refptr<webrtc::DataChannelInterface> event_data_channel_;

  // Preferred bitrates set by the client. nullopt if the client has not
  // provided any preferred bitrates.
  std::optional<int> preferred_min_bitrate_bps_;
  std::optional<int> preferred_max_bitrate_bps_;

  // Stores event log data generated by WebRTC for the PeerConnection.
  WebrtcEventLogData rtc_event_log_;

  // Callback to apply network settings on the port allocator. Reset to null
  // once network settings are applied.
  PortAllocatorFactory::ApplyNetworkSettingsCallback apply_network_settings_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<WebrtcTransport> weak_factory_{this};
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_TRANSPORT_H_
