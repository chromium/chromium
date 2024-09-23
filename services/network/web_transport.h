// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEB_TRANSPORT_H_
#define SERVICES_NETWORK_WEB_TRANSPORT_H_

#include <memory>
#include <string_view>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/quic/web_transport_client.h"
#include "services/network/public/mojom/web_transport.mojom.h"

class GURL;

namespace url {
class Origin;
}  // namespace url

namespace net {
class NetworkAnonymizationKey;
}  // namespace net

namespace network {

class NetworkContext;

// The implementation for WebTransport
// (https://w3c.github.io/webtransport/#web-transport) in the NetworkService.
// Implements mojom::WebTransport with the net/ implementation.
class COMPONENT_EXPORT(NETWORK_SERVICE) WebTransport final
    : public mojom::WebTransport,
      public net::WebTransportClientVisitor {
 public:
  class Stream;
  using BidirectionalStreamAcceptanceCallback =
      base::OnceCallback<void(uint32_t,
                              mojo::ScopedDataPipeConsumerHandle,
                              mojo::ScopedDataPipeProducerHandle)>;
  using UnidirectionalStreamAcceptanceCallback =
      base::OnceCallback<void(uint32_t, mojo::ScopedDataPipeConsumerHandle)>;
  WebTransport(
      const GURL& url,
      const url::Origin& origin,
      const net::NetworkAnonymizationKey& key,
      const std::vector<mojom::WebTransportCertificateFingerprintPtr>&
          fingerprints,
      NetworkContext* context,
      mojo::PendingRemote<mojom::WebTransportHandshakeClient> handshake_client);
  ~WebTransport() override;

  // mojom::WebTransport implementation:
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
  void AbortStream(uint32_t stream_id, uint8_t code) override;
  void StopSending(uint32_t stream_id, uint8_t code) override;
  void SetOutgoingDatagramExpirationDuration(base::TimeDelta duration) override;
  void GetStats(GetStatsCallback callback) override;
  void Close(mojom::WebTransportCloseInfoPtr close_info) override;

  // WebTransportClientVisitor implementation:
  void OnConnected(
      scoped_refptr<net::HttpResponseHeaders> response_headers) override;
  void OnConnectionFailed(const net::WebTransportError& error) override;
  void OnClosed(
      const std::optional<net::WebTransportCloseInfo>& close_info) override;
  void OnError(const net::WebTransportError& error) override;
  void OnIncomingBidirectionalStreamAvailable() override;
  void OnIncomingUnidirectionalStreamAvailable() override;
  void OnDatagramReceived(std::string_view datagram) override;
  void OnCanCreateNewOutgoingBidirectionalStream() override;
  void OnCanCreateNewOutgoingUnidirectionalStream() override;
  void OnDatagramProcessed(std::optional<quic::MessageStatus> status) override;

  bool torn_down() const { return torn_down_; }

 private:
  void TearDown();
  void Dispose();

  const std::unique_ptr<net::WebTransportClient> transport_;
  const raw_ptr<NetworkContext> context_;  // outlives |this|.

  bool closing_ = false;
  bool torn_down_ = false;

  // Destroy `streams_` before `closing_` and `torn_down_`; its destructor
  // calls back into `WebTransport` to check those flags.
  std::map<uint32_t, std::unique_ptr<Stream>> streams_;

  // These callbacks must be destroyed after |client_| because of mojo callback
  // destruction checks, so they are declared first.
  base::queue<BidirectionalStreamAcceptanceCallback>
      bidirectional_stream_acceptances_;
  base::queue<UnidirectionalStreamAcceptanceCallback>
      unidirectional_stream_acceptances_;

  mojo::Receiver<mojom::WebTransport> receiver_;
  mojo::Remote<mojom::WebTransportHandshakeClient> handshake_client_;
  mojo::Remote<mojom::WebTransportClient> client_;
  base::queue<base::OnceCallback<void(bool)>> datagram_callbacks_;

  // This must be the last member.
  base::WeakPtrFactory<WebTransport> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_TRANSPORT_H_
