// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_DEPENDENCY_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_DEPENDENCY_FACTORY_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/task/current_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/peer_connection_interface.h"
#include "third_party/webrtc/p2p/stunprober/stun_prober.h"

namespace base {
class WaitableEvent;
}

namespace cricket {
class PortAllocator;
}

namespace media {
class GpuVideoAcceleratorFactories;
}

namespace rtc {
class Thread;
}

namespace blink {

class IpcNetworkManager;
class IpcPacketSocketFactory;
class MdnsResponderAdapter;
class P2PSocketDispatcher;
class RTCPeerConnectionHandlerClient;
class RTCPeerConnectionHandler;
class StunProberTrial;
class WebLocalFrame;
class WebRtcAudioDeviceImpl;

// Object factory for RTC PeerConnections.
class MODULES_EXPORT PeerConnectionDependencyFactory
    : base::CurrentThread::DestructionObserver {
 public:
  ~PeerConnectionDependencyFactory() override;

  static PeerConnectionDependencyFactory* GetInstance();

  // Create a RTCPeerConnectionHandler object.
  std::unique_ptr<RTCPeerConnectionHandler> CreateRTCPeerConnectionHandler(
      RTCPeerConnectionHandlerClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool force_encoded_audio_insertable_streams,
      bool force_encoded_video_insertable_streams);

  // Create a proxy object for a VideoTrackSource that makes sure it's called on
  // the correct threads.
  virtual scoped_refptr<webrtc::VideoTrackSourceInterface>
  CreateVideoTrackSourceProxy(webrtc::VideoTrackSourceInterface* source);

  // Asks the PeerConnection factory to create a Local MediaStream object.
  virtual scoped_refptr<webrtc::MediaStreamInterface> CreateLocalMediaStream(
      const String& label);

  // Asks the PeerConnection factory to create a Local VideoTrack object.
  virtual scoped_refptr<webrtc::VideoTrackInterface> CreateLocalVideoTrack(
      const String& id,
      webrtc::VideoTrackSourceInterface* source);

  // Asks the libjingle PeerConnection factory to create a libjingle
  // PeerConnection object.
  // The PeerConnection object is owned by PeerConnectionHandler.
  virtual scoped_refptr<webrtc::PeerConnectionInterface> CreatePeerConnection(
      const webrtc::PeerConnectionInterface::RTCConfiguration& config,
      blink::WebLocalFrame* web_frame,
      webrtc::PeerConnectionObserver* observer,
      ExceptionState& exception_state);

  // Creates a PortAllocator that uses Chrome IPC sockets and enforces privacy
  // controls according to the permissions granted on the page.
  virtual std::unique_ptr<cricket::PortAllocator> CreatePortAllocator(
      blink::WebLocalFrame* web_frame);

  // Creates an AsyncResolverFactory that uses the networking Mojo service.
  virtual std::unique_ptr<webrtc::AsyncResolverFactory>
  CreateAsyncResolverFactory();

  // Creates a libjingle representation of a Session description. Used by a
  // RTCPeerConnectionHandler instance.
  virtual webrtc::SessionDescriptionInterface* CreateSessionDescription(
      const String& type,
      const String& sdp,
      webrtc::SdpParseError* error);

  // Creates a libjingle representation of an ice candidate.
  virtual webrtc::IceCandidateInterface* CreateIceCandidate(
      const String& sdp_mid,
      int sdp_mline_index,
      const String& sdp);

  // Returns the most optimistic view of the capabilities of the system for
  // sending or receiving media of the given kind ("audio" or "video").
  virtual std::unique_ptr<webrtc::RtpCapabilities> GetSenderCapabilities(
      const String& kind);
  virtual std::unique_ptr<webrtc::RtpCapabilities> GetReceiverCapabilities(
      const String& kind);

  blink::WebRtcAudioDeviceImpl* GetWebRtcAudioDevice();

  void EnsureInitialized();

  // Returns the SingleThreadTaskRunner suitable for running WebRTC networking.
  // An rtc::Thread will have already been created.
  scoped_refptr<base::SingleThreadTaskRunner> GetWebRtcNetworkTaskRunner();

  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetWebRtcSignalingTaskRunner();

 protected:
  PeerConnectionDependencyFactory(bool create_p2p_socket_dispatcher);

  virtual const scoped_refptr<webrtc::PeerConnectionFactoryInterface>&
  GetPcFactory();
  virtual bool PeerConnectionFactoryCreated();

  // Helper method to create a WebRtcAudioDeviceImpl.
  void EnsureWebRtcAudioDeviceImpl();

 private:
  // Implement base::CurrentThread::DestructionObserver.
  // This makes sure the libjingle PeerConnectionFactory is released before
  // the renderer message loop is destroyed.
  void WillDestroyCurrentMessageLoop() override;

  // Functions related to Stun probing trial to determine how fast we could send
  // Stun request without being dropped by NAT.
  void TryScheduleStunProbeTrial();
  void StartStunProbeTrialOnNetworkThread(const String& params);

  // Creates |pc_factory_|, which in turn is used for
  // creating PeerConnection objects.
  void CreatePeerConnectionFactory();

  void InitializeSignalingThread(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      base::WaitableEvent* event);

  void CreateIpcNetworkManagerOnNetworkThread(
      base::WaitableEvent* event,
      std::unique_ptr<MdnsResponderAdapter> mdns_responder,
      rtc::Thread** thread);
  void DeleteIpcNetworkManager();
  void CleanupPeerConnectionFactory();

  // network_manager_ must be deleted on the worker thread. The network manager
  // uses |p2p_socket_dispatcher_|.
  std::unique_ptr<blink::IpcNetworkManager> network_manager_;
  std::unique_ptr<IpcPacketSocketFactory> socket_factory_;

  scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;

  // Dispatches all P2P sockets.
  scoped_refptr<P2PSocketDispatcher> p2p_socket_dispatcher_;

  scoped_refptr<blink::WebRtcAudioDeviceImpl> audio_device_;

  std::unique_ptr<blink::StunProberTrial> stun_trial_;

  // PeerConnection threads. signaling_thread_ is created from the
  // "current" chrome thread.
  rtc::Thread* signaling_thread_ = nullptr;
  rtc::Thread* network_thread_ = nullptr;
  base::Thread chrome_signaling_thread_;
  base::Thread chrome_network_thread_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(PeerConnectionDependencyFactory);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_PEER_CONNECTION_DEPENDENCY_FACTORY_H_
