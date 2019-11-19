// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_PROXY_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/ice_transport_adapter_cross_thread_factory.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/webrtc/p2p/base/p2p_transport_channel.h"

namespace rtc {
class Thread;
}

namespace blink {

class IceTransportHost;
class LocalFrame;
class QuicTransportProxy;

// This class allows the ICE implementation (P2PTransportChannel) to run on a
// thread different from the thread from which it is controlled. All
// interactions with the ICE implementation happen asynchronously.
//
// Terminology:
// - Proxy thread: Thread from which the P2PTransportChannel is controlled. This
//       is the thread on which the IceTransportProxy is created.
// - Host thread: Thread on which the P2PTransportChannel runs. This is usually
//       the WebRTC worker thread and is specified when creating the
//       IceTransportProxy.
//
// The client must create the IceTransportProxy on the same thread it wishes
// to control it from. The Proxy will manage all cross-thread interactions; the
// client should call all methods from the proxy thread and all callbacks will
// be run on the proxy thread.
class IceTransportProxy final {
  USING_FAST_MALLOC(IceTransportProxy);

 public:
  // Delegate for receiving callbacks from the ICE implementation. These all run
  // on the proxy thread.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void OnGatheringStateChanged(cricket::IceGatheringState new_state) {
    }
    virtual void OnCandidateGathered(const cricket::Candidate& candidate) {}
    virtual void OnStateChanged(webrtc::IceTransportState new_state) {}
    virtual void OnSelectedCandidatePairChanged(
        const std::pair<cricket::Candidate, cricket::Candidate>&
            selected_candidate_pair) {}
  };

  // Construct a Proxy with the underlying ICE implementation running on the
  // given host thread and callbacks serviced by the given delegate.
  // The P2PTransportChannel will be created with the given PortAllocator.
  // The delegate must outlive the IceTransportProxy.
  IceTransportProxy(
      LocalFrame& frame,
      scoped_refptr<base::SingleThreadTaskRunner> proxy_thread,
      scoped_refptr<base::SingleThreadTaskRunner> host_thread,
      Delegate* delegate,
      std::unique_ptr<IceTransportAdapterCrossThreadFactory> adapter_factory);
  ~IceTransportProxy();

  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread() const;
  scoped_refptr<base::SingleThreadTaskRunner> host_thread() const;

  // These methods are proxied to an IceTransportAdapter instance.
  void StartGathering(const cricket::IceParameters& local_parameters,
                      const cricket::ServerAddresses& stun_servers,
                      const WebVector<cricket::RelayServerConfig>& turn_servers,
                      IceTransportPolicy policy);
  void Start(const cricket::IceParameters& remote_parameters,
             cricket::IceRole role,
             const Vector<cricket::Candidate>& initial_remote_candidates);
  void HandleRemoteRestart(const cricket::IceParameters& new_remote_parameters);
  void AddRemoteCandidate(const cricket::Candidate& candidate);

  // A QuicTransportProxy can be connected to this IceTransportProxy. Only one
  // can be connected at a time, and the caller must ensure that the consumer
  // is disconnected before destroying the IceTransportProxy.
  // ConnectConsumer returns an IceTransportHost that can be used to connect
  // a QuicTransportHost.
  bool HasConsumer() const;
  IceTransportHost* ConnectConsumer(QuicTransportProxy* consumer_proxy);
  void DisconnectConsumer(QuicTransportProxy* consumer_proxy);

 private:
  // Callbacks from RTCIceTransportHost.
  friend class IceTransportHost;
  void OnGatheringStateChanged(cricket::IceGatheringState new_state);
  void OnCandidateGathered(const cricket::Candidate& candidate);
  void OnStateChanged(webrtc::IceTransportState new_state);
  void OnSelectedCandidatePairChanged(
      const std::pair<cricket::Candidate, cricket::Candidate>&
          selected_candidate_pair);

  const scoped_refptr<base::SingleThreadTaskRunner> proxy_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> host_thread_;
  // Since the Host is deleted on the host thread (via OnTaskRunnerDeleter), as
  // long as this is alive it is safe to post tasks to it (using unretained).
  std::unique_ptr<IceTransportHost, base::OnTaskRunnerDeleter> host_;
  Delegate* const delegate_;
  QuicTransportProxy* consumer_proxy_ = nullptr;

  // This handle notifies scheduler about an active connection associated
  // with a frame. Handle should be destroyed when connection is closed.
  // This should have the same lifetime as |proxy_|.
  FrameScheduler::SchedulingAffectingFeatureHandle
      feature_handle_for_scheduler_;

  THREAD_CHECKER(thread_checker_);

  // Must be the last member.
  base::WeakPtrFactory<IceTransportProxy> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_ICE_TRANSPORT_PROXY_H_
