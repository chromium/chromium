// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_PROXY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_PROXY_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_stats.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/webrtc/api/scoped_refptr.h"

namespace blink {

class IceTransportProxy;
class QuicStreamProxy;
class QuicTransportHost;

// This class allows the QUIC implementation (P2PQuicTransport) to run on a
// thread different from the thread from which it is controlled. All
// interactions with the QUIC implementation happen asynchronously.
//
// The QuicTransportProxy is intended to be used with an IceTransportProxy --
// see the IceTransportProxy class documentation for background and terms. The
// proxy and host threads used with the QuicTransportProxy should be the same as
// the ones used with the connected IceTransportProxy.
class QuicTransportProxy final {
  USING_FAST_MALLOC(QuicTransportProxy);

 public:
  // Delegate for receiving callbacks from the QUIC implementation. These all
  // run on the proxy thread.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the QUIC handshake finishes and fingerprints have been
    // verified.
    virtual void OnConnected(P2PQuicNegotiatedParams negotiated_params) {}
    // Called when the remote side has indicated it is closed.
    virtual void OnRemoteStopped() {}
    // Called when the connection is closed due to a QUIC error. This can happen
    // locally by the framer or remotely by the peer.
    virtual void OnConnectionFailed(const std::string& error_details,
                                    bool from_remote) {}
    // Called when the remote side has created a new stream.
    virtual void OnStream(QuicStreamProxy* stream_proxy) {}

    // Called after the stats have been gathered on the host thread. The
    // |request_id| maps to |request_id| used in GetStats().
    virtual void OnStats(uint32_t request_id,
                         const P2PQuicTransportStats& stats) {}
    // Called when the datagram (sent with SendDatagram) has been
    // consumed by the QUIC library and sent on the network.
    virtual void OnDatagramSent() {}
    // Called when we receive a datagram from the remote side.
    virtual void OnDatagramReceived(Vector<uint8_t> datagram) {}
  };

  // Construct a Proxy with the underlying QUIC implementation running on the
  // same thread as the IceTransportProxy. Callbacks will be serviced by the
  // given delegate.
  // The delegate and IceTransportProxy must outlive the QuicTransportProxy.
  // The QuicTransportProxy will immediately connect to the given
  // IceTransportProxy; it can be disconnected by destroying the
  // QuicTransportProxy object.
  QuicTransportProxy(
      Delegate* delegate,
      IceTransportProxy* ice_transport_proxy,
      std::unique_ptr<P2PQuicTransportFactory> quic_transport_factory,
      const P2PQuicTransportConfig& config);
  ~QuicTransportProxy();

  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread() const;
  scoped_refptr<base::SingleThreadTaskRunner> host_thread() const;

  void Start(P2PQuicTransport::StartConfig config);
  void Stop();

  QuicStreamProxy* CreateStream();

  void SendDatagram(Vector<uint8_t> datagram);

  // Gathers stats on the host thread, then returns them asynchronously with
  // Delegate::OnStats. The |request_id| is used to map the GetStats call to the
  // returned stats.
  void GetStats(uint32_t request_id);

  // QuicStreamProxy callbacks.
  void OnRemoveStream(QuicStreamProxy* stream_proxy);

 private:
  // Callbacks from QuicTransportHost.
  friend class QuicTransportHost;
  void OnConnected(P2PQuicNegotiatedParams negotiated_params);
  void OnRemoteStopped();
  void OnConnectionFailed(const std::string& error_details, bool from_remote);
  void OnStream(std::unique_ptr<QuicStreamProxy> stream_proxy);
  void OnStats(uint32_t request_id, const P2PQuicTransportStats& stats);
  void OnDatagramSent();
  void OnDatagramReceived(Vector<uint8_t> datagram);

  // Since the Host is deleted on the host thread (Via OnTaskRunnerDeleter), as
  // long as this is alive it is safe to post tasks to it (using unretained).
  std::unique_ptr<QuicTransportHost, base::OnTaskRunnerDeleter> host_;
  Delegate* const delegate_;
  IceTransportProxy* ice_transport_proxy_;
  HashMap<QuicStreamProxy*, std::unique_ptr<QuicStreamProxy>> stream_proxies_;

  THREAD_CHECKER(thread_checker_);

  // Must be the last member.
  base::WeakPtrFactory<QuicTransportProxy> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_PROXY_H_
