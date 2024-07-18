// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/dedicated_web_transport_http3_client.h"

#include <string_view>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/address_list.h"
#include "net/base/port_util.h"
#include "net/base/url_util.h"
#include "net/http/http_network_session.h"
#include "net/log/net_log_values.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/proxy_resolution_request.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/web_transport_http3.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_utils.h"
#include "net/url_request/url_request_context.h"
#include "url/scheme_host_port.h"

namespace net {

namespace {

// From
// https://wicg.github.io/web-transport/#dom-quictransportconfiguration-server_certificate_fingerprints
constexpr int kCustomCertificateMaxValidityDays = 14;

// The time the client would wait for the server to acknowledge the session
// being closed.
constexpr base::TimeDelta kMaxCloseTimeout = base::Seconds(2);

// Enables custom congestion control for WebTransport over HTTP/3.
BASE_FEATURE(kWebTransportCongestionControl,
             "WebTransportCongestionControl",
             base::FEATURE_DISABLED_BY_DEFAULT);
constexpr base::FeatureParam<quic::CongestionControlType>::Option
    kWebTransportCongestionControlAlgorithms[] = {
        {quic::kCubicBytes, "CUBIC"},
        {quic::kRenoBytes, "Reno"},
        {quic::kBBR, "BBRv1"},
        {quic::kBBRv2, "BBRv2"},
};
constexpr base::FeatureParam<quic::CongestionControlType>
    kWebTransportCongestionControlAlgorithm{
        &kWebTransportCongestionControl, /*name=*/"algorithm",
        /*default_value=*/quic::kCubicBytes,
        &kWebTransportCongestionControlAlgorithms};

std::set<std::string> HostsFromOrigins(std::set<HostPortPair> origins) {
  std::set<std::string> hosts;
  for (const auto& origin : origins) {
    hosts.insert(origin.host());
  }
  return hosts;
}

// A version of WebTransportFingerprintProofVerifier that enforces
// Chromium-specific policies.
class ChromiumWebTransportFingerprintProofVerifier
    : public quic::WebTransportFingerprintProofVerifier {
 public:
  using WebTransportFingerprintProofVerifier::
      WebTransportFingerprintProofVerifier;

 protected:
  bool IsKeyTypeAllowedByPolicy(
      const quic::CertificateView& certificate) override {
    if (certificate.public_key_type() == quic::PublicKeyType::kRsa) {
      return false;
    }
    return WebTransportFingerprintProofVerifier::IsKeyTypeAllowedByPolicy(
        certificate);
  }
};

std::unique_ptr<quic::ProofVerifier> CreateProofVerifier(
    const NetworkAnonymizationKey& anonymization_key,
    URLRequestContext* context,
    const WebTransportParameters& parameters) {
  if (parameters.server_certificate_fingerprints.empty()) {
    std::set<std::string> hostnames_to_allow_unknown_roots = HostsFromOrigins(
        context->quic_context()->params()->origins_to_force_quic_on);
    if (context->quic_context()->params()->webtransport_developer_mode) {
      hostnames_to_allow_unknown_roots.insert("");
    }
    return std::make_unique<ProofVerifierChromium>(
        context->cert_verifier(), context->transport_security_state(),
        context->sct_auditing_delegate(),
        std::move(hostnames_to_allow_unknown_roots), anonymization_key);
  }

  auto verifier =
      std::make_unique<ChromiumWebTransportFingerprintProofVerifier>(
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

void RecordNetLogQuicSessionClientStateChanged(
    NetLogWithSource& net_log,
    WebTransportState last_state,
    WebTransportState next_state,
    const std::optional<WebTransportError>& error) {
  net_log.AddEvent(
      NetLogEventType::QUIC_SESSION_WEBTRANSPORT_CLIENT_STATE_CHANGED, [&] {
        auto dict = base::Value::Dict()
                        .Set("last_state", WebTransportStateString(last_state))
                        .Set("next_state", WebTransportStateString(next_state));
        if (error.has_value()) {
          dict.Set("error",
                   base::Value::Dict()
                       .Set("net_error", error->net_error)
                       .Set("quic_error", static_cast<int>(error->quic_error))
                       .Set("details", error->details));
        }
        return dict;
      });
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

  ~ConnectStream() override { client_->OnConnectStreamDeleted(); }

  void OnInitialHeadersComplete(
      bool fin,
      size_t frame_len,
      const quic::QuicHeaderList& header_list) override {
    quic::QuicSpdyClientStream::OnInitialHeadersComplete(fin, frame_len,
                                                         header_list);
    client_->OnHeadersComplete(response_headers());
  }

  void OnClose() override {
    quic::QuicSpdyClientStream::OnClose();
    if (fin_received() && fin_sent()) {
      // Clean close.
      return;
    }
    if (stream_error() == quic::QUIC_STREAM_CONNECTION_ERROR) {
      // If stream is closed due to the connection error, OnConnectionClosed()
      // will populate the correct error details.
      return;
    }
    client_->OnConnectStreamAborted();
  }

  void OnWriteSideInDataRecvdState() override {
    quic::QuicSpdyClientStream::OnWriteSideInDataRecvdState();
    client_->OnConnectStreamWriteSideInDataRecvdState();
  }

 private:
  raw_ptr<DedicatedWebTransportHttp3Client> client_;
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
      DedicatedWebTransportHttp3Client* client)
      : quic::QuicSpdyClientSession(config,
                                    supported_versions,
                                    connection,
                                    server_id,
                                    crypto_config),
        client_(client) {}

  bool OnSettingsFrame(const quic::SettingsFrame& frame) override {
    if (!quic::QuicSpdyClientSession::OnSettingsFrame(frame)) {
      return false;
    }
    client_->OnSettingsReceived();
    return true;
  }

  quic::WebTransportHttp3VersionSet LocallySupportedWebTransportVersions()
      const override {
    quic::WebTransportHttp3VersionSet versions =
        quic::WebTransportHttp3VersionSet(
            {quic::WebTransportHttp3Version::kDraft02});
    if (base::FeatureList::IsEnabled(features::kEnableWebTransportDraft07)) {
      versions.Set(quic::WebTransportHttp3Version::kDraft07);
    }
    return versions;
  }

  quic::HttpDatagramSupport LocalHttpDatagramSupport() override {
    return quic::HttpDatagramSupport::kRfcAndDraft04;
  }

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

  void OnDatagramProcessed(std::optional<quic::MessageStatus> status) override {
    client_->OnDatagramProcessed(
        status.has_value() ? std::optional<quic::MessageStatus>(*status)
                           : std::optional<quic::MessageStatus>());
  }

 private:
  raw_ptr<DedicatedWebTransportHttp3Client> client_;
};

class WebTransportVisitorProxy : public quic::WebTransportVisitor {
 public:
  explicit WebTransportVisitorProxy(quic::WebTransportVisitor* visitor)
      : visitor_(visitor) {}

  void OnSessionReady() override { visitor_->OnSessionReady(); }
  void OnSessionClosed(quic::WebTransportSessionError error_code,
                       const std::string& error_message) override {
    visitor_->OnSessionClosed(error_code, error_message);
  }
  void OnIncomingBidirectionalStreamAvailable() override {
    visitor_->OnIncomingBidirectionalStreamAvailable();
  }
  void OnIncomingUnidirectionalStreamAvailable() override {
    visitor_->OnIncomingUnidirectionalStreamAvailable();
  }
  void OnDatagramReceived(std::string_view datagram) override {
    visitor_->OnDatagramReceived(datagram);
  }
  void OnCanCreateNewOutgoingBidirectionalStream() override {
    visitor_->OnCanCreateNewOutgoingBidirectionalStream();
  }
  void OnCanCreateNewOutgoingUnidirectionalStream() override {
    visitor_->OnCanCreateNewOutgoingUnidirectionalStream();
  }

 private:
  raw_ptr<quic::WebTransportVisitor> visitor_;
};

bool IsTerminalState(WebTransportState state) {
  return state == WebTransportState::CLOSED ||
         state == WebTransportState::FAILED;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class NegotiatedHttpDatagramVersion {
  kNone = 0,
  kDraft04 = 1,
  kRfc = 2,
  kMaxValue = kRfc,
};

void RecordNegotiatedHttpDatagramSupport(quic::HttpDatagramSupport support) {
  NegotiatedHttpDatagramVersion negotiated;
  switch (support) {
    case quic::HttpDatagramSupport::kNone:
      negotiated = NegotiatedHttpDatagramVersion::kNone;
      break;
    case quic::HttpDatagramSupport::kDraft04:
      negotiated = NegotiatedHttpDatagramVersion::kDraft04;
      break;
    case quic::HttpDatagramSupport::kRfc:
      negotiated = NegotiatedHttpDatagramVersion::kRfc;
      break;
    case quic::HttpDatagramSupport::kRfcAndDraft04:
      NOTREACHED_IN_MIGRATION();
      return;
  }
  base::UmaHistogramEnumeration(
      "Net.WebTransport.NegotiatedHttpDatagramVersion", negotiated);
}

const char* WebTransportHttp3VersionString(
    quic::WebTransportHttp3Version version) {
  switch (version) {
    case quic::WebTransportHttp3Version::kDraft02:
      return "draft-02";
    case quic::WebTransportHttp3Version::kDraft07:
      return "draft-07";
  }
}

enum class NegotiatedWebTransportVersion {
  kDraft02 = 0,
  kDraft07 = 1,
  kMaxValue = kDraft07,
};

void RecordNegotiatedWebTransportVersion(
    quic::WebTransportHttp3Version version) {
  NegotiatedWebTransportVersion negotiated;
  switch (version) {
    case quic::WebTransportHttp3Version::kDraft02:
      negotiated = NegotiatedWebTransportVersion::kDraft02;
      break;
    case quic::WebTransportHttp3Version::kDraft07:
      negotiated = NegotiatedWebTransportVersion::kDraft07;
      break;
  }
  base::UmaHistogramEnumeration(
      "Net.WebTransport.NegotiatedWebTransportVersion", negotiated);
}

void AdjustSendAlgorithm(quic::QuicConnection& connection) {
  if (!base::FeatureList::IsEnabled(kWebTransportCongestionControl)) {
    return;
  }
  connection.sent_packet_manager().SetSendAlgorithm(
      kWebTransportCongestionControlAlgorithm.Get());
}

}  // namespace

DedicatedWebTransportHttp3Client::DedicatedWebTransportHttp3Client(
    const GURL& url,
    const url::Origin& origin,
    WebTransportClientVisitor* visitor,
    const NetworkAnonymizationKey& anonymization_key,
    URLRequestContext* context,
    const WebTransportParameters& parameters)
    : url_(url),
      origin_(origin),
      anonymization_key_(anonymization_key),
      context_(context),
      visitor_(visitor),
      quic_context_(context->quic_context()),
      net_log_(NetLogWithSource::Make(context->net_log(),
                                      NetLogSourceType::WEB_TRANSPORT_CLIENT)),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault().get()),
      alarm_factory_(
          std::make_unique<QuicChromiumAlarmFactory>(task_runner_,
                                                     quic_context_->clock())),
      // TODO(vasilvv): proof verifier should have proper error reporting
      // (currently, all certificate verification errors result in "TLS
      // handshake error" even when more detailed message is available).  This
      // requires implementing ProofHandler::OnProofVerifyDetailsAvailable.
      crypto_config_(
          CreateProofVerifier(anonymization_key_, context, parameters),
          /* session_cache */ nullptr) {
  ConfigureQuicCryptoClientConfig(crypto_config_);
  net_log_.BeginEvent(
      NetLogEventType::QUIC_SESSION_WEBTRANSPORT_CLIENT_ALIVE, [&] {
        base::Value::Dict dict;
        dict.Set("url", url.possibly_invalid_spec());
        dict.Set("network_anonymization_key",
                 anonymization_key.ToDebugString());
        return dict;
      });
}

DedicatedWebTransportHttp3Client::~DedicatedWebTransportHttp3Client() {
  net_log_.EndEventWithNetErrorCode(
      NetLogEventType::QUIC_SESSION_WEBTRANSPORT_CLIENT_ALIVE,
      error_ ? error_->net_error : OK);
  // |session_| owns this, so we need to make sure we release it before
  // it gets dangling.
  connection_ = nullptr;
}

void DedicatedWebTransportHttp3Client::Connect() {
  if (state_ != WebTransportState::NEW ||
      next_connect_state_ != CONNECT_STATE_NONE) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  TransitionToState(WebTransportState::CONNECTING);
  next_connect_state_ = CONNECT_STATE_INIT;
  DoLoop(OK);
}

void DedicatedWebTransportHttp3Client::Close(
    const std::optional<WebTransportCloseInfo>& close_info) {
  CHECK(session());
  base::TimeDelta probe_timeout = base::Microseconds(
      connection_->sent_packet_manager().GetPtoDelay().ToMicroseconds());
  // Wait for at least three PTOs similar to what's used in
  // https://www.rfc-editor.org/rfc/rfc9000.html#name-immediate-close
  base::TimeDelta close_timeout = std::min(3 * probe_timeout, kMaxCloseTimeout);
  close_timeout_timer_.Start(
      FROM_HERE, close_timeout,
      base::BindOnce(&DedicatedWebTransportHttp3Client::OnCloseTimeout,
                     weak_factory_.GetWeakPtr()));
  if (close_info.has_value()) {
    session()->CloseSession(close_info->code, close_info->reason);
  } else {
    session()->CloseSession(0, "");
  }
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
      case CONNECT_STATE_CONNECT_CONFIGURE:
        rv = DoConnectConfigure(rv);
        break;
      case CONNECT_STATE_CONNECT_COMPLETE:
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
        NOTREACHED_IN_MIGRATION() << "Invalid state reached: " << connect_state;
        rv = ERR_FAILED;
        break;
    }
  } while (rv == OK && next_connect_state_ != CONNECT_STATE_NONE);

  if (rv == OK || rv == ERR_IO_PENDING)
    return;
  SetErrorIfNecessary(rv);
  TransitionToState(WebTransportState::FAILED);
}

int DedicatedWebTransportHttp3Client::DoInit() {
  if (!url_.is_valid())
    return ERR_INVALID_URL;
  if (url_.scheme_piece() != url::kHttpsScheme)
    return ERR_DISALLOWED_URL_SCHEME;

  if (!IsPortAllowedForScheme(url_.EffectiveIntPort(), url_.scheme_piece()))
    return ERR_UNSAFE_PORT;

  // TODO(vasilvv): check if QUIC is disabled by policy.

  // Ensure that RFC 9000 is always supported.
  supported_versions_ = quic::ParsedQuicVersionVector{
      quic::ParsedQuicVersion::RFCv1(),
  };
  // Add other supported versions if available.
  for (quic::ParsedQuicVersion& version :
       quic_context_->params()->supported_versions) {
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
      url_, /* method */ "CONNECT", anonymization_key_, &proxy_info_,
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
      url::SchemeHostPort(url_), anonymization_key_, net_log_, std::nullopt);
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
  next_connect_state_ = CONNECT_STATE_CONNECT_CONFIGURE;

  // TODO(vasilvv): consider unifying parts of this code with QuicSocketFactory
  // (which currently has a lot of code specific to QuicChromiumClientSession).
  socket_ = context_->GetNetworkSessionContext()
                ->client_socket_factory->CreateDatagramClientSocket(
                    DatagramSocket::DEFAULT_BIND, net_log_.net_log(),
                    net_log_.source());
  if (quic_context_->params()->enable_socket_recv_optimization)
    socket_->EnableRecvOptimization();
  socket_->UseNonBlockingIO();

  IPEndPoint server_address =
      *resolve_host_request_->GetAddressResults()->begin();
  return socket_->ConnectAsync(
      server_address, base::BindOnce(&DedicatedWebTransportHttp3Client::DoLoop,
                                     base::Unretained(this)));
}

void DedicatedWebTransportHttp3Client::CreateConnection() {
  // Delete the objects in the same order they would be normally deleted by the
  // destructor.
  session_ = nullptr;
  packet_reader_ = nullptr;

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
      /* owns_writer */ true, quic::Perspective::IS_CLIENT, supported_versions_,
      connection_id_generator_);
  connection_ = connection.get();
  connection->SetMaxPacketLength(quic_context_->params()->max_packet_length);

  session_ = std::make_unique<DedicatedWebTransportHttp3ClientSession>(
      InitializeQuicConfig(*quic_context_->params()), supported_versions_,
      connection.release(),
      quic::QuicServerId(url_.host(), url_.EffectiveIntPort()), &crypto_config_,
      this);
  if (!original_supported_versions_.empty()) {
    session_->set_client_original_supported_versions(
        original_supported_versions_);
  }

  packet_reader_ = std::make_unique<QuicChromiumPacketReader>(
      std::move(socket_), quic_context_->clock(), this,
      kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          kQuicYieldAfterDurationMilliseconds),
      quic_context_->params()->report_ecn, net_log_);

  event_logger_ = std::make_unique<QuicEventLogger>(session_.get(), net_log_);
  connection_->set_debug_visitor(event_logger_.get());
  connection_->set_creator_debug_delegate(event_logger_.get());
  AdjustSendAlgorithm(*connection_);

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
  safe_to_report_error_details_ = true;
  next_connect_state_ = CONNECT_STATE_SEND_REQUEST;
  return OK;
}

int DedicatedWebTransportHttp3Client::DoConnectConfigure(int rv) {
  if (rv != OK) {
    return rv;
  }

  rv = socket_->SetReceiveBufferSize(kQuicSocketReceiveBufferSize);
  if (rv != OK) {
    return rv;
  }

  rv = socket_->SetDoNotFragment();
  if (rv == ERR_NOT_IMPLEMENTED) {
    rv = OK;
  }
  if (rv != OK) {
    return rv;
  }

  rv = socket_->SetSendBufferSize(quic::kMaxOutgoingPacketSize * 20);
  if (rv != OK) {
    return rv;
  }

  next_connect_state_ = CONNECT_STATE_CONNECT_COMPLETE;
  CreateConnection();
  return ERR_IO_PENDING;
}

void DedicatedWebTransportHttp3Client::OnSettingsReceived() {
  DCHECK_EQ(next_connect_state_, CONNECT_STATE_CONNECT_COMPLETE);
  // Wait until the SETTINGS parser is finished, and then send the request.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DedicatedWebTransportHttp3Client::DoLoop,
                                weak_factory_.GetWeakPtr(), OK));
}

void DedicatedWebTransportHttp3Client::OnHeadersComplete(
    const quiche::HttpHeaderBlock& headers) {
  http_response_info_ = std::make_unique<HttpResponseInfo>();
  const int rv = SpdyHeadersToHttpResponse(headers, http_response_info_.get());
  if (rv != OK) {
    SetErrorIfNecessary(ERR_QUIC_PROTOCOL_ERROR);
    TransitionToState(WebTransportState::FAILED);
    return;
  }
  // TODO(vasilvv): add support for this header in downstream tests and remove
  // this.
  DCHECK(http_response_info_->headers);
  http_response_info_->headers->RemoveHeader("sec-webtransport-http3-draft");

  DCHECK_EQ(next_connect_state_, CONNECT_STATE_CONFIRM_CONNECTION);
  DoLoop(OK);
}

void DedicatedWebTransportHttp3Client::
    OnConnectStreamWriteSideInDataRecvdState() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DedicatedWebTransportHttp3Client::TransitionToState,
                     weak_factory_.GetWeakPtr(), WebTransportState::CLOSED));
}

void DedicatedWebTransportHttp3Client::OnConnectStreamAborted() {
  SetErrorIfNecessary(session_ready_ ? ERR_FAILED : ERR_METHOD_NOT_SUPPORTED);
  TransitionToState(WebTransportState::FAILED);
}

void DedicatedWebTransportHttp3Client::OnConnectStreamDeleted() {
  // `web_transport_session_` is owned by ConnectStream. Clear so that it
  // doesn't get dangling.
  web_transport_session_ = nullptr;
}

void DedicatedWebTransportHttp3Client::OnCloseTimeout() {
  SetErrorIfNecessary(ERR_TIMED_OUT);
  TransitionToState(WebTransportState::FAILED);
}

int DedicatedWebTransportHttp3Client::DoSendRequest() {
  quic::QuicConnection::ScopedPacketFlusher scope(connection_);

  DedicatedWebTransportHttp3ClientSession* session =
      static_cast<DedicatedWebTransportHttp3ClientSession*>(session_.get());
  ConnectStream* stream = session->CreateConnectStream();
  if (stream == nullptr) {
    return ERR_QUIC_PROTOCOL_ERROR;
  }

  quiche::HttpHeaderBlock headers;
  DCHECK_EQ(url_.scheme(), url::kHttpsScheme);
  headers[":scheme"] = url_.scheme();
  headers[":method"] = "CONNECT";
  headers[":authority"] = GetHostAndOptionalPort(url_);
  headers[":path"] = url_.PathForRequest();
  headers[":protocol"] = "webtransport";
  headers["sec-webtransport-http3-draft02"] = "1";
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

  TransitionToState(WebTransportState::CONNECTED);
  return OK;
}

void DedicatedWebTransportHttp3Client::TransitionToState(
    WebTransportState next_state) {
  // Ignore all state transition requests if we have reached the terminal
  // state.
  if (IsTerminalState(state_)) {
    DCHECK(IsTerminalState(next_state))
        << "from: " << state_ << ", to: " << next_state;
    return;
  }

  DCHECK_NE(state_, next_state);
  const WebTransportState last_state = state_;
  state_ = next_state;
  RecordNetLogQuicSessionClientStateChanged(net_log_, last_state, next_state,
                                            error_);
  switch (next_state) {
    case WebTransportState::CONNECTING:
      DCHECK_EQ(last_state, WebTransportState::NEW);
      break;

    case WebTransportState::CONNECTED:
      DCHECK_EQ(last_state, WebTransportState::CONNECTING);
      visitor_->OnConnected(http_response_info_->headers);
      break;

    case WebTransportState::CLOSED:
      DCHECK_EQ(last_state, WebTransportState::CONNECTED);
      connection_->CloseConnection(quic::QUIC_NO_ERROR,
                                   "WebTransport client terminated",
                                   quic::ConnectionCloseBehavior::SILENT_CLOSE);
      visitor_->OnClosed(close_info_);
      break;

    case WebTransportState::FAILED:
      DCHECK(error_.has_value());
      if (last_state == WebTransportState::CONNECTING) {
        visitor_->OnConnectionFailed(*error_);
        break;
      }
      DCHECK_EQ(last_state, WebTransportState::CONNECTED);
      // Ensure the connection is properly closed before deleting it.
      connection_->CloseConnection(
          quic::QUIC_INTERNAL_ERROR,
          "WebTransportState::ERROR reached but the connection still open",
          quic::ConnectionCloseBehavior::SILENT_CLOSE);
      visitor_->OnError(*error_);
      break;

    default:
      NOTREACHED_IN_MIGRATION() << "Invalid state reached: " << next_state;
      break;
  }
}

void DedicatedWebTransportHttp3Client::SetErrorIfNecessary(int error) {
  SetErrorIfNecessary(error, quic::QUIC_NO_ERROR, ErrorToString(error));
}

void DedicatedWebTransportHttp3Client::SetErrorIfNecessary(
    int error,
    quic::QuicErrorCode quic_error,
    std::string_view details) {
  if (!error_) {
    error_ = WebTransportError(error, quic_error, details,
                               safe_to_report_error_details_);
  }
}

void DedicatedWebTransportHttp3Client::OnSessionReady() {
  CHECK(session_->SupportsWebTransport());

  session_ready_ = true;

  RecordNegotiatedWebTransportVersion(
      *session_->SupportedWebTransportVersion());
  RecordNegotiatedHttpDatagramSupport(session_->http_datagram_support());
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_WEBTRANSPORT_SESSION_READY,
                    [&] {
                      base::Value::Dict dict;
                      dict.Set("http_datagram_version",
                               quic::HttpDatagramSupportToString(
                                   session_->http_datagram_support()));
                      dict.Set("webtransport_http3_version",
                               WebTransportHttp3VersionString(
                                   *session_->SupportedWebTransportVersion()));
                      return dict;
                    });
}

void DedicatedWebTransportHttp3Client::OnSessionClosed(
    quic::WebTransportSessionError error_code,
    const std::string& error_message) {
  close_info_ = WebTransportCloseInfo(error_code, error_message);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DedicatedWebTransportHttp3Client::TransitionToState,
                     weak_factory_.GetWeakPtr(), WebTransportState::CLOSED));
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
    std::string_view datagram) {
  visitor_->OnDatagramReceived(datagram);
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
  SetErrorIfNecessary(result);
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
  SetErrorIfNecessary(error_code);
  connection_->OnWriteError(error_code);
}

void DedicatedWebTransportHttp3Client::OnWriteUnblocked() {
  connection_->OnCanWrite();
}

void DedicatedWebTransportHttp3Client::OnConnectionClosed(
    quic::QuicErrorCode error,
    const std::string& error_details,
    quic::ConnectionCloseSource source) {
  // If the session is already in a terminal state due to reasons other than
  // connection close, we should ignore it; otherwise we risk re-entering the
  // connection teardown process.
  if (IsTerminalState(state_)) {
    return;
  }

  if (!retried_with_new_version_ &&
      session_->error() == quic::QUIC_INVALID_VERSION) {
    retried_with_new_version_ = true;
    DCHECK(original_supported_versions_.empty());
    original_supported_versions_ = supported_versions_;
    std::erase_if(
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
    TransitionToState(WebTransportState::CLOSED);
    return;
  }

  SetErrorIfNecessary(ERR_QUIC_PROTOCOL_ERROR, error, error_details);

  if (state_ == WebTransportState::CONNECTING) {
    DoLoop(OK);
    return;
  }

  TransitionToState(WebTransportState::FAILED);
}

void DedicatedWebTransportHttp3Client::OnDatagramProcessed(
    std::optional<quic::MessageStatus> status) {
  visitor_->OnDatagramProcessed(status);
}

}  // namespace net
