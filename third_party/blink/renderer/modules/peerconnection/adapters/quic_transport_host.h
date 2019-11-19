// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_HOST_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport_factory.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class IceTransportHost;
class QuicStreamHost;
class QuicTransportProxy;

// The host class is the host side correspondent to the QuicTransportProxy. See
// the QuicTransportProxy documentation for background. This class lives on the
// host thread and proxies calls between the QuicTransportProxy and the
// P2PQuicTransport (which is single-threaded).
//
//        proxy thread                                host thread
// +-----------------------+             +-----------------------------------+
// |                       |             |                                   |
// |        <-> ICE Proxy  |  =========> |  ICE Host <-> P2PTransportChannel |
// |               ^       |  <--------- |     ^                ^            |
// | client        |       |             |     |                |            |
// |               v       |             |     v                v            |
// |        <-> QUIC Proxy |  =========> | QUIC Host <-> P2PQuicTransport    |
// |                       |  <--------- |                                   |
// +-----------------------+             +-----------------------------------+
//
// The QuicTransportHost connects to the underlying IceTransportHost in
// Initialize and disconnects in the destructor. The IceTransportHost must
// outlive the QuicTransportHost.
//
// The Host can be constructed on any thread but after that point all methods
// must be called on the host thread.
class QuicTransportHost final : public P2PQuicTransport::Delegate {
 public:
  QuicTransportHost(
      base::WeakPtr<QuicTransportProxy> transport_proxy,
      std::unique_ptr<P2PQuicTransportFactory> quic_transport_factory);
  ~QuicTransportHost() override;

  void Initialize(IceTransportHost* ice_transport_host,
                  const P2PQuicTransportConfig& config);

  scoped_refptr<base::SingleThreadTaskRunner> proxy_thread() const;
  scoped_refptr<base::SingleThreadTaskRunner> host_thread() const;

  void Start(P2PQuicTransport::StartConfig config);
  void Stop();

  void CreateStream(std::unique_ptr<QuicStreamHost> stream_host);

  void SendDatagram(Vector<uint8_t> datagram);

  void GetStats(uint32_t request_id);

  // QuicStreamHost callbacks.
  void OnRemoveStream(QuicStreamHost* stream_host_to_remove);

 private:
  // P2PQuicTransport::Delegate overrides.
  void OnRemoteStopped() override;
  void OnConnectionFailed(const std::string& error_details,
                          bool from_remote) override;
  void OnConnected(P2PQuicNegotiatedParams negotiated_params) override;
  void OnStream(P2PQuicStream* stream) override;
  void OnDatagramSent() override;
  void OnDatagramReceived(Vector<uint8_t> datagram) override;

  std::unique_ptr<P2PQuicTransportFactory> quic_transport_factory_;
  std::unique_ptr<P2PQuicTransport> quic_transport_;
  base::WeakPtr<QuicTransportProxy> proxy_;
  IceTransportHost* ice_transport_host_ = nullptr;
  HashMap<QuicStreamHost*, std::unique_ptr<QuicStreamHost>> stream_hosts_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_QUIC_TRANSPORT_HOST_H_
