// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_DEDICATED_WEB_TRANSPORT_HTTP3_CLIENT_H_
#define NET_QUIC_DEDICATED_WEB_TRANSPORT_HTTP3_CLIENT_H_

#include <optional>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/network_anonymization_key.h"
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
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/crypto/web_transport_fingerprint_proof_verifier.h"
#include "net/third_party/quiche/src/quiche/quic/core/deterministic_connection_id_generator.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_spdy_client_session.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection_id.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"
#include "net/third_party/quiche/src/quiche/quic/core/web_transport_interface.h"
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
  // |visitor| and |context| must outlive this object.
  DedicatedWebTransportHttp3Client(
      const GURL& url,
      const url::Origin& origin,
      WebTransportClientVisitor* visitor,
      const NetworkAnonymizationKey& anonymization_key,
      URLRequestContext* context,
      const WebTransportParameters& parameters);
  ~DedicatedWebTransportHttp3Client() override;

  WebTransportState state() const { return state_; }

  // Connect() is an asynchronous operation.  Once the operation is finished,
  // OnConnected() or OnConnectionFailed() is called on the Visitor.
  void Connect() override;
  void Close(const std::optional<WebTransportCloseInfo>& close_info) override;

  quic::WebTransportSession* session() override;

  void OnSettingsReceived();
  void OnHeadersComplete(const quiche::HttpHeaderBlock& headers);
  void OnConnectStreamWriteSideInDataRecvdState();
  void OnConnectStreamAborted();
  void OnConnectStreamDeleted();
  void OnCloseTimeout();
  void OnDatagramProcessed(std::optional<quic::MessageStatus> status);

  // QuicTransportClientSession::ClientVisitor methods.
  void OnSessionReady() override;
  void OnSessionClosed(quic::WebTransportSessionError error_code,
                       const std::string& error_message) override;
  void OnIncomingBidirectionalStreamAvailable() override;
  void OnIncomingUnidirectionalStreamAvailable() override;
  void OnDatagramReceived(std::string_view datagram) override;
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
    CONNECT_STATE_CONNECT_CONFIGURE,
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
  int DoConnectConfigure(int rv);
  int DoConnectComplete();
  void CreateConnection();
  // Sends the CONNECT request to establish a WebTransport session.
  int DoSendRequest();
  // Verifies that the connection has succeeded.
  int DoConfirmConnection();

  void TransitionToState(WebTransportState next_state);

  void SetErrorIfNecessary(int error);
  void SetErrorIfNecessary(int error,
                           quic::QuicErrorCode quic_error,
                           std::string_view details);

  const GURL url_;
  const url::Origin origin_;
  const NetworkAnonymizationKey anonymization_key_;
  const raw_ptr<URLRequestContext> context_;          // Unowned.
  const raw_ptr<WebTransportClientVisitor> visitor_;  // Unowned.

  const raw_ptr<QuicContext> quic_context_;                   // Unowned.
  NetLogWithSource net_log_;
  raw_ptr<base::SequencedTaskRunner> task_runner_;  // Unowned.

  quic::ParsedQuicVersionVector supported_versions_;
  // |original_supported_versions_| starts off empty. If a version negotiation
  // packet is received, versions not supported by the server are removed from
  // |supported_versions_| but the original list is saved in
  // |original_supported_versions_|. This prevents version downgrade attacks.
  quic::ParsedQuicVersionVector original_supported_versions_;
  // TODO(vasilvv): move some of those into QuicContext.
  std::unique_ptr<QuicChromiumAlarmFactory> alarm_factory_;
  quic::QuicCryptoClientConfig crypto_config_;

  WebTransportState state_ = WebTransportState::NEW;
  ConnectState next_connect_state_ = CONNECT_STATE_NONE;
  std::optional<WebTransportError> error_;
  bool retried_with_new_version_ = false;
  bool session_ready_ = false;
  bool safe_to_report_error_details_ = false;
  std::unique_ptr<HttpResponseInfo> http_response_info_;

  ProxyInfo proxy_info_;
  std::unique_ptr<ProxyResolutionRequest> proxy_resolution_request_;
  std::unique_ptr<HostResolver::ResolveHostRequest> resolve_host_request_;

  std::unique_ptr<DatagramClientSocket> socket_;
  // This must be destroyed after `session_`, as it owns the underlying socket
  // and `session_` owns the packet writer, which has a raw pointer to the
  // socket.
  std::unique_ptr<QuicChromiumPacketReader> packet_reader_;
  std::unique_ptr<quic::QuicSpdyClientSession> session_;
  raw_ptr<quic::QuicConnection> connection_;  // owned by |session_|
  raw_ptr<quic::WebTransportSession> web_transport_session_ = nullptr;
  std::unique_ptr<QuicEventLogger> event_logger_;
  quic::DeterministicConnectionIdGenerator connection_id_generator_{
      quic::kQuicDefaultConnectionIdLength};

  std::optional<WebTransportCloseInfo> close_info_;

  base::OneShotTimer close_timeout_timer_;
  base::WeakPtrFactory<DedicatedWebTransportHttp3Client> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_DEDICATED_WEB_TRANSPORT_HTTP3_CLIENT_H_
