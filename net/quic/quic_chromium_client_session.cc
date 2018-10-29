// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_client_session.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/network_activity_monitor.h"
#include "net/http/transport_security_state.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
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
#include "net/ssl/channel_id_service.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/ssl/ssl_info.h"
#include "net/third_party/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
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

// The maximum size of uncompressed QUIC headers that will be allowed.
const size_t kMaxUncompressedHeaderSize = 256 * 1024;

// The maximum time allowed to have no retransmittable packets on the wire
// (after sending the first retransmittable packet) if
// |migrate_session_early_v2_| is true. PING frames will be sent as needed to
// enforce this.
const size_t kDefaultRetransmittableOnWireTimeoutMillisecs = 100;

// Histograms for tracking down the crashes from http://crbug.com/354669
// Note: these values must be kept in sync with the corresponding values in:
// tools/metrics/histograms/histograms.xml
enum Location {
  DESTRUCTOR = 0,
  ADD_OBSERVER = 1,
  TRY_CREATE_STREAM = 2,
  CREATE_OUTGOING_RELIABLE_STREAM = 3,
  NOTIFY_FACTORY_OF_SESSION_CLOSED_LATER = 4,
  NOTIFY_FACTORY_OF_SESSION_CLOSED = 5,
  NUM_LOCATIONS = 6,
};

void RecordUnexpectedOpenStreams(Location location) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.UnexpectedOpenStreams", location,
                            NUM_LOCATIONS);
}

void RecordUnexpectedObservers(Location location) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.UnexpectedObservers", location,
                            NUM_LOCATIONS);
}

void RecordUnexpectedNotGoingAway(Location location) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.UnexpectedNotGoingAway", location,
                            NUM_LOCATIONS);
}

std::unique_ptr<base::Value> NetLogQuicConnectionMigrationTriggerCallback(
    std::string trigger,
    NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("trigger", trigger);
  return std::move(dict);
}

std::unique_ptr<base::Value> NetLogQuicConnectionMigrationFailureCallback(
    quic::QuicConnectionId connection_id,
    std::string reason,
    NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("connection_id", base::NumberToString(connection_id));
  dict->SetString("reason", reason);
  return std::move(dict);
}

std::unique_ptr<base::Value> NetLogQuicConnectionMigrationSuccessCallback(
    quic::QuicConnectionId connection_id,
    NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("connection_id", base::NumberToString(connection_id));
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

void RecordHandshakeState(HandshakeState state) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicHandshakeState", state,
                            NUM_HANDSHAKE_STATES);
}

std::string ConnectionMigrationCauseToString(ConnectionMigrationCause cause) {
  switch (cause) {
    case UNKNOWN:
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
    case ON_PATH_DEGRADING:
      return "OnPathDegrading";
    default:
      QUIC_NOTREACHED();
      break;
  }
  return "InvalidCause";
}

std::unique_ptr<base::Value> NetLogQuicClientSessionCallback(
    const quic::QuicServerId* server_id,
    int cert_verify_flags,
    bool require_confirmation,
    NetLogCaptureMode /* capture_mode */) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("host", server_id->host());
  dict->SetInteger("port", server_id->port());
  dict->SetBoolean("privacy_mode", server_id->privacy_mode_enabled());
  dict->SetBoolean("require_confirmation", require_confirmation);
  dict->SetInteger("cert_verify_flags", cert_verify_flags);
  return std::move(dict);
}

std::unique_ptr<base::Value> NetLogQuicPushPromiseReceivedCallback(
    const spdy::SpdyHeaderBlock* headers,
    spdy::SpdyStreamId stream_id,
    spdy::SpdyStreamId promised_stream_id,
    NetLogCaptureMode capture_mode) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->Set("headers", ElideSpdyHeaderBlockForNetLog(*headers, capture_mode));
  dict->SetInteger("id", stream_id);
  dict->SetInteger("promised_stream_id", promised_stream_id);
  return std::move(dict);
}

// TODO(fayang): Remove this when necessary data is collected.
void LogProbeResultToHistogram(ConnectionMigrationCause cause, bool success) {
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.ConnectionMigrationProbeSuccess",
                        success);
  const std::string histogram_name =
      "Net.QuicSession.ConnectionMigrationProbeSuccess." +
      ConnectionMigrationCauseToString(cause);
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
      was_handshake_confirmed_(session->IsCryptoHandshakeConfirmed()),
      net_error_(OK),
      quic_error_(quic::QUIC_NO_ERROR),
      port_migration_detected_(false),
      server_id_(session_->server_id()),
      quic_version_(session->connection()->transport_version()),
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
    quic::QuicTransportVersion quic_version,
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

bool QuicChromiumClientSession::Handle::IsCryptoHandshakeConfirmed() const {
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

quic::QuicTransportVersion QuicChromiumClientSession::Handle::GetQuicVersion()
    const {
  if (!session_)
    return quic_version_;

  return session_->connection()->transport_version();
}

void QuicChromiumClientSession::Handle::ResetPromised(
    quic::QuicStreamId id,
    quic::QuicRstStreamErrorCode error_code) {
  if (session_)
    session_->ResetPromised(id, error_code);
}

std::unique_ptr<quic::QuicConnection::ScopedPacketFlusher>
QuicChromiumClientSession::Handle::CreatePacketBundler(
    quic::QuicConnection::AckBundling bundling_mode) {
  if (!session_)
    return nullptr;

  return std::make_unique<quic::QuicConnection::ScopedPacketFlusher>(
      session_->connection(), bundling_mode);
}

bool QuicChromiumClientSession::Handle::SharesSameSession(
    const Handle& other) const {
  return session_.get() == other.session_.get();
}

int QuicChromiumClientSession::Handle::RendezvousWithPromised(
    const spdy::SpdyHeaderBlock& headers,
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

  *address = session_->peer_address().impl().socket_address();
  return OK;
}

int QuicChromiumClientSession::Handle::GetSelfAddress(
    IPEndPoint* address) const {
  if (!session_)
    return ERR_CONNECTION_CLOSED;

  *address = session_->self_address().impl().socket_address();
  return OK;
}

bool QuicChromiumClientSession::Handle::WasEverUsed() const {
  if (!session_)
    return was_ever_used_;

  return session_->WasConnectionEverUsed();
}

bool QuicChromiumClientSession::Handle::CheckVary(
    const spdy::SpdyHeaderBlock& client_request,
    const spdy::SpdyHeaderBlock& promise_request,
    const spdy::SpdyHeaderBlock& promise_response) {
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
    base::ResetAndReturn(&push_callback_).Run(rv);
  }
}

QuicChromiumClientSession::StreamRequest::StreamRequest(
    QuicChromiumClientSession::Handle* session,
    bool requires_confirmation,
    const NetworkTrafficAnnotationTag& traffic_annotation)
    : session_(session),
      requires_confirmation_(requires_confirmation),
      stream_(nullptr),
      traffic_annotation_(traffic_annotation),
      weak_factory_(this) {}

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
        base::Bind(&QuicChromiumClientSession::StreamRequest::DoCallback,
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
  base::ResetAndReturn(&callback_).Run(rv);
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
        base::Bind(&QuicChromiumClientSession::StreamRequest::OnIOComplete,
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

QuicChromiumClientSession::QuicChromiumClientSession(
    quic::QuicConnection* connection,
    std::unique_ptr<DatagramClientSocket> socket,
    QuicStreamFactory* stream_factory,
    QuicCryptoClientStreamFactory* crypto_client_stream_factory,
    quic::QuicClock* clock,
    TransportSecurityState* transport_security_state,
    SSLConfigService* ssl_config_service,
    std::unique_ptr<QuicServerInfo> server_info,
    const QuicSessionKey& session_key,
    bool require_confirmation,
    bool migrate_session_early_v2,
    bool migrate_sessions_on_network_change_v2,
    bool go_away_on_path_degrading,
    NetworkChangeNotifier::NetworkHandle default_network,
    base::TimeDelta max_time_on_non_default_network,
    int max_migrations_to_non_default_network_on_write_error,
    int max_migrations_to_non_default_network_on_path_degrading,
    int yield_after_packets,
    quic::QuicTime::Delta yield_after_duration,
    bool headers_include_h2_stream_dependency,
    int cert_verify_flags,
    const quic::QuicConfig& config,
    quic::QuicCryptoClientConfig* crypto_config,
    const char* const connection_description,
    base::TimeTicks dns_resolution_start_time,
    base::TimeTicks dns_resolution_end_time,
    quic::QuicClientPushPromiseIndex* push_promise_index,
    ServerPushDelegate* push_delegate,
    base::SequencedTaskRunner* task_runner,
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    NetLog* net_log)
    : quic::QuicSpdyClientSessionBase(connection, push_promise_index, config),
      session_key_(session_key),
      require_confirmation_(require_confirmation),
      migrate_session_early_v2_(migrate_session_early_v2),
      migrate_session_on_network_change_v2_(
          migrate_sessions_on_network_change_v2),
      go_away_on_path_degrading_(go_away_on_path_degrading),
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
      most_recent_path_degrading_timestamp_(base::TimeTicks()),
      most_recent_network_disconnected_timestamp_(base::TimeTicks()),
      most_recent_write_error_(0),
      most_recent_write_error_timestamp_(base::TimeTicks()),
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
      going_away_(false),
      port_migration_detected_(false),
      push_delegate_(push_delegate),
      streams_pushed_count_(0),
      streams_pushed_and_claimed_count_(0),
      bytes_pushed_count_(0),
      bytes_pushed_and_unclaimed_count_(0),
      probing_manager_(this, task_runner_),
      retry_migrate_back_count_(0),
      current_connection_migration_cause_(UNKNOWN),
      send_packet_after_migration_(false),
      wait_for_new_network_(false),
      ignore_read_error_(false),
      headers_include_h2_stream_dependency_(
          headers_include_h2_stream_dependency &&
          this->connection()->transport_version() >= quic::QUIC_VERSION_43),
      weak_factory_(this) {
  // Make sure connection migration and goaway on path degrading are not turned
  // on at the same time.
  DCHECK(!(migrate_session_early_v2_ && go_away_on_path_degrading_));
  default_network_ = default_network;
  sockets_.push_back(std::move(socket));
  packet_readers_.push_back(std::make_unique<QuicChromiumPacketReader>(
      sockets_.back().get(), clock, this, yield_after_packets,
      yield_after_duration, net_log_));
  crypto_stream_.reset(
      crypto_client_stream_factory->CreateQuicCryptoClientStream(
          session_key.server_id(), this,
          std::make_unique<ProofVerifyContextChromium>(cert_verify_flags,
                                                       net_log_),
          crypto_config));
  connection->set_debug_visitor(logger_.get());
  connection->set_creator_debug_delegate(logger_.get());
  migrate_back_to_default_timer_.SetTaskRunner(task_runner_);
  net_log_.BeginEvent(
      NetLogEventType::QUIC_SESSION,
      base::Bind(NetLogQuicClientSessionCallback, &session_key.server_id(),
                 cert_verify_flags, require_confirmation_));
  IPEndPoint address;
  if (socket && socket->GetLocalAddress(&address) == OK &&
      address.GetFamily() == ADDRESS_FAMILY_IPV6) {
    connection->SetMaxPacketLength(connection->max_packet_length() -
                                   kAdditionalOverheadForIPv6);
  }
  connect_timing_.dns_start = dns_resolution_start_time;
  connect_timing_.dns_end = dns_resolution_end_time;
  if (migrate_session_early_v2_) {
    connection->set_retransmittable_on_wire_timeout(
        quic::QuicTime::Delta::FromMilliseconds(
            kDefaultRetransmittableOnWireTimeoutMillisecs));
  }
}

QuicChromiumClientSession::~QuicChromiumClientSession() {
  DCHECK(callback_.is_null());

  net_log_.EndEvent(NetLogEventType::QUIC_SESSION);
  DCHECK(waiting_for_confirmation_callbacks_.empty());
  if (!dynamic_streams().empty())
    RecordUnexpectedOpenStreams(DESTRUCTOR);
  if (!handles_.empty())
    RecordUnexpectedObservers(DESTRUCTOR);
  if (!going_away_)
    RecordUnexpectedNotGoingAway(DESTRUCTOR);

  while (!dynamic_streams().empty() || !handles_.empty() ||
         !stream_requests_.empty()) {
    // The session must be closed before it is destroyed.
    DCHECK(dynamic_streams().empty());
    CloseAllStreams(ERR_UNEXPECTED);
    DCHECK(handles_.empty());
    CloseAllHandles(ERR_UNEXPECTED);
    CancelAllRequests(ERR_UNEXPECTED);

    connection()->set_debug_visitor(nullptr);
  }

  if (connection()->connected()) {
    // Ensure that the connection is closed by the time the session is
    // destroyed.
    connection()->CloseConnection(quic::QUIC_INTERNAL_ERROR,
                                  "session torn down",
                                  quic::ConnectionCloseBehavior::SILENT_CLOSE);
  }

  if (IsEncryptionEstablished())
    RecordHandshakeState(STATE_ENCRYPTION_ESTABLISHED);
  if (IsCryptoHandshakeConfirmed())
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

  if (!IsCryptoHandshakeConfirmed())
    return;

  // Sending one client_hello means we had zero handshake-round-trips.
  int round_trip_handshakes = crypto_stream_->num_sent_client_hellos() - 1;

  // Don't bother with these histogram during tests, which mock out
  // num_sent_client_hellos().
  if (round_trip_handshakes < 0 || !stream_factory_)
    return;

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
  quic::QuicSpdyClientSessionBase::Initialize();
  SetHpackEncoderDebugVisitor(std::make_unique<HpackEncoderDebugVisitor>());
  SetHpackDecoderDebugVisitor(std::make_unique<HpackDecoderDebugVisitor>());
  set_max_uncompressed_header_bytes(kMaxUncompressedHeaderSize);
}

size_t QuicChromiumClientSession::WriteHeaders(
    quic::QuicStreamId id,
    spdy::SpdyHeaderBlock headers,
    bool fin,
    spdy::SpdyPriority priority,
    quic::QuicReferenceCountedPointer<quic::QuicAckListenerInterface>
        ack_notifier_delegate) {
  spdy::SpdyStreamId parent_stream_id = 0;
  int weight = 0;
  bool exclusive = false;
  if (headers_include_h2_stream_dependency_) {
    priority_dependency_state_.OnStreamCreation(id, priority, &parent_stream_id,
                                                &weight, &exclusive);
  } else {
    weight = spdy::Spdy3PriorityToHttp2Weight(priority);
  }
  return WriteHeadersImpl(id, std::move(headers), fin, weight, parent_stream_id,
                          exclusive, std::move(ack_notifier_delegate));
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
    spdy::SpdyPriority new_priority) {
  if (headers_include_h2_stream_dependency_) {
    auto updates = priority_dependency_state_.OnStreamUpdate(id, new_priority);
    for (auto update : updates) {
      WritePriority(update.id, update.parent_stream_id, update.weight,
                    update.exclusive);
    }
  }
  quic::QuicSpdySession::UpdateStreamPriority(id, new_priority);
}

void QuicChromiumClientSession::OnStreamFrame(
    const quic::QuicStreamFrame& frame) {
  // Record total number of stream frames.
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicNumStreamFramesInPacket", 1);

  // Record number of frames per stream in packet.
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicNumStreamFramesPerStreamInPacket", 1);

  return quic::QuicSpdySession::OnStreamFrame(frame);
}

void QuicChromiumClientSession::AddHandle(Handle* handle) {
  if (going_away_) {
    RecordUnexpectedObservers(ADD_OBSERVER);
    handle->OnSessionClosed(connection()->transport_version(), ERR_UNEXPECTED,
                            error(), port_migration_detected_,
                            GetConnectTiming(), WasConnectionEverUsed());
    return;
  }

  DCHECK(!base::ContainsKey(handles_, handle));
  handles_.insert(handle);
}

void QuicChromiumClientSession::RemoveHandle(Handle* handle) {
  DCHECK(base::ContainsKey(handles_, handle));
  handles_.erase(handle);
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

  if (IsCryptoHandshakeConfirmed())
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
    RecordUnexpectedOpenStreams(TRY_CREATE_STREAM);
    return ERR_CONNECTION_CLOSED;
  }

  if (GetNumOpenOutgoingStreams() < max_open_outgoing_streams()) {
    request->stream_ =
        CreateOutgoingReliableStreamImpl(request->traffic_annotation())
            ->CreateHandle();
    return OK;
  }

  request->pending_start_time_ = base::TimeTicks::Now();
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

bool QuicChromiumClientSession::ShouldCreateOutgoingStream() {
  if (!crypto_stream_->encryption_established()) {
    DVLOG(1) << "Encryption not active so no outgoing stream created.";
    return false;
  }
  if (GetNumOpenOutgoingStreams() >= max_open_outgoing_streams()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already " << GetNumOpenOutgoingStreams() << " open.";
    return false;
  }
  if (goaway_received()) {
    DVLOG(1) << "Failed to create a new outgoing stream. "
             << "Already received goaway.";
    return false;
  }
  if (going_away_) {
    RecordUnexpectedOpenStreams(CREATE_OUTGOING_RELIABLE_STREAM);
    return false;
  }
  return true;
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
      GetNextOutgoingStreamId(), this, quic::BIDIRECTIONAL, net_log_,
      traffic_annotation);
  ActivateStream(base::WrapUnique(stream));
  ++num_total_streams_;
  UMA_HISTOGRAM_COUNTS_1M("Net.QuicSession.NumOpenStreams",
                          GetNumOpenOutgoingStreams());
  // The previous histogram puts 100 in a bucket betweeen 86-113 which does
  // not shed light on if chrome ever things it has more than 100 streams open.
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.TooManyOpenStreams",
                        GetNumOpenOutgoingStreams() > 100);
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
  *endpoint = peer_address().impl().socket_address();
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

  // Map QUIC AEADs to the corresponding TLS 1.3 cipher. OpenSSL's cipher suite
  // numbers begin with a stray 0x03, so mask them off.
  quic::QuicTag aead = crypto_stream_->crypto_negotiated_params().aead;
  uint16_t cipher_suite;
  int security_bits;
  switch (aead) {
    case quic::kAESG:
      cipher_suite = TLS1_CK_AES_128_GCM_SHA256 & 0xffff;
      security_bits = 128;
      break;
    case quic::kCC20:
      cipher_suite = TLS1_CK_CHACHA20_POLY1305_SHA256 & 0xffff;
      security_bits = 256;
      break;
    default:
      NOTREACHED();
      return false;
  }
  int ssl_connection_status = 0;
  SSLConnectionStatusSetCipherSuite(cipher_suite, &ssl_connection_status);
  SSLConnectionStatusSetVersion(SSL_CONNECTION_VERSION_QUIC,
                                &ssl_connection_status);

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
  //
  // TODO(nharper): This will no longer be true in TLS 1.3. This logic, and
  // likely the rest of this logic, will want some adjustments for QUIC with TLS
  // 1.3.
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

  ssl_info->public_key_hashes = cert_verify_result_->public_key_hashes;
  ssl_info->is_issued_by_known_root =
      cert_verify_result_->is_issued_by_known_root;
  ssl_info->pkp_bypassed = pkp_bypassed_;

  ssl_info->connection_status = ssl_connection_status;
  ssl_info->client_cert_sent = false;
  ssl_info->channel_id_sent = crypto_stream_->WasChannelIDSent();
  ssl_info->security_bits = security_bits;
  ssl_info->handshake_type = SSLInfo::HANDSHAKE_FULL;
  ssl_info->pinning_failure_log = pinning_failure_log_;
  ssl_info->is_fatal_cert_error = is_fatal_cert_error_;

  ssl_info->UpdateCertificateTransparencyInfo(*ct_verify_result_);

  return true;
}

int QuicChromiumClientSession::CryptoConnect(CompletionOnceCallback callback) {
  connect_timing_.connect_start = base::TimeTicks::Now();
  RecordHandshakeState(STATE_STARTED);
  DCHECK(flow_controller());

  if (!crypto_stream_->CryptoConnect())
    return ERR_QUIC_HANDSHAKE_FAILED;

  if (IsCryptoHandshakeConfirmed()) {
    connect_timing_.connect_end = base::TimeTicks::Now();
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

bool QuicChromiumClientSession::CanPool(const std::string& hostname,
                                        PrivacyMode privacy_mode,
                                        const SocketTag& socket_tag) const {
  DCHECK(connection()->connected());
  if (privacy_mode != session_key_.privacy_mode() ||
      socket_tag != session_key_.socket_tag()) {
    // Privacy mode and socket tag must always match.
    return false;
  }
  SSLInfo ssl_info;
  if (!GetSSLInfo(&ssl_info) || !ssl_info.cert.get()) {
    NOTREACHED() << "QUIC should always have certificates.";
    return false;
  }

  return SpdySession::CanPool(transport_security_state_, ssl_info,
                              *ssl_config_service_, session_key_.host(),
                              hostname);
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
  if (id % 2 != 0) {
    LOG(WARNING) << "Received invalid push stream id " << id;
    connection()->CloseConnection(
        quic::QUIC_INVALID_STREAM_ID, "Server created odd numbered stream",
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

void QuicChromiumClientSession::CloseStream(quic::QuicStreamId stream_id) {
  quic::QuicStream* stream = GetOrCreateStream(stream_id);
  if (stream) {
    logger_->UpdateReceivedFrameCounts(stream_id, stream->num_frames_received(),
                                       stream->num_duplicate_frames_received());
    if (stream_id % 2 == 0) {
      // Stream with even stream is initiated by server for PUSH.
      bytes_pushed_count_ += stream->stream_bytes_read();
    }
  }
  quic::QuicSpdySession::CloseStream(stream_id);
  OnClosedStream();
}

void QuicChromiumClientSession::SendRstStream(
    quic::QuicStreamId id,
    quic::QuicRstStreamErrorCode error,
    quic::QuicStreamOffset bytes_written) {
  quic::QuicStream* stream = GetOrCreateStream(id);
  if (stream) {
    if (id % 2 == 0) {
      // Stream with even stream is initiated by server for PUSH.
      bytes_pushed_count_ += stream->stream_bytes_read();
    }
  }
  quic::QuicSpdySession::SendRstStream(id, error, bytes_written);
  OnClosedStream();
}

void QuicChromiumClientSession::OnClosedStream() {
  if (GetNumOpenOutgoingStreams() < max_open_outgoing_streams() &&
      !stream_requests_.empty() && crypto_stream_->encryption_established() &&
      !goaway_received() && !going_away_ && connection()->connected()) {
    StreamRequest* request = stream_requests_.front();
    // TODO(ckrasic) - analyze data and then add logic to mark QUIC
    // broken if wait times are excessive.
    UMA_HISTOGRAM_TIMES("Net.QuicSession.PendingStreamsWaitTime",
                        base::TimeTicks::Now() - request->pending_start_time_);
    stream_requests_.pop_front();
    request->OnRequestCompleteSuccess(
        CreateOutgoingReliableStreamImpl(request->traffic_annotation())
            ->CreateHandle());
  }
}

void QuicChromiumClientSession::OnConfigNegotiated() {
  quic::QuicSpdyClientSessionBase::OnConfigNegotiated();
  if (!stream_factory_ || !config()->HasReceivedAlternateServerAddress())
    return;

  // Server has sent an alternate address to connect to.
  IPEndPoint new_address =
      config()->ReceivedAlternateServerAddress().impl().socket_address();
  IPEndPoint old_address;
  GetDefaultSocket()->GetPeerAddress(&old_address);

  // Migrate only if address families match, or if new address family is v6,
  // since a v4 address should be reachable over a v6 network (using a
  // v4-mapped v6 address).
  if (old_address.GetFamily() != new_address.GetFamily() &&
      old_address.GetFamily() == ADDRESS_FAMILY_IPV4) {
    return;
  }

  if (old_address.GetFamily() != new_address.GetFamily()) {
    DCHECK_EQ(old_address.GetFamily(), ADDRESS_FAMILY_IPV6);
    DCHECK_EQ(new_address.GetFamily(), ADDRESS_FAMILY_IPV4);
    // Use a v4-mapped v6 address.
    new_address = IPEndPoint(ConvertIPv4ToIPv4MappedIPv6(new_address.address()),
                             new_address.port());
  }

  if (!stream_factory_->allow_server_migration())
    return;

  // Specifying kInvalidNetworkHandle for the |network| parameter
  // causes the session to use the default network for the new socket.
  Migrate(NetworkChangeNotifier::kInvalidNetworkHandle, new_address,
          /*close_session_on_error=*/true, net_log_);
}

void QuicChromiumClientSession::OnCryptoHandshakeEvent(
    CryptoHandshakeEvent event) {
  if (!callback_.is_null() &&
      (!require_confirmation_ || event == HANDSHAKE_CONFIRMED ||
       event == ENCRYPTION_REESTABLISHED)) {
    // TODO(rtenneti): Currently for all CryptoHandshakeEvent events, callback_
    // could be called because there are no error events in CryptoHandshakeEvent
    // enum. If error events are added to CryptoHandshakeEvent, then the
    // following code needs to changed.
    base::ResetAndReturn(&callback_).Run(OK);
  }
  if (event == HANDSHAKE_CONFIRMED) {
    if (stream_factory_)
      stream_factory_->set_require_confirmation(false);

    // Update |connect_end| only when handshake is confirmed. This should also
    // take care of any failed 0-RTT request.
    connect_timing_.connect_end = base::TimeTicks::Now();
    DCHECK_LE(connect_timing_.connect_start, connect_timing_.connect_end);
    UMA_HISTOGRAM_TIMES(
        "Net.QuicSession.HandshakeConfirmedTime",
        connect_timing_.connect_end - connect_timing_.connect_start);
    // Track how long it has taken to finish handshake after we have finished
    // DNS host resolution.
    if (!connect_timing_.dns_end.is_null()) {
      UMA_HISTOGRAM_TIMES(
          "Net.QuicSession.HostResolution.HandshakeConfirmedTime",
          base::TimeTicks::Now() - connect_timing_.dns_end);
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
        GetDefaultSocket()->GetBoundNetwork() != default_network_) {
      current_connection_migration_cause_ = ON_MIGRATE_BACK_TO_DEFAULT_NETWORK;
      StartMigrateBackToDefaultNetworkTimer(
          base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs));
    }
  }
  quic::QuicSpdySession::OnCryptoHandshakeEvent(event);
}

void QuicChromiumClientSession::OnCryptoHandshakeMessageSent(
    const quic::CryptoHandshakeMessage& message) {
  logger_->OnCryptoHandshakeMessageSent(message);
}

void QuicChromiumClientSession::OnCryptoHandshakeMessageReceived(
    const quic::CryptoHandshakeMessage& message) {
  logger_->OnCryptoHandshakeMessageReceived(message);
  if (message.tag() == quic::kREJ || message.tag() == quic::kSREJ) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Net.QuicSession.RejectLength",
                                message.GetSerialized().length(), 1000, 10000,
                                50);
    quic::QuicStringPiece proof;
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

void QuicChromiumClientSession::OnRstStream(
    const quic::QuicRstStreamFrame& frame) {
  quic::QuicSession::OnRstStream(frame);
  OnClosedStream();
}

void QuicChromiumClientSession::OnConnectionClosed(
    quic::QuicErrorCode error,
    const std::string& error_details,
    quic::ConnectionCloseSource source) {
  DCHECK(!connection()->connected());
  logger_->OnConnectionClosed(error, error_details, source);
  if (source == quic::ConnectionCloseSource::FROM_PEER) {
    if (IsCryptoHandshakeConfirmed()) {
      base::UmaHistogramSparse(
          "Net.QuicSession.ConnectionCloseErrorCodeServer.HandshakeConfirmed",
          error);
      base::HistogramBase* histogram = base::SparseHistogram::FactoryGet(
          "Net.QuicSession.StreamCloseErrorCodeServer.HandshakeConfirmed",
          base::HistogramBase::kUmaTargetedHistogramFlag);
      size_t num_streams = GetNumActiveStreams();
      if (num_streams > 0)
        histogram->AddCount(error, num_streams);
    }
    base::UmaHistogramSparse("Net.QuicSession.ConnectionCloseErrorCodeServer",
                             error);
  } else {
    if (IsCryptoHandshakeConfirmed()) {
      base::UmaHistogramSparse(
          "Net.QuicSession.ConnectionCloseErrorCodeClient.HandshakeConfirmed",
          error);
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
    base::UmaHistogramSparse("Net.QuicSession.ConnectionCloseErrorCodeClient",
                             error);
  }

  if (error == quic::QUIC_NETWORK_IDLE_TIMEOUT) {
    UMA_HISTOGRAM_COUNTS_1M(
        "Net.QuicSession.ConnectionClose.NumOpenStreams.TimedOut",
        GetNumOpenOutgoingStreams());
    if (IsCryptoHandshakeConfirmed()) {
      if (GetNumOpenOutgoingStreams() > 0) {
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
          GetNumOpenOutgoingStreams());
      UMA_HISTOGRAM_COUNTS_1M(
          "Net.QuicSession.ConnectionClose.NumTotalStreams.HandshakeTimedOut",
          num_total_streams_);
    }
  }

  if (IsCryptoHandshakeConfirmed()) {
    // QUIC connections should not timeout while there are open streams,
    // since PING frames are sent to prevent timeouts. If, however, the
    // connection timed out with open streams then QUIC traffic has become
    // blackholed. Alternatively, if too many retransmission timeouts occur
    // then QUIC traffic has become blackholed.
    if (stream_factory_ && (error == quic::QUIC_TOO_MANY_RTOS ||
                            (error == quic::QUIC_NETWORK_IDLE_TIMEOUT &&
                             GetNumOpenOutgoingStreams() > 0))) {
      stream_factory_->OnBlackholeAfterHandshakeConfirmed(this);
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
  }

  base::UmaHistogramSparse("Net.QuicSession.QuicVersion",
                           connection()->transport_version());
  NotifyFactoryOfSessionGoingAway();
  quic::QuicSession::OnConnectionClosed(error, error_details, source);

  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(ERR_QUIC_PROTOCOL_ERROR);
  }

  for (auto& socket : sockets_) {
    socket->Close();
  }
  DCHECK(dynamic_streams().empty());
  CloseAllStreams(ERR_UNEXPECTED);
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

void QuicChromiumClientSession::OnConnectivityProbeReceived(
    const quic::QuicSocketAddress& self_address,
    const quic::QuicSocketAddress& peer_address) {
  DVLOG(1) << "Speculative probing response from ip:port: "
           << peer_address.ToString()
           << " to ip:port: " << self_address.ToString() << " is received";
  // Notify the probing manager that a connectivity probing packet is received.
  probing_manager_.OnConnectivityProbingReceived(self_address, peer_address);
}

int QuicChromiumClientSession::HandleWriteError(
    int error_code,
    scoped_refptr<QuicChromiumPacketWriter::ReusableIOBuffer> packet) {
  current_connection_migration_cause_ = ON_WRITE_ERROR;
  LogHandshakeStatusOnConnectionMigrationSignal();

  base::UmaHistogramSparse("Net.QuicSession.WriteError", -error_code);
  if (IsCryptoHandshakeConfirmed()) {
    base::UmaHistogramSparse("Net.QuicSession.WriteError.HandshakeConfirmed",
                             -error_code);
  }

  if (error_code == ERR_MSG_TOO_BIG || stream_factory_ == nullptr ||
      !migrate_session_on_network_change_v2_ || !IsCryptoHandshakeConfirmed()) {
    return error_code;
  }

  NetworkChangeNotifier::NetworkHandle current_network =
      GetDefaultSocket()->GetBoundNetwork();

  net_log_.AddEvent(NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_WRITE_ERROR,
                    NetLog::Int64Callback("network", current_network));

  DCHECK(packet != nullptr);
  DCHECK_NE(ERR_IO_PENDING, error_code);
  DCHECK_GT(0, error_code);
  DCHECK(packet_ == nullptr);

  // Post a task to migrate the session onto a new network.
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&QuicChromiumClientSession::MigrateSessionOnWriteError,
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

  most_recent_write_error_timestamp_ = base::TimeTicks::Now();
  most_recent_write_error_ = error_code;

  if (stream_factory_ == nullptr) {
    // Close the connection if migration failed. Do not cause a
    // connection close packet to be sent since socket may be borked.
    connection()->CloseConnection(quic::QUIC_PACKET_WRITE_ERROR,
                                  "Write error with nulled stream factory",
                                  quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }

  current_connection_migration_cause_ = ON_WRITE_ERROR;

  if (!IsSessionMigratable(/*close_session_if_not_migratable*/ false)) {
    // Close the connection if migration failed. Do not cause a
    // connection close packet to be sent since socket may be borked.
    connection()->CloseConnection(quic::QUIC_PACKET_WRITE_ERROR,
                                  "Write error for non-migratable session",
                                  quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }

  NetworkChangeNotifier::NetworkHandle new_network =
      stream_factory_->FindAlternateNetwork(
          GetDefaultSocket()->GetBoundNetwork());
  if (new_network == NetworkChangeNotifier::kInvalidNetworkHandle) {
    // No alternate network found.
    HistogramAndLogMigrationFailure(
        net_log_, MIGRATION_STATUS_NO_ALTERNATE_NETWORK, connection_id(),
        "No alternate network found");
    OnNoNewNetwork();
    return;
  }

  if (GetDefaultSocket()->GetBoundNetwork() == default_network_ &&
      current_migrations_to_non_default_network_on_write_error_ >=
          max_migrations_to_non_default_network_on_write_error_) {
    HistogramAndLogMigrationFailure(
        net_log_, MIGRATION_STATUS_ON_WRITE_ERROR_DISABLED, connection_id(),
        "Exceeds maximum number of migrations on write errpr");
    connection()->CloseConnection(
        quic::QUIC_PACKET_WRITE_ERROR,
        "Too many migration for write error for the same network",
        quic::ConnectionCloseBehavior::SILENT_CLOSE);
    return;
  }
  current_migrations_to_non_default_network_on_write_error_++;

  const NetLogWithSource migration_net_log = NetLogWithSource::Make(
      net_log_.net_log(), NetLogSourceType::QUIC_CONNECTION_MIGRATION);
  migration_net_log.BeginEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_TRIGGERED,
      base::Bind(&NetLogQuicConnectionMigrationTriggerCallback, "WriteError"));
  MigrationResult result =
      Migrate(new_network, connection()->peer_address().impl().socket_address(),
              /*close_session_on_error=*/false, migration_net_log);
  migration_net_log.EndEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_TRIGGERED);

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
  DCHECK(IsCryptoHandshakeConfirmed());
  wait_for_new_network_ = true;

  DVLOG(1) << "Force blocking the packet writer";
  // Force blocking the packet writer to avoid any writes since there is no
  // alternate network available.
  static_cast<QuicChromiumPacketWriter*>(connection()->writer())
      ->set_force_write_blocked(true);

  // Post a task to maybe close the session if the alarm fires.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&QuicChromiumClientSession::OnMigrationTimeout,
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

  LogConnectionMigrationResultToHistogram(MIGRATION_STATUS_TIMEOUT);
  CloseSessionOnError(ERR_NETWORK_CHANGED,
                      quic::QUIC_CONNECTION_MIGRATION_NO_NEW_NETWORK,
                      quic::ConnectionCloseBehavior::SILENT_CLOSE);
}

void QuicChromiumClientSession::OnProbeNetworkSucceeded(
    NetworkChangeNotifier::NetworkHandle network,
    const quic::QuicSocketAddress& self_address,
    std::unique_ptr<DatagramClientSocket> socket,
    std::unique_ptr<QuicChromiumPacketWriter> writer,
    std::unique_ptr<QuicChromiumPacketReader> reader) {
  DCHECK(socket);
  DCHECK(writer);
  DCHECK(reader);

  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTION_CONNECTIVITY_PROBING_SUCCEEDED,
      NetLog::Int64Callback("network", network));

  LogProbeResultToHistogram(current_connection_migration_cause_, true);

  // Remove |this| as the old packet writer's delegate. Write error on old
  // writers will be ignored.
  // Set |this| to listen on socket write events on the packet writer
  // that was used for probing.
  static_cast<QuicChromiumPacketWriter*>(connection()->writer())
      ->set_delegate(nullptr);
  writer->set_delegate(this);
  connection()->SetSelfAddress(self_address);

  // Close streams that are not migratable to the probed |network|.
  // If session then becomes idle, close the connection.
  ResetNonMigratableStreams();
  if (GetNumActiveStreams() == 0 && GetNumDrainingStreams() == 0) {
    CloseSessionOnErrorLater(
        ERR_NETWORK_CHANGED,
        quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return;
  }

  // Migrate to the probed socket immediately: socket, writer and reader will
  // be acquired by connection and used as default on success.
  if (!MigrateToSocket(std::move(socket), std::move(reader),
                       std::move(writer))) {
    net_log_.AddEvent(
        NetLogEventType::QUIC_CONNECTION_MIGRATION_FAILURE_AFTER_PROBING);
    return;
  }

  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_SUCCESS_AFTER_PROBING,
      NetLog::Int64Callback("migrate_to_network", network));
  if (network == default_network_) {
    DVLOG(1) << "Client successfully migrated to default network.";
    CancelMigrateBackToDefaultNetworkTimer();
  } else {
    DVLOG(1) << "Client successfully got off default network after "
             << "successful probing network: " << network << ".";
    current_migrations_to_non_default_network_on_path_degrading_++;
    if (!migrate_back_to_default_timer_.IsRunning()) {
      current_connection_migration_cause_ = ON_MIGRATE_BACK_TO_DEFAULT_NETWORK;
      // Session gets off the |default_network|, stay on |network| for now but
      // try to migrate back to default network after 1 second.
      StartMigrateBackToDefaultNetworkTimer(
          base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs));
    }
  }
}

void QuicChromiumClientSession::OnProbeNetworkFailed(
    NetworkChangeNotifier::NetworkHandle network) {
  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTION_CONNECTIVITY_PROBING_FAILED,
      NetLog::Int64Callback("network", network));

  LogProbeResultToHistogram(current_connection_migration_cause_, false);
  // Probing failure for default network can be ignored.
  DVLOG(1) << "Connectivity probing failed on NetworkHandle " << network;
  DVLOG_IF(1, network == default_network_ &&
                  GetDefaultSocket()->GetBoundNetwork() != default_network_)
      << "Client probing failed on the default network, QUIC still "
         "using non-default network.";
}

bool QuicChromiumClientSession::OnSendConnectivityProbingPacket(
    QuicChromiumPacketWriter* writer,
    const quic::QuicSocketAddress& peer_address) {
  return connection()->SendConnectivityProbingPacket(writer, peer_address);
}

void QuicChromiumClientSession::OnNetworkConnected(
    NetworkChangeNotifier::NetworkHandle network,
    const NetLogWithSource& net_log) {
  DCHECK(migrate_session_on_network_change_v2_);
  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_NETWORK_CONNECTED,
      NetLog::Int64Callback("connected_network", network));
  // If there was no migration waiting for new network and the path is not
  // degrading, ignore this signal.
  if (!wait_for_new_network_ && !connection()->IsPathDegrading())
    return;

  if (connection()->IsPathDegrading()) {
    current_connection_migration_cause_ = ON_PATH_DEGRADING;
  }

  if (wait_for_new_network_) {
    wait_for_new_network_ = false;
    if (current_connection_migration_cause_ == ON_WRITE_ERROR)
      current_migrations_to_non_default_network_on_write_error_++;
    // |wait_for_new_network_| is true, there was no working network previously.
    // |network| is now the only possible candidate, migrate immediately.
    MigrateImmediately(network);
  } else {
    // The connection is path degrading.
    DCHECK(connection()->IsPathDegrading());
    OnPathDegrading();
  }
}

void QuicChromiumClientSession::OnNetworkDisconnectedV2(
    NetworkChangeNotifier::NetworkHandle disconnected_network,
    const NetLogWithSource& migration_net_log) {
  DCHECK(migrate_session_on_network_change_v2_);
  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_NETWORK_DISCONNECTED,
      NetLog::Int64Callback("disconnected_network", disconnected_network));
  LogMetricsOnNetworkDisconnected();

  // Stop probing the disconnected network if there is one.
  probing_manager_.CancelProbing(disconnected_network);
  if (disconnected_network == default_network_) {
    DVLOG(1) << "Default network: " << default_network_ << " is disconnected.";
    default_network_ = NetworkChangeNotifier::kInvalidNetworkHandle;
    current_migrations_to_non_default_network_on_write_error_ = 0;
  }

  // Ignore the signal if the current active network is not affected.
  if (GetDefaultSocket()->GetBoundNetwork() != disconnected_network) {
    DVLOG(1) << "Client's current default network is not affected by the "
             << "disconnected one.";
    return;
  }

  current_connection_migration_cause_ = ON_NETWORK_DISCONNECTED;
  LogHandshakeStatusOnConnectionMigrationSignal();
  if (!IsCryptoHandshakeConfirmed()) {
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
  MigrateImmediately(new_network);
}

void QuicChromiumClientSession::OnNetworkMadeDefault(
    NetworkChangeNotifier::NetworkHandle new_network,
    const NetLogWithSource& migration_net_log) {
  DCHECK(migrate_session_on_network_change_v2_);
  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_NETWORK_MADE_DEFAULT,
      NetLog::Int64Callback("new_default_network", new_network));
  LogMetricsOnNetworkMadeDefault();

  DCHECK_NE(NetworkChangeNotifier::kInvalidNetworkHandle, new_network);
  DVLOG(1) << "Network: " << new_network
           << " becomes default, old default: " << default_network_;
  default_network_ = new_network;
  current_connection_migration_cause_ = ON_NETWORK_MADE_DEFAULT;
  current_migrations_to_non_default_network_on_write_error_ = 0;
  current_migrations_to_non_default_network_on_path_degrading_ = 0;

  // Simply cancel the timer to migrate back to the default network if session
  // is already on the default network.
  if (GetDefaultSocket()->GetBoundNetwork() == new_network) {
    CancelMigrateBackToDefaultNetworkTimer();
    HistogramAndLogMigrationFailure(
        migration_net_log, MIGRATION_STATUS_ALREADY_MIGRATED, connection_id(),
        "Already migrated on the new network");
    return;
  }

  LogHandshakeStatusOnConnectionMigrationSignal();

  // Stay on the current network. Try to migrate back to default network
  // without any delay, which will start probing the new default network and
  // migrate to the new network immediately on success.
  StartMigrateBackToDefaultNetworkTimer(base::TimeDelta());
}

void QuicChromiumClientSession::MigrateImmediately(
    NetworkChangeNotifier::NetworkHandle network) {
  // We have no choice but to migrate to |network|. If any error encoutered,
  // close the session. When migration succeeds: if we are no longer on the
  // default interface, start timer to migrate back to the default network;
  // otherwise, we are now on default networ, cancel timer to migrate back
  // to the defautlt network if it is running.

  if (!IsSessionMigratable(/*close_session_if_not_migratable=*/true))
    return;

  if (network == GetDefaultSocket()->GetBoundNetwork()) {
    HistogramAndLogMigrationFailure(net_log_, MIGRATION_STATUS_ALREADY_MIGRATED,
                                    connection_id(),
                                    "Already bound to new network");
    return;
  }

  // Cancel probing on |network| if there is any.
  probing_manager_.CancelProbing(network);

  MigrationResult result =
      Migrate(network, connection()->peer_address().impl().socket_address(),
              /*close_session_on_error=*/true, net_log_);
  if (result == MigrationResult::FAILURE)
    return;

  if (network != default_network_) {
    // TODO(zhongyi): reconsider this, maybe we just want to hear back
    // We are forced to migrate to |network|, probably |default_network_| is
    // not working, start to migrate back to default network after 1 secs.
    StartMigrateBackToDefaultNetworkTimer(
        base::TimeDelta::FromSeconds(kMinRetryTimeForDefaultNetworkSecs));
  } else {
    CancelMigrateBackToDefaultNetworkTimer();
  }
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
      SendPing();
    }
  }
  return;
}

void QuicChromiumClientSession::OnPathDegrading() {
  if (go_away_on_path_degrading_) {
    net_log_.AddEvent(
        NetLogEventType::QUIC_SESSION_CLIENT_GOAWAY_ON_PATH_DEGRADING);
    NotifyFactoryOfSessionGoingAway();
    UMA_HISTOGRAM_COUNTS_1M(
        "Net.QuicSession.ActiveStreamsOnGoAwayAfterPathDegrading",
        GetNumActiveStreams());
    UMA_HISTOGRAM_COUNTS_1M(
        "Net.QuicSession.DrainingStreamsOnGoAwayAfterPathDegrading",
        GetNumDrainingStreams());
    return;
  }

  net_log_.AddEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_PATH_DEGRADING);
  if (most_recent_path_degrading_timestamp_ == base::TimeTicks())
    most_recent_path_degrading_timestamp_ = base::TimeTicks::Now();

  if (!stream_factory_)
    return;

  current_connection_migration_cause_ = ON_PATH_DEGRADING;

  if (!migrate_session_early_v2_) {
    HistogramAndLogMigrationFailure(
        net_log_, MIGRATION_STATUS_PATH_DEGRADING_NOT_ENABLED, connection_id(),
        "Migration on path degrading not enabled");
    return;
  }

  if (GetDefaultSocket()->GetBoundNetwork() == default_network_ &&
      current_migrations_to_non_default_network_on_path_degrading_ >=
          max_migrations_to_non_default_network_on_path_degrading_) {
    HistogramAndLogMigrationFailure(
        net_log_, MIGRATION_STATUS_ON_PATH_DEGRADING_DISABLED, connection_id(),
        "Exceeds maximum number of migrations on path degrading");
    return;
  }

  NetworkChangeNotifier::NetworkHandle alternate_network =
      stream_factory_->FindAlternateNetwork(
          GetDefaultSocket()->GetBoundNetwork());
  if (alternate_network == NetworkChangeNotifier::kInvalidNetworkHandle) {
    HistogramAndLogMigrationFailure(
        net_log_, MIGRATION_STATUS_NO_ALTERNATE_NETWORK, connection_id(),
        "No alternative network on path degrading");
    return;
  }

  LogHandshakeStatusOnConnectionMigrationSignal();

  if (!IsCryptoHandshakeConfirmed()) {
    HistogramAndLogMigrationFailure(
        net_log_, MIGRATION_STATUS_PATH_DEGRADING_BEFORE_HANDSHAKE_CONFIRMED,
        connection_id(), "Path degrading before handshake confirmed");
    return;
  }

  const NetLogWithSource migration_net_log = NetLogWithSource::Make(
      net_log_.net_log(), NetLogSourceType::QUIC_CONNECTION_MIGRATION);
  migration_net_log.BeginEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_TRIGGERED,
      base::Bind(&NetLogQuicConnectionMigrationTriggerCallback,
                 "PathDegrading"));
  // Probe alternative network, session will migrate to the probed
  // network and decide whether it wants to migrate back to the default
  // network on success.
  StartProbeNetwork(alternate_network,
                    connection()->peer_address().impl().socket_address(),
                    migration_net_log);
  migration_net_log.EndEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_TRIGGERED);
}

bool QuicChromiumClientSession::HasOpenDynamicStreams() const {
  return quic::QuicSession::HasOpenDynamicStreams() ||
         GetNumDrainingOutgoingStreams() > 0;
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
  std::unique_ptr<ct::CTVerifyResult> ct_verify_result_copy(
      new ct::CTVerifyResult(verify_details_chromium->ct_verify_result));
  ct_verify_result_ = std::move(ct_verify_result_copy);
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
    base::ResetAndReturn(&callback_).Run(net_error);
  }
  CloseAllStreams(net_error);
  CloseAllHandles(net_error);
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CLOSE_ON_ERROR,
                    NetLog::IntCallback("net_error", net_error));

  if (connection()->connected())
    connection()->CloseConnection(quic_error, "net error", behavior);
  DCHECK(!connection()->connected());

  NotifyFactoryOfSessionClosed();
}

void QuicChromiumClientSession::CloseSessionOnErrorLater(
    int net_error,
    quic::QuicErrorCode quic_error,
    quic::ConnectionCloseBehavior behavior) {
  base::UmaHistogramSparse("Net.QuicSession.CloseSessionOnError", -net_error);

  if (!callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(net_error);
  }
  CloseAllStreams(net_error);
  CloseAllHandles(net_error);
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_CLOSE_ON_ERROR,
                    NetLog::IntCallback("net_error", net_error));

  if (connection()->connected())
    connection()->CloseConnection(quic_error, "net error", behavior);
  DCHECK(!connection()->connected());

  NotifyFactoryOfSessionClosedLater();
}

void QuicChromiumClientSession::CloseAllStreams(int net_error) {
  while (!dynamic_streams().empty()) {
    quic::QuicStream* stream = dynamic_streams().begin()->second.get();
    quic::QuicStreamId id = stream->id();
    static_cast<QuicChromiumClientStream*>(stream)->OnError(net_error);
    CloseStream(id);
  }
}

void QuicChromiumClientSession::CloseAllHandles(int net_error) {
  while (!handles_.empty()) {
    Handle* handle = *handles_.begin();
    handles_.erase(handle);
    handle->OnSessionClosed(connection()->transport_version(), net_error,
                            error(), port_migration_detected_,
                            GetConnectTiming(), WasConnectionEverUsed());
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

ProbingResult QuicChromiumClientSession::StartProbeNetwork(
    NetworkChangeNotifier::NetworkHandle network,
    IPEndPoint peer_address,
    const NetLogWithSource& migration_net_log) {
  if (!stream_factory_)
    return ProbingResult::FAILURE;

  CHECK_NE(NetworkChangeNotifier::kInvalidNetworkHandle, network);

  if (GetNumActiveStreams() == 0 && GetNumDrainingStreams() == 0) {
    HistogramAndLogMigrationFailure(migration_net_log,
                                    MIGRATION_STATUS_NO_MIGRATABLE_STREAMS,
                                    connection_id(), "No active streams");
    CloseSessionOnErrorLater(
        ERR_NETWORK_CHANGED,
        quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
        quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    return ProbingResult::DISABLED_WITH_IDLE_SESSION;
  }

  // Abort probing if connection migration is disabled by config.
  if (config()->DisableConnectionMigration()) {
    DVLOG(1) << "Client disables probing network with connection migration "
             << "disabled by config";
    HistogramAndLogMigrationFailure(
        migration_net_log, MIGRATION_STATUS_DISABLED_BY_CONFIG, connection_id(),
        "Migration disabled by config");
    // TODO(zhongyi): do we want to close the session?
    return ProbingResult::DISABLED_BY_CONFIG;
  }

  // Check if probing manager is probing the same path.
  if (probing_manager_.IsUnderProbing(
          network,
          quic::QuicSocketAddress(quic::QuicSocketAddressImpl(peer_address)))) {
    return ProbingResult::PENDING;
  }

  // Create and configure socket on |network|.
  std::unique_ptr<DatagramClientSocket> probing_socket =
      stream_factory_->CreateSocket(net_log_.net_log(), net_log_.source());
  if (stream_factory_->ConfigureSocket(probing_socket.get(), peer_address,
                                       network,
                                       session_key_.socket_tag()) != OK) {
    HistogramAndLogMigrationFailure(
        migration_net_log, MIGRATION_STATUS_INTERNAL_ERROR, connection_id(),
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

  probing_manager_.StartProbing(
      network,
      quic::QuicSocketAddress(quic::QuicSocketAddressImpl(peer_address)),
      std::move(probing_socket), std::move(probing_writer),
      std::move(probing_reader), base::TimeDelta::FromMilliseconds(timeout_ms),
      net_log_);
  return ProbingResult::PENDING;
}

void QuicChromiumClientSession::StartMigrateBackToDefaultNetworkTimer(
    base::TimeDelta delay) {
  if (current_connection_migration_cause_ != ON_NETWORK_MADE_DEFAULT)
    current_connection_migration_cause_ = ON_MIGRATE_BACK_TO_DEFAULT_NETWORK;

  CancelMigrateBackToDefaultNetworkTimer();
  // Post a task to try migrate back to default network after |delay|.
  migrate_back_to_default_timer_.Start(
      FROM_HERE, delay,
      base::Bind(
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

  net_log_.AddEvent(NetLogEventType::QUIC_CONNECTION_MIGRATION_ON_MIGRATE_BACK,
                    base::Bind(NetLog::Int64Callback(
                        "retry_count", retry_migrate_back_count_)));
  // Start probe default network immediately, if manager is probing
  // the same network, this will be a no-op. Otherwise, previous probe
  // will be cancelled and manager starts to probe |default_network_|
  // immediately.
  ProbingResult result = StartProbeNetwork(
      default_network_, connection()->peer_address().impl().socket_address(),
      net_log_);

  if (result == ProbingResult::DISABLED_WITH_IDLE_SESSION) {
    // |this| session has been closed due to idle session.
    return;
  }

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
      base::Bind(
          &QuicChromiumClientSession::MaybeRetryMigrateBackToDefaultNetwork,
          weak_factory_.GetWeakPtr()));
}

void QuicChromiumClientSession::MaybeRetryMigrateBackToDefaultNetwork() {
  base::TimeDelta retry_migrate_back_timeout =
      base::TimeDelta::FromSeconds(UINT64_C(1) << retry_migrate_back_count_);
  if (default_network_ == GetDefaultSocket()->GetBoundNetwork()) {
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

bool QuicChromiumClientSession::IsSessionMigratable(
    bool close_session_if_not_migratable) {
  // Close idle sessions.
  if (GetNumActiveStreams() == 0 && GetNumDrainingStreams() == 0) {
    HistogramAndLogMigrationFailure(net_log_,
                                    MIGRATION_STATUS_NO_MIGRATABLE_STREAMS,
                                    connection_id(), "No active streams");
    if (close_session_if_not_migratable) {
      CloseSessionOnErrorLater(
          ERR_NETWORK_CHANGED,
          quic::QUIC_CONNECTION_MIGRATION_NO_MIGRATABLE_STREAMS,
          quic::ConnectionCloseBehavior::SILENT_CLOSE);
    }
    return false;
  }

  // Do not migrate sessions where connection migration is disabled.
  if (config()->DisableConnectionMigration()) {
    HistogramAndLogMigrationFailure(
        net_log_, MIGRATION_STATUS_DISABLED_BY_CONFIG, connection_id(),
        "Migration disabled by config");
    if (close_session_if_not_migratable) {
      CloseSessionOnErrorLater(
          ERR_NETWORK_CHANGED,
          quic::QUIC_CONNECTION_MIGRATION_DISABLED_BY_CONFIG,
          quic::ConnectionCloseBehavior::SILENT_CLOSE);
    }
    return false;
  }
  return true;
}

void QuicChromiumClientSession::ResetNonMigratableStreams() {
  // TODO(zhongyi): may close non-migratable draining streams as well to avoid
  // sending additional data on alternate networks.
  auto it = dynamic_streams().begin();
  // Stream may be deleted when iterating through the map.
  while (it != dynamic_streams().end()) {
    QuicChromiumClientStream* stream =
        static_cast<QuicChromiumClientStream*>(it->second.get());
    if (!stream->can_migrate_to_cellular_network()) {
      // Close the stream in both direction by resetting the stream.
      // TODO(zhongyi): use a different error code to reset streams for
      // connection migration.
      stream->Reset(quic::QUIC_STREAM_CANCELLED);
    } else {
      it++;
    }
  }
}

void QuicChromiumClientSession::LogMetricsOnNetworkDisconnected() {
  if (most_recent_path_degrading_timestamp_ != base::TimeTicks()) {
    most_recent_network_disconnected_timestamp_ = base::TimeTicks::Now();
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
      base::TimeTicks now = base::TimeTicks::Now();
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

void QuicChromiumClientSession::LogConnectionMigrationResultToHistogram(
    QuicConnectionMigrationStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Net.QuicSession.ConnectionMigration", status,
                            MIGRATION_STATUS_MAX);

  // Log the connection migraiton result to different histograms based on the
  // cause of the connection migration.
  std::string histogram_name =
      "Net.QuicSession.ConnectionMigration." +
      ConnectionMigrationCauseToString(current_connection_migration_cause_);
  base::UmaHistogramEnumeration(histogram_name, status, MIGRATION_STATUS_MAX);
  current_connection_migration_cause_ = UNKNOWN;
}

void QuicChromiumClientSession::LogHandshakeStatusOnConnectionMigrationSignal()
    const {
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.HandshakeStatusOnConnectionMigration",
                        IsCryptoHandshakeConfirmed());

  const std::string histogram_name =
      "Net.QuicSession.HandshakeStatusOnConnectionMigration." +
      ConnectionMigrationCauseToString(current_connection_migration_cause_);
  STATIC_HISTOGRAM_POINTER_GROUP(
      histogram_name, current_connection_migration_cause_, MIGRATION_CAUSE_MAX,
      AddBoolean(IsCryptoHandshakeConfirmed()),
      base::BooleanHistogram::FactoryGet(
          histogram_name, base::HistogramBase::kUmaTargetedHistogramFlag));
}

void QuicChromiumClientSession::HistogramAndLogMigrationFailure(
    const NetLogWithSource& net_log,
    QuicConnectionMigrationStatus status,
    quic::QuicConnectionId connection_id,
    const std::string& reason) {
  LogConnectionMigrationResultToHistogram(status);
  net_log.AddEvent(NetLogEventType::QUIC_CONNECTION_MIGRATION_FAILURE,
                   base::Bind(&NetLogQuicConnectionMigrationFailureCallback,
                              connection_id, reason));
}

void QuicChromiumClientSession::HistogramAndLogMigrationSuccess(
    const NetLogWithSource& net_log,
    quic::QuicConnectionId connection_id) {
  LogConnectionMigrationResultToHistogram(MIGRATION_STATUS_SUCCESS);
  net_log.AddEvent(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_SUCCESS,
      base::Bind(&NetLogQuicConnectionMigrationSuccessCallback, connection_id));
}

std::unique_ptr<base::Value> QuicChromiumClientSession::GetInfoAsValue(
    const std::set<HostPortPair>& aliases) {
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
  dict->SetString("version",
                  QuicVersionToString(connection()->transport_version()));
  dict->SetInteger("open_streams", GetNumOpenOutgoingStreams());
  std::unique_ptr<base::ListValue> stream_list(new base::ListValue());
  for (DynamicStreamMap::const_iterator it = dynamic_streams().begin();
       it != dynamic_streams().end(); ++it) {
    stream_list->AppendString(base::UintToString(it->second->id()));
  }
  dict->Set("active_streams", std::move(stream_list));

  dict->SetInteger("total_streams", num_total_streams_);
  dict->SetString("peer_address", peer_address().ToString());
  dict->SetString("connection_id", base::NumberToString(connection_id()));
  dict->SetBoolean("connected", connection()->connected());
  const quic::QuicConnectionStats& stats = connection()->GetStats();
  dict->SetInteger("packets_sent", stats.packets_sent);
  dict->SetInteger("packets_received", stats.packets_received);
  dict->SetInteger("packets_lost", stats.packets_lost);
  SSLInfo ssl_info;

  std::unique_ptr<base::ListValue> alias_list(new base::ListValue());
  for (auto it = aliases.begin(); it != aliases.end(); it++) {
    alias_list->AppendString(it->ToString());
  }
  dict->Set("aliases", std::move(alias_list));

  return std::move(dict);
}

std::unique_ptr<QuicChromiumClientSession::Handle>
QuicChromiumClientSession::CreateHandle(const HostPortPair& destination) {
  return std::make_unique<QuicChromiumClientSession::Handle>(
      weak_factory_.GetWeakPtr(), destination);
}

void QuicChromiumClientSession::OnReadError(
    int result,
    const DatagramClientSocket* socket) {
  DCHECK(socket != nullptr);
  base::UmaHistogramSparse("Net.QuicSession.ReadError.AnyNetwork", -result);
  if (socket != GetDefaultSocket()) {
    DVLOG(1) << "Ignore read error on old sockets";
    base::UmaHistogramSparse("Net.QuicSession.ReadError.OtherNetworks",
                             -result);
    // Ignore read errors from sockets that are not affecting the current
    // network, i.e., sockets that are no longer active and probing socket.
    // TODO(jri): Maybe clean up old sockets on error.
    return;
  }

  base::UmaHistogramSparse("Net.QuicSession.ReadError.CurrentNetwork", -result);
  if (IsCryptoHandshakeConfirmed()) {
    base::UmaHistogramSparse(
        "Net.QuicSession.ReadError.CurrentNetwork.HandshakeConfirmed", -result);
  }

  if (ignore_read_error_) {
    DVLOG(1) << "Ignore read error.";
    // Ignore read errors during pending migration. Connection will be closed if
    // pending migration failed or timed out.
    base::UmaHistogramSparse("Net.QuicSession.ReadError.PendingMigration",
                             -result);
    return;
  }

  DVLOG(1) << "Closing session on read error: " << result;
  connection()->CloseConnection(quic::QUIC_PACKET_READ_ERROR,
                                ErrorToString(result),
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
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
  if (!dynamic_streams().empty())
    RecordUnexpectedOpenStreams(NOTIFY_FACTORY_OF_SESSION_CLOSED_LATER);

  if (!going_away_)
    RecordUnexpectedNotGoingAway(NOTIFY_FACTORY_OF_SESSION_CLOSED_LATER);

  going_away_ = true;
  DCHECK_EQ(0u, GetNumActiveStreams());
  DCHECK(!connection()->connected());
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&QuicChromiumClientSession::NotifyFactoryOfSessionClosed,
                 weak_factory_.GetWeakPtr()));
}

void QuicChromiumClientSession::NotifyFactoryOfSessionClosed() {
  if (!dynamic_streams().empty())
    RecordUnexpectedOpenStreams(NOTIFY_FACTORY_OF_SESSION_CLOSED);

  if (!going_away_)
    RecordUnexpectedNotGoingAway(NOTIFY_FACTORY_OF_SESSION_CLOSED);

  going_away_ = true;
  DCHECK_EQ(0u, GetNumActiveStreams());
  // Will delete |this|.
  if (stream_factory_)
    stream_factory_->OnSessionClosed(this);
}

MigrationResult QuicChromiumClientSession::Migrate(
    NetworkChangeNotifier::NetworkHandle network,
    IPEndPoint peer_address,
    bool close_session_on_error,
    const NetLogWithSource& migration_net_log) {
  if (!stream_factory_)
    return MigrationResult::FAILURE;

  if (network != NetworkChangeNotifier::kInvalidNetworkHandle) {
    // This is a migration attempt from connection migration. Close
    // streams that are not migratable to |network|. If session then becomes
    // idle, close the connection if |close_session_on_error| is true.
    ResetNonMigratableStreams();
    if (GetNumActiveStreams() == 0 && GetNumDrainingStreams() == 0) {
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
    HistogramAndLogMigrationFailure(
        migration_net_log, MIGRATION_STATUS_INTERNAL_ERROR, connection_id(),
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
    HistogramAndLogMigrationFailure(migration_net_log,
                                    MIGRATION_STATUS_TOO_MANY_CHANGES,
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
  HistogramAndLogMigrationSuccess(migration_net_log, connection_id());
  return MigrationResult::SUCCESS;
}

bool QuicChromiumClientSession::MigrateToSocket(
    std::unique_ptr<DatagramClientSocket> socket,
    std::unique_ptr<QuicChromiumPacketReader> reader,
    std::unique_ptr<QuicChromiumPacketWriter> writer) {
  DCHECK_EQ(sockets_.size(), packet_readers_.size());

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
      FROM_HERE, base::Bind(&QuicChromiumClientSession::WriteToNewSocket,
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

bool QuicChromiumClientSession::IsAuthorized(const std::string& hostname) {
  bool result =
      CanPool(hostname, session_key_.privacy_mode(), session_key_.socket_tag());
  if (result)
    streams_pushed_count_++;
  return result;
}

bool QuicChromiumClientSession::HandlePromised(
    quic::QuicStreamId id,
    quic::QuicStreamId promised_id,
    const spdy::SpdyHeaderBlock& headers) {
  bool result =
      quic::QuicSpdyClientSessionBase::HandlePromised(id, promised_id, headers);
  if (result) {
    // The push promise is accepted, notify the push_delegate that a push
    // promise has been received.
    if (push_delegate_) {
      std::string pushed_url =
          quic::SpdyUtils::GetPromisedUrlFromHeaders(headers);
      push_delegate_->OnPush(std::make_unique<QuicServerPushHelper>(
                                 weak_factory_.GetWeakPtr(), GURL(pushed_url)),
                             net_log_);
    }
    if (headers_include_h2_stream_dependency_) {
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
      WritePriority(promised_id, parent_stream_id, weight, exclusive);
    }
  }
  net_log_.AddEvent(NetLogEventType::QUIC_SESSION_PUSH_PROMISE_RECEIVED,
                    base::Bind(&NetLogQuicPushPromiseReceivedCallback, &headers,
                               id, promised_id));
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

quic::QuicTransportVersion QuicChromiumClientSession::GetQuicVersion() const {
  return connection()->transport_version();
}

size_t QuicChromiumClientSession::EstimateMemoryUsage() const {
  // TODO(xunjieli): Estimate |crypto_stream_|, quic::QuicSpdySession's
  // quic::QuicHeaderList, quic::QuicSession's QuiCWriteBlockedList, open
  // streams and unacked packet map.
  return base::trace_event::EstimateMemoryUsage(packet_readers_);
}

}  // namespace net
