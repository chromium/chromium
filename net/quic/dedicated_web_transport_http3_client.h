// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_DEDICATED_WEB_TRANSPORT_HTTP3_CLIENT_H_
#define NET_QUIC_DEDICATED_WEB_TRANSPORT_HTTP3_CLIENT_H_

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
#include "net/third_party/quiche/src/quic/core/http/quic_client_push_promise_index.h"
#include "net/third_party/quiche/src/quic/core/http/quic_spdy_client_session.h"
#include "net/third_party/quiche/src/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quic/core/web_transport_interface.h"
#include "net/third_party/quiche/src/quic/quic_transport/web_transport_fingerprint_proof_verifier.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class ProxyResolutionRequest;
class QuicChromiumAlarmFactory;
class URLRequestContext;

// Creates a dedicated HTTP/3 connection for a WebTransport session.
class NET_EXPORT DedicatedWebTransportHttp3Client
    : public WebTransportClient,
      public quic::WebTransportVisitor,
      public QuicChromiumPacketReader::Visitor,
      public QuicChromiumPacketWriter::Delegate {
 public:
  // QUIC protocol versions that are used in the origin trial.
  static quic::ParsedQuicVersionVector
  QuicVersionsForWebTransportOriginTrial() {
    return quic::ParsedQuicVersionVector{
        quic::ParsedQuicVersion::Draft29(),
    };
  }

  // |visitor| and |context| must outlive this object.
  DedicatedWebTransportHttp3Client(const GURL& url,
                                   const url::Origin& origin,
                                   WebTransportClientVisitor* visitor,
                                   const NetworkIsolationKey& isolation_key,
                                   URLRequestContext* context,
                                   const WebTransportParameters& parameters);
  ~DedicatedWebTransportHttp3Client() override;

  WebTransportState state() const { return state_; }
  const WebTransportError& error() const override;

  // Connect() is an asynchronous operation.  Once the operation is finished,
  // OnConnected() or OnConnectionFailed() is called on the Visitor.
  void Connect() override;

  quic::WebTransportSession* session() override;

  void OnSettingsReceived();
  void OnHeadersComplete();
  void OnConnectStreamClosed();
  void OnDatagramProcessed(absl::optional<quic::MessageStatus> status);

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

  void OnConnectionClosed(quic::QuicErrorCode error,
                          const std::string& error_details,
                          quic::ConnectionCloseSource source);

 private:
  // State of the connection establishment process.
  enum ConnectState {
    CONNECT_STATE_NONE,
    CONNECT_STATE_INIT,
    CONNECT_STATE_CHECK_PROXY,
    CONNECT_STATE_CHECK_PROXY_COMPLETE,
    CONNECT_STATE_RESOLVE_HOST,
    CONNECT_STATE_RESOLVE_HOST_COMPLETE,
    CONNECT_STATE_CONNECT,
    CONNECT_STATE_CONNECT_COMPLETE,
    CONNECT_STATE_SEND_REQUEST,
    CONNECT_STATE_CONFIRM_CONNECTION,

    CONNECT_STATE_NUM_STATES,
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
  int DoConnectComplete();
  void CreateConnection();
  // Sends the CONNECT request to establish a WebTransport session.
  int DoSendRequest();
  // Verifies that the connection has succeeded.
  int DoConfirmConnection();

  void TransitionToState(WebTransportState next_state);

  const GURL url_;
  const url::Origin origin_;
  const NetworkIsolationKey isolation_key_;
  URLRequestContext* const context_;          // Unowned.
  WebTransportClientVisitor* const visitor_;  // Unowned.

  ClientSocketFactory* const client_socket_factory_;  // Unowned.
  QuicContext* const quic_context_;                   // Unowned.
  NetLogWithSource net_log_;
  base::SequencedTaskRunner* task_runner_;  // Unowned.

  quic::ParsedQuicVersionVector supported_versions_;
  // TODO(vasilvv): move some of those into QuicContext.
  std::unique_ptr<QuicChromiumAlarmFactory> alarm_factory_;
  quic::QuicCryptoClientConfig crypto_config_;

  WebTransportState state_ = NEW;
  ConnectState next_connect_state_ = CONNECT_STATE_NONE;
  WebTransportError error_;
  bool retried_with_new_version_ = false;
  bool session_ready_ = false;

  ProxyInfo proxy_info_;
  std::unique_ptr<ProxyResolutionRequest> proxy_resolution_request_;
  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_host_request_;

  std::unique_ptr<DatagramClientSocket> socket_;
  quic::QuicConnection* connection_;  // owned by |session_|
  std::unique_ptr<quic::QuicSpdyClientSession> session_;
  quic::QuicSpdyStream* connect_stream_ = nullptr;
  quic::WebTransportSession* web_transport_session_ = nullptr;
  std::unique_ptr<QuicChromiumPacketReader> packet_reader_;
  std::unique_ptr<QuicEventLogger> event_logger_;
  quic::QuicClientPushPromiseIndex push_promise_index_;

  base::WeakPtrFactory<DedicatedWebTransportHttp3Client> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_DEDICATED_WEB_TRANSPORT_HTTP3_CLIENT_H_
