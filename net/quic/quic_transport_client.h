// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_TRANSPORT_CLIENT_H_
#define NET_QUIC_QUIC_TRANSPORT_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_context.h"
#include "net/quic/quic_event_logger.h"
#include "net/quic/web_transport_client.h"
#include "net/quic/web_transport_error.h"
#include "net/socket/client_socket_factory.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quic/core/web_transport_interface.h"
#include "net/third_party/quiche/src/quic/quic_transport/quic_transport_client_session.h"
#include "net/third_party/quiche/src/quic/quic_transport/web_transport_fingerprint_proof_verifier.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class ProxyResolutionRequest;
class QuicChromiumAlarmFactory;
class URLRequestContext;

// QuicTransportClient is the top-level API for QuicTransport in //net.
class NET_EXPORT QuicTransportClient
    : public WebTransportClient,
      public quic::WebTransportVisitor,
      public QuicChromiumPacketReader::Visitor,
      public QuicChromiumPacketWriter::Delegate,
      public quic::QuicSession::Visitor {
 public:
  // QUIC protocol versions that are used in the origin trial.
  static quic::ParsedQuicVersionVector
  QuicVersionsForWebTransportOriginTrial() {
    return quic::ParsedQuicVersionVector{
        quic::ParsedQuicVersion::Draft29(),  // Enabled in M85
    };
  }

  // |visitor| and |context| must outlive this object.
  QuicTransportClient(const GURL& url,
                      const url::Origin& origin,
                      WebTransportClientVisitor* visitor,
                      const NetworkIsolationKey& isolation_key,
                      URLRequestContext* context,
                      const WebTransportParameters& parameters);
  ~QuicTransportClient() override;

  WebTransportState state() const { return state_; }
  const WebTransportError& error() const override;

  // Connect() is an asynchronous operation.  Once the operation is finished,
  // OnConnected() or OnConnectionFailed() is called on the Visitor.
  void Connect() override;

  quic::WebTransportSession* session() override;
  quic::QuicTransportClientSession* quic_session();

  // QuicTransportClientSession::ClientVisitor methods.
  void OnSessionReady() override;
  void OnIncomingBidirectionalStreamAvailable() override;
  void OnIncomingUnidirectionalStreamAvailable() override;
  void OnDatagramReceived(absl::string_view datagram) override;
  void OnCanCreateNewOutgoingBidirectionalStream() override;
  void OnCanCreateNewOutgoingUnidirectionalStream() override;

  // QuicChromiumPacketReader::Visitor methods.
  bool OnReadError(int result, const DatagramClientSocket* socket) override;
  bool OnPacket(const quic::QuicReceivedPacket& packet,
                const quic::QuicSocketAddress& local_address,
                const quic::QuicSocketAddress& peer_address) override;

  // QuicChromiumPacketWriter::Delegate methods.
  int HandleWriteError(int error_code,
                       scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer>
                           last_packet) override;
  void OnWriteError(int error_code) override;
  void OnWriteUnblocked() override;

  // QuicSession::Visitor methods.
  void OnConnectionClosed(quic::QuicConnectionId server_connection_id,
                          quic::QuicErrorCode error,
                          const std::string& error_details,
                          quic::ConnectionCloseSource source) override;
  void OnWriteBlocked(
      quic::QuicBlockedWriterInterface* /*blocked_writer*/) override {}
  void OnRstStreamReceived(const quic::QuicRstStreamFrame& /*frame*/) override {
  }
  void OnStopSendingReceived(
      const quic::QuicStopSendingFrame& /*frame*/) override {}
  void OnNewConnectionIdSent(
      const quic::QuicConnectionId& /*server_connection_id*/,
      const quic::QuicConnectionId& /*new_connecition_id*/) override {}
  void OnConnectionIdRetired(
      const quic::QuicConnectionId& /*server_connection_id*/) override {}

 private:
  // State of the connection establishment process.
  //
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "QuicTransportClientConnectState" in
  // src/tools/metrics/histograms/enums.xml.
  enum ConnectState {
    CONNECT_STATE_NONE,
    CONNECT_STATE_INIT,
    CONNECT_STATE_CHECK_PROXY,
    CONNECT_STATE_CHECK_PROXY_COMPLETE,
    CONNECT_STATE_RESOLVE_HOST,
    CONNECT_STATE_RESOLVE_HOST_COMPLETE,
    CONNECT_STATE_CONNECT,
    CONNECT_STATE_CONFIRM_CONNECTION,

    CONNECT_STATE_NUM_STATES,
  };

  class DatagramObserverProxy : public quic::QuicDatagramQueue::Observer {
   public:
    explicit DatagramObserverProxy(QuicTransportClient* client)
        : client_(client) {}
    void OnDatagramProcessed(
        absl::optional<quic::MessageStatus> status) override;

   private:
    QuicTransportClient* client_;
  };

  // DoLoop processing the Connect() call.
  void DoLoop(int rv);
  // Verifies the basic preconditions for setting up the connection.
  int DoInit();
  // Verifies that there is no mandatory proxy configured for the specified URL.
  int DoCheckProxy();
  int DoCheckProxyComplete(int rv);
  // Resolves the hostname in the URL.
  int DoResolveHost();
  int DoResolveHostComplete(int rv);
  // Establishes the QUIC connection.
  int DoConnect();
  void CreateConnection();
  // Verifies that the connection has succeeded.
  int DoConfirmConnection();

  void TransitionToState(WebTransportState next_state);

  const GURL url_;
  const url::Origin origin_;
  const NetworkIsolationKey isolation_key_;
  URLRequestContext* const context_;  // Unowned.
  WebTransportClientVisitor* const visitor_;  // Unowned.

  ClientSocketFactory* const client_socket_factory_;
  QuicContext* const quic_context_;
  NetLogWithSource net_log_;
  base::SequencedTaskRunner* task_runner_;

  quic::ParsedQuicVersionVector supported_versions_;
  // TODO(vasilvv): move some of those into QuicContext.
  std::unique_ptr<QuicChromiumAlarmFactory> alarm_factory_;
  quic::QuicCryptoClientConfig crypto_config_;

  WebTransportState state_ = NEW;
  ConnectState next_connect_state_ = CONNECT_STATE_NONE;
  WebTransportError error_;
  bool retried_with_new_version_ = false;

  ProxyInfo proxy_info_;
  std::unique_ptr<ProxyResolutionRequest> proxy_resolution_request_;
  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_host_request_;

  std::unique_ptr<DatagramClientSocket> socket_;
  std::unique_ptr<quic::QuicConnection> connection_;
  std::unique_ptr<quic::QuicTransportClientSession> session_;
  std::unique_ptr<QuicChromiumPacketReader> packet_reader_;
  std::unique_ptr<QuicEventLogger> event_logger_;

  base::WeakPtrFactory<QuicTransportClient> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_TRANSPORT_CLIENT_H_
