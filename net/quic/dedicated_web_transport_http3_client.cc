// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/dedicated_web_transport_http3_client.h"

#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/strings/abseil_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/url_util.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/third_party/quiche/src/quic/core/http/web_transport_http3.h"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/url_request/url_request_context.h"

namespace net {

namespace {
// From
// https://wicg.github.io/web-transport/#dom-quictransportconfiguration-server_certificate_fingerprints
constexpr int kCustomCertificateMaxValidityDays = 14;

std::set<std::string> HostsFromOrigins(std::set<HostPortPair> origins) {
  std::set<std::string> hosts;
  for (const auto& origin : origins) {
    hosts.insert(origin.host());
  }
  return hosts;
}

std::unique_ptr<quic::ProofVerifier> CreateProofVerifier(
    const NetworkIsolationKey& isolation_key,
    URLRequestContext* context,
    const WebTransportParameters& parameters) {
  if (parameters.server_certificate_fingerprints.empty()) {
    return std::make_unique<ProofVerifierChromium>(
        context->cert_verifier(), context->ct_policy_enforcer(),
        context->transport_security_state(), context->sct_auditing_delegate(),
        HostsFromOrigins(
            context->quic_context()->params()->origins_to_force_quic_on),
        isolation_key);
  }

  auto verifier = std::make_unique<quic::WebTransportFingerprintProofVerifier>(
      context->quic_context()->clock(), kCustomCertificateMaxValidityDays);
  for (const quic::CertificateFingerprint& fingerprint :
       parameters.server_certificate_fingerprints) {
    bool success = verifier->AddFingerprint(fingerprint);
    if (!success) {
      DLOG(WARNING) << "Failed to add a certificate fingerprint: "
                    << fingerprint.fingerprint;
    }
  }
  return verifier;
}

// The stream associated with an extended CONNECT request for the WebTransport
// session.
class ConnectStream : public quic::QuicSpdyClientStream {
 public:
  ConnectStream(quic::QuicStreamId id,
                quic::QuicSpdyClientSession* session,
                quic::StreamType type,
                DedicatedWebTransportHttp3Client* client)
      : quic::QuicSpdyClientStream(id, session, type), client_(client) {}

  void OnInitialHeadersComplete(
      bool fin,
      size_t frame_len,
      const quic::QuicHeaderList& header_list) override {
    quic::QuicSpdyClientStream::OnInitialHeadersComplete(fin, frame_len,
                                                         header_list);
    client_->OnHeadersComplete();
  }

  void OnClose() override {
    quic::QuicSpdyClientStream::OnClose();
    client_->OnConnectStreamClosed();
  }

 private:
  DedicatedWebTransportHttp3Client* client_;
};

class DedicatedWebTransportHttp3ClientSession
    : public quic::QuicSpdyClientSession {
 public:
  DedicatedWebTransportHttp3ClientSession(
      const quic::QuicConfig& config,
      const quic::ParsedQuicVersionVector& supported_versions,
      quic::QuicConnection* connection,
      const quic::QuicServerId& server_id,
      quic::QuicCryptoClientConfig* crypto_config,
      quic::QuicClientPushPromiseIndex* push_promise_index,
      DedicatedWebTransportHttp3Client* client)
      : quic::QuicSpdyClientSession(config,
                                    supported_versions,
                                    connection,
                                    server_id,
                                    crypto_config,
                                    push_promise_index),
        client_(client) {}

  bool OnSettingsFrame(const quic::SettingsFrame& frame) override {
    if (!quic::QuicSpdyClientSession::OnSettingsFrame(frame)) {
      return false;
    }
    client_->OnSettingsReceived();
    return true;
  }

  bool ShouldNegotiateWebTransport() override { return true; }
  bool ShouldNegotiateHttp3Datagram() override { return true; }

  void OnConnectionClosed(const quic::QuicConnectionCloseFrame& frame,
                          quic::ConnectionCloseSource source) override {
    quic::QuicSpdyClientSession::OnConnectionClosed(frame, source);
    client_->OnConnectionClosed(frame.quic_error_code, frame.error_details,
                                source);
  }

  ConnectStream* CreateConnectStream() {
    if (!ShouldCreateOutgoingBidirectionalStream()) {
      return nullptr;
    }
    std::unique_ptr<ConnectStream> stream =
        std::make_unique<ConnectStream>(GetNextOutgoingBidirectionalStreamId(),
                                        this, quic::BIDIRECTIONAL, client_);
    ConnectStream* stream_ptr = stream.get();
    ActivateStream(std::move(stream));
    return stream_ptr;
  }

  void OnDatagramProcessed(
      absl::optional<quic::MessageStatus> status) override {
    client_->OnDatagramProcessed(
        status.has_value() ? base::Optional<quic::MessageStatus>(*status)
                           : base::Optional<quic::MessageStatus>());
  }

 private:
  DedicatedWebTransportHttp3Client* client_;
};

class WebTransportVisitorProxy : public quic::WebTransportVisitor {
 public:
  explicit WebTransportVisitorProxy(quic::WebTransportVisitor* visitor)
      : visitor_(visitor) {}

  void OnSessionReady() override { visitor_->OnSessionReady(); }
  void OnIncomingBidirectionalStreamAvailable() override {
    visitor_->OnIncomingBidirectionalStreamAvailable();
  }
  void OnIncomingUnidirectionalStreamAvailable() override {
    visitor_->OnIncomingUnidirectionalStreamAvailable();
  }
  void OnDatagramReceived(absl::string_view datagram) override {
    visitor_->OnDatagramReceived(datagram);
  }
  void OnCanCreateNewOutgoingBidirectionalStream() override {
    visitor_->OnCanCreateNewOutgoingBidirectionalStream();
  }
  void OnCanCreateNewOutgoingUnidirectionalStream() override {
    visitor_->OnCanCreateNewOutgoingUnidirectionalStream();
  }

 private:
  quic::WebTransportVisitor* visitor_;
};

}  // namespace

DedicatedWebTransportHttp3Client::DedicatedWebTransportHttp3Client(
    const GURL& url,
    const url::Origin& origin,
    WebTransportClientVisitor* visitor,
    const NetworkIsolationKey& isolation_key,
    URLRequestContext* context,
    const WebTransportParameters& parameters)
    : url_(url),
      origin_(origin),
      isolation_key_(isolation_key),
      context_(context),
      visitor_(visitor),
      // TODO(vasilvv): pass ClientSocketFactory through QuicContext.
      client_socket_factory_(ClientSocketFactory::GetDefaultFactory()),
      quic_context_(context->quic_context()),
      net_log_(NetLogWithSource::Make(context->net_log(),
                                      NetLogSourceType::QUIC_TRANSPORT_CLIENT)),
      task_runner_(base::ThreadTaskRunnerHandle::Get().get()),
      alarm_factory_(
          std::make_unique<QuicChromiumAlarmFactory>(task_runner_,
                                                     quic_context_->clock())),
      // TODO(vasilvv): proof verifier should have proper error reporting
      // (currently, all certificate verification errors result in "TLS
      // handshake error" even when more detailed message is available).  This
      // requires implementing ProofHandler::OnProofVerifyDetailsAvailable.
      crypto_config_(CreateProofVerifier(isolation_key_, context, parameters),
                     /* session_cache */ nullptr) {}

DedicatedWebTransportHttp3Client::~DedicatedWebTransportHttp3Client() = default;

void DedicatedWebTransportHttp3Client::Connect() {
  if (state_ != NEW || next_connect_state_ != CONNECT_STATE_NONE) {
    NOTREACHED();
    return;
  }

  TransitionToState(CONNECTING);
  next_connect_state_ = CONNECT_STATE_INIT;
  DoLoop(OK);
}

const QuicTransportError& DedicatedWebTransportHttp3Client::error() const {
  return error_;
}

quic::WebTransportSession* DedicatedWebTransportHttp3Client::session() {
  if (web_transport_session_ == nullptr)
    return nullptr;
  return web_transport_session_;
}

void DedicatedWebTransportHttp3Client::DoLoop(int rv) {
  do {
    ConnectState connect_state = next_connect_state_;
    next_connect_state_ = CONNECT_STATE_NONE;
    switch (connect_state) {
      case CONNECT_STATE_INIT:
        DCHECK_EQ(rv, OK);
        rv = DoInit();
        break;
      case CONNECT_STATE_CHECK_PROXY:
        DCHECK_EQ(rv, OK);
        rv = DoCheckProxy();
        break;
      case CONNECT_STATE_CHECK_PROXY_COMPLETE:
        rv = DoCheckProxyComplete(rv);
        break;
      case CONNECT_STATE_RESOLVE_HOST:
        DCHECK_EQ(rv, OK);
        rv = DoResolveHost();
        break;
      case CONNECT_STATE_RESOLVE_HOST_COMPLETE:
        rv = DoResolveHostComplete(rv);
        break;
      case CONNECT_STATE_CONNECT:
        DCHECK_EQ(rv, OK);
        rv = DoConnect();
        break;
      case CONNECT_STATE_CONNECT_COMPLETE:
        DCHECK_EQ(rv, OK);
        rv = DoConnectComplete();
        break;
      case CONNECT_STATE_SEND_REQUEST:
        DCHECK_EQ(rv, OK);
        rv = DoSendRequest();
        break;
      case CONNECT_STATE_CONFIRM_CONNECTION:
        DCHECK_EQ(rv, OK);
        rv = DoConfirmConnection();
        break;
      default:
        NOTREACHED() << "Invalid state reached: " << connect_state;
        rv = ERR_FAILED;
        break;
    }
  } while (rv == OK && next_connect_state_ != CONNECT_STATE_NONE);

  if (rv == OK || rv == ERR_IO_PENDING)
    return;
  if (error_.net_error == OK)
    error_.net_error = rv;
  TransitionToState(FAILED);
}

int DedicatedWebTransportHttp3Client::DoInit() {
  if (!url_.is_valid())
    return ERR_INVALID_URL;
  if (url_.scheme_piece() != url::kHttpsScheme)
    return ERR_DISALLOWED_URL_SCHEME;

  // TODO(vasilvv): check if QUIC is disabled by policy.

  // Ensure that for the duration of the origin trial, a fixed QUIC transport
  // version is available.
  supported_versions_ = QuicVersionsForWebTransportOriginTrial();
  // Add other supported versions if available.
  for (quic::ParsedQuicVersion& version :
       quic_context_->params()->supported_versions) {
    if (!version.UsesHttp3())
      continue;
    if (base::Contains(supported_versions_, version))
      continue;  // Skip as we've already added it above.
    supported_versions_.push_back(version);
  }
  if (supported_versions_.empty()) {
    DLOG(ERROR) << "Attempted using WebTransport with no compatible QUIC "
                   "versions available";
    return ERR_NOT_IMPLEMENTED;
  }

  next_connect_state_ = CONNECT_STATE_CHECK_PROXY;
  return OK;
}

int DedicatedWebTransportHttp3Client::DoCheckProxy() {
  next_connect_state_ = CONNECT_STATE_CHECK_PROXY_COMPLETE;
  return context_->proxy_resolution_service()->ResolveProxy(
      url_, /* method */ "CONNECT", isolation_key_, &proxy_info_,
      base::BindOnce(&DedicatedWebTransportHttp3Client::DoLoop,
                     base::Unretained(this)),
      &proxy_resolution_request_, net_log_);
}

int DedicatedWebTransportHttp3Client::DoCheckProxyComplete(int rv) {
  if (rv != OK)
    return rv;

  // If a proxy is configured, we fail the connection.
  if (!proxy_info_.is_direct())
    return ERR_TUNNEL_CONNECTION_FAILED;

  next_connect_state_ = CONNECT_STATE_RESOLVE_HOST;
  return OK;
}

int DedicatedWebTransportHttp3Client::DoResolveHost() {
  next_connect_state_ = CONNECT_STATE_RESOLVE_HOST_COMPLETE;
  HostResolver::ResolveHostParameters parameters;
  resolve_host_request_ = context_->host_resolver()->CreateRequest(
      HostPortPair::FromURL(url_), isolation_key_, net_log_, base::nullopt);
  return resolve_host_request_->Start(base::BindOnce(
      &DedicatedWebTransportHttp3Client::DoLoop, base::Unretained(this)));
}

int DedicatedWebTransportHttp3Client::DoResolveHostComplete(int rv) {
  if (rv != OK)
    return rv;

  DCHECK(resolve_host_request_->GetAddressResults());
  next_connect_state_ = CONNECT_STATE_CONNECT;
  return OK;
}

int DedicatedWebTransportHttp3Client::DoConnect() {
  int rv = OK;

  // TODO(vasilvv): consider unifying parts of this code with QuicSocketFactory
  // (which currently has a lot of code specific to QuicChromiumClientSession).
  socket_ = client_socket_factory_->CreateDatagramClientSocket(
      DatagramSocket::DEFAULT_BIND, net_log_.net_log(), net_log_.source());
  if (quic_context_->params()->enable_socket_recv_optimization)
    socket_->EnableRecvOptimization();
  socket_->UseNonBlockingIO();

  IPEndPoint server_address =
      *resolve_host_request_->GetAddressResults()->begin();
  rv = socket_->Connect(server_address);
  if (rv != OK)
    return rv;

  rv = socket_->SetReceiveBufferSize(kQuicSocketReceiveBufferSize);
  if (rv != OK)
    return rv;

  rv = socket_->SetDoNotFragment();
  if (rv == ERR_NOT_IMPLEMENTED)
    rv = OK;
  if (rv != OK)
    return rv;

  rv = socket_->SetSendBufferSize(quic::kMaxOutgoingPacketSize * 20);
  if (rv != OK)
    return rv;

  CreateConnection();
  next_connect_state_ = CONNECT_STATE_CONNECT_COMPLETE;
  return ERR_IO_PENDING;
}

void DedicatedWebTransportHttp3Client::CreateConnection() {
  // Delete the objects in the same order they would be normally deleted by the
  // destructor.
  packet_reader_ = nullptr;
  session_ = nullptr;

  IPEndPoint server_address =
      *resolve_host_request_->GetAddressResults()->begin();
  quic::QuicConnectionId connection_id =
      quic::QuicUtils::CreateRandomConnectionId(
          quic_context_->random_generator());
  auto connection = std::make_unique<quic::QuicConnection>(
      connection_id, quic::QuicSocketAddress(),
      ToQuicSocketAddress(server_address), quic_context_->helper(),
      alarm_factory_.get(),
      new QuicChromiumPacketWriter(socket_.get(), task_runner_),
      /* owns_writer */ true, quic::Perspective::IS_CLIENT,
      supported_versions_);
  connection_ = connection.get();
  connection->SetMaxPacketLength(quic_context_->params()->max_packet_length);

  session_ = std::make_unique<DedicatedWebTransportHttp3ClientSession>(
      InitializeQuicConfig(*quic_context_->params()), supported_versions_,
      connection.release(),
      quic::QuicServerId(url_.host(), url_.EffectiveIntPort()), &crypto_config_,
      &push_promise_index_, this);

  packet_reader_ = std::make_unique<QuicChromiumPacketReader>(
      socket_.get(), quic_context_->clock(), this, kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      net_log_);

  event_logger_ = std::make_unique<QuicEventLogger>(session_.get(), net_log_);
  connection_->set_debug_visitor(event_logger_.get());
  connection_->set_creator_debug_delegate(event_logger_.get());

  session_->Initialize();
  packet_reader_->StartReading();

  DCHECK(session_->WillNegotiateWebTransport());
  session_->CryptoConnect();
}

int DedicatedWebTransportHttp3Client::DoConnectComplete() {
  if (!connection_->connected()) {
    return ERR_QUIC_PROTOCOL_ERROR;
  }
  // Fail the connection if the received SETTINGS do not support WebTransport.
  if (!session_->SupportsWebTransport()) {
    return ERR_METHOD_NOT_SUPPORTED;
  }
  error_.safe_to_report_details = true;

  next_connect_state_ = CONNECT_STATE_SEND_REQUEST;
  return OK;
}

void DedicatedWebTransportHttp3Client::OnSettingsReceived() {
  DCHECK_EQ(next_connect_state_, CONNECT_STATE_CONNECT_COMPLETE);
  // Wait until the SETTINGS parser is finished, and then send the request.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DedicatedWebTransportHttp3Client::DoLoop,
                                weak_factory_.GetWeakPtr(), OK));
}

void DedicatedWebTransportHttp3Client::OnHeadersComplete() {
  DCHECK_EQ(next_connect_state_, CONNECT_STATE_CONFIRM_CONNECTION);
  DoLoop(OK);
}

void DedicatedWebTransportHttp3Client::OnConnectStreamClosed() {
  error_.net_error = FAILED;
  TransitionToState(FAILED);
}

int DedicatedWebTransportHttp3Client::DoSendRequest() {
  quic::QuicConnection::ScopedPacketFlusher scope(connection_);

  DedicatedWebTransportHttp3ClientSession* session =
      static_cast<DedicatedWebTransportHttp3ClientSession*>(session_.get());
  ConnectStream* stream = session->CreateConnectStream();
  if (stream == nullptr) {
    return ERR_QUIC_PROTOCOL_ERROR;
  }
  connect_stream_ = stream;

  spdy::SpdyHeaderBlock headers;
  DCHECK_EQ(url_.scheme(), url::kHttpsScheme);
  headers[":scheme"] = url_.scheme();
  headers[":method"] = "CONNECT";
  headers[":authority"] = GetHostAndOptionalPort(url_);
  headers[":path"] = url_.PathForRequest();
  headers[":protocol"] = "webtransport";
  headers["origin"] = origin_.Serialize();
  stream->WriteHeaders(std::move(headers), /*fin=*/false, nullptr);

  web_transport_session_ = stream->web_transport();
  if (web_transport_session_ == nullptr) {
    return ERR_METHOD_NOT_SUPPORTED;
  }
  stream->web_transport()->SetVisitor(
      std::make_unique<WebTransportVisitorProxy>(this));

  next_connect_state_ = CONNECT_STATE_CONFIRM_CONNECTION;
  return ERR_IO_PENDING;
}

int DedicatedWebTransportHttp3Client::DoConfirmConnection() {
  if (!session_ready_) {
    return ERR_METHOD_NOT_SUPPORTED;
  }

  TransitionToState(CONNECTED);
  return OK;
}

void DedicatedWebTransportHttp3Client::TransitionToState(
    WebTransportState next_state) {
  const WebTransportState last_state = state_;
  state_ = next_state;
  switch (next_state) {
    case CONNECTING:
      DCHECK_EQ(last_state, NEW);
      break;

    case CONNECTED:
      DCHECK_EQ(last_state, CONNECTING);
      visitor_->OnConnected();
      break;

    case CLOSED:
      DCHECK_EQ(last_state, CONNECTED);
      visitor_->OnClosed();
      break;

    case FAILED:
      DCHECK_NE(error_.net_error, OK);
      if (error_.details.empty()) {
        error_.details = ErrorToString(error_.net_error);
      }

      if (last_state == CONNECTING) {
        visitor_->OnConnectionFailed();
        break;
      }
      DCHECK_EQ(last_state, CONNECTED);
      visitor_->OnError();
      break;

    default:
      NOTREACHED() << "Invalid state reached: " << next_state;
      break;
  }
}

void DedicatedWebTransportHttp3Client::OnSessionReady() {
  session_ready_ = true;
}

void DedicatedWebTransportHttp3Client::
    OnIncomingBidirectionalStreamAvailable() {
  visitor_->OnIncomingBidirectionalStreamAvailable();
}

void DedicatedWebTransportHttp3Client::
    OnIncomingUnidirectionalStreamAvailable() {
  visitor_->OnIncomingUnidirectionalStreamAvailable();
}

void DedicatedWebTransportHttp3Client::OnDatagramReceived(
    absl::string_view datagram) {
  visitor_->OnDatagramReceived(base::StringViewToStringPiece(datagram));
}

void DedicatedWebTransportHttp3Client::
    OnCanCreateNewOutgoingBidirectionalStream() {
  visitor_->OnCanCreateNewOutgoingBidirectionalStream();
}

void DedicatedWebTransportHttp3Client::
    OnCanCreateNewOutgoingUnidirectionalStream() {
  visitor_->OnCanCreateNewOutgoingUnidirectionalStream();
}

bool DedicatedWebTransportHttp3Client::OnReadError(
    int result,
    const DatagramClientSocket* socket) {
  error_.net_error = result;
  connection_->CloseConnection(quic::QUIC_PACKET_READ_ERROR,
                               ErrorToString(result),
                               quic::ConnectionCloseBehavior::SILENT_CLOSE);
  return false;
}

bool DedicatedWebTransportHttp3Client::OnPacket(
    const quic::QuicReceivedPacket& packet,
    const quic::QuicSocketAddress& local_address,
    const quic::QuicSocketAddress& peer_address) {
  session_->ProcessUdpPacket(local_address, peer_address, packet);
  return connection_->connected();
}

int DedicatedWebTransportHttp3Client::HandleWriteError(
    int error_code,
    scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer> /*last_packet*/) {
  return error_code;
}

void DedicatedWebTransportHttp3Client::OnWriteError(int error_code) {
  error_.net_error = error_code;
  connection_->OnWriteError(error_code);
}

void DedicatedWebTransportHttp3Client::OnWriteUnblocked() {
  connection_->OnCanWrite();
}

void DedicatedWebTransportHttp3Client::OnConnectionClosed(
    quic::QuicErrorCode error,
    const std::string& error_details,
    quic::ConnectionCloseSource source) {
  if (!retried_with_new_version_ &&
      session_->error() == quic::QUIC_INVALID_VERSION) {
    retried_with_new_version_ = true;
    base::EraseIf(
        supported_versions_, [this](const quic::ParsedQuicVersion& version) {
          return !base::Contains(
              session_->connection()->server_supported_versions(), version);
        });
    if (!supported_versions_.empty()) {
      // Since this is a callback from QuicConnection, we can't replace the
      // connection object in this method; do it from the top of the event loop
      // instead.
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&DedicatedWebTransportHttp3Client::CreateConnection,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    // If there are no supported versions, treat this as a regular error.
  }

  if (error == quic::QUIC_NO_ERROR) {
    TransitionToState(CLOSED);
    return;
  }

  if (error_.net_error == OK)
    error_.net_error = ERR_QUIC_PROTOCOL_ERROR;
  error_.quic_error = error;
  error_.details = error_details;

  if (state_ == CONNECTING) {
    DoLoop(OK);
    return;
  }

  TransitionToState(FAILED);
}

void DedicatedWebTransportHttp3Client::OnDatagramProcessed(
    base::Optional<quic::MessageStatus> status) {
  visitor_->OnDatagramProcessed(status);
}

}  // namespace net
