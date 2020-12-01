// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_QUIC_TRANSPORT_H_
#define SERVICES_NETWORK_QUIC_TRANSPORT_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/quic/quic_transport_client.h"
#include "services/network/public/mojom/quic_transport.mojom.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace net {
class NetworkIsolationKey;
}  // namespace net

namespace network {

class NetworkContext;

// The implementation for QuicTransport
// (https://wicg.github.io/web-transport/#quic-transport) in the NetworkService.
// Implements mojom::QuicTransport with the net/ implementation.
class COMPONENT_EXPORT(NETWORK_SERVICE) QuicTransport final
    : public mojom::QuicTransport,
      public net::QuicTransportClient::Visitor {
 public:
  class Stream;
  using BidirectionalStreamAcceptanceCallback =
      base::OnceCallback<void(uint32_t,
                              mojo::ScopedDataPipeConsumerHandle,
                              mojo::ScopedDataPipeProducerHandle)>;
  using UnidirectionalStreamAcceptanceCallback =
      base::OnceCallback<void(uint32_t, mojo::ScopedDataPipeConsumerHandle)>;
  QuicTransport(
      const GURL& url,
      const url::Origin& origin,
      const net::NetworkIsolationKey& key,
      const std::vector<mojom::QuicTransportCertificateFingerprintPtr>&
          fingerprints,
      NetworkContext* context,
      mojo::PendingRemote<mojom::QuicTransportHandshakeClient>
          handshake_client);
  ~QuicTransport() override;

  // mojom::QuicTransport implementation:
  void SendDatagram(base::span<const uint8_t> data,
                    base::OnceCallback<void(bool)> callback) override;
  void CreateStream(mojo::ScopedDataPipeConsumerHandle readable,
                    mojo::ScopedDataPipeProducerHandle writable,
                    base::OnceCallback<void(bool, uint32_t)> callback) override;
  void AcceptBidirectionalStream(
      BidirectionalStreamAcceptanceCallback callback) override;
  void AcceptUnidirectionalStream(
      UnidirectionalStreamAcceptanceCallback callback) override;
  void SendFin(uint32_t stream_id) override;
  void AbortStream(uint32_t stream_id, uint64_t code) override;
  void SetOutgoingDatagramExpirationDuration(base::TimeDelta duration) override;

  // net::QuicTransportClient::Visitor implementation:
  void OnConnected() override;
  void OnConnectionFailed() override;
  void OnClosed() override;
  void OnError() override;
  void OnIncomingBidirectionalStreamAvailable() override;
  void OnIncomingUnidirectionalStreamAvailable() override;
  void OnDatagramReceived(base::StringPiece datagram) override;
  void OnCanCreateNewOutgoingBidirectionalStream() override;
  void OnCanCreateNewOutgoingUnidirectionalStream() override;
  void OnDatagramProcessed(base::Optional<quic::MessageStatus> status) override;

  bool torn_down() const { return torn_down_; }

 private:
  void TearDown();
  void Dispose();

  const std::unique_ptr<net::QuicTransportClient> transport_;
  NetworkContext* const context_;  // outlives |this|.

  std::map<uint32_t, std::unique_ptr<Stream>> streams_;

  // These callbacks must be destroyed after |client_| because of mojo callback
  // destruction checks, so they are declared first.
  base::queue<BidirectionalStreamAcceptanceCallback>
      bidirectional_stream_acceptances_;
  base::queue<UnidirectionalStreamAcceptanceCallback>
      unidirectional_stream_acceptances_;

  mojo::Receiver<mojom::QuicTransport> receiver_;
  mojo::Remote<mojom::QuicTransportHandshakeClient> handshake_client_;
  mojo::Remote<mojom::QuicTransportClient> client_;
  base::queue<base::OnceCallback<void(bool)>> datagram_callbacks_;

  bool torn_down_ = false;

  // This must be the last member.
  base::WeakPtrFactory<QuicTransport> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_QUIC_TRANSPORT_H_
