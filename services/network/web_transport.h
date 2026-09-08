// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEB_TRANSPORT_H_
#define SERVICES_NETWORK_WEB_TRANSPORT_H_

#include <map>
#include <memory>
#include <string_view>
#include <vector>

#include "base/containers/lru_cache.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/web_transport_client.h"
#include "net/third_party/quiche/src/quiche/web_transport/web_transport.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
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
class WebTransportTestPeer;

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
      const std::vector<std::string>& application_protocols,
      mojom::WebTransportCongestionControl congestion_control,
      std::optional<uint16_t>
          anticipated_concurrent_incoming_unidirectional_streams,
      std::optional<uint16_t>
          anticipated_concurrent_incoming_bidirectional_streams,
      std::vector<net::HttpRequestHeaders::HeaderKeyValuePair>
          additional_headers,
      NetworkContext* context,
      mojo::PendingRemote<mojom::WebTransportHandshakeClient> handshake_client,
      mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojom::ClientSecurityStatePtr client_security_state);
  ~WebTransport() override;

  // mojom::WebTransport implementation:
  void SendDatagram(base::span<const uint8_t> data,
                    base::OnceCallback<void(bool)> callback) override;
  void CreateDatagramWritable(
      mojo::PendingReceiver<mojom::WebTransportDatagramWritable> writable,
      mojom::WebTransportStreamPriorityPtr priority) override;
  void CreateStream(mojo::ScopedDataPipeConsumerHandle readable,
                    mojo::ScopedDataPipeProducerHandle writable,
                    mojom::WebTransportStreamPriorityPtr priority,
                    base::OnceCallback<void(bool, uint32_t)> callback) override;
  void AcceptBidirectionalStream(
      BidirectionalStreamAcceptanceCallback callback) override;
  void AcceptUnidirectionalStream(
      UnidirectionalStreamAcceptanceCallback callback) override;
  void SendFin(uint32_t stream_id) override;
  void AbortStream(uint32_t stream_id, uint8_t code) override;
  void StopSending(uint32_t stream_id, uint8_t code) override;
  void SetStreamPriority(
      uint32_t stream_id,
      mojom::WebTransportStreamPriorityPtr priority) override;
  void SetOutgoingDatagramExpirationDuration(base::TimeDelta duration) override;
  void GetStats(GetStatsCallback callback) override;
  void GetReceiveStreamStats(uint32_t stream_id,
                             GetReceiveStreamStatsCallback callback) override;
  void Close(mojom::WebTransportCloseInfoPtr close_info) override;

  // WebTransportClientVisitor implementation:
  void OnLocalNetworkAccessCheck(const net::IPEndPoint& server_address,
                                 const net::NetLogWithSource& net_log,
                                 net::CompletionOnceCallback callback) override;
  void OnBeforeConnect(const net::IPEndPoint& server_address) override;
  void OnConnected(
      scoped_refptr<net::HttpResponseHeaders> response_headers) override;
  void OnConnectionFailed(const net::WebTransportError& error) override;
  void OnClosed(
      const std::optional<net::WebTransportCloseInfo>& close_info) override;
  void OnError(const net::WebTransportError& error) override;
  void OnDraining() override;
  void OnIncomingBidirectionalStreamAvailable() override;
  void OnIncomingUnidirectionalStreamAvailable() override;
  void OnDatagramReceived(std::string_view datagram) override;
  void OnCanCreateNewOutgoingBidirectionalStream() override;
  void OnCanCreateNewOutgoingUnidirectionalStream() override;
  void OnDatagramProcessed(std::optional<quic::DatagramStatus> status) override;

  bool torn_down() const { return torn_down_; }
  bool HasFinalReceiveStreamStatsForTesting(uint32_t stream_id) const {
    return final_receive_stream_stats_.Peek(stream_id) !=
           final_receive_stream_stats_.end();
  }

 private:
  friend class WebTransportTestPeer;

  class DatagramWritable;

  static constexpr size_t kMaxDatagramWritables = 1024;
  static constexpr size_t kMaxPendingDatagrams = 4096;
  static constexpr size_t kMaxPendingDatagramBytes = 16 * 1024 * 1024;

  struct PendingDatagram {
    std::vector<uint8_t> data;
    base::TimeTicks queued_at;
    base::OnceCallback<void(bool)> callback;
  };

  void QueueDatagram(DatagramWritable* writable,
                     base::span<const uint8_t> data,
                     base::OnceCallback<void(bool)> callback);
  void AccountForRemovedPendingDatagram(size_t size);
  void ScheduleDatagramWritable(DatagramWritable* writable);
  DatagramWritable* SelectNextDatagramWritable();
  void RescheduleDatagramGroup(webtransport::SendGroupId send_group_id);
  void MaybeSendDatagrams();
  void CompleteNextDatagram(bool sent);
  void ScheduleDatagramPump();
  base::TimeDelta GetOutgoingDatagramExpirationDuration() const;
  void ResetDatagramExpirationTimer();
  void ExpirePendingDatagrams();
  // Discards the queued Datagrams of a writable whose pipe the renderer closed
  // and stops scheduling it. A Datagram which is already in flight keeps its
  // slot in `datagram_callbacks_` so that QUICHE's FIFO completion order stays
  // aligned.
  void OnDatagramWritableDisconnected(DatagramWritable* writable);
  // Closes every Datagram writable and drops the Datagrams they have queued.
  void ClearDatagramState();
  void MovePendingDatagramToInFlightForTesting(size_t index);
  void ExpireNextDatagramForTesting(size_t index);

  void TearDown();
  void Dispose();

  const std::unique_ptr<net::WebTransportClient> transport_;
  const GURL url_;
  const url::Origin origin_;
  const raw_ptr<NetworkContext> context_;  // outlives |this|.

  bool closing_ = false;
  bool torn_down_ = false;

  bool draining_received_ = false;

  // Destroy `streams_` before `closing_` and `torn_down_`; its destructor
  // calls back into `WebTransport` to check those flags.
  std::map<uint32_t, std::unique_ptr<Stream>> streams_;
  // Maps a stream ID to its final received bytes after the renderer closes its
  // data pipe. The renderer requests these stats before sending StopSending(),
  // which erases the entry. If the renderer disconnects before then, Dispose()
  // destroys this WebTransport and the map with it. Entries cannot be erased
  // when Stream is disposed because the stats request and data-pipe closure
  // arrive on independently ordered Mojo pipes. Capped to
  // kMaxFinalReceiveStreamStatsEntries, in order to avoid unbounded memory
  // growth if the renderer never calls stop sending data.
  base::LRUCache<uint32_t, uint64_t> final_receive_stream_stats_;

  // These callbacks must be destroyed after |client_| because of mojo callback
  // destruction checks, so they are declared first.
  base::queue<BidirectionalStreamAcceptanceCallback>
      bidirectional_stream_acceptances_;
  base::queue<UnidirectionalStreamAcceptanceCallback>
      unidirectional_stream_acceptances_;

  mojo::Receiver<mojom::WebTransport> receiver_;
  mojo::Remote<mojom::WebTransportHandshakeClient> handshake_client_;
  mojo::Remote<mojom::WebTransportClient> client_;
  mojo::Remote<mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_observer_;
  mojom::ClientSecurityStatePtr client_security_state_;
  // Completion callbacks for the Datagrams which have been handed to QUICHE,
  // in the order QUICHE will report them. Entries are only ever appended and
  // removed from the front, including when the writable a Datagram came from
  // goes away. These callbacks belong to the pipes owned by `receiver_` and
  // `datagram_writables_`, so they must be destroyed after those, which is why
  // they are declared first.
  base::queue<base::OnceCallback<void(bool)>> datagram_callbacks_;
  // The writables participating in Datagram scheduling. This owns the Mojo
  // receivers of the writables created by CreateDatagramWritable(), plus the
  // writable backing the legacy SendDatagram() path, which has no receiver.
  // Order is stable so that scheduling ties are broken by creation order.
  std::vector<std::unique_ptr<DatagramWritable>> datagram_writables_;
  // Both are owned by `datagram_writables_` and cleared when the writable they
  // point at goes away.
  raw_ptr<DatagramWritable> legacy_datagram_writable_ = nullptr;
  raw_ptr<DatagramWritable> in_flight_datagram_writable_ = nullptr;
  base::TimeDelta outgoing_datagram_expiration_duration_;
  base::OneShotTimer datagram_expiration_timer_;
  uint64_t expired_pending_datagram_count_ = 0;
  // Generates both group and writable schedule orders. Those values are only
  // compared within their respective ordering domains.
  uint64_t next_datagram_schedule_order_ = 0;
  size_t pending_datagram_count_ = 0;
  size_t pending_datagram_bytes_ = 0;
  // These flags detect a synchronous OnDatagramProcessed() callback from
  // SendOrQueueDatagram().
  bool datagram_send_in_progress_ = false;
  bool datagram_processed_during_send_ = false;
  bool datagram_blocked_ = false;
  bool datagram_pump_scheduled_ = false;

  // This must be the last member.
  base::WeakPtrFactory<WebTransport> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEB_TRANSPORT_H_
