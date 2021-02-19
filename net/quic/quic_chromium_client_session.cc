// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_session.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_activity_monitor.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/url_util.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/quic/address_utils.h"
#include "net/quic/crypto/proof_verifier_chromium.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/quic_connectivity_probing_manager.h"
#include "net/quic/quic_crypto_client_stream_factory.h"
#include "net/quic/quic_server_info.h"
#include "net/quic/quic_stream_factory.h"
#include "net/socket/datagram_client_socket.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_log_util.h"
#include "net/spdy/spdy_session.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/third_party/quiche/src/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quiche/src/quic/core/http/spdy_server_push_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_ptr_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

// IPv6 packets have an additional 20 bytes of overhead than IPv4 packets.
const size_t kAdditionalOverheadForIPv6 = 20;

// Maximum number of Readers that are created for any session due to
// connection migration. A new Reader is created every time this endpoint's
// IP address changes.
const size_t kMaxReadersPerQuicSession = 5;

// Time to wait (in seconds) when no networks are available and
// migrating sessions need to wait for a new network to connect.
const size_t kWaitTimeForNewNetworkSecs = 10;

const size_t kMinRetryTimeForDefaultNetworkSecs = 1;

// Maximum RTT time for this session when set initial timeout for probing
// network.
const int kDefaultRTTMilliSecs = 300;

void RecordConnectionCloseErrorCodeImpl(const std::string& histogram,
                                        uint64_t error,
                                        bool is_google_host,
                                        bool handshake_confirmed) {
  base::UmaHistogramSparse(histogram, error);

  if (handshake_confirmed) {
    base::UmaHistogramSparse(histogram + ".HandshakeConfirmed", error);
  } else {
    base::UmaHistogramSparse(histogram + ".HandshakeNotConfirmed", error);
  }

  if (is_google_host) {
    base::UmaHistogramSparse(histogram + "Google", error);

    if (handshake_confirmed) {
      base::UmaHistogramSparse(histogram + "Google.HandshakeConfirmed", error);
    } else {
      base::UmaHistogramSparse(histogram + "Google.HandshakeNotConfirmed",
                               error);
    }
  }
}

void LogMigrateToSocketStatus(bool success) {
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.MigrateToSocketSuccess", success);
}

void RecordConnectionCloseErrorCode(const quic::QuicConnectionCloseFrame& frame,
                                    quic::ConnectionCloseSource source,
                                    const std::string& hostname,
                                    bool handshake_confirmed) {
  bool is_google_host = IsGoogleHost(hostname);
  std::string histogram = "Net.QuicSession.ConnectionCloseErrorCode";

  if (source == quic::ConnectionCloseSource::FROM_SELF) {
    // When sending a CONNECTION_CLOSE frame, it is sufficient to record
    // |quic_error_code|.
    histogram += "Client";
    RecordConnectionCloseErrorCodeImpl(histogram, frame.quic_error_code,
                                       is_google_host, handshake_confirmed);
    return;
  }

  histogram += "Server";

  // Record |quic_error_code|.  Note that when using IETF QUIC, this is
  // extracted from the CONNECTION_CLOSE frame reason phrase, and might be
  // QUIC_IETF_GQUIC_ERROR_MISSING.
  RecordConnectionCloseErrorCodeImpl(histogram, frame.quic_error_code,
                                     is_google_host, handshake_confirmed);

  // For IETF QUIC frames, also record the error code received on the wire.
  if (frame.close_type == quic::IETF_QUIC_TRANSPORT_CONNECTION_CLOSE) {
    histogram += "IetfTransport";
    RecordConnectionCloseErrorCodeImpl(histogram, frame.wire_error_code,
                                       is_google_host, handshake_confirmed);
    if (frame.quic_error_code == quic::QUIC_IETF_GQUIC_ERROR_MISSING) {
      histogram += "GQuicErrorMissing";
      RecordConnectionCloseErrorCodeImpl(histogram, frame.wire_error_code,
                                         is_google_host, handshake_confirmed);
    }
  } else if (frame.close_type == quic::IETF_QUIC_APPLICATION_CONNECTION_CLOSE) {
    histogram += "IetfApplication";
    RecordConnectionCloseErrorCodeImpl(histogram, frame.wire_error_code,
                                       is_google_host, handshake_confirmed);
    if (frame.quic_error_code == quic::QUIC_IETF_GQUIC_ERROR_MISSING) {
      histogram += "GQuicErrorMissing";
      RecordConnectionCloseErrorCodeImpl(histogram, frame.wire_error_code,
                                         is_google_host, handshake_confirmed);
    }
  }
}

base::Value NetLogQuicMigrationFailureParams(
    quic::QuicConnectionId connection_id,
    base::StringPiece reason) {
  base::DictionaryValue dict;
  dict.SetString("connection_id", connection_id.ToString());
  dict.SetString("reason", reason);
  return std::move(dict);
}

base::Value NetLogQuicMigrationSuccessParams(
    quic::QuicConnectionId connection_id) {
  base::DictionaryValue dict;
  dict.SetString("connection_id", connection_id.ToString());
  return std::move(dict);
}

base::Value NetLogProbingResultParams(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress* peer_address,
    bool is_success) {
  base::DictionaryValue dict;
  dict.SetString("network", base::NumberToString(network));
  dict.SetString("peer address", peer_address->ToString());
  dict.SetBoolean("is_success", is_success);
  return std::move(dict);
}

// Histogram for recording the different reasons that a QUIC session is unable
// to complete the handshake.
enum HandshakeFailureReason {
  HANDSHAKE_FAILURE_UNKNOWN = 0,
  HANDSHAKE_FAILURE_BLACK_HOLE = 1,
  HANDSHAKE_FAILURE_PUBLIC_RESET = 2,
  NUM_HANDSHAKE_FAILURE_REASONS = 3,
};

void RecordHandshakeFailureReason(HandshakeFailureReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "Net.QuicSession.ConnectionClose.HandshakeNotConfirmed.Reason", reason,
      NUM_HANDSHAKE_FAILURE_REASONS);
}

// Note: these values must be kept in sync with the corresponding values in:
// tools/metrics/histograms/histograms.xml
enum HandshakeState {
  STATE_STARTED = 0,
  STATE_ENCRYPTION_ESTABLISHED = 1,
  STATE_HANDSHAKE_CONFIRMED = 2,
  STATE_FAILED = 3,
  NUM_HANDSHAKE_STATES = 4
};

enum class ZeroRttState {
  kAttemptedAndSucceeded = 0,
  kAttemptedAndRejected = 1,
  kNotAttempted = 2,
  kMaxValue = kNotAttempted,
};

void RecordHandshakeState(HandshakeState state) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicHandshakeState", state,
                            NUM_HANDSHAKE_STATES);
}

std::string MigrationCauseToString(MigrationCause cause) {
  switch (cause) {
    case UNKNOWN_CAUSE:
      return "Unknown";
    case ON_NETWORK_CONNECTED:
      return "OnNetworkConnected";
    case ON_NETWORK_DISCONNECTED:
      return "OnNetworkDisconnected";
    case ON_WRITE_ERROR:
      return "OnWriteError";
    case ON_NETWORK_MADE_DEFAULT:
      return "OnNetworkMadeDefault";
    case ON_MIGRATE_BACK_TO_DEFAULT_NETWORK:
      return "OnMigrateBackToDefaultNetwork";
    case CHANGE_NETWORK_ON_PATH_DEGRADING:
      return "OnPathDegrading";
    case CHANGE_PORT_ON_PATH_DEGRADING:
      return "ChangePortOnPathDegrading";
    case NEW_NETWORK_CONNECTED_POST_PATH_DEGRADING:
      return "NewNetworkConnectedPostPathDegrading";
    default:
      QUIC_NOTREACHED();
      break;
  }
  return "InvalidCause";
}

base::Value NetLogQuicClientSessionParams(
    const QuicSessionKey* session_key,
    const quic::QuicConnectionId& connection_id,
    const quic::QuicConnectionId& client_connection_id,
    const quic::ParsedQuicVersionVector& supported_versions,
    int cert_verify_flags,
    bool require_confirmation) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("host", session_key->server_id().host());
  dict.SetIntKey("port", session_key->server_id().port());
  dict.SetStringKey("privacy_mode",
                    PrivacyModeToDebugString(session_key->privacy_mode()));
  dict.SetStringKey("network_isolation_key",
                    session_key->network_isolation_key().ToDebugString());
  dict.SetBoolKey("require_confirmation", require_confirmation);
  dict.SetIntKey("cert_verify_flags", cert_verify_flags);
  dict.SetStringKey("connection_id", connection_id.ToString());
  if (!client_connection_id.IsEmpty()) {
    dict.SetStringKey("client_connection_id", client_connection_id.ToString());
  }
  dict.SetStringKey("versions",
                    ParsedQuicVersionVectorToString(supported_versions));
  return dict;
}

base::Value NetLogQuicPushPromiseReceivedParams(
    const spdy::Http2HeaderBlock* headers,
    spdy::SpdyStreamId stream_id,
    spdy::SpdyStreamId promised_stream_id,
    NetLogCaptureMode capture_mode) {
  base::DictionaryValue dict;
  dict.SetKey("headers",
              ElideHttp2HeaderBlockForNetLog(*headers, capture_mode));
  dict.SetInteger("id", stream_id);
  dict.SetInteger("promised_stream_id", promised_stream_id);
  return std::move(dict);
}

// TODO(fayang): Remove this when necessary data is collected.
void LogProbeResultToHistogram(MigrationCause cause, bool success) {
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.PathValidationSuccess", success);
  const std::string histogram_name =
      "Net.QuicSession.PathValidationSuccess." + MigrationCauseToString(cause);
  STATIC_HISTOGRAM_POINTER_GROUP(
      histogram_name, cause, MIGRATION_CAUSE_MAX, AddBoolean(success),
      base::BooleanHistogram::FactoryGet(
          histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag));
}

class HpackEncoderDebugVisitor : public quic::QuicHpackDebugVisitor {
  void OnUseEntry(quic::QuicTime::Delta elapsed) override {
    UMA_HISTOGRAM_TIMES(
        "Net.QuicHpackEncoder.IndexedEntryAge",
        base::TimeDelta::FromMicroseconds(elapsed.ToMicroseconds()));
  }
};

class HpackDecoderDebugVisitor : public quic::QuicHpackDebugVisitor {
  void OnUseEntry(quic::QuicTime::Delta elapsed) override {
    UMA_HISTOGRAM_TIMES(
        "Net.QuicHpackDecoder.IndexedEntryAge",
        base::TimeDelta::FromMicroseconds(elapsed.ToMicroseconds()));
  }
};

class QuicServerPushHelper : public ServerPushDelegate::ServerPushHelper {
 public:
  explicit QuicServerPushHelper(
      base::WeakPtr<QuicChromiumClientSession> session,
      const GURL& url)
      : session_(session), request_url_(url) {}

  void Cancel() override {
    if (session_) {
      session_->CancelPush(request_url_);
    }
  }

  const GURL& GetURL() const override { return request_url_; }

  NetworkIsolationKey GetNetworkIsolationKey() const override {
    if (session_) {
      return session_->quic_session_key().network_isolation_key();
    }
    return NetworkIsolationKey();
  }

 private:
  base::WeakPtr<QuicChromiumClientSession> session_;
  const GURL request_url_;
};

}  // namespace

QuicChromiumClientSession::Handle::Handle(
    const base::WeakPtr<QuicChromiumClientSession>& session,
    const HostPortPair& destination)
    : MultiplexedSessionHandle(session),
      session_(session),
      destination_(destination),
      net_log_(session_->net_log()),
      was_handshake_confirmed_(session->OneRttKeysAvailable()),
      net_error_(OK),
      quic_error_(quic::QUIC_NO_ERROR),
      port_migration_detected_(false),
      server_id_(session_->server_id()),
      quic_version_(session->connection()->version()),
      push_handle_(nullptr),
      was_ever_used_(false) {
  DCHECK(session_);
  session_->AddHandle(this);
}

QuicChromiumClientSession::Handle::~Handle() {
  if (push_handle_) {
    auto* push_handle = push_handle_;
    push_handle_ = nullptr;
    push_handle->Cancel();
  }

  if (session_)
    session_->RemoveHandle(this);
}

void QuicChromiumClientSession::Handle::OnCryptoHandshakeConfirmed() {
  was_handshake_confirmed_ = true;
}

void QuicChromiumClientSession::Handle::OnSessionClosed(
    quic::ParsedQuicVersion quic_version,
    int net_error,
    quic::QuicErrorCode quic_error,
    bool port_migration_detected,
    LoadTimingInfo::ConnectTiming connect_timing,
    bool was_ever_used) {
  session_ = nullptr;
  port_migration_detected_ = port_migration_detected;
  net_error_ = net_error;
  quic_error_ = quic_error;
  quic_version_ = quic_version;
  connect_timing_ = connect_timing;
  push_handle_ = nullptr;
  was_ever_used_ = was_ever_used;
}

bool QuicChromiumClientSession::Handle::IsConnected() const {
  return session_ != nullptr;
}

bool QuicChromiumClientSession::Handle::OneRttKeysAvailable() const {
  return was_handshake_confirmed_;
}

const LoadTimingInfo::ConnectTiming&
QuicChromiumClientSession::Handle::GetConnectTiming() {
  if (!session_)
    return connect_timing_;

  return session_->GetConnectTiming();
}

void QuicChromiumClientSession::Handle::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  if (session_) {
    session_->PopulateNetErrorDetails(details);
  } else {
    details->quic_port_migration_detected = port_migration_detected_;
    details->quic_connection_error = quic_error_;
  }
}

quic::ParsedQuicVersion QuicChromiumClientSession::Handle::GetQuicVersion()
    const {
  if (!session_)
    return quic_version_;

  return session_->GetQuicVersion();
}

void QuicChromiumClientSession::Handle::ResetPromised(
    quic::QuicStreamId id,
    quic::QuicRstStreamErrorCode error_code) {
  if (session_)
    session_->ResetPromised(id, error_code);
}

std::unique_ptr<quic::QuicConnection::ScopedPacketFlusher>
QuicChromiumClientSession::Handle::CreatePacketBundler() {
  if (!session_)
    return nullptr;

  return std::make_unique<quic::QuicConnection::ScopedPacketFlusher>(
      session_->connection());
}

bool QuicChromiumClientSession::Handle::SharesSameSession(
    const Handle& other) const {
  return session_.get() == other.session_.get();
}

int QuicChromiumClientSession::Handle::RendezvousWithPromised(
    const spdy::Http2HeaderBlock& headers,
    CompletionOnceCallback callback) {
  if (!session_)
    return ERR_CONNECTION_CLOSED;

  quic::QuicAsyncStatus push_status =
      session_->push_promise_index()->Try(headers, this, &push_handle_);

  switch (push_status) {
    case quic::QUIC_FAILURE:
      return ERR_FAILED;
    case quic::QUIC_SUCCESS:
      return OK;
    case quic::QUIC_PENDING:
      push_callback_ = std::move(callback);
      return ERR_IO_PENDING;
  }
  NOTREACHED();
  return ERR_UNEXPECTED;
}

int QuicChromiumClientSession::Handle::RequestStream(
    bool requires_confirmation,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!stream_request_);

  if (!session_)
    return ERR_CONNECTION_CLOSED;

  requires_confirmation |= session_->gquic_zero_rtt_disabled();

  // std::make_unique does not work because the StreamRequest constructor
  // is private.
  stream_request_ = base::WrapUnique(
      new StreamRequest(this, requires_confirmation, traffic_annotation));
  return stream_request_->StartRequest(std::move(callback));
}

std::unique_ptr<QuicChromiumClientStream::Handle>
QuicChromiumClientSession::Handle::ReleaseStream() {
  DCHECK(stream_request_);

  auto handle = stream_request_->ReleaseStream();
  stream_request_.reset();
  return handle;
}

std::unique_ptr<QuicChromiumClientStream::Handle>
QuicChromiumClientSession::Handle::ReleasePromisedStream() {
  DCHECK(push_stream_);
  return std::move(push_stream_);
}

int QuicChromiumClientSession::Handle::WaitForHandshakeConfirmation(
    CompletionOnceCallback callback) {
  if (!session_)
    return ERR_CONNECTION_CLOSED;

  return session_->WaitForHandshakeConfirmation(std::move(callback));
}

void QuicChromiumClientSession::Handle::CancelRequest(StreamRequest* request) {
  if (session_)
    session_->CancelRequest(request);
}

int QuicChromiumClientSession::Handle::TryCreateStream(StreamRequest* request) {
  if (!session_)
    return ERR_CONNECTION_CLOSED;

  return session_->TryCreateStream(request);
}

quic::QuicClientPushPromiseIndex*
QuicChromiumClientSession::Handle::GetPushPromiseIndex() {
  if (!session_)
    return push_promise_index_;

  return session_->push_promise_index();
}

int QuicChromiumClientSession::Handle::GetPeerAddress(
    IPEndPoint* address) const {
  if (!session_)
    return ERR_CONNECTION_CLOSED;

  *address = ToIPEndPoint(session_->peer_address());
  return OK;
}

int QuicChromiumClientSession::Handle::GetSelfAddress(
    IPEndPoint* address) const {
  if (!session_)
    return ERR_CONNECTION_CLOSED;

  *address = ToIPEndPoint(session_->self_address());
  return OK;
}

bool QuicChromiumClientSession::Handle::WasEverUsed() const {
  if (!session_)
    return was_ever_used_;

  return session_->WasConnectionEverUsed();
}

bool QuicChromiumClientSession::Handle::CheckVary(
    const spdy::Http2HeaderBlock& client_request,
    const spdy::Http2HeaderBlock& promise_request,
    const spdy::Http2HeaderBlock& promise_response) {
  HttpRequestInfo promise_request_info;
  ConvertHeaderBlockToHttpRequestHeaders(promise_request,
                                         &promise_request_info.extra_headers);
  HttpRequestInfo client_request_info;
  ConvertHeaderBlockToHttpRequestHeaders(client_request,
                                         &client_request_info.extra_headers);

  HttpResponseInfo promise_response_info;
  if (!SpdyHeadersToHttpResponse(promise_response, &promise_response_info)) {
    DLOG(WARNING) << "Invalid headers";
    return false;
  }

  HttpVaryData vary_data;
  if (!vary_data.Init(promise_request_info,
                      *promise_response_info.headers.get())) {
    // Promise didn't contain valid vary info, so URL match was sufficient.
    return true;
  }
  // Now compare the client request for matching.
  return vary_data.MatchesRequest(client_request_info,
                                  *promise_response_info.headers.get());
}

void QuicChromiumClientSession::Handle::OnRendezvousResult(
    quic::QuicSpdyStream* stream) {
  DCHECK(!push_stream_);
  int rv = ERR_FAILED;
  if (stream) {
    rv = OK;
    push_stream_ =
        static_cast<QuicChromiumClientStream*>(stream)->CreateHandle();
  }

  if (push_callback_) {
    DCHECK(push_handle_);
    push_handle_ = nullptr;
    std::move(push_callback_).Run(rv);
  }
}

QuicChromiumClientSession::StreamRequest::StreamRequest(
    QuicChromiumClientSession::Handle* session,
    bool requires_confirmation,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : session_(session),
      requires_confirmation_(requires_confirmation),
      stream_(nullptr),
      traffic_annotation_(traffic_annotation) {}

QuicChromiumClientSession::StreamRequest::~StreamRequest() {
  if (stream_)
    stream_->Reset(quic::QUIC_STREAM_CANCELLED);

  if (session_)
    session_->CancelRequest(this);
}

int QuicChromiumClientSession::StreamRequest::StartRequest(
    CompletionOnceCallback callback) {
  if (!session_->IsConnected())
    return ERR_CONNECTION_CLOSED;

  next_state_ = STATE_WAIT_FOR_CONFIRMATION;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);

  return rv;
}

std::unique_ptr<QuicChromiumClientStream::Handle>
QuicChromiumClientSession::StreamRequest::ReleaseStream() {
  DCHECK(stream_);
  return std::move(stream_);
}

void QuicChromiumClientSession::StreamRequest::OnRequestCompleteSuccess(
    std::unique_ptr<QuicChromiumClientStream::Handle> stream) {
  DCHECK_EQ(STATE_REQUEST_STREAM_COMPLETE, next_state_);

  stream_ = std::move(stream);
  // This method is called even when the request completes synchronously.
  if (callback_)
    DoCallback(OK);
}

void QuicChromiumClientSession::StreamRequest::OnRequestCompleteFailure(
    int rv) {
  DCHECK_EQ(STATE_REQUEST_STREAM_COMPLETE, next_state_);
  // This method is called even when the request completes synchronously.
  if (callback_) {
    // Avoid re-entrancy if the callback calls into the session.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&QuicChromiumClientSession::StreamRequest::DoCallback,
                       weak_factory_.GetWeakPtr(), rv));
  }
}

void QuicChromiumClientSession::StreamRequest::OnIOComplete(int rv) {
  rv = DoLoop(rv);

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    DoCallback(rv);
  }
}

void QuicChromiumClientSession::StreamRequest::DoCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(!callback_.is_null());

  // The client callback can do anything, including destroying this class,
  // so any pending callback must be issued after everything else is done.
  std::move(callback_).Run(rv);
}

int QuicChromiumClientSession::StreamRequest::DoLoop(int rv) {
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_WAIT_FOR_CONFIRMATION:
        CHECK_EQ(OK, rv);
        rv = DoWaitForConfirmation();
        break;
      case STATE_WAIT_FOR_CONFIRMATION_COMPLETE:
        rv = DoWaitForConfirmationComplete(rv);
        break;
      case STATE_REQUEST_STREAM:
        CHECK_EQ(OK, rv);
        rv = DoRequestStream();
        break;
      case STATE_REQUEST_STREAM_COMPLETE:
        rv = DoRequestStreamComplete(rv);
        break;
      default:
        NOTREACHED() << "next_state_: " << next_state_;
        break;
    }
  } while (next_state_ != STATE_NONE && next_state_ && rv != ERR_IO_PENDING);

  return rv;
}

int QuicChromiumClientSession::StreamRequest::DoWaitForConfirmation() {
  next_state_ = STATE_WAIT_FOR_CONFIRMATION_COMPLETE;
  if (requires_confirmation_) {
    return session_->WaitForHandshakeConfirmation(
        base::BindOnce(&QuicChromiumClientSession::StreamRequest::OnIOComplete,
                       weak_factory_.GetWeakPtr()));
  }

  return OK;
}

int QuicChromiumClientSession::StreamRequest::DoWaitForConfirmationComplete(
    int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv < 0)
    return rv;

  next_state_ = STATE_REQUEST_STREAM;
  return OK;
}

int QuicChromiumClientSession::StreamRequest::DoRequestStream() {
  next_state_ = STATE_REQUEST_STREAM_COMPLETE;

  return session_->TryCreateStream(this);
}

int QuicChromiumClientSession::StreamRequest::DoRequestStreamComplete(int rv) {
  DCHECK(rv == OK || !stream_);

  return rv;
}

QuicChromiumClientSession::QuicChromiumPathValidationContext::
    QuicChromiumPathValidationContext(
        const quic::QuicSocketAddress& self_address,
        const quic::QuicSocketAddress& peer_address,
        NetworkChangeNotifier::NetworkHandle network,
        std::unique_ptr<DatagramClientSocket> socket,
        std::unique_ptr<QuicChromiumPacketWriter> writer,
        std::unique_ptr<QuicChromiumPacketReader> reader)
    : QuicPathValidationContext(self_address, peer_address),
      network_handle_(network),
      socket_(std::move(socket)),
      writer_(std::move(writer)),
      reader_(std::move(reader)) {}

QuicChromiumClientSession::QuicChromiumPathValidationContext::
    ~QuicChromiumPathValidationContext() = default;

NetworkChangeNotifier::NetworkHandle
QuicChromiumClientSession::QuicChromiumPathValidationContext::network() {
  return network_handle_;
}
quic::QuicPacketWriter*
QuicChromiumClientSession::QuicChromiumPathValidationContext::WriterToUse() {
  return writer_.get();
}
std::unique_ptr<QuicChromiumPacketWriter>
QuicChromiumClientSession::QuicChromiumPathValidationContext::ReleaseWriter() {
  return std::move(writer_);
}
std::unique_ptr<DatagramClientSocket>
QuicChromiumClientSession::QuicChromiumPathValidationContext::ReleaseSocket() {
  return std::move(socket_);
}
std::unique_ptr<QuicChromiumPacketReader>
QuicChromiumClientSession::QuicChromiumPathValidationContext::ReleaseReader() {
  return std::move(reader_);
}

QuicChromiumClientSession::ConnectionMigrationValidationResultDelegate::
    ConnectionMigrationValidationResultDelegate(
        QuicChromiumClientSession* session)
    : session_(session) {}

void QuicChromiumClientSession::ConnectionMigrationValidationResultDelegate::
    OnPathValidationSuccess(
        std::unique_ptr<quic::QuicPathValidationContext> context) {
  auto* chrome_context =
      static_cast<QuicChromiumPathValidationContext*>(context.get());
  session_->OnConnectionMigrationProbeSucceeded(
      chrome_context->network(), chrome_context->peer_address(),
      chrome_context->self_address(), chrome_context->ReleaseSocket(),
      chrome_context->ReleaseWriter(), chrome_context->ReleaseReader());
}

void QuicChromiumClientSession::ConnectionMigrationValidationResultDelegate::
    OnPathValidationFailure(
        std::unique_ptr<quic::QuicPathValidationContext> context) {
  // Note that socket, packet writer, and packet reader in |context| will be
  // discarded.
  auto* chrome_context =
      static_cast<QuicChromiumPathValidationContext*>(context.get());
  session_->OnProbeFailed(chrome_context->network(),
                          chrome_context->peer_address());
}

QuicChromiumClientSession::PortMigrationValidationResultDelegate::
    PortMigrationValidationResultDelegate(QuicChromiumClientSession* session)
    : session_(session) {}

void QuicChromiumClientSession::PortMigrationValidationResultDelegate::
    OnPathValidationSuccess(
        std::unique_ptr<quic::QuicPathValidationContext> context) {
  auto* chrome_context =
      static_cast<QuicChromiumPathValidationContext*>(context.get());
  session_->OnPortMigrationProbeSucceeded(
      chrome_context->network(), chrome_context->peer_address(),
      chrome_context->self_address(), chrome_context->ReleaseSocket(),
      chrome_context->ReleaseWriter(), chrome_context->ReleaseReader());
}

void QuicChromiumClientSession::PortMigrationValidationResultDelegate::
    OnPathValidationFailure(
        std::unique_ptr<quic::QuicPathValidationContext> context) {
  // Note that socket, packet writer, and packet reader in |context| will be
  // discarded.
  auto* chrome_context =
      static_cast<QuicChromiumPathValidationContext*>(context.get());
  session_->OnProbeFailed(chrome_context->network(),
                          chrome_context->peer_address());
}

QuicChromiumClientSession::QuicChromiumPathValidationWriterDelegate::
    QuicChromiumPathValidationWriterDelegate(
        QuicChromiumClientSession* session,
        base::SequencedTaskRunner* task_runner)
    : session_(session),
      task_runner_(task_runner),
      network_(NetworkChangeNotifier::kInvalidNetworkHandle) {}

QuicChromiumClientSession::QuicChromiumPathValidationWriterDelegate::
    ~QuicChromiumPathValidationWriterDelegate() = default;

int QuicChromiumClientSession::QuicChromiumPathValidationWriterDelegate::
    HandleWriteError(
        int error_code,
        scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer> last_packet) {
  // Write error on the probing network is not recoverable.
  DVLOG(1) << "Probing packet encounters write error " << error_code;
  // Post a task to notify |session_| that this probe failed and cancel
  // undergoing probing, which will delete the packet writer.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &QuicChromiumPathValidationWriterDelegate::NotifySessionProbeFailed,
          weak_factory_.GetWeakPtr(), network_));
  return error_code;
}

void QuicChromiumClientSession::QuicChromiumPathValidationWriterDelegate::
    OnWriteError(int error_code) {
  NotifySessionProbeFailed(network_);
}
void QuicChromiumClientSession::QuicChromiumPathValidationWriterDelegate::
    OnWriteUnblocked() {}

void QuicChromiumClientSession::QuicChromiumPathValidationWriterDelegate::
    NotifySessionProbeFailed(NetworkChangeNotifier::NetworkHandle network) {
  session_->OnProbeFailed(network, peer_address_);
}

void QuicChromiumClientSession::QuicChromiumPathValidationWriterDelegate::
    set_peer_address(const quic::QuicSocketAddress& peer_address) {
  peer_address_ = peer_address;
}

void QuicChromiumClientSession::QuicChromiumPathValidationWriterDelegate::
    set_network(NetworkChangeNotifier::NetworkHandle network) {
  network_ = network;
}

QuicChromiumClientSession::QuicChromiumClientSession(
    quic::QuicConnection* connection,
    std::unique_ptr<DatagramClientSocket> socket,
    QuicStreamFactory* stream_factory,
    QuicCryptoClientStreamFactory* crypto_client_stream_factory,
    const quic::QuicClock* clock,
    TransportSecurityState* transport_security_state,
    SSLConfigService* ssl_config_service,
    std::unique_ptr<QuicServerInfo> server_info,
    const QuicSessionKey& session_key,
    bool require_confirmation,
    quic::QuicStreamId max_allowed_push_id,
    bool migrate_session_early_v2,
    bool migrate_sessions_on_network_change_v2,
    NetworkChangeNotifier::NetworkHandle default_network,
    quic::QuicTime::Delta retransmittable_on_wire_timeout,
    bool migrate_idle_session,
    bool allow_port_migration,
    base::TimeDelta idle_migration_period,
    base::TimeDelta max_time_on_non_default_network,
    int max_migrations_to_non_default_network_on_write_error,
    int max_migrations_to_non_default_network_on_path_degrading,
    int yield_after_packets,
    quic::QuicTime::Delta yield_after_duration,
    bool go_away_on_path_degrading,
    bool headers_include_h2_stream_dependency,
    int cert_verify_flags,
    const quic::QuicConfig& config,
    std::unique_ptr<QuicCryptoClientConfigHandle> crypto_config,
    const char* const connection_description,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    std::unique_ptr<quic::QuicClientPushPromiseIndex> push_promise_index,
    ServerPushDelegate* push_delegate,
    const base::TickClock* tick_clock,
    base::SequencedTaskRunner* task_runner,
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLog* net_log)
    : quic::QuicSpdyClientSessionBase(connection,
                                      push_promise_index.get(),
                                      config,
                                      connection->supported_versions()),
      session_key_(session_key),
      require_confirmation_(require_confirmation),
      migrate_session_early_v2_(migrate_session_early_v2),
      migrate_session_on_network_change_v2_(
          migrate_sessions_on_network_change_v2),
      migrate_idle_session_(migrate_idle_session),
      allow_port_migration_(allow_port_migration),
      idle_migration_period_(idle_migration_period),
      max_time_on_non_default_network_(max_time_on_non_default_network),
      max_migrations_to_non_default_network_on_write_error_(
          max_migrations_to_non_default_network_on_write_error),
      current_migrations_to_non_default_network_on_write_error_(0),
      max_migrations_to_non_default_network_on_path_degrading_(
          max_migrations_to_non_default_network_on_path_degrading),
      current_migrations_to_non_default_network_on_path_degrading_(0),
      clock_(clock),
      yield_after_packets_(yield_after_packets),
      yield_after_duration_(yield_after_duration),
      go_away_on_path_degrading_(go_away_on_path_degrading),
      most_recent_path_degrading_timestamp_(base::TimeTicks()),
      most_recent_network_disconnected_timestamp_(base::TimeTicks()),
      tick_clock_(tick_clock),
      most_recent_stream_close_time_(tick_clock_->NowTicks()),
      most_recent_write_error_(0),
      most_recent_write_error_timestamp_(base::TimeTicks()),
      crypto_config_(std::move(crypto_config)),
      stream_factory_(stream_factory),
      transport_security_state_(transport_security_state),
      ssl_config_service_(ssl_config_service),
      server_info_(std::move(server_info)),
      pkp_bypassed_(false),
      is_fatal_cert_error_(false),
      num_total_streams_(0),
      task_runner_(task_runner),
      net_log_(NetLogWithSource::Make(net_log, NetLogSourceType::QUIC_SESSION)),
      logger_(new QuicConnectionLogger(this,
                                       connection_description,
                                       std::move(socket_performance_watcher),
                                       net_log_)),
      http3_logger_(VersionUsesHttp3(connection->transport_version())
                        ? new QuicHttp3Logger(net_log_)
                        : nullptr),
      going_away_(false),
      port_migration_detected_(false),
      push_delegate_(push_delegate),
      streams_pushed_count_(0),
      streams_pushed_and_claimed_count_(0),
      bytes_pushed_count_(0),
      bytes_pushed_and_unclaimed_count_(0),
      probing_manager_(this, task_runner_),
      retry_migrate_back_count_(0),
      current_migration_cause_(UNKNOWN_CAUSE),
      send_packet_after_migration_(false),
      wait_for_new_network_(false),
      ignore_read_error_(false),
      headers_include_h2_stream_dependency_(
          headers_include_h2_stream_dependency),
      max_allowed_push_id_(max_allowed_push_id),
      attempted_zero_rtt_(false),
      num_migrations_(0),
      last_key_update_reason_(quic::KeyUpdateReason::kInvalid),
      push_promise_index_(std::move(push_promise_index)),
      path_validation_writer_delegate_(this, task_runner_) {
  // Make sure connection migration and goaway on path degrading are not turned
  // on at the same time.
  DCHECK(!(migrate_session_early_v2_ && go_away_on_path_degrading_));
  DCHECK(!(allow_port_migration_ && go_away_on_path_degrading_));

  default_network_ = default_network;
  auto* socket_raw = socket.get();
  sockets_.push_back(std::move(socket));
  packet_readers_.push_back(std::make_unique<QuicChromiumPacketReader>(
      sockets_.back().get(), clock, this, yield_after_packets,
      yield_after_duration, net_log_));
  CHECK_EQ(packet_readers_.size(), sockets_.size());
  crypto_stream_.reset(
      crypto_client_stream_factory->CreateQuicCryptoClientStream(
          session_key.server_id(), this,
          std::make_unique<ProofVerifyContextChromium>(cert_verify_flags,
                                                       net_log_),
          crypto_config_->GetConfig()));
  if (VersionUsesHttp3(transport_version()))
    set_debug_visitor(http3_logger_.get());
  connection->set_debug_visitor(logger_.get());
  connection->set_creator_debug_delegate(logger_.get());
  migrate_back_to_default_timer_.SetTaskRunner(task_runner_);
  net_log_.BeginEvent(NetLogEventType::QUIC_SESSION, [&] {
    return NetLogQuicClientSessionParams(
        &session_key, connection_id(), connection->client_connection_id(),
        supported_versions(), cert_verify_flags, require_confirmation_);
  });
  IPEndPoint address;
  if (socket_raw && socket_raw->GetLocalAddress(&address) == OK &&
      address.GetFamily() == ADDRESS_FAMILY_IPV6) {
    connection->SetMaxPacketLength(connection->max_packet_length() -
                                   kAdditionalOverheadForIPv6);
  }
  connect_timing_.dns_start = dns_resolution_start_time;
  connect_timing_.dns_end = dns_resolution_end_time;
  if (!retransmittable_on_wire_timeout.IsZero()) {
    connection->set_initial_retransmittable_on_wire_timeout(
        retransmittable_on_wire_timeout);
  }
}

QuicChromiumClientSession::~QuicChromiumClientSession() {
  // This is referenced by the parent class's destructor, so have to delete it
  // asynchronously, unfortunately. Don't use DeleteSoon, since that leaks if
  // the task is not run, which is often the case in tests.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce([](std::unique_ptr<quic::QuicClientPushPromiseIndex>
                            push_promise_index) {},
                     std::move(push_promise_index_)));

  DCHECK(callback_.is_null());

  for (auto& observer : connectivity_observer_list_)
    observer.OnSessionRemoved(this);

  net_log_.EndEvent(NetLogEventType::QUIC_SESSION);
  DCHECK(waiting_for_confirmation_callbacks_.empty());
  DCHECK(!HasActiveRequestStreams());
  DCHECK(handles_.empty());
  if (!stream_requests_.empty()) {
    // The session must be closed before it is destroyed.
    CancelAllRequests(ERR_UNEXPECTED);
  }
  connection()->set_debug_visitor(nullptr);

  if (connection()->connected()) {
    // Ensure that the connection is closed by the time the session is
    // destroyed.
    connection()->CloseConnection(quic::QUIC_PEER_GOING_AWAY,
                                  "session torn down",
                                  quic::ConnectionCloseBehavior::SILENT_CLOSE);
  }

  if (IsEncryptionEstablished())
    RecordHandshakeState(STATE_ENCRYPTION_ESTABLISHED);
  if (OneRttKeysAvailable())
    RecordHandshakeState(STATE_HANDSHAKE_CONFIRMED);
  else
    RecordHandshakeState(STATE_FAILED);

  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.NumTotalStreams",
                          num_total_streams_);
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicNumSentClientHellos",
                          crypto_stream_->num_sent_client_hellos());
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.Pushed", streams_pushed_count_);
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.PushedAndClaimed",
                          streams_pushed_and_claimed_count_);
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.PushedBytes", bytes_pushed_count_);
  DCHECK_LE(bytes_pushed_and_unclaimed_count_, bytes_pushed_count_);
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.PushedAndUnclaimedBytes",
                          bytes_pushed_and_unclaimed_count_);

  if (!OneRttKeysAvailable())
    return;

  // Sending one client_hello means we had zero handshake-round-trips.
  int round_trip_handshakes = crypto_stream_->num_sent_client_hellos() - 1;

  SSLInfo ssl_info;
  // QUIC supports only secure urls.
  if (GetSSLInfo(&ssl_info) && ssl_info.cert.get()) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.ConnectRandomPortForHTTPS",
                                round_trip_handshakes, 1, 3, 4);
    if (require_confirmation_) {
      UMA_HISTOGRAM_CUSTOM_COUNTS(
          "Net.QuicSession.ConnectRandomPortRequiringConfirmationForHTTPS",
          round_trip_handshakes, 1, 3, 4);
    }
  }

  const quic::QuicConnectionStats stats = connection()->GetStats();

  // The MTU used by QUIC is limited to a fairly small set of predefined values
  // (initial values and MTU discovery values), but does not fare well when
  // bucketed.  Because of that, a sparse histogram is used here.
  base::UmaHistogramSparse("Net.QuicSession.ClientSideMtu",
                           connection()->max_packet_length());
  base::UmaHistogramSparse("Net.QuicSession.ServerSideMtu",
                           stats.max_received_packet_size);

  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.MtuProbesSent",
                          connection()->mtu_probe_count());

  if (stats.packets_sent >= 100) {
    // Used to monitor for regressions that effect large uploads.
    UMA_HISTOGRAM_COUNTS_1000(
        "Net.QuicSession.PacketRetransmitsPerMille",
        1000 * stats.packets_retransmitted / stats.packets_sent);
  }

  if (stats.max_sequence_reordering == 0)
    return;
  const base::HistogramBase::Sample kMaxReordering = 100;
  base::HistogramBase::Sample reordering = kMaxReordering;
  if (stats.min_rtt_us > 0) {
    reordering = static_cast<base::HistogramBase::Sample>(
        100 * stats.max_time_reordering_us / stats.min_rtt_us);
  }
  UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.MaxReorderingTime", reordering,
                              1, kMaxReordering, 50);
  if (stats.min_rtt_us > 100 * 1000) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.MaxReorderingTimeLongRtt",
                                reordering, 1, kMaxReordering, 50);
  }
  UMA_HISTOGRAM_COUNTS_1M(
      "Net.QuicSession.MaxReordering",
      static_cast<base::HistogramBase::Sample>(stats.max_sequence_reordering));
}

void QuicChromiumClientSession::Initialize() {
  if (VersionUsesHttp3(transport_version())) {
    quic::QuicSpdyClientSessionBase::SetMaxPushId(max_allowed_push_id_);
  }
  set_max_inbound_header_list_size(kQuicMaxHeaderListSize);
  if (config()->HasClientRequestedIndependentOption(
          quic::kQLVE, quic::Perspective::IS_CLIENT)) {
    connection()->EnableLegacyVersionEncapsulation(session_key_.host());
  }
  quic::QuicSpdyClientSessionBase::Initialize();
  SetHpackEncoderDebugVisitor(std::make_unique<HpackEncoderDebugVisitor>());
  SetHpackDecoderDebugVisitor(std::make_unique<HpackDecoderDebugVisitor>());
}

size_t QuicChromiumClientSession::WriteHeadersOnHeadersStream(
    quic::QuicStreamId id,
    spdy::Http2HeaderBlock headers,
    bool fin,
    const spdy::SpdyStreamPrecedence& precedence,
    quic::QuicReferenceCountedPointer<quic::QuicAckListenerInterface>
        ack_listener) {
  spdy::SpdyStreamId parent_stream_id = 0;
  int weight = 0;
  bool exclusive = false;

  if (headers_include_h2_stream_dependency_) {
    priority_dependency_state_.OnStreamCreation(id, precedence.spdy3_priority(),
                                                &parent_stream_id, &weight,
                                                &exclusive);
  } else {
    weight = spdy::Spdy3PriorityToHttp2Weight(precedence.spdy3_priority());
  }

  return WriteHeadersOnHeadersStreamImpl(id, std::move(headers), fin,
                                         parent_stream_id, weight, exclusive,
                                         std::move(ack_listener));
}

void QuicChromiumClientSession::UnregisterStreamPriority(quic::QuicStreamId id,
                                                         bool is_static) {
  if (headers_include_h2_stream_dependency_ && !is_static) {
    priority_dependency_state_.OnStreamDestruction(id);
  }
  quic::QuicSpdySession::UnregisterStreamPriority(id, is_static);
}

void QuicChromiumClientSession::UpdateStreamPriority(
    quic::QuicStreamId id,
    const spdy::SpdyStreamPrecedence& new_precedence) {
  if (headers_include_h2_stream_dependency_ ||
      VersionUsesHttp3(connection()->transport_version())) {
    auto updates = priority_dependency_state_.OnStreamUpdate(
        id, new_precedence.spdy3_priority());
    for (auto update : updates) {
      if (!VersionUsesHttp3(connection()->transport_version())) {
        WritePriority(update.id, update.parent_stream_id, update.weight,
                      update.exclusive);
      }
    }
  }
  quic::QuicSpdySession::UpdateStreamPriority(id, new_precedence);
}

void QuicChromiumClientSession::OnHttp3GoAway(uint64_t id) {
  quic::QuicSpdySession::OnHttp3GoAway(id);
  NotifyFactoryOfSessionGoingAway();

  PerformActionOnActiveStreams([id](quic::QuicStream* stream) {
    if (stream->id() >= id) {
      static_cast<QuicChromiumClientStream*>(stream)->OnError(
          ERR_QUIC_GOAWAY_REQUEST_CAN_BE_RETRIED);
    }
    return true;
  });
}

void QuicChromiumClientSession::AddHandle(Handle* handle) {
  if (going_away_) {
    handle->OnSessionClosed(connection()->version(), ERR_UNEXPECTED, error(),
                            port_migration_detected_, GetConnectTiming(),
                            WasConnectionEverUsed());
    return;
  }

  DCHECK(!base::Contains(handles_, handle));
  handles_.insert(handle);
}

void QuicChromiumClientSession::RemoveHandle(Handle* handle) {
  DCHECK(base::Contains(handles_, handle));
  handles_.erase(handle);
}

void QuicChromiumClientSession::AddConnectivityObserver(
    ConnectivityObserver* observer) {
  connectivity_observer_list_.AddObserver(observer);
  observer->OnSessionRegistered(this, GetCurrentNetwork());
}

void QuicChromiumClientSession::RemoveConnectivityObserver(
    ConnectivityObserver* observer) {
  connectivity_observer_list_.RemoveObserver(observer);
}

// TODO(zhongyi): replace migration_session_* booleans with
// ConnectionMigrationMode.
ConnectionMigrationMode QuicChromiumClientSession::connection_migration_mode()
    const {
  if (migrate_session_early_v2_)
    return ConnectionMigrationMode::FULL_MIGRATION_V2;

  if (migrate_session_on_network_change_v2_)
    return ConnectionMigrationMode::NO_MIGRATION_ON_PATH_DEGRADING_V2;

  return ConnectionMigrationMode::NO_MIGRATION;
}

int QuicChromiumClientSession::WaitForHandshakeConfirmation(
    CompletionOnceCallback callback) {
  if (!connection()->connected())
    return ERR_CONNECTION_CLOSED;

  if (OneRttKeysAvailable())
    return OK;

  waiting_for_confirmation_callbacks_.push_back(std::move(callback));
  return ERR_IO_PENDING;
}

int QuicChromiumClientSession::TryCreateStream(StreamRequest* request) {
  if (goaway_received()) {
    DVLOG(1) << "Going away.";
    return ERR_CONNECTION_CLOSED;
  }

  if (!connection()->connected()) {
    DVLOG(1) << "Already closed.";
    return ERR_CONNECTION_CLOSED;
  }

  if (going_away_) {
    return ERR_CONNECTION_CLOSED;
  }

  bool can_open_next = CanOpenNextOutgoingBidirectionalStream();
  if (can_open_next) {
    request->stream_ =
        CreateOutgoingReliableStreamImpl(request->traffic_annotation())
            ->CreateHandle();
    return OK;
  }

  request->pending_start_time_ = tick_clock_->NowTicks();
  stream_requests_.push_back(request);
  UMA_HISTOGRAM_COUNTS_1000("Net.QuicSession.NumPendingStreamRequests",
                            stream_requests_.size());
  return ERR_IO_PENDING;
}

void QuicChromiumClientSession::CancelRequest(StreamRequest* request) {
  // Remove |request| from the queue while preserving the order of the
  // other elements.
  auto it =
      std::find(stream_requests_.begin(), stream_requests_.end(), request);
  if (it != stream_requests_.end()) {
    it = stream_requests_.erase(it);
  }
}

bool QuicChromiumClientSession::ShouldCreateOutgoingBidirectionalStream() {
  if (!crypto_stream_->encryption_established()) {
    DVLOG(1) << "Encryption not active so no outgoing stream created.";
    return false;
  }
  if (!CanOpenNextOutgoingBidirectionalStream()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already " << GetNumActiveStreams() << " open.";
    return false;
  }
  if (goaway_received()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already received goaway.";
    return false;
  }
  if (going_away_) {
    return false;
  }
  return true;
}

bool QuicChromiumClientSession::ShouldCreateOutgoingUnidirectionalStream() {
  NOTREACHED() << "Try to create outgoing unidirectional streams";
  return false;
}

bool QuicChromiumClientSession::WasConnectionEverUsed() {
  const quic::QuicConnectionStats& stats = connection()->GetStats();
  return stats.bytes_sent > 0 || stats.bytes_received > 0;
}

QuicChromiumClientStream*
QuicChromiumClientSession::CreateOutgoingBidirectionalStream() {
  NOTREACHED() << "CreateOutgoingReliableStreamImpl should be called directly";
  return nullptr;
}

QuicChromiumClientStream*
QuicChromiumClientSession::CreateOutgoingUnidirectionalStream() {
  NOTREACHED() << "Try to create outgoing unidirectional stream";
  return nullptr;
}

QuicChromiumClientStream*
QuicChromiumClientSession::CreateOutgoingReliableStreamImpl(
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(connection()->connected());
  QuicChromiumClientStream* stream = new QuicChromiumClientStream(
      GetNextOutgoingBidirectionalStreamId(), this, quic::BIDIRECTIONAL,
      net_log_, traffic_annotation);
  ActivateStream(base::WrapUnique(stream));
  ++num_total_streams_;
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.NumOpenStreams",
                          GetNumActiveStreams());
  // The previous histogram puts 100 in a bucket betweeen 86-113 which does
  // not shed light on if chrome ever things it has more than 100 streams open.
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.TooManyOpenStreams",
                        GetNumActiveStreams() > 100);
  return stream;
}

quic::QuicCryptoClientStream*
QuicChromiumClientSession::GetMutableCryptoStream() {
  return crypto_stream_.get();
}

const quic::QuicCryptoClientStream* QuicChromiumClientSession::GetCryptoStream()
    const {
  return crypto_stream_.get();
}

bool QuicChromiumClientSession::GetRemoteEndpoint(IPEndPoint* endpoint) {
  *endpoint = ToIPEndPoint(peer_address());
  return true;
}

// TODO(rtenneti): Add unittests for GetSSLInfo which exercise the various ways
// we learn about SSL info (sync vs async vs cached).
bool QuicChromiumClientSession::GetSSLInfo(SSLInfo* ssl_info) const {
  ssl_info->Reset();
  if (!cert_verify_result_) {
    return false;
  }

  ssl_info->cert_status = cert_verify_result_->cert_status;
  ssl_info->cert = cert_verify_result_->verified_cert;

  ssl_info->public_key_hashes = cert_verify_result_->public_key_hashes;
  ssl_info->is_issued_by_known_root =
      cert_verify_result_->is_issued_by_known_root;
  ssl_info->pkp_bypassed = pkp_bypassed_;

  ssl_info->client_cert_sent = false;
  ssl_info->handshake_type = SSLInfo::HANDSHAKE_FULL;
  ssl_info->pinning_failure_log = pinning_failure_log_;
  ssl_info->is_fatal_cert_error = is_fatal_cert_error_;

  ssl_info->signed_certificate_timestamps = cert_verify_result_->scts;
  ssl_info->ct_policy_compliance = cert_verify_result_->policy_compliance;

  const auto& crypto_params = crypto_stream_->crypto_negotiated_params();
  uint16_t cipher_suite;
  if (connection()->version().UsesTls()) {
    cipher_suite = crypto_params.cipher_suite;
  } else {
    // Map QUIC AEADs to the corresponding TLS 1.3 cipher. OpenSSL's cipher
    // suite numbers begin with a stray 0x03, so mask them off.
    quic::QuicTag aead = crypto_params.aead;
    switch (aead) {
      case quic::kAESG:
        cipher_suite = TLS1_CK_AES_128_GCM_SHA256 & 0xffff;
        break;
      case quic::kCC20:
        cipher_suite = TLS1_CK_CHACHA20_POLY1305_SHA256 & 0xffff;
        break;
      default:
        NOTREACHED();
        return false;
    }
  }
  int ssl_connection_status = 0;
  SSLConnectionStatusSetCipherSuite(cipher_suite, &ssl_connection_status);
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_QUIC,
                                &ssl_connection_status);
  ssl_info->connection_status = ssl_connection_status;

  if (connection()->version().UsesTls()) {
    ssl_info->key_exchange_group = crypto_params.key_exchange_group;
    ssl_info->peer_signature_algorithm = crypto_params.peer_signature_algorithm;
    return true;
  }

  // Report the QUIC key exchange as the corresponding TLS curve.
  switch (crypto_stream_->crypto_negotiated_params().key_exchange) {
    case quic::kP256:
      ssl_info->key_exchange_group = SSL_CURVE_SECP256R1;
      break;
    case quic::kC255:
      ssl_info->key_exchange_group = SSL_CURVE_X25519;
      break;
    default:
      NOTREACHED();
      return false;
  }

  // QUIC-Crypto always uses RSA-PSS or ECDSA with SHA-256.
  size_t unused;
  X509Certificate::PublicKeyType key_type;
  X509Certificate::GetPublicKeyInfo(ssl_info->cert->cert_buffer(), &unused,
                                    &key_type);
  switch (key_type) {
    case X509Certificate::kPublicKeyTypeRSA:
      ssl_info->peer_signature_algorithm = SSL_SIGN_RSA_PSS_RSAE_SHA256;
      break;
    case X509Certificate::kPublicKeyTypeECDSA:
      ssl_info->peer_signature_algorithm = SSL_SIGN_ECDSA_SECP256R1_SHA256;
      break;
    default:
      NOTREACHED();
      return false;
  }

  return true;
}

int QuicChromiumClientSession::CryptoConnect(CompletionOnceCallback callback) {
  connect_timing_.connect_start = tick_clock_->NowTicks();
  RecordHandshakeState(STATE_STARTED);
  DCHECK(flow_controller());

  if (!crypto_stream_->CryptoConnect())
    return ERR_QUIC_HANDSHAKE_FAILED;

  if (OneRttKeysAvailable()) {
    connect_timing_.connect_end = tick_clock_->NowTicks();
    return OK;
  }

  // Unless we require handshake confirmation, activate the session if
  // we have established initial encryption.
  if (!require_confirmation_ && IsEncryptionEstablished())
    return OK;

  callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int QuicChromiumClientSession::GetNumSentClientHellos() const {
  return crypto_stream_->num_sent_client_hellos();
}

bool QuicChromiumClientSession::CanPool(
    const std::string& hostname,
    const QuicSessionKey& other_session_key) const {
  DCHECK(connection()->connected());
  if (!session_key_.CanUseForAliasing(other_session_key))
    return false;
  SSLInfo ssl_info;
  if (!GetSSLInfo(&ssl_info) || !ssl_info.cert.get()) {
    NOTREACHED() << "QUIC should always have certificates.";
    return false;
  }

  return SpdySession::CanPool(transport_security_state_, ssl_info,
                              *ssl_config_service_, session_key_.host(),
                              hostname, session_key_.network_isolation_key());
}

bool QuicChromiumClientSession::ShouldCreateIncomingStream(
    quic::QuicStreamId id) {
  if (!connection()->connected()) {
    LOG(DFATAL) << "ShouldCreateIncomingStream called when disconnected";
    return false;
  }
  if (goaway_received()) {
    DVLOG(1) << "Cannot create a new outgoing stream. "
             << "Already received goaway.";
    return false;
  }
  if (going_away_) {
    return false;
  }
  if (quic::QuicUtils::IsClientInitiatedStreamId(
          connection()->transport_version(), id) ||
      (connection()->version().HasIetfQuicFrames() &&
       quic::QuicUtils::IsBidirectionalStreamId(id, connection()->version()))) {
    LOG(WARNING) << "Received invalid push stream id " << id;
    connection()->CloseConnection(
        quic::QUIC_INVALID_STREAM_ID,
        "Server created non write unidirectional stream",
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return false;
  }
  return true;
}

QuicChromiumClientStream* QuicChromiumClientSession::CreateIncomingStream(
    quic::QuicStreamId id) {
  if (!ShouldCreateIncomingStream(id)) {
    return nullptr;
  }
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("quic_chromium_incoming_session", R"(
      semantics {
        sender: "Quic Chromium Client Session"
        description:
          "When a web server needs to push a response to a client, an incoming "
          "stream is created to reply the client with pushed message instead "
          "of a message from the network."
        trigger:
          "A request by a server to push a response to the client."
        data: "None."
        destination: OTHER
        destination_other:
          "This stream is not used for sending data."
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled in settings."
        policy_exception_justification:
          "Essential for network access."
      }
  )");
  return CreateIncomingReliableStreamImpl(id, traffic_annotation);
}

QuicChromiumClientStream* QuicChromiumClientSession::CreateIncomingStream(
    quic::PendingStream* pending) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "quic_chromium_incoming_pending_session", R"(
      semantics {
        sender: "Quic Chromium Client Session Pending Stream"
        description:
          "When a web server needs to push a response to a client, an incoming "
          "stream is created to reply to the client with pushed message instead "
          "of a message from the network."
        trigger:
          "A request by a server to push a response to the client."
        data: "This stream is only used to receive data from the server."
        destination: OTHER
        destination_other:
          "The web server pushing the response."
      }
      policy {
        cookies_allowed: NO
        setting: "This feature cannot be disabled in settings."
        policy_exception_justification:
          "Essential for network access."
      }
  )");
  return CreateIncomingReliableStreamImpl(pending, traffic_annotation);
}

QuicChromiumClientStream*
QuicChromiumClientSession::CreateIncomingReliableStreamImpl(
    quic::QuicStreamId id,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(connection()->connected());

  QuicChromiumClientStream* stream = new QuicChromiumClientStream(
      id, this, quic::READ_UNIDIRECTIONAL, net_log_, traffic_annotation);
  ActivateStream(base::WrapUnique(stream));
  ++num_total_streams_;
  return stream;
}

QuicChromiumClientStream*
QuicChromiumClientSession::CreateIncomingReliableStreamImpl(
    quic::PendingStream* pending,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(connection()->connected());

  QuicChromiumClientStream* stream = new QuicChromiumClientStream(
      pending, this, quic::READ_UNIDIRECTIONAL, net_log_, traffic_annotation);
  ActivateStream(base::WrapUnique(stream));
  ++num_total_streams_;
  return stream;
}

void QuicChromiumClientSession::OnStreamClosed(quic::QuicStreamId stream_id) {
  most_recent_stream_close_time_ = tick_clock_->NowTicks();
  quic::QuicStream* stream = GetActiveStream(stream_id);
  if (stream != nullptr) {
    logger_->UpdateReceivedFrameCounts(stream_id, stream->num_frames_received(),
                                       stream->num_duplicate_frames_received());
    if (quic::QuicUtils::IsServerInitiatedStreamId(
            connection()->transport_version(), stream_id)) {
      bytes_pushed_count_ += stream->stream_bytes_read();
    }
  }
  quic::QuicSpdyClientSessionBase::OnStreamClosed(stream_id);
}

void QuicChromiumClientSession::OnCanCreateNewOutgoingStream(
    bool unidirectional) {
  if (CanOpenNextOutgoingBidirectionalStream() && !stream_requests_.empty() &&
      crypto_stream_->encryption_established() && !goaway_received() &&
      !going_away_ && connection()->connected()) {
    StreamRequest* request = stream_requests_.front();
    // TODO(ckrasic) - analyze data and then add logic to mark QUIC
    // broken if wait times are excessive.
    UMA_HISTOGRAM_TIMES("Net.QuicSession.PendingStreamsWaitTime",
                        tick_clock_->NowTicks() - request->pending_start_time_);
    stream_requests_.pop_front();
    request->OnRequestCompleteSuccess(
        CreateOutgoingReliableStreamImpl(request->traffic_annotation())
            ->CreateHandle());
  }
}

void QuicChromiumClientSession::OnConfigNegotiated() {
  quic::QuicSpdyClientSessionBase::OnConfigNegotiated();
  if (!stream_factory_ || !stream_factory_->allow_server_migration() ||
      (!config()->HasReceivedIPv6AlternateServerAddress() &&
       !config()->HasReceivedIPv4AlternateServerAddress())) {
    return;
  }

  // Server has sent an alternate address to connect to.
  IPEndPoint old_address;
  GetDefaultSocket()->GetPeerAddress(&old_address);

  // Migrate only if address families match.
  IPEndPoint new_address;
  if (old_address.GetFamily() == ADDRESS_FAMILY_IPV6) {
    if (!config()->HasReceivedIPv6AlternateServerAddress()) {
      return;
    }
      new_address =
          ToIPEndPoint(config()->ReceivedIPv6AlternateServerAddress());
  } else if (old_address.GetFamily() == ADDRESS_FAMILY_IPV4) {
    if (!config()->HasReceivedIPv4AlternateServerAddress()) {
      return;
    }
    new_address = ToIPEndPoint(config()->ReceivedIPv4AlternateServerAddress());
  }
  DCHECK_EQ(new_address.GetFamily(), old_address.GetFamily());

  // Specifying kInvalidNetworkHandle for the |network| parameter
  // causes the session to use the default network for the new socket.
  Migrate(NetworkChangeNotifier::kInvalidNetworkHandle, new_address,
          /*close_session_on_error=*/true);
}

void QuicChromiumClientSession::SetDefaultEncryptionLevel(
    quic::EncryptionLevel level) {
  if (!callback_.is_null() &&
      (!require_confirmation_ || level == quic::ENCRYPTION_FORWARD_SECURE ||
       level == quic::ENCRYPTION_ZERO_RTT)) {
    // Currently for all CryptoHandshakeEvent events, callback_
    // could be called because there are no error events in CryptoHandshakeEvent
    // enum. If error events are added to CryptoHandshakeEvent, then the
    // following code needs to changed.
    std::move(callback_).Run(OK);
  }
  if (level == quic::ENCRYPTION_FORWARD_SECURE) {
    OnCryptoHandshakeComplete();
    LogZeroRttStats();
  }
  if (level == quic::ENCRYPTION_ZERO_RTT)
    attempted_zero_rtt_ = true;
  quic::QuicSpdySession::SetDefaultEncryptionLevel(level);
}

void QuicChromiumClientSession::OnTlsHandshakeComplete() {
  if (!callback_.is_null()) {
    // Currently for all CryptoHandshakeEvent events, callback_
    // could be called because there are no error events in CryptoHandshakeEvent
    // enum. If error events are added to CryptoHandshakeEvent, then the
    // following code needs to changed.
    std::move(callback_).Run(OK);
  }

  OnCryptoHandshakeComplete();
  LogZeroRttStats();
  quic::QuicSpdySession::OnTlsHandshakeComplete();
}

void QuicChromiumClientSession::OnNewEncryptionKeyAvailable(
    quic::EncryptionLevel level,
    std::unique_ptr<quic::QuicEncrypter> encrypter) {
  if (!attempted_zero_rtt_ && (level == quic::ENCRYPTION_ZERO_RTT ||
                               level == quic::ENCRYPTION_FORWARD_SECURE)) {
    base::TimeTicks now = tick_clock_->NowTicks();
    DCHECK_LE(connect_timing_.connect_start, now);
    UMA_HISTOGRAM_TIMES("Net.QuicSession.EncryptionEstablishedTime",
                        now - connect_timing_.connect_start);
  }
  if (level == quic::ENCRYPTION_ZERO_RTT)
    attempted_zero_rtt_ = true;
  QuicSpdySession::OnNewEncryptionKeyAvailable(level, std::move(encrypter));

  if (!callback_.is_null() &&
      (!require_confirmation_ && level == quic::ENCRYPTION_ZERO_RTT)) {
    // Currently for all CryptoHandshakeEvent events, callback_
    // could be called because there are no error events in CryptoHandshakeEvent
    // enum. If error events are added to CryptoHandshakeEvent, then the
    // following code needs to changed.
    std::move(callback_).Run(OK);
  }
}

void QuicChromiumClientSession::LogZeroRttStats() {
  DCHECK(OneRttKeysAvailable());

  ZeroRttState state;

  ssl_early_data_reason_t early_data_reason = crypto_stream_->EarlyDataReason();
  if (early_data_reason == ssl_early_data_accepted) {
    state = ZeroRttState::kAttemptedAndSucceeded;
  } else if (early_data_reason == ssl_early_data_peer_declined ||
             early_data_reason == ssl_early_data_session_not_resumed ||
             early_data_reason == ssl_early_data_hello_retry_request) {
    state = ZeroRttState::kAttemptedAndRejected;
  } else {
    state = ZeroRttState::kNotAttempted;
  }
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.ZeroRttState", state);
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.ZeroRttReason", early_data_reason,
                            ssl_early_data_reason_max_value + 1);
  if (IsGoogleHost(session_key_.host())) {
    UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.ZeroRttReasonGoogle",
                              early_data_reason,
                              ssl_early_data_reason_max_value + 1);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.ZeroRttReasonNonGoogle",
                              early_data_reason,
                              ssl_early_data_reason_max_value + 1);
  }
}

void QuicChromiumClientSession::OnCryptoHandshakeMessageSent(
    const quic::CryptoHandshakeMessage& message) {
  logger_->OnCryptoHandshakeMessageSent(message);
}

void QuicChromiumClientSession::OnCryptoHandshakeMessageReceived(
    const quic::CryptoHandshakeMessage& message) {
  logger_->OnCryptoHandshakeMessageReceived(message);
  if (message.tag() == quic::kREJ) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.RejectLength",
                                message.GetSerialized().length(), 1000, 10000,
                                50);
    absl::string_view proof;
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.RejectHasProof",
                          message.GetStringPiece(quic::kPROF, &proof));
  }
}

void QuicChromiumClientSession::OnGoAway(const quic::QuicGoAwayFrame& frame) {
  quic::QuicSession::OnGoAway(frame);
  NotifyFactoryOfSessionGoingAway();
  port_migration_detected_ =
      frame.error_code == quic::QUIC_ERROR_MIGRATING_PORT;
}

void QuicChromiumClientSession::OnConnectionClosed(
    const quic::QuicConnectionCloseFrame& frame,
    quic::ConnectionCloseSource source) {
  DCHECK(!connection()->connected());

  logger_->OnConnectionClosed(frame, source);

  RecordConnectionCloseErrorCode(frame, source, session_key_.host(),
                                 OneRttKeysAvailable());
  if (OneRttKeysAvailable()) {
    NetworkChangeNotifier::NetworkHandle current_network = GetCurrentNetwork();
    for (auto& observer : connectivity_observer_list_)
      observer.OnSessionClosedAfterHandshake(this, current_network, source,
                                             frame.quic_error_code);
  }

  const quic::QuicErrorCode error = frame.quic_error_code;
  const std::string& error_details = frame.error_details;

  if (source == quic::ConnectionCloseSource::FROM_SELF &&
      error == quic::QUIC_NETWORK_IDLE_TIMEOUT && ShouldKeepConnectionAlive()) {
    quic::QuicStreamCount streams_waiting_to_write = 0;
    PerformActionOnActiveStreams(
        [&streams_waiting_to_write](quic::QuicStream* stream) {
          if (stream->HasBufferedData())
            ++streams_waiting_to_write;
          return true;
        });

    UMA_HISTOGRAM_COUNTS_100(
        "Net.QuicSession.NumStreamsWaitingToWriteOnIdleTimeout",
        streams_waiting_to_write);
    UMA_HISTOGRAM_COUNTS_100("Net.QuicSession.NumActiveStreamsOnIdleTimeout",
                             GetNumActiveStreams());
  }

  if (source == quic::ConnectionCloseSource::FROM_PEER) {
    if (error == quic::QUIC_PUBLIC_RESET) {
      // is_from_google_server will be true if the received EPID is
      // kEPIDGoogleFrontEnd or kEPIDGoogleFrontEnd0.
      const bool is_from_google_server =
          error_details.find(base::StringPrintf(
              "From %s", quic::kEPIDGoogleFrontEnd)) != std::string::npos;

      if (OneRttKeysAvailable()) {
        UMA_HISTOGRAM_BOOLEAN(
            "Net.QuicSession.ClosedByPublicReset.HandshakeConfirmed",
            is_from_google_server);
      } else {
        UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ClosedByPublicReset",
                              is_from_google_server);
      }

      if (is_from_google_server) {
        UMA_HISTOGRAM_COUNTS_100(
            "Net.QuicSession.NumMigrationsExercisedBeforePublicReset",
            sockets_.size() - 1);
      }

      base::UmaHistogramSparse(
          "Net.QuicSession.LastSentPacketContentBeforePublicReset",
          connection()
              ->sent_packet_manager()
              .unacked_packets()
              .GetLastPacketContent());

      const quic::QuicTime last_in_flight_packet_sent_time =
          connection()
              ->sent_packet_manager()
              .unacked_packets()
              .GetLastInFlightPacketSentTime();
      const quic::QuicTime handshake_completion_time =
          connection()->GetStats().handshake_completion_time;
      if (last_in_flight_packet_sent_time.IsInitialized() &&
          handshake_completion_time.IsInitialized() &&
          last_in_flight_packet_sent_time >= handshake_completion_time) {
        const quic::QuicTime::Delta delay =
            last_in_flight_packet_sent_time - handshake_completion_time;
        UMA_HISTOGRAM_LONG_TIMES_100(
            "Net.QuicSession."
            "LastInFlightPacketSentTimeFromHandshakeCompletionWithPublicReset",
            base::TimeDelta::FromMilliseconds(delay.ToMilliseconds()));
      }

      UMA_HISTOGRAM_LONG_TIMES_100(
          "Net.QuicSession.ConnectionDurationWithPublicReset",
          tick_clock_->NowTicks() - connect_timing_.connect_end);
    }
    if (OneRttKeysAvailable()) {
      base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
          "Net.QuicSession.StreamCloseErrorCodeServer.HandshakeConfirmed",
          base::HistogramBase::kUmaTargetedHistogramFlag);
      size_t num_streams = GetNumActiveStreams();
      if (num_streams > 0)
        histogram->AddCount(error, num_streams);
    }
  } else {
    if (OneRttKeysAvailable()) {
      base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
          "Net.QuicSession.StreamCloseErrorCodeClient.HandshakeConfirmed",
          base::HistogramBase::kUmaTargetedHistogramFlag);
      size_t num_streams = GetNumActiveStreams();
      if (num_streams > 0)
        histogram->AddCount(error, num_streams);
    } else {
      if (error == quic::QUIC_HANDSHAKE_TIMEOUT) {
        UMA_HISTOGRAM_BOOLEAN(
            "Net.QuicSession.HandshakeTimeout.PathDegradingDetected",
            connection()->IsPathDegrading());
      }
    }
    if (error == quic::QUIC_TOO_MANY_RTOS) {
      UMA_HISTOGRAM_COUNTS_1000(
          "Net.QuicSession.ClosedByRtoAtClient.ReceivedPacketCount",
          connection()->GetStats().packets_received);
      UMA_HISTOGRAM_COUNTS_1000(
          "Net.QuicSession.ClosedByRtoAtClient.SentPacketCount",
          connection()->GetStats().packets_sent);
      UMA_HISTOGRAM_COUNTS_100(
          "Net.QuicSession."
          "MaxConsecutiveRtoWithForwardProgressAndBlackholeDetected",
          connection()->GetStats().max_consecutive_rto_with_forward_progress);
    }
  }

  if (error == quic::QUIC_NETWORK_IDLE_TIMEOUT) {
    UMA_HISTOGRAM_COUNTS_1M(
        "Net.QuicSession.ConnectionClose.NumOpenStreams.TimedOut",
        GetNumActiveStreams());
    if (OneRttKeysAvailable()) {
      if (GetNumActiveStreams() > 0) {
        UMA_HISTOGRAM_BOOLEAN(
            "Net.QuicSession.TimedOutWithOpenStreams.HasUnackedPackets",
            connection()->sent_packet_manager().HasInFlightPackets());
        UMA_HISTOGRAM_COUNTS_1M(
            "Net.QuicSession.TimedOutWithOpenStreams.ConsecutiveRTOCount",
            connection()->sent_packet_manager().GetConsecutiveRtoCount());
        UMA_HISTOGRAM_COUNTS_1M(
            "Net.QuicSession.TimedOutWithOpenStreams.ConsecutiveTLPCount",
            connection()->sent_packet_manager().GetConsecutiveTlpCount());
        base::UmaHistogramSparse(
            "Net.QuicSession.TimedOutWithOpenStreams.LocalPort",
            connection()->self_address().port());
      }
    } else {
      UMA_HISTOGRAM_COUNTS_1M(
          "Net.QuicSession.ConnectionClose.NumOpenStreams.HandshakeTimedOut",
          GetNumActiveStreams());
      UMA_HISTOGRAM_COUNTS_1M(
          "Net.QuicSession.ConnectionClose.NumTotalStreams.HandshakeTimedOut",
          num_total_streams_);
    }
  }

  if (OneRttKeysAvailable()) {
    // QUIC connections should not timeout while there are open streams,
    // since PING frames are sent to prevent timeouts. If, however, the
    // connection timed out with open streams then QUIC traffic has become
    // blackholed. Alternatively, if too many retransmission timeouts occur
    // then QUIC traffic has become blackholed.
    if (stream_factory_ && (error == quic::QUIC_TOO_MANY_RTOS ||
                            (error == quic::QUIC_NETWORK_IDLE_TIMEOUT &&
                             GetNumActiveStreams() > 0))) {
      stream_factory_->OnBlackholeAfterHandshakeConfirmed(this);
    }
    UMA_HISTOGRAM_COUNTS_100(
        "Net.QuicSession.CryptoRetransmitCount.HandshakeConfirmed",
        connection()->GetStats().crypto_retransmit_count);
    UMA_HISTOGRAM_COUNTS_100(
        "Net.QuicSession.MaxConsecutiveRtoWithForwardProgress",
        connection()->GetStats().max_consecutive_rto_with_forward_progress);
    UMA_HISTOGRAM_COUNTS_1000("Net.QuicSession.NumPingsSent",
                              connection()->GetStats().ping_frames_sent);
    UMA_HISTOGRAM_LONG_TIMES_100(
        "Net.QuicSession.ConnectionDuration",
        tick_clock_->NowTicks() - connect_timing_.connect_end);
    UMA_HISTOGRAM_COUNTS_100("Net.QuicSession.NumMigrations", num_migrations_);
    // These values are persisted to logs. Entries should not be renumbered
    // and numeric values should never be reused.
    enum class KeyUpdateSupported {
      kInvalid = 0,
      kUnsupported = 1,
      kSupported = 2,
      kSupportedLocallyOnly = 3,
      kSupportedRemotelyOnly = 4,
      kMaxValue = kSupportedRemotelyOnly,
    };
    KeyUpdateSupported key_update_supported = KeyUpdateSupported::kInvalid;
    if (config()->KeyUpdateSupportedForConnection()) {
      key_update_supported = KeyUpdateSupported::kSupported;
    } else if (config()->KeyUpdateSupportedLocally()) {
      key_update_supported = KeyUpdateSupported::kSupportedLocallyOnly;
    } else if (config()->KeyUpdateSupportedRemotely()) {
      key_update_supported = KeyUpdateSupported::kSupportedRemotelyOnly;
    } else {
      key_update_supported = KeyUpdateSupported::kUnsupported;
    }
    base::UmaHistogramEnumeration("Net.QuicSession.KeyUpdate.Supported",
                                  key_update_supported);
    if (config()->KeyUpdateSupportedForConnection()) {
      base::UmaHistogramCounts100("Net.QuicSession.KeyUpdate.PerConnection2",
                                  connection()->GetStats().key_update_count);
      base::UmaHistogramCounts100(
          "Net.QuicSession.KeyUpdate.PotentialPeerKeyUpdateAttemptCount",
          connection()->PotentialPeerKeyUpdateAttemptCount());
      if (last_key_update_reason_ != quic::KeyUpdateReason::kInvalid) {
        std::string suffix =
            last_key_update_reason_ == quic::KeyUpdateReason::kRemote ? "Remote"
                                                                      : "Local";
        // These values are persisted to logs. Entries should not be renumbered
        // and numeric values should never be reused.
        enum class KeyUpdateSuccess {
          kInvalid = 0,
          kSuccess = 1,
          kFailedInitial = 2,
          kFailedNonInitial = 3,
          kMaxValue = kFailedNonInitial,
        };
        KeyUpdateSuccess value = KeyUpdateSuccess::kInvalid;
        if (connection()->HaveSentPacketsInCurrentKeyPhaseButNoneAcked()) {
          if (connection()->GetStats().key_update_count >= 2) {
            value = KeyUpdateSuccess::kFailedNonInitial;
          } else {
            value = KeyUpdateSuccess::kFailedInitial;
          }
        } else {
          value = KeyUpdateSuccess::kSuccess;
        }
        base::UmaHistogramEnumeration(
            "Net.QuicSession.KeyUpdate.Success." + suffix, value);
      }
    }
  } else {
    if (error == quic::QUIC_PUBLIC_RESET) {
      RecordHandshakeFailureReason(HANDSHAKE_FAILURE_PUBLIC_RESET);
    } else if (connection()->GetStats().packets_received == 0) {
      RecordHandshakeFailureReason(HANDSHAKE_FAILURE_BLACK_HOLE);
      base::UmaHistogramSparse(
          "Net.QuicSession.ConnectionClose.HandshakeFailureBlackHole.QuicError",
          error);
    } else {
      RecordHandshakeFailureReason(HANDSHAKE_FAILURE_UNKNOWN);
      base::UmaHistogramSparse(
          "Net.QuicSession.ConnectionClose.HandshakeFailureUnknown.QuicError",
          error);
    }
    UMA_HISTOGRAM_COUNTS_100(
        "Net.QuicSession.CryptoRetransmitCount.HandshakeNotConfirmed",
        connection()->GetStats().crypto_retransmit_count);
  }

  base::UmaHistogramCounts1M(
      "Net.QuicSession.UndecryptablePacketsReceivedWithDecrypter",
      connection()->GetStats().num_failed_authentication_packets_received);
  base::UmaHistogramSparse("Net.QuicSession.QuicVersion",
                           connection()->transport_version());
  NotifyFactoryOfSessionGoingAway();
  quic::QuicSession::OnConnectionClosed(frame, source);

  if (!callback_.is_null()) {
    std::move(callback_).Run(ERR_QUIC_PROTOCOL_ERROR);
  }

  CHECK_EQ(sockets_.size(), packet_readers_.size());
  for (auto& socket : sockets_) {
    socket->Close();
  }
  DCHECK(!HasActiveRequestStreams());
  CloseAllHandles(ERR_UNEXPECTED);
  CancelAllRequests(ERR_CONNECTION_CLOSED);
  NotifyRequestsOfConfirmation(ERR_CONNECTION_CLOSED);
  NotifyFactoryOfSessionClosedLater();
}

void QuicChromiumClientSession::OnSuccessfulVersionNegotiation(
    const quic::ParsedQuicVersion& version) {
  logger_->OnSuccessfulVersionNegotiation(version);
  quic::QuicSpdySession::OnSuccessfulVersionNegotiation(version);
}

void QuicChromiumClientSession::OnPacketReceived(
    const quic::QuicSocketAddress& self_address,
    const quic::QuicSocketAddress& peer_address,
    bool is_connectivity_probe) {
  // Notify the probing manager that a new packet is received.
  probing_manager_.OnPacketReceived(self_address, peer_address,
                                    is_connectivity_probe);
}

int QuicChromiumClientSession::HandleWriteError(
    int error_code,
    scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer> packet) {
  current_migration_cause_ = ON_WRITE_ERROR;
  LogHandshakeStatusOnMigrationSignal();

  base::UmaHistogramSparse("Net.QuicSession.WriteError", -error_code);
  if (OneRttKeysAvailable()) {
    base::UmaHistogramSparse("Net.QuicSession.WriteError.HandshakeConfirmed",
                             -error_code);
  }

  // For now, skip reporting if there are multiple packet writers and
  // connection migration is enabled.
  if (sockets_.size() == 1u || !migrate_session_early_v2_) {
    NetworkChangeNotifier::NetworkHandle current_network = GetCurrentNetwork();
    for (auto& observer : connectivity_observer_list_) {
      observer.OnSessionEncounteringWriteError(this, current_network,
                                               error_code);
    }
  }

  if (error_code == ERR_MSG_TOO_BIG || stream_factory_ == nullptr ||
      !migrate_session_on_network_change_v2_ || !OneRttKeysAvailable()) {
    return error_code;
  }

  NetworkChangeNotifier::NetworkHandle current_network = GetCurrentNetwork();

  net_log_.AddEventWithInt64Params(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_WRITE_ERROR, "network",
      current_network);

  DCHECK(packet != nullptr);
  DCHECK_NE(ERR_IO_PENDING, error_code);
  DCHECK_GT(0, error_code);
  DCHECK(packet_ == nullptr);

  // Post a task to migrate the session onto a new network.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&QuicChromiumClientSession::MigrateSessionOnWriteError,
                     weak_factory_.GetWeakPtr(), error_code,
                     connection()->writer()));

  // Store packet in the session since the actual migration and packet rewrite
  // can happen via this posted task or via an async network notification.
  packet_ = std::move(packet);
  ignore_read_error_ = true;

  // Cause the packet writer to return ERR_IO_PENDING and block so
  // that the actual migration happens from the message loop instead
  // of under the call stack of quic::QuicConnection::WritePacket.
  return ERR_IO_PENDING;
}

void QuicChromiumClientSession::MigrateSessionOnWriteError(
    int error_code,
    quic::QuicPacketWriter* writer) {
  DCHECK(migrate_session_on_network_change_v2_);
  // If |writer| is no longer actively in use, abort this migration attempt.
  if (writer != connection()->writer())
    return;

  most_recent_write_error_timestamp_ = tick_clock_->NowTicks();
  most_recent_write_error_ = error_code;

  if (stream_factory_ == nullptr) {
    // Close the connection if migration failed. Do not cause a
    // connection close packet to be sent since socket may be borked.
    connection()->CloseConnection(quic::QUIC_PACKET_WRITE_ERROR,
                                  "Write error with nulled stream factory",
                                  quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }

  current_migration_cause_ = ON_WRITE_ERROR;

  if (migrate_idle_session_ && CheckIdleTimeExceedsIdleMigrationPeriod())
    return;

  if (!migrate_idle_session_ && !HasActiveRequestStreams()) {
    // connection close packet to be sent since socket may be borked.
    connection()->CloseConnection(quic::QUIC_PACKET_WRITE_ERROR,
                                  "Write error for non-migratable session",
                                  quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }

  // Do not migrate if connection migration is disabled.
  if (config()->DisableConnectionMigration()) {
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_DISABLED_BY_CONFIG,
                                    connection_id(),
                                    "Migration disabled by config");
    // Close the connection since migration was disabled. Do not cause a
    // connection close packet to be sent since socket may be borked.
    connection()->CloseConnection(quic::QUIC_PACKET_WRITE_ERROR,
                                  "Write error for non-migratable session",
                                  quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }

  NetworkChangeNotifier::NetworkHandle new_network =
      stream_factory_->FindAlternateNetwork(GetCurrentNetwork());
  if (new_network == NetworkChangeNotifier::kInvalidNetworkHandle) {
    // No alternate network found.
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_NO_ALTERNATE_NETWORK,
                                    connection_id(),
                                    "No alternate network found");
    OnNoNewNetwork();
    return;
  }

  if (GetCurrentNetwork() == default_network_ &&
      current_migrations_to_non_default_network_on_write_error_ >=
          max_migrations_to_non_default_network_on_write_error_) {
    HistogramAndLogMigrationFailure(
        MIGRATION_STATUS_ON_WRITE_ERROR_DISABLED, connection_id(),
        "Exceeds maximum number of migrations on write error");
    connection()->CloseConnection(
        quic::QUIC_PACKET_WRITE_ERROR,
        "Too many migrations for write error for the same network",
        quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }
  current_migrations_to_non_default_network_on_write_error_++;

  net_log_.BeginEventWithStringParams(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_TRIGGERED, "trigger",
      "WriteError");
  MigrationResult result =
      Migrate(new_network, ToIPEndPoint(connection()->peer_address()),
              /*close_session_on_error=*/false);
  net_log_.EndEvent(NetLogEventType::QUIC_CONNECTION_MIGRATION_TRIGGERED);

  if (result == MigrationResult::FAILURE) {
    // Close the connection if migration failed. Do not cause a
    // connection close packet to be sent since socket may be borked.
    connection()->CloseConnection(quic::QUIC_PACKET_WRITE_ERROR,
                                  "Write and subsequent migration failed",
                                  quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }

  if (new_network != default_network_) {
    StartMigrateBackToDefaultNetworkTimer(
        base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs));
  } else {
    CancelMigrateBackToDefaultNetworkTimer();
  }
}

void QuicChromiumClientSession::OnNoNewNetwork() {
  DCHECK(OneRttKeysAvailable());
  wait_for_new_network_ = true;

  DVLOG(1) << "Force blocking the packet writer";
  // Force blocking the packet writer to avoid any writes since there is no
  // alternate network available.
  static_cast<QuicChromiumPacketWriter*>(connection()->writer())
      ->set_force_write_blocked(true);

  // Post a task to maybe close the session if the alarm fires.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&QuicChromiumClientSession::OnMigrationTimeout,
                     weak_factory_.GetWeakPtr(), sockets_.size()),
      base::TimeDelta::FromSeconds(kWaitTimeForNewNetworkSecs));
}

void QuicChromiumClientSession::WriteToNewSocket() {
  // Set |send_packet_after_migration_| to true so that a packet will be
  // sent when the writer becomes unblocked.
  send_packet_after_migration_ = true;

  DVLOG(1) << "Cancel force blocking the packet writer";
  // Notify writer that it is no longer forced blocked, which may call
  // OnWriteUnblocked() if the writer has no write in progress.
  static_cast<QuicChromiumPacketWriter*>(connection()->writer())
      ->set_force_write_blocked(false);
}

void QuicChromiumClientSession::OnMigrationTimeout(size_t num_sockets) {
  // If number of sockets has changed, this migration task is stale.
  if (num_sockets != sockets_.size())
    return;

  int net_error = current_migration_cause_ == ON_NETWORK_DISCONNECTED
                      ? ERR_INTERNET_DISCONNECTED
                      : ERR_NETWORK_CHANGED;

  // |current_migration_cause_| will be reset after logging.
  LogMigrationResultToHistogram(MIGRATION_STATUS_TIMEOUT);

  CloseSessionOnError(net_error, quic::QUIC_CONNECTION_MIGRATION_NO_NEW_NETWORK,
                      quic::ConnectionCloseBehavior::SILENT_CLOSE);
}

// TODO(renjietang): Deprecate this method once IETF QUIC supports connection
// migration.
void QuicChromiumClientSession::OnProbeSucceeded(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress& peer_address,
    const quic::QuicSocketAddress& self_address,
    std::unique_ptr<DatagramClientSocket> socket,
    std::unique_ptr<QuicChromiumPacketWriter> writer,
    std::unique_ptr<QuicChromiumPacketReader> reader) {
  if (current_migration_cause_ == CHANGE_PORT_ON_PATH_DEGRADING) {
    DCHECK(allow_port_migration_);
    OnPortMigrationProbeSucceeded(network, peer_address, self_address,
                                  std::move(socket), std::move(writer),
                                  std::move(reader));
    return;
  }
  OnConnectionMigrationProbeSucceeded(network, peer_address, self_address,
                                      std::move(socket), std::move(writer),
                                      std::move(reader));
}

void QuicChromiumClientSession::OnPortMigrationProbeSucceeded(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress& peer_address,
    const quic::QuicSocketAddress& self_address,
    std::unique_ptr<DatagramClientSocket> socket,
    std::unique_ptr<QuicChromiumPacketWriter> writer,
    std::unique_ptr<QuicChromiumPacketReader> reader) {
  DCHECK(socket);
  DCHECK(writer);
  DCHECK(reader);

  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CONNECTIVITY_PROBING_FINISHED,
                    [&] {
                      return NetLogProbingResultParams(network, &peer_address,
                                                       /*is_success=*/true);
                    });

  LogProbeResultToHistogram(current_migration_cause_, true);

  // Remove |this| as the old packet writer's delegate. Write error on old
  // writers will be ignored.
  // Set |this| to listen on socket write events on the packet writer
  // that was used for probing.
  static_cast<QuicChromiumPacketWriter*>(connection()->writer())
      ->set_delegate(nullptr);
  writer->set_delegate(this);
  connection()->SetSelfAddress(self_address);

  if (!migrate_idle_session_ && !HasActiveRequestStreams()) {
    // If idle sessions won't be migrated, close the connection.
    CloseSessionOnErrorLater(
        ERR_NETWORK_CHANGED,
        quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }

  if (migrate_idle_session_ && CheckIdleTimeExceedsIdleMigrationPeriod())
    return;

  // Migrate to the probed socket immediately: socket, writer and reader will
  // be acquired by connection and used as default on success.
  if (!MigrateToSocket(std::move(socket), std::move(reader),
                       std::move(writer))) {
    LogMigrateToSocketStatus(false);
    net_log_.AddEvent(
        NetLogEventType::QUIC_CONNECTION_MIGRATION_FAILURE_AFTER_PROBING);
    return;
  }

  LogMigrateToSocketStatus(true);

  // Notify the connection that migration succeeds after probing.
  connection()->OnSuccessfulMigration(/*is_port_change=*/true);
  num_migrations_++;
  HistogramAndLogMigrationSuccess(connection_id());
}

void QuicChromiumClientSession::OnConnectionMigrationProbeSucceeded(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress& peer_address,
    const quic::QuicSocketAddress& self_address,
    std::unique_ptr<DatagramClientSocket> socket,
    std::unique_ptr<QuicChromiumPacketWriter> writer,
    std::unique_ptr<QuicChromiumPacketReader> reader) {
  DCHECK(socket);
  DCHECK(writer);
  DCHECK(reader);

  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CONNECTIVITY_PROBING_FINISHED,
                    [&] {
                      return NetLogProbingResultParams(network, &peer_address,
                                                       /*is_success=*/true);
                    });
  if (network == NetworkChangeNotifier::kInvalidNetworkHandle)
    return;

  LogProbeResultToHistogram(current_migration_cause_, true);

  // Remove |this| as the old packet writer's delegate. Write error on old
  // writers will be ignored.
  // Set |this| to listen on socket write events on the packet writer
  // that was used for probing.
  static_cast<QuicChromiumPacketWriter*>(connection()->writer())
      ->set_delegate(nullptr);
  writer->set_delegate(this);
  connection()->SetSelfAddress(self_address);

  // Close streams that are not migratable to the probed |network|.
  ResetNonMigratableStreams();

  if (!migrate_idle_session_ && !HasActiveRequestStreams()) {
    // If idle sessions won't be migrated, close the connection.
    CloseSessionOnErrorLater(
        ERR_NETWORK_CHANGED,
        quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }

  if (migrate_idle_session_ && CheckIdleTimeExceedsIdleMigrationPeriod())
    return;

  // Migrate to the probed socket immediately: socket, writer and reader will
  // be acquired by connection and used as default on success.
  if (!MigrateToSocket(std::move(socket), std::move(reader),
                       std::move(writer))) {
    LogMigrateToSocketStatus(false);
    net_log_.AddEvent(
        NetLogEventType::QUIC_CONNECTION_MIGRATION_FAILURE_AFTER_PROBING);
    return;
  }

  LogMigrateToSocketStatus(true);

  // Notify the connection that migration succeeds after probing.
  // TODO(b/159074035) Change is_port_change to false here once the
  // QuicStreamFactoryTest migration tests have been fixed to account for the
  // retransmitted STREAM frames.
  connection()->OnSuccessfulMigration(/*is_port_change=*/true);

  net_log_.AddEventWithInt64Params(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_SUCCESS_AFTER_PROBING,
      "migrate_to_network", network);
  num_migrations_++;
  HistogramAndLogMigrationSuccess(connection_id());
  if (network == default_network_) {
    DVLOG(1) << "Client successfully migrated to default network: "
             << default_network_;
    CancelMigrateBackToDefaultNetworkTimer();
    return;
  }

  DVLOG(1) << "Client successfully got off default network after "
           << "successful probing network: " << network << ".";
  current_migrations_to_non_default_network_on_path_degrading_++;
  if (!migrate_back_to_default_timer_.IsRunning()) {
    current_migration_cause_ = ON_MIGRATE_BACK_TO_DEFAULT_NETWORK;
    // Session gets off the |default_network|, stay on |network| for now but
    // try to migrate back to default network after 1 second.
    StartMigrateBackToDefaultNetworkTimer(
        base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs));
  }
}

void QuicChromiumClientSession::OnProbeFailed(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress& peer_address) {
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CONNECTIVITY_PROBING_FINISHED,
                    [&] {
                      return NetLogProbingResultParams(network, &peer_address,
                                                       /*is_success=*/false);
                    });

  if (version().HasIetfQuicFrames() && connection()->use_path_validator()) {
    auto* context = static_cast<QuicChromiumPathValidationContext*>(
        connection()->GetPathValidationContext());

    if (!context)
      return;

    if (context->network() == network &&
        context->peer_address() == peer_address) {
      connection()->CancelPathValidation();
    }
  }

  LogProbeResultToHistogram(current_migration_cause_, false);

  if (network != NetworkChangeNotifier::kInvalidNetworkHandle) {
    // Probing failure can be ignored.
    DVLOG(1) << "Connectivity probing failed on <network: " << network
             << ", peer_address: " << peer_address.ToString() << ">.";
    DVLOG_IF(1, network == default_network_ &&
                    GetCurrentNetwork() != default_network_)
        << "Client probing failed on the default network, still using "
           "non-default network.";
  }
}

bool QuicChromiumClientSession::OnSendConnectivityProbingPacket(
    QuicChromiumPacketWriter* writer,
    const quic::QuicSocketAddress& peer_address) {
  return connection()->SendConnectivityProbingPacket(writer, peer_address);
}

void QuicChromiumClientSession::OnNetworkConnected(
    NetworkChangeNotifier::NetworkHandle network) {
  if (connection()->IsPathDegrading()) {
    base::TimeDelta duration =
        tick_clock_->NowTicks() - most_recent_path_degrading_timestamp_;
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.QuicNetworkDegradingDurationTillConnected",
                               duration, base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMinutes(10), 50);
  }
  if (!migrate_session_on_network_change_v2_)
    return;

  net_log_.AddEventWithInt64Params(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_NETWORK_CONNECTED,
      "connected_network", network);
  // If there was no migration waiting for new network and the path is not
  // degrading, ignore this signal.
  if (!wait_for_new_network_ && !connection()->IsPathDegrading())
    return;

  if (connection()->IsPathDegrading())
    current_migration_cause_ = NEW_NETWORK_CONNECTED_POST_PATH_DEGRADING;

  if (wait_for_new_network_) {
    wait_for_new_network_ = false;
    if (current_migration_cause_ == ON_WRITE_ERROR)
      current_migrations_to_non_default_network_on_write_error_++;
    // |wait_for_new_network_| is true, there was no working network previously.
    // |network| is now the only possible candidate, migrate immediately.
    MigrateNetworkImmediately(network);
  } else {
    // The connection is path degrading.
    DCHECK(connection()->IsPathDegrading());
    MaybeMigrateToAlternateNetworkOnPathDegrading();
  }
}

void QuicChromiumClientSession::OnNetworkDisconnectedV2(
    NetworkChangeNotifier::NetworkHandle disconnected_network) {
  LogMetricsOnNetworkDisconnected();
  if (!migrate_session_on_network_change_v2_)
    return;
  net_log_.AddEventWithInt64Params(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_NETWORK_DISCONNECTED,
      "disconnected_network", disconnected_network);

  // Stop probing the disconnected network if there is one.
  if (version().HasIetfQuicFrames() && connection()->use_path_validator()) {
    auto* context = static_cast<QuicChromiumPathValidationContext*>(
        connection()->GetPathValidationContext());
    if (context && context->network() == disconnected_network &&
        context->peer_address() == peer_address()) {
      connection()->CancelPathValidation();
    }
  } else {
    probing_manager_.CancelProbing(disconnected_network, peer_address());
  }
  if (disconnected_network == default_network_) {
    DVLOG(1) << "Default network: " << default_network_ << " is disconnected.";
    default_network_ = NetworkChangeNotifier::kInvalidNetworkHandle;
    current_migrations_to_non_default_network_on_write_error_ = 0;
  }

  // Ignore the signal if the current active network is not affected.
  if (GetCurrentNetwork() != disconnected_network) {
    DVLOG(1) << "Client's current default network is not affected by the "
             << "disconnected one.";
    return;
  }

  current_migration_cause_ = ON_NETWORK_DISCONNECTED;
  LogHandshakeStatusOnMigrationSignal();
  if (!OneRttKeysAvailable()) {
    // Close the connection if handshake is not confirmed. Migration before
    // handshake is not allowed.
    CloseSessionOnErrorLater(
        ERR_NETWORK_CHANGED,
        quic::QUIC_CONNECTION_MIGRATION_HANDSHAKE_UNCONFIRMED,
        quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }

  // Attempt to find alternative network.
  NetworkChangeNotifier::NetworkHandle new_network =
      stream_factory_->FindAlternateNetwork(disconnected_network);

  if (new_network == NetworkChangeNotifier::kInvalidNetworkHandle) {
    OnNoNewNetwork();
    return;
  }

  // Current network is being disconnected, migrate immediately to the
  // alternative network.
  MigrateNetworkImmediately(new_network);
}

void QuicChromiumClientSession::OnNetworkMadeDefault(
    NetworkChangeNotifier::NetworkHandle new_network) {
  LogMetricsOnNetworkMadeDefault();

  if (!migrate_session_on_network_change_v2_)
    return;

  DCHECK_NE(NetworkChangeNotifier::kInvalidNetworkHandle, new_network);
  net_log_.AddEventWithInt64Params(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_NETWORK_MADE_DEFAULT,
      "new_default_network", new_network);
  default_network_ = new_network;

  DVLOG(1) << "Network: " << new_network
           << " becomes default, old default: " << default_network_;
  current_migration_cause_ = ON_NETWORK_MADE_DEFAULT;
  current_migrations_to_non_default_network_on_write_error_ = 0;
  current_migrations_to_non_default_network_on_path_degrading_ = 0;

  // Simply cancel the timer to migrate back to the default network if session
  // is already on the default network.
  if (GetCurrentNetwork() == new_network) {
    CancelMigrateBackToDefaultNetworkTimer();
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_ALREADY_MIGRATED,
                                    connection_id(),
                                    "Already migrated on the new network");
    return;
  }

  LogHandshakeStatusOnMigrationSignal();

  // Stay on the current network. Try to migrate back to default network
  // without any delay, which will start probing the new default network and
  // migrate to the new network immediately on success.
  StartMigrateBackToDefaultNetworkTimer(base::TimeDelta());
}

void QuicChromiumClientSession::MigrateNetworkImmediately(
    NetworkChangeNotifier::NetworkHandle network) {
  // There is no choice but to migrate to |network|. If any error encoutered,
  // close the session. When migration succeeds:
  // - if no longer on the default network, start timer to migrate back;
  // - otherwise, it's brought to default network, cancel the running timer to
  //   migrate back.

  if (!migrate_idle_session_ && !HasActiveRequestStreams()) {
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_NO_MIGRATABLE_STREAMS,
                                    connection_id(), "No active streams");
    CloseSessionOnErrorLater(
        ERR_NETWORK_CHANGED,
        quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
        quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }

  if (migrate_idle_session_ && CheckIdleTimeExceedsIdleMigrationPeriod())
    return;

  // Do not migrate if connection migration is disabled.
  if (config()->DisableConnectionMigration()) {
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_DISABLED_BY_CONFIG,
                                    connection_id(),
                                    "Migration disabled by config");
    CloseSessionOnErrorLater(ERR_NETWORK_CHANGED,
                             quic::QUIC_CONNECTION_MIGRATION_DISABLED_BY_CONFIG,
                             quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }

  if (network == GetCurrentNetwork()) {
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_ALREADY_MIGRATED,
                                    connection_id(),
                                    "Already bound to new network");
    return;
  }

  // Cancel probing on |network| if there is any.
  if (version().HasIetfQuicFrames() && connection()->use_path_validator()) {
    auto* context = static_cast<QuicChromiumPathValidationContext*>(
        connection()->GetPathValidationContext());
    if (context && context->network() == network &&
        context->peer_address() == peer_address()) {
      connection()->CancelPathValidation();
    }
  } else {
    probing_manager_.CancelProbing(network, peer_address());
  }

  MigrationResult result =
      Migrate(network, ToIPEndPoint(connection()->peer_address()),
              /*close_session_on_error=*/true);
  if (result == MigrationResult::FAILURE)
    return;

  if (network == default_network_) {
    CancelMigrateBackToDefaultNetworkTimer();
    return;
  }

  // TODO(zhongyi): reconsider this, maybe we just want to hear back
  // We are forced to migrate to |network|, probably |default_network_| is
  // not working, start to migrate back to default network after 1 secs.
  StartMigrateBackToDefaultNetworkTimer(
      base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs));
}

void QuicChromiumClientSession::OnWriteError(int error_code) {
  DCHECK_NE(ERR_IO_PENDING, error_code);
  DCHECK_GT(0, error_code);
  connection()->OnWriteError(error_code);
}

void QuicChromiumClientSession::OnWriteUnblocked() {
  DCHECK(!connection()->writer()->IsWriteBlocked());

  // A new packet will be written after migration completes, unignore read
  // errors.
  if (ignore_read_error_)
    ignore_read_error_ = false;

  if (packet_) {
    DCHECK(send_packet_after_migration_);
    send_packet_after_migration_ = false;
    static_cast<QuicChromiumPacketWriter*>(connection()->writer())
        ->WritePacketToSocket(std::move(packet_));
    return;
  }

  // Unblock the connection, which may send queued packets.
  connection()->OnCanWrite();
  if (send_packet_after_migration_) {
    send_packet_after_migration_ = false;
    if (!connection()->writer()->IsWriteBlocked()) {
      connection()->SendPing();
    }
  }
}

void QuicChromiumClientSession::OnPathDegrading() {
  if (most_recent_path_degrading_timestamp_ == base::TimeTicks())
    most_recent_path_degrading_timestamp_ = tick_clock_->NowTicks();

  if (go_away_on_path_degrading_ && OneRttKeysAvailable()) {
    net_log_.AddEvent(
        NetLogEventType::QUIC_SESSION_CLIENT_GOAWAY_ON_PATH_DEGRADING);
    NotifyFactoryOfSessionGoingAway();
    UMA_HISTOGRAM_COUNTS_1M(
        "Net.QuicSession.ActiveStreamsOnGoAwayAfterPathDegrading",
        GetNumActiveStreams());
    UMA_HISTOGRAM_COUNTS_1M(
        "Net.QuicSession.DrainingStreamsOnGoAwayAfterPathDegrading",
        num_outgoing_draining_streams());
    return;
  }

  if (!go_away_on_path_degrading_) {
    NetworkChangeNotifier::NetworkHandle current_network = GetCurrentNetwork();
    for (auto& observer : connectivity_observer_list_)
      observer.OnSessionPathDegrading(this, current_network);
  }

  if (!stream_factory_)
    return;

  if (allow_port_migration_) {
    current_migration_cause_ = CHANGE_PORT_ON_PATH_DEGRADING;
    MaybeMigrateToDifferentPortOnPathDegrading();
    return;
  }

  MaybeMigrateToAlternateNetworkOnPathDegrading();
}

void QuicChromiumClientSession::OnForwardProgressMadeAfterPathDegrading() {
  if (go_away_on_path_degrading_)
    return;

  NetworkChangeNotifier::NetworkHandle current_network = GetCurrentNetwork();
  for (auto& observer : connectivity_observer_list_)
    observer.OnSessionResumedPostPathDegrading(this, current_network);
}

void QuicChromiumClientSession::OnKeyUpdate(quic::KeyUpdateReason reason) {
  net_log_.AddEventWithStringParams(NetLogEventType::QUIC_SESSION_KEY_UPDATE,
                                    "reason",
                                    quic::KeyUpdateReasonString(reason));

  base::UmaHistogramEnumeration("Net.QuicSession.KeyUpdate.Reason", reason);

  last_key_update_reason_ = reason;
}

void QuicChromiumClientSession::OnProofValid(
    const quic::QuicCryptoClientConfig::CachedState& cached) {
  DCHECK(cached.proof_valid());

  if (!server_info_) {
    return;
  }

  QuicServerInfo::State* state = server_info_->mutable_state();

  state->server_config = cached.server_config();
  state->source_address_token = cached.source_address_token();
  state->cert_sct = cached.cert_sct();
  state->chlo_hash = cached.chlo_hash();
  state->server_config_sig = cached.signature();
  state->certs = cached.certs();

  server_info_->Persist();
}

void QuicChromiumClientSession::OnProofVerifyDetailsAvailable(
    const quic::ProofVerifyDetails& verify_details) {
  const ProofVerifyDetailsChromium* verify_details_chromium =
      reinterpret_cast<const ProofVerifyDetailsChromium*>(&verify_details);
  cert_verify_result_.reset(
      new CertVerifyResult(verify_details_chromium->cert_verify_result));
  pinning_failure_log_ = verify_details_chromium->pinning_failure_log;
  logger_->OnCertificateVerified(*cert_verify_result_);
  pkp_bypassed_ = verify_details_chromium->pkp_bypassed;
  is_fatal_cert_error_ = verify_details_chromium->is_fatal_cert_error;
}

void QuicChromiumClientSession::StartReading() {
  for (auto& packet_reader : packet_readers_) {
    packet_reader->StartReading();
  }
}

void QuicChromiumClientSession::CloseSessionOnError(
    int net_error,
    quic::QuicErrorCode quic_error,
    quic::ConnectionCloseBehavior behavior) {
  base::UmaHistogramSparse("Net.QuicSession.CloseSessionOnError", -net_error);

  if (!callback_.is_null()) {
    std::move(callback_).Run(net_error);
  }

  NotifyAllStreamsOfError(net_error);

  net_log_.AddEventWithIntParams(NetLogEventType::QUIC_SESSION_CLOSE_ON_ERROR,
                                 "net_error", net_error);

  if (connection()->connected())
    connection()->CloseConnection(quic_error, "net error", behavior);
  DCHECK(!connection()->connected());

  CloseAllHandles(net_error);
  NotifyFactoryOfSessionClosed();
}

void QuicChromiumClientSession::CloseSessionOnErrorLater(
    int net_error,
    quic::QuicErrorCode quic_error,
    quic::ConnectionCloseBehavior behavior) {
  base::UmaHistogramSparse("Net.QuicSession.CloseSessionOnError", -net_error);

  if (!callback_.is_null()) {
    std::move(callback_).Run(net_error);
  }
  NotifyAllStreamsOfError(net_error);
  CloseAllHandles(net_error);
  net_log_.AddEventWithIntParams(NetLogEventType::QUIC_SESSION_CLOSE_ON_ERROR,
                                 "net_error", net_error);

  if (connection()->connected())
    connection()->CloseConnection(quic_error, "net error", behavior);
  DCHECK(!connection()->connected());

  NotifyFactoryOfSessionClosedLater();
}

void QuicChromiumClientSession::NotifyAllStreamsOfError(int net_error) {
  PerformActionOnActiveStreams([net_error](quic::QuicStream* stream) {
    static_cast<QuicChromiumClientStream*>(stream)->OnError(net_error);
    return true;
  });
}

void QuicChromiumClientSession::CloseAllHandles(int net_error) {
  while (!handles_.empty()) {
    Handle* handle = *handles_.begin();
    handles_.erase(handle);
    handle->OnSessionClosed(connection()->version(), net_error, error(),
                            port_migration_detected_, GetConnectTiming(),
                            WasConnectionEverUsed());
  }
}

void QuicChromiumClientSession::CancelAllRequests(int net_error) {
  UMA_HISTOGRAM_COUNTS_1000("Net.QuicSession.AbortedPendingStreamRequests",
                            stream_requests_.size());

  while (!stream_requests_.empty()) {
    StreamRequest* request = stream_requests_.front();
    stream_requests_.pop_front();
    request->OnRequestCompleteFailure(net_error);
  }
}

void QuicChromiumClientSession::NotifyRequestsOfConfirmation(int net_error) {
  // Post tasks to avoid reentrancy.
  for (auto& callback : waiting_for_confirmation_callbacks_)
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), net_error));

  waiting_for_confirmation_callbacks_.clear();
}

void QuicChromiumClientSession::MaybeMigrateToDifferentPortOnPathDegrading() {
  DCHECK(allow_port_migration_ && !migrate_session_early_v2_);

  // Migration before handshake is not allowed.
  if (!OneRttKeysAvailable()) {
    HistogramAndLogMigrationFailure(
        MIGRATION_STATUS_PATH_DEGRADING_BEFORE_HANDSHAKE_CONFIRMED,
        connection_id(), "Path degrading before handshake confirmed");
    return;
  }

  net_log_.BeginEvent(NetLogEventType::QUIC_PORT_MIGRATION_TRIGGERED);

  if (!stream_factory_)
    return;

  // Probe a different port, session will migrate to the probed port on success.
  StartProbing(default_network_, peer_address());
  net_log_.EndEvent(NetLogEventType::QUIC_PORT_MIGRATION_TRIGGERED);
}

void QuicChromiumClientSession::
    MaybeMigrateToAlternateNetworkOnPathDegrading() {
  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_PATH_DEGRADING);

  current_migration_cause_ = CHANGE_NETWORK_ON_PATH_DEGRADING;

  if (!migrate_session_early_v2_) {
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_PATH_DEGRADING_NOT_ENABLED,
                                    connection_id(),
                                    "Migration on path degrading not enabled");
    return;
  }

  if (GetCurrentNetwork() == default_network_ &&
      current_migrations_to_non_default_network_on_path_degrading_ >=
          max_migrations_to_non_default_network_on_path_degrading_) {
    HistogramAndLogMigrationFailure(
        MIGRATION_STATUS_ON_PATH_DEGRADING_DISABLED, connection_id(),
        "Exceeds maximum number of migrations on path degrading");
    return;
  }

  NetworkChangeNotifier::NetworkHandle alternate_network =
      stream_factory_->FindAlternateNetwork(GetCurrentNetwork());
  if (alternate_network == NetworkChangeNotifier::kInvalidNetworkHandle) {
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_NO_ALTERNATE_NETWORK,
                                    connection_id(),
                                    "No alternative network on path degrading");
    return;
  }

  LogHandshakeStatusOnMigrationSignal();

  if (!OneRttKeysAvailable()) {
    HistogramAndLogMigrationFailure(
        MIGRATION_STATUS_PATH_DEGRADING_BEFORE_HANDSHAKE_CONFIRMED,
        connection_id(), "Path degrading before handshake confirmed");
    return;
  }

  net_log_.BeginEventWithStringParams(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_TRIGGERED, "trigger",
      "PathDegrading");
  // Probe the alternative network, session will migrate to the probed
  // network and decide whether it wants to migrate back to the default
  // network on success.
  MaybeStartProbing(alternate_network, peer_address());
  net_log_.EndEvent(NetLogEventType::QUIC_CONNECTION_MIGRATION_TRIGGERED);
}

ProbingResult QuicChromiumClientSession::MaybeStartProbing(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress& peer_address) {
  if (!stream_factory_)
    return ProbingResult::FAILURE;

  CHECK_NE(NetworkChangeNotifier::kInvalidNetworkHandle, network);

  if (!migrate_idle_session_ && !HasActiveRequestStreams()) {
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_NO_MIGRATABLE_STREAMS,
                                    connection_id(), "No active streams");
    CloseSessionOnErrorLater(
        ERR_NETWORK_CHANGED,
        quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return ProbingResult::DISABLED_WITH_IDLE_SESSION;
  }

  if (migrate_idle_session_ && CheckIdleTimeExceedsIdleMigrationPeriod())
    return ProbingResult::DISABLED_WITH_IDLE_SESSION;

  // Abort probing if connection migration is disabled by config.
  if (config()->DisableConnectionMigration() ||
      (version().HasIetfQuicFrames() && !connection()->use_path_validator())) {
    DVLOG(1) << "Client disables probing network with connection migration "
             << "disabled by config";
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_DISABLED_BY_CONFIG,
                                    connection_id(),
                                    "Migration disabled by config");
    return ProbingResult::DISABLED_BY_CONFIG;
  }

  return StartProbing(network, peer_address);
}

ProbingResult QuicChromiumClientSession::StartProbing(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress& peer_address) {
  // Check if probing manager is probing the same path.
  if (version().HasIetfQuicFrames() && connection()->use_path_validator()) {
    auto* context = static_cast<QuicChromiumPathValidationContext*>(
        connection()->GetPathValidationContext());
    if (context && context->network() == network &&
        context->peer_address() == peer_address) {
      return ProbingResult::PENDING;
    }
  } else if (probing_manager_.IsUnderProbing(network, peer_address)) {
    return ProbingResult::PENDING;
  }

  // Create and configure socket on |network|.
  std::unique_ptr<DatagramClientSocket> probing_socket =
      stream_factory_->CreateSocket(net_log_.net_log(), net_log_.source());
  if (stream_factory_->ConfigureSocket(probing_socket.get(),
                                       ToIPEndPoint(peer_address), network,
                                       session_key_.socket_tag()) != OK) {
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_INTERNAL_ERROR,
                                    connection_id(),
                                    "Socket configuration failed");
    return ProbingResult::INTERNAL_ERROR;
  }

  // Create new packet writer and reader on the probing socket.
  std::unique_ptr<QuicChromiumPacketWriter> probing_writer(
      new QuicChromiumPacketWriter(probing_socket.get(), task_runner_));
  std::unique_ptr<QuicChromiumPacketReader> probing_reader(
      new QuicChromiumPacketReader(probing_socket.get(), clock_, this,
                                   yield_after_packets_, yield_after_duration_,
                                   net_log_));

  int rtt_ms = connection()
                   ->sent_packet_manager()
                   .GetRttStats()
                   ->smoothed_rtt()
                   .ToMilliseconds();
  if (rtt_ms == 0 || rtt_ms > kDefaultRTTMilliSecs)
    rtt_ms = kDefaultRTTMilliSecs;
  int timeout_ms = rtt_ms * 2;

  if (connection()->use_path_validator() && version().HasIetfQuicFrames()) {
    probing_reader->StartReading();
    path_validation_writer_delegate_.set_network(network);
    path_validation_writer_delegate_.set_peer_address(peer_address);
    probing_writer->set_delegate(&path_validation_writer_delegate_);
    IPEndPoint local_address;
    probing_socket->GetLocalAddress(&local_address);
    auto context = std::make_unique<QuicChromiumPathValidationContext>(
        ToQuicSocketAddress(local_address), peer_address, network,
        std::move(probing_socket), std::move(probing_writer),
        std::move(probing_reader));
    if (current_migration_cause_ != CHANGE_PORT_ON_PATH_DEGRADING) {
      ValidatePath(
          std::move(context),
          std::make_unique<ConnectionMigrationValidationResultDelegate>(this));
      return ProbingResult::PENDING;
    }
    ValidatePath(std::move(context),
                 std::make_unique<PortMigrationValidationResultDelegate>(this));
    return ProbingResult::PENDING;
  }

  probing_manager_.StartProbing(
      network, peer_address, std::move(probing_socket),
      std::move(probing_writer), std::move(probing_reader),
      base::TimeDelta::FromMilliseconds(timeout_ms), net_log_);
  return ProbingResult::PENDING;
}

void QuicChromiumClientSession::StartMigrateBackToDefaultNetworkTimer(
    base::TimeDelta delay) {
  if (current_migration_cause_ != ON_NETWORK_MADE_DEFAULT)
    current_migration_cause_ = ON_MIGRATE_BACK_TO_DEFAULT_NETWORK;

  CancelMigrateBackToDefaultNetworkTimer();
  // Post a task to try migrate back to default network after |delay|.
  migrate_back_to_default_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(
          &QuicChromiumClientSession::MaybeRetryMigrateBackToDefaultNetwork,
          weak_factory_.GetWeakPtr()));
}

void QuicChromiumClientSession::CancelMigrateBackToDefaultNetworkTimer() {
  retry_migrate_back_count_ = 0;
  migrate_back_to_default_timer_.Stop();
}

void QuicChromiumClientSession::TryMigrateBackToDefaultNetwork(
    base::TimeDelta timeout) {
  if (default_network_ == NetworkChangeNotifier::kInvalidNetworkHandle) {
    DVLOG(1) << "Default network is not connected";
    return;
  }

  net_log_.AddEventWithInt64Params(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_MIGRATE_BACK, "retry_count",
      retry_migrate_back_count_);
  // Start probe default network immediately, if manager is probing
  // the same network, this will be a no-op. Otherwise, previous probe
  // will be cancelled and manager starts to probe |default_network_|
  // immediately.
  ProbingResult result = MaybeStartProbing(default_network_, peer_address());

  if (result == ProbingResult::DISABLED_WITH_IDLE_SESSION)
    return;

  if (result != ProbingResult::PENDING) {
    // Session is not allowed to migrate, mark session as going away, cancel
    // migrate back to default timer.
    NotifyFactoryOfSessionGoingAway();
    CancelMigrateBackToDefaultNetworkTimer();
    return;
  }

  retry_migrate_back_count_++;
  migrate_back_to_default_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(
          &QuicChromiumClientSession::MaybeRetryMigrateBackToDefaultNetwork,
          weak_factory_.GetWeakPtr()));
}

void QuicChromiumClientSession::MaybeRetryMigrateBackToDefaultNetwork() {
  base::TimeDelta retry_migrate_back_timeout =
      base::TimeDelta::FromSeconds(UINT64_C(1) << retry_migrate_back_count_);
  if (default_network_ == GetCurrentNetwork()) {
    // If session has been back on the default already by other direct
    // migration attempt, cancel migrate back now.
    CancelMigrateBackToDefaultNetworkTimer();
    return;
  }
  if (retry_migrate_back_timeout > max_time_on_non_default_network_) {
    // Mark session as going away to accept no more streams.
    NotifyFactoryOfSessionGoingAway();
    return;
  }
  TryMigrateBackToDefaultNetwork(retry_migrate_back_timeout);
}

bool QuicChromiumClientSession::CheckIdleTimeExceedsIdleMigrationPeriod() {
  if (!migrate_idle_session_)
    return false;

  if (HasActiveRequestStreams()) {
    return false;
  }

  // There are no active/drainning streams, check the last stream's finish time.
  if (tick_clock_->NowTicks() - most_recent_stream_close_time_ <
      idle_migration_period_) {
    // Still within the idle migration period.
    return false;
  }

  HistogramAndLogMigrationFailure(MIGRATION_STATUS_IDLE_MIGRATION_TIMEOUT,
                                  connection_id(),
                                  "Ilde migration period exceeded");
  CloseSessionOnErrorLater(ERR_NETWORK_CHANGED, quic::QUIC_NETWORK_IDLE_TIMEOUT,
                           quic::ConnectionCloseBehavior::SILENT_CLOSE);
  return true;
}

void QuicChromiumClientSession::ResetNonMigratableStreams() {
  // TODO(zhongyi): may close non-migratable draining streams as well to avoid
  // sending additional data on alternate networks.
  PerformActionOnActiveStreams([](quic::QuicStream* stream) {
    QuicChromiumClientStream* chrome_stream =
        static_cast<QuicChromiumClientStream*>(stream);
    if (!chrome_stream->can_migrate_to_cellular_network()) {
      // Close the stream in both direction by resetting the stream.
      // TODO(zhongyi): use a different error code to reset streams for
      // connection migration.
      chrome_stream->Reset(quic::QUIC_STREAM_CANCELLED);
    }
    return true;
  });
}

void QuicChromiumClientSession::LogMetricsOnNetworkDisconnected() {
  if (most_recent_path_degrading_timestamp_ != base::TimeTicks()) {
    most_recent_network_disconnected_timestamp_ = tick_clock_->NowTicks();
    base::TimeDelta degrading_duration =
        most_recent_network_disconnected_timestamp_ -
        most_recent_path_degrading_timestamp_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.QuicNetworkDegradingDurationTillDisconnected", degrading_duration,
        base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(10),
        100);
  }
  if (most_recent_write_error_timestamp_ != base::TimeTicks()) {
    base::TimeDelta write_error_to_disconnection_gap =
        most_recent_network_disconnected_timestamp_ -
        most_recent_write_error_timestamp_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "Net.QuicNetworkGapBetweenWriteErrorAndDisconnection",
        write_error_to_disconnection_gap, base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromMinutes(10), 100);
    base::UmaHistogramSparse("Net.QuicSession.WriteError.NetworkDisconnected",
                             -most_recent_write_error_);
    most_recent_write_error_ = 0;
    most_recent_write_error_timestamp_ = base::TimeTicks();
  }
}

void QuicChromiumClientSession::LogMetricsOnNetworkMadeDefault() {
  if (most_recent_path_degrading_timestamp_ != base::TimeTicks()) {
    if (most_recent_network_disconnected_timestamp_ != base::TimeTicks()) {
      // NetworkDiscconected happens before NetworkMadeDefault, the platform
      // is dropping WiFi.
      base::TimeTicks now = tick_clock_->NowTicks();
      base::TimeDelta disconnection_duration =
          now - most_recent_network_disconnected_timestamp_;
      base::TimeDelta degrading_duration =
          now - most_recent_path_degrading_timestamp_;
      UMA_HISTOGRAM_CUSTOM_TIMES("Net.QuicNetworkDisconnectionDuration",
                                 disconnection_duration,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromMinutes(10), 100);
      UMA_HISTOGRAM_CUSTOM_TIMES(
          "Net.QuicNetworkDegradingDurationTillNewNetworkMadeDefault",
          degrading_duration, base::TimeDelta::FromMilliseconds(1),
          base::TimeDelta::FromMinutes(10), 100);
      most_recent_network_disconnected_timestamp_ = base::TimeTicks();
    }
    most_recent_path_degrading_timestamp_ = base::TimeTicks();
  }
}

void QuicChromiumClientSession::LogMigrationResultToHistogram(
    QuicConnectionMigrationStatus status) {
  if (current_migration_cause_ == CHANGE_PORT_ON_PATH_DEGRADING) {
    UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.PortMigration", status,
                              MIGRATION_STATUS_MAX);
    current_migration_cause_ = UNKNOWN_CAUSE;
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.ConnectionMigration", status,
                            MIGRATION_STATUS_MAX);

  // Log the connection migraiton result to different histograms based on the
  // cause of the connection migration.
  std::string histogram_name = "Net.QuicSession.ConnectionMigration." +
                               MigrationCauseToString(current_migration_cause_);
  base::UmaHistogramEnumeration(histogram_name, status, MIGRATION_STATUS_MAX);
  current_migration_cause_ = UNKNOWN_CAUSE;
}

void QuicChromiumClientSession::LogHandshakeStatusOnMigrationSignal() const {
  if (current_migration_cause_ == CHANGE_PORT_ON_PATH_DEGRADING) {
    UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.HandshakeStatusOnPortMigration",
                          OneRttKeysAvailable());
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.HandshakeStatusOnConnectionMigration",
                        OneRttKeysAvailable());

  const std::string histogram_name =
      "Net.QuicSession.HandshakeStatusOnConnectionMigration." +
      MigrationCauseToString(current_migration_cause_);
  STATIC_HISTOGRAM_POINTER_GROUP(
      histogram_name, current_migration_cause_, MIGRATION_CAUSE_MAX,
      AddBoolean(OneRttKeysAvailable()),
      base::BooleanHistogram::FactoryGet(
          histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag));
}

void QuicChromiumClientSession::HistogramAndLogMigrationFailure(
    QuicConnectionMigrationStatus status,
    quic::QuicConnectionId connection_id,
    const char* reason) {
  NetLogEventType event_type =
      current_migration_cause_ == CHANGE_PORT_ON_PATH_DEGRADING
          ? NetLogEventType::QUIC_PORT_MIGRATION_FAILURE
          : NetLogEventType::QUIC_CONNECTION_MIGRATION_FAILURE;

  net_log_.AddEvent(event_type, [&] {
    return NetLogQuicMigrationFailureParams(connection_id, reason);
  });

  // |current_migration_cause_| will be reset afterwards.
  LogMigrationResultToHistogram(status);
}

void QuicChromiumClientSession::HistogramAndLogMigrationSuccess(
    quic::QuicConnectionId connection_id) {
  NetLogEventType event_type =
      current_migration_cause_ == CHANGE_PORT_ON_PATH_DEGRADING
          ? NetLogEventType::QUIC_PORT_MIGRATION_SUCCESS
          : NetLogEventType::QUIC_CONNECTION_MIGRATION_SUCCESS;

  net_log_.AddEvent(event_type, [&] {
    return NetLogQuicMigrationSuccessParams(connection_id);
  });

  // |current_migration_cause_| will be reset afterwards.
  LogMigrationResultToHistogram(MIGRATION_STATUS_SUCCESS);
}

base::Value QuicChromiumClientSession::GetInfoAsValue(
    const std::set<HostPortPair>& aliases) {
  base::DictionaryValue dict;
  dict.SetString("version", ParsedQuicVersionToString(connection()->version()));
  dict.SetInteger("open_streams", GetNumActiveStreams());
  std::unique_ptr<base::ListValue> stream_list(new base::ListValue());
  auto* stream_list_ptr = stream_list.get();

  PerformActionOnActiveStreams([stream_list_ptr](quic::QuicStream* stream) {
    stream_list_ptr->AppendString(base::NumberToString(stream->id()));
    return true;
  });

  dict.Set("active_streams", std::move(stream_list));

  dict.SetInteger("total_streams", num_total_streams_);
  dict.SetString("peer_address", peer_address().ToString());
  dict.SetString("network_isolation_key",
                 session_key_.network_isolation_key().ToDebugString());
  dict.SetString("connection_id", connection_id().ToString());
  if (!connection()->client_connection_id().IsEmpty()) {
    dict.SetString("client_connection_id",
                   connection()->client_connection_id().ToString());
  }
  dict.SetBoolean("connected", connection()->connected());
  const quic::QuicConnectionStats& stats = connection()->GetStats();
  dict.SetInteger("packets_sent", stats.packets_sent);
  dict.SetInteger("packets_received", stats.packets_received);
  dict.SetInteger("packets_lost", stats.packets_lost);
  SSLInfo ssl_info;

  std::unique_ptr<base::ListValue> alias_list(new base::ListValue());
  for (const auto& alias : aliases) {
    alias_list->AppendString(alias.ToString());
  }
  dict.Set("aliases", std::move(alias_list));

  return std::move(dict);
}

bool QuicChromiumClientSession::gquic_zero_rtt_disabled() const {
  if (!stream_factory_)
    return false;
  return stream_factory_->gquic_zero_rtt_disabled();
}

std::unique_ptr<QuicChromiumClientSession::Handle>
QuicChromiumClientSession::CreateHandle(const HostPortPair& destination) {
  return std::make_unique<QuicChromiumClientSession::Handle>(
      weak_factory_.GetWeakPtr(), destination);
}

bool QuicChromiumClientSession::OnReadError(
    int result,
    const DatagramClientSocket* socket) {
  DCHECK(socket != nullptr);
  base::UmaHistogramSparse("Net.QuicSession.ReadError.AnyNetwork", -result);
  if (socket != GetDefaultSocket()) {
    DVLOG(1) << "Ignoring read error " << ErrorToString(result)
             << " on old socket";
    base::UmaHistogramSparse("Net.QuicSession.ReadError.OtherNetworks",
                             -result);
    // Ignore read errors from sockets that are not affecting the current
    // network, i.e., sockets that are no longer active and probing socket.
    // TODO(jri): Maybe clean up old sockets on error.
    return false;
  }

  if (ignore_read_error_) {
    DVLOG(1) << "Ignoring read error " << ErrorToString(result)
             << " during pending migration";
    // Ignore read errors during pending migration. Connection will be closed if
    // pending migration failed or timed out.
    base::UmaHistogramSparse("Net.QuicSession.ReadError.PendingMigration",
                             -result);
    return false;
  }

  base::UmaHistogramSparse("Net.QuicSession.ReadError.CurrentNetwork", -result);
  if (OneRttKeysAvailable()) {
    base::UmaHistogramSparse(
        "Net.QuicSession.ReadError.CurrentNetwork.HandshakeConfirmed", -result);
  }

  DVLOG(1) << "Closing session on read error " << ErrorToString(result);
  connection()->CloseConnection(quic::QUIC_PACKET_READ_ERROR,
                                ErrorToString(result),
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
  return false;
}

bool QuicChromiumClientSession::OnPacket(
    const quic::QuicReceivedPacket& packet,
    const quic::QuicSocketAddress& local_address,
    const quic::QuicSocketAddress& peer_address) {
  ProcessUdpPacket(local_address, peer_address, packet);
  if (!connection()->connected()) {
    NotifyFactoryOfSessionClosedLater();
    return false;
  }
  return true;
}

void QuicChromiumClientSession::NotifyFactoryOfSessionGoingAway() {
  going_away_ = true;
  if (stream_factory_)
    stream_factory_->OnSessionGoingAway(this);
}

void QuicChromiumClientSession::NotifyFactoryOfSessionClosedLater() {
  going_away_ = true;
  DCHECK_EQ(0u, GetNumActiveStreams());
  DCHECK(!connection()->connected());
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&QuicChromiumClientSession::NotifyFactoryOfSessionClosed,
                     weak_factory_.GetWeakPtr()));
}

void QuicChromiumClientSession::NotifyFactoryOfSessionClosed() {
  going_away_ = true;
  DCHECK_EQ(0u, GetNumActiveStreams());
  // Will delete |this|.
  if (stream_factory_)
    stream_factory_->OnSessionClosed(this);
}

void QuicChromiumClientSession::OnCryptoHandshakeComplete() {
  if (stream_factory_)
    stream_factory_->set_is_quic_known_to_work_on_current_network(true);

  // Update |connect_end| only when handshake is confirmed. This should also
  // take care of any failed 0-RTT request.
  connect_timing_.connect_end = tick_clock_->NowTicks();
  DCHECK_LE(connect_timing_.connect_start, connect_timing_.connect_end);
  UMA_HISTOGRAM_TIMES(
      "Net.QuicSession.HandshakeConfirmedTime",
      connect_timing_.connect_end - connect_timing_.connect_start);
  // Track how long it has taken to finish handshake after we have finished
  // DNS host resolution.
  if (!connect_timing_.dns_end.is_null()) {
    UMA_HISTOGRAM_TIMES("Net.QuicSession.HostResolution.HandshakeConfirmedTime",
                        tick_clock_->NowTicks() - connect_timing_.dns_end);
  }

  auto it = handles_.begin();
  while (it != handles_.end()) {
    Handle* handle = *it;
    ++it;
    handle->OnCryptoHandshakeConfirmed();
  }

  NotifyRequestsOfConfirmation(OK);
  // Attempt to migrate back to the default network after handshake has been
  // confirmed if the session is not created on the default network.
  if (migrate_session_on_network_change_v2_ &&
      default_network_ != NetworkChangeNotifier::kInvalidNetworkHandle &&
      GetCurrentNetwork() != default_network_) {
    current_migration_cause_ = ON_MIGRATE_BACK_TO_DEFAULT_NETWORK;
    StartMigrateBackToDefaultNetworkTimer(
        base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs));
  }
}

MigrationResult QuicChromiumClientSession::Migrate(
    NetworkChangeNotifier::NetworkHandle network,
    IPEndPoint peer_address,
    bool close_session_on_error) {
  if (!stream_factory_)
    return MigrationResult::FAILURE;

  if (network != NetworkChangeNotifier::kInvalidNetworkHandle) {
    // This is a migration attempt from connection migration.
    ResetNonMigratableStreams();
    if (!migrate_idle_session_ && !HasActiveRequestStreams()) {
      // If idle sessions can not be migrated, close the session if needed.
      if (close_session_on_error) {
        CloseSessionOnErrorLater(
            ERR_NETWORK_CHANGED,
            quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
            quic::ConnectionCloseBehavior::SILENT_CLOSE);
      }
      return MigrationResult::FAILURE;
    }
  }

  // Create and configure socket on |network|.
  std::unique_ptr<DatagramClientSocket> socket(
      stream_factory_->CreateSocket(net_log_.net_log(), net_log_.source()));
  if (stream_factory_->ConfigureSocket(socket.get(), peer_address, network,
                                       session_key_.socket_tag()) != OK) {
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_INTERNAL_ERROR,
                                    connection_id(),
                                    "Socket configuration failed");
    if (close_session_on_error) {
      if (migrate_session_on_network_change_v2_) {
        CloseSessionOnErrorLater(ERR_NETWORK_CHANGED,
                                 quic::QUIC_CONNECTION_MIGRATION_INTERNAL_ERROR,
                                 quic::ConnectionCloseBehavior::SILENT_CLOSE);
      } else {
        CloseSessionOnError(ERR_NETWORK_CHANGED,
                            quic::QUIC_CONNECTION_MIGRATION_INTERNAL_ERROR,
                            quic::ConnectionCloseBehavior::SILENT_CLOSE);
      }
    }
    return MigrationResult::FAILURE;
  }

  // Create new packet reader and writer on the new socket.
  std::unique_ptr<QuicChromiumPacketReader> new_reader(
      new QuicChromiumPacketReader(socket.get(), clock_, this,
                                   yield_after_packets_, yield_after_duration_,
                                   net_log_));
  new_reader->StartReading();
  std::unique_ptr<QuicChromiumPacketWriter> new_writer(
      new QuicChromiumPacketWriter(socket.get(), task_runner_));

  static_cast<QuicChromiumPacketWriter*>(connection()->writer())
      ->set_delegate(nullptr);
  new_writer->set_delegate(this);

  // Migrate to the new socket.
  if (!MigrateToSocket(std::move(socket), std::move(new_reader),
                       std::move(new_writer))) {
    HistogramAndLogMigrationFailure(MIGRATION_STATUS_TOO_MANY_CHANGES,
                                    connection_id(), "Too many changes");
    if (close_session_on_error) {
      if (migrate_session_on_network_change_v2_) {
        CloseSessionOnErrorLater(
            ERR_NETWORK_CHANGED,
            quic::QUIC_CONNECTION_MIGRATION_TOO_MANY_CHANGES,
            quic::ConnectionCloseBehavior::SILENT_CLOSE);
      } else {
        CloseSessionOnError(ERR_NETWORK_CHANGED,
                            quic::QUIC_CONNECTION_MIGRATION_TOO_MANY_CHANGES,
                            quic::ConnectionCloseBehavior::SILENT_CLOSE);
      }
    }
    return MigrationResult::FAILURE;
  }
  HistogramAndLogMigrationSuccess(connection_id());
  return MigrationResult::SUCCESS;
}

bool QuicChromiumClientSession::MigrateToSocket(
    std::unique_ptr<DatagramClientSocket> socket,
    std::unique_ptr<QuicChromiumPacketReader> reader,
    std::unique_ptr<QuicChromiumPacketWriter> writer) {
  CHECK_EQ(sockets_.size(), packet_readers_.size());

  // TODO(zhongyi): figure out whether we want to limit the number of
  // connection migrations for v2, which includes migration on platform signals,
  // write error events, and path degrading on original network.
  if (!migrate_session_on_network_change_v2_ &&
      sockets_.size() >= kMaxReadersPerQuicSession) {
    return false;
  }

  packet_readers_.push_back(std::move(reader));
  sockets_.push_back(std::move(socket));
  // Froce the writer to be blocked to prevent it being used until
  // WriteToNewSocket completes.
  DVLOG(1) << "Force blocking the packet writer";
  writer->set_force_write_blocked(true);
  // TODO(jri): Make SetQuicPacketWriter take a scoped_ptr.
  connection()->SetQuicPacketWriter(writer.release(), /*owns_writer=*/true);

  // Post task to write the pending packet or a PING packet to the new
  // socket. This avoids reentrancy issues if there is a write error
  // on the write to the new socket.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&QuicChromiumClientSession::WriteToNewSocket,
                                weak_factory_.GetWeakPtr()));
  return true;
}

void QuicChromiumClientSession::PopulateNetErrorDetails(
    NetErrorDetails* details) const {
  details->quic_port_migration_detected = port_migration_detected_;
  details->quic_connection_error = error();
}

const DatagramClientSocket* QuicChromiumClientSession::GetDefaultSocket()
    const {
  DCHECK(sockets_.back().get() != nullptr);
  // The most recently added socket is the currently active one.
  return sockets_.back().get();
}

NetworkChangeNotifier::NetworkHandle
QuicChromiumClientSession::GetCurrentNetwork() const {
  // If connection migration is enabled, alternate network interface may be
  // used to send packet, it is identified as the bound network of the default
  // socket. Otherwise, always use |default_network_|.
  return migrate_session_on_network_change_v2_
             ? GetDefaultSocket()->GetBoundNetwork()
             : default_network_;
}

bool QuicChromiumClientSession::IsAuthorized(const std::string& hostname) {
  bool result = CanPool(hostname, session_key_);
  if (result)
    streams_pushed_count_++;
  return result;
}

bool QuicChromiumClientSession::HandlePromised(
    quic::QuicStreamId id,
    quic::QuicStreamId promised_id,
    const spdy::Http2HeaderBlock& headers) {
  bool result =
      quic::QuicSpdyClientSessionBase::HandlePromised(id, promised_id, headers);
  if (result) {
    // The push promise is accepted, notify the push_delegate that a push
    // promise has been received.
    if (push_delegate_) {
      std::string pushed_url =
          quic::SpdyServerPushUtils::GetPromisedUrlFromHeaders(headers);
      push_delegate_->OnPush(std::make_unique<QuicServerPushHelper>(
                                 weak_factory_.GetWeakPtr(), GURL(pushed_url)),
                             net_log_);
    }
    if (headers_include_h2_stream_dependency_ ||
        VersionUsesHttp3(connection()->transport_version())) {
      // Even though the promised stream will not be created until after the
      // push promise headers are received, send a PRIORITY frame for the
      // promised stream ID. Send |kDefaultPriority| since that will be the
      // initial spdy::SpdyPriority of the push promise stream when created.
      const spdy::SpdyPriority priority = quic::QuicStream::kDefaultPriority;
      spdy::SpdyStreamId parent_stream_id = 0;
      int weight = 0;
      bool exclusive = false;
      priority_dependency_state_.OnStreamCreation(
          promised_id, priority, &parent_stream_id, &weight, &exclusive);
      if (!VersionUsesHttp3(connection()->transport_version())) {
        WritePriority(promised_id, parent_stream_id, weight, exclusive);
      }
    }
  }
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PUSH_PROMISE_RECEIVED,
                    [&](NetLogCaptureMode capture_mode) {
                      return NetLogQuicPushPromiseReceivedParams(
                          &headers, id, promised_id, capture_mode);
                    });
  return result;
}

void QuicChromiumClientSession::DeletePromised(
    quic::QuicClientPromisedInfo* promised) {
  if (IsOpenStream(promised->id()))
    streams_pushed_and_claimed_count_++;
  quic::QuicSpdyClientSessionBase::DeletePromised(promised);
}

void QuicChromiumClientSession::OnPushStreamTimedOut(
    quic::QuicStreamId stream_id) {
  quic::QuicSpdyStream* stream = GetPromisedStream(stream_id);
  if (stream != nullptr)
    bytes_pushed_and_unclaimed_count_ += stream->stream_bytes_read();
}

void QuicChromiumClientSession::CancelPush(const GURL& url) {
  quic::QuicClientPromisedInfo* promised_info =
      quic::QuicSpdyClientSessionBase::GetPromisedByUrl(url.spec());
  if (!promised_info || promised_info->is_validating()) {
    // Push stream has already been claimed or is pending matched to a request.
    return;
  }

  quic::QuicStreamId stream_id = promised_info->id();

  // Collect data on the cancelled push stream.
  quic::QuicSpdyStream* stream = GetPromisedStream(stream_id);
  if (stream != nullptr)
    bytes_pushed_and_unclaimed_count_ += stream->stream_bytes_read();

  // Send the reset and remove the promised info from the promise index.
  quic::QuicSpdyClientSessionBase::ResetPromised(stream_id,
                                                 quic::QUIC_STREAM_CANCELLED);
  DeletePromised(promised_info);
}

const LoadTimingInfo::ConnectTiming&
QuicChromiumClientSession::GetConnectTiming() {
  connect_timing_.ssl_start = connect_timing_.connect_start;
  connect_timing_.ssl_end = connect_timing_.connect_end;
  return connect_timing_;
}

quic::ParsedQuicVersion QuicChromiumClientSession::GetQuicVersion() const {
  return connection()->version();
}

size_t QuicChromiumClientSession::EstimateMemoryUsage() const {
  // TODO(xunjieli): Estimate |crypto_stream_|, quic::QuicSpdySession's
  // quic::QuicHeaderList, quic::QuicSession's QuiCWriteBlockedList, open
  // streams and unacked packet map.
  return base::trace_event::EstimateMemoryUsage(packet_readers_);
}

quic::QuicClientPromisedInfo* QuicChromiumClientSession::GetPromised(
    const GURL& url,
    const QuicSessionKey& session_key) {
  if (!session_key_.CanUseForAliasing(session_key)) {
    return nullptr;
  }
  return push_promise_index_->GetPromised(url.spec());
}

bool QuicChromiumClientSession::ValidateStatelessReset(
    const quic::QuicSocketAddress& self_address,
    const quic::QuicSocketAddress& peer_address) {
  if (probing_manager_.ValidateStatelessReset(self_address, peer_address)) {
    // The stateless reset is received from probing path. We shouldn't close the
    // connection, but should disable further port migration attempt.
    if (allow_port_migration_)
      allow_port_migration_ = false;
    return false;
  }
  return true;
}

}  // namespace net
