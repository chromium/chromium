// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_http_stream.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/quic/quic_http_utils.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/ssl/ssl_info.h"
#include "net/third_party/quiche/src/quic/core/http/quic_client_promised_info.h"
#include "net/third_party/quiche/src/quic/core/http/spdy_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_stream_sequencer.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quiche/src/spdy/core/spdy_frame_builder.h"
#include "net/third_party/quiche/src/spdy/core/spdy_framer.h"

namespace net {

namespace {

base::Value NetLogQuicPushStreamParams(quic::QuicStreamId stream_id,
                                       const GURL& url) {
  base::DictionaryValue dict;
  dict.SetInteger("stream_id", stream_id);
  dict.SetString("url", url.spec());
  return std::move(dict);
}

void NetLogQuicPushStream(const NetLogWithSource& net_log1,
                          const NetLogWithSource& net_log2,
                          NetLogEventType type,
                          quic::QuicStreamId stream_id,
                          const GURL& url) {
  net_log1.AddEvent(type,
                    [&] { return NetLogQuicPushStreamParams(stream_id, url); });
  net_log2.AddEvent(type,
                    [&] { return NetLogQuicPushStreamParams(stream_id, url); });
}

}  // namespace

QuicHttpStream::QuicHttpStream(
    std::unique_ptr<QuicChromiumClientSession::Handle> session)
    : MultiplexedHttpStream(std::move(session)),
      next_state_(STATE_NONE),
      stream_(nullptr),
      request_info_(nullptr),
      can_send_early_(false),
      request_body_stream_(nullptr),
      priority_(MINIMUM_PRIORITY),
      response_info_(nullptr),
      has_response_status_(false),
      response_status_(ERR_UNEXPECTED),
      response_headers_received_(false),
      trailing_headers_received_(false),
      headers_bytes_received_(0),
      headers_bytes_sent_(0),
      closed_stream_received_bytes_(0),
      closed_stream_sent_bytes_(0),
      closed_is_first_stream_(false),
      user_buffer_len_(0),
      session_error_(ERR_UNEXPECTED),
      found_promise_(false),
      in_loop_(false) {}

QuicHttpStream::~QuicHttpStream() {
  CHECK(!in_loop_);
  Close(false);
}

HttpResponseInfo::ConnectionInfo QuicHttpStream::ConnectionInfoFromQuicVersion(
    quic::ParsedQuicVersion quic_version) {
  switch (quic_version.transport_version) {
    case quic::QUIC_VERSION_UNSUPPORTED:
      return HttpResponseInfo::CONNECTION_INFO_QUIC_UNKNOWN_VERSION;
    case quic::QUIC_VERSION_43:
      return HttpResponseInfo::CONNECTION_INFO_QUIC_43;
    case quic::QUIC_VERSION_46:
      return HttpResponseInfo::CONNECTION_INFO_QUIC_46;
    case quic::QUIC_VERSION_48:
      return quic_version.handshake_protocol == quic::PROTOCOL_TLS1_3
                 ? HttpResponseInfo::CONNECTION_INFO_QUIC_T048
                 : HttpResponseInfo::CONNECTION_INFO_QUIC_Q048;
    case quic::QUIC_VERSION_49:
      return quic_version.handshake_protocol == quic::PROTOCOL_TLS1_3
                 ? HttpResponseInfo::CONNECTION_INFO_QUIC_T049
                 : HttpResponseInfo::CONNECTION_INFO_QUIC_Q049;
    case quic::QUIC_VERSION_50:
      return quic_version.handshake_protocol == quic::PROTOCOL_TLS1_3
                 ? HttpResponseInfo::CONNECTION_INFO_QUIC_T050
                 : HttpResponseInfo::CONNECTION_INFO_QUIC_Q050;
    case quic::QUIC_VERSION_99:
      return quic_version.handshake_protocol == quic::PROTOCOL_TLS1_3
                 ? HttpResponseInfo::CONNECTION_INFO_QUIC_T099
                 : HttpResponseInfo::CONNECTION_INFO_QUIC_Q099;
    case quic::QUIC_VERSION_RESERVED_FOR_NEGOTIATION:
      return HttpResponseInfo::CONNECTION_INFO_QUIC_999;
  }
  NOTREACHED();
  return HttpResponseInfo::CONNECTION_INFO_QUIC_UNKNOWN_VERSION;
}

int QuicHttpStream::InitializeStream(const HttpRequestInfo* request_info,
                                     bool can_send_early,
                                     RequestPriority priority,
                                     const NetLogWithSource& stream_net_log,
                                     CompletionOnceCallback callback) {
  CHECK(callback_.is_null());
  DCHECK(!stream_);
  DCHECK(request_info->traffic_annotation.is_valid());

  // HttpNetworkTransaction will retry any request that fails with
  // ERR_QUIC_HANDSHAKE_FAILED. It will retry any request with
  // ERR_CONNECTION_CLOSED so long as the connection has been used for other
  // streams first and headers have not yet been received.
  if (!quic_session()->IsConnected())
    return GetResponseStatus();

  stream_net_log.AddEventReferencingSource(
      NetLogEventType::HTTP_STREAM_REQUEST_BOUND_TO_QUIC_SESSION,
      quic_session()->net_log().source());
  stream_net_log.AddEventWithIntParams(
      NetLogEventType::QUIC_CONNECTION_MIGRATION_MODE,
      "connection_migration_mode",
      static_cast<int>(quic_session()->connection_migration_mode()));

  stream_net_log_ = stream_net_log;
  request_info_ = request_info;
  can_send_early_ = can_send_early;
  request_time_ = base::Time::Now();
  priority_ = priority;

  SaveSSLInfo();

  std::string url(request_info->url.spec());
  quic::QuicClientPromisedInfo* promised =
      quic_session()->GetPushPromiseIndex()->GetPromised(url);
  if (promised) {
    found_promise_ = true;
    NetLogQuicPushStream(
        stream_net_log_, quic_session()->net_log(),
        NetLogEventType::QUIC_HTTP_STREAM_PUSH_PROMISE_RENDEZVOUS,
        promised->id(), request_info_->url);
    return OK;
  }

  next_state_ = STATE_REQUEST_STREAM;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);

  return MapStreamError(rv);
}

int QuicHttpStream::DoHandlePromise() {
  next_state_ = STATE_HANDLE_PROMISE_COMPLETE;
  return quic_session()->RendezvousWithPromised(
      request_headers_, base::BindOnce(&QuicHttpStream::OnIOComplete,
                                       weak_factory_.GetWeakPtr()));
}

int QuicHttpStream::DoHandlePromiseComplete(int rv) {
  DCHECK_NE(ERR_IO_PENDING, rv);
  DCHECK_GE(OK, rv);
  if (rv != OK) {
    // rendezvous has failed so proceed as with a non-push request.
    next_state_ = STATE_REQUEST_STREAM;
    return OK;
  }

  stream_ = quic_session()->ReleasePromisedStream();

  spdy::SpdyPriority spdy_priority =
      ConvertRequestPriorityToQuicPriority(priority_);
  const spdy::SpdyStreamPrecedence precedence(spdy_priority);
  stream_->SetPriority(precedence);

  next_state_ = STATE_OPEN;
  NetLogQuicPushStream(stream_net_log_, quic_session()->net_log(),
                       NetLogEventType::QUIC_HTTP_STREAM_ADOPTED_PUSH_STREAM,
                       stream_->id(), request_info_->url);
  return OK;
}

int QuicHttpStream::SendRequest(const HttpRequestHeaders& request_headers,
                                HttpResponseInfo* response,
                                CompletionOnceCallback callback) {
  CHECK(!request_body_stream_);
  CHECK(!response_info_);
  CHECK(callback_.is_null());
  CHECK(!callback.is_null());
  CHECK(response);

  // In order to rendezvous with a push stream, the session still needs to be
  // available. Otherwise the stream needs to be available.
  if ((!found_promise_ && !stream_) || !quic_session()->IsConnected())
    return GetResponseStatus();

  // Store the serialized request headers.
  CreateSpdyHeadersFromHttpRequest(*request_info_, request_headers,
                                   &request_headers_);

  // Store the request body.
  request_body_stream_ = request_info_->upload_data_stream;
  if (request_body_stream_) {
    // A request with a body is ineligible for push, so reset the
    // promised stream and request a new stream.
    if (found_promise_) {
      std::string url(request_info_->url.spec());
      quic::QuicClientPromisedInfo* promised =
          quic_session()->GetPushPromiseIndex()->GetPromised(url);
      if (promised != nullptr) {
        quic_session()->ResetPromised(promised->id(),
                                      quic::QUIC_STREAM_CANCELLED);
      }
    }

    // TODO(rch): Can we be more precise about when to allocate
    // raw_request_body_buf_. Removed the following check. DoReadRequestBody()
    // was being called even if we didn't yet allocate raw_request_body_buf_.
    //   && (request_body_stream_->size() ||
    //       request_body_stream_->is_chunked()))
    // Set the body buffer size to be the size of the body clamped
    // into the range [10 * quic::kMaxOutgoingPacketSize, 256 *
    // quic::kMaxOutgoingPacketSize]. With larger bodies, larger buffers reduce
    // CPU usage.
    raw_request_body_buf_ =
        base::MakeRefCounted<IOBufferWithSize>(static_cast<size_t>(
            std::max(10 * quic::kMaxOutgoingPacketSize,
                     std::min(request_body_stream_->size(),
                              256 * quic::kMaxOutgoingPacketSize))));
    // The request body buffer is empty at first.
    request_body_buf_ =
        base::MakeRefCounted<DrainableIOBuffer>(raw_request_body_buf_, 0);
  }

  // Store the response info.
  response_info_ = response;

  int rv;

  if (!found_promise_) {
    next_state_ = STATE_SET_REQUEST_PRIORITY;
  } else if (!request_body_stream_) {
    next_state_ = STATE_HANDLE_PROMISE;
  } else {
    found_promise_ = false;
    next_state_ = STATE_REQUEST_STREAM;
  }
  rv = DoLoop(OK);

  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);

  return rv > 0 ? OK : MapStreamError(rv);
}

int QuicHttpStream::ReadResponseHeaders(CompletionOnceCallback callback) {
  CHECK(callback_.is_null());
  CHECK(!callback.is_null());

  int rv = stream_->ReadInitialHeaders(
      &response_header_block_,
      base::BindOnce(&QuicHttpStream::OnReadResponseHeadersComplete,
                     weak_factory_.GetWeakPtr()));

  if (rv == ERR_IO_PENDING) {
    // Still waiting for the response, return IO_PENDING.
    CHECK(callback_.is_null());
    callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  if (rv < 0)
    return MapStreamError(rv);

  // Check if we already have the response headers. If so, return synchronously.
  if (response_headers_received_)
    return OK;

  headers_bytes_received_ += rv;
  return ProcessResponseHeaders(response_header_block_);
}

int QuicHttpStream::ReadResponseBody(IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  CHECK(callback_.is_null());
  CHECK(!callback.is_null());
  CHECK(!user_buffer_.get());
  CHECK_EQ(0, user_buffer_len_);

  // Invalidate HttpRequestInfo pointer. This is to allow the stream to be
  // shared across multiple transactions which might require this
  // stream to outlive the request_info_'s owner.
  // Only allowed when Read state machine starts. It is safe to reset it at
  // this point since request_info_->upload_data_stream is also not needed
  // anymore.
  request_info_ = nullptr;

  // If the stream is already closed, there is no body to read.
  if (stream_->IsDoneReading())
    return HandleReadComplete(OK);

  int rv = stream_->ReadBody(buf, buf_len,
                             base::BindOnce(&QuicHttpStream::OnReadBodyComplete,
                                            weak_factory_.GetWeakPtr()));
  if (rv == ERR_IO_PENDING) {
    callback_ = std::move(callback);
    user_buffer_ = buf;
    user_buffer_len_ = buf_len;
    return ERR_IO_PENDING;
  }

  if (rv < 0)
    return MapStreamError(rv);

  return HandleReadComplete(rv);
}

void QuicHttpStream::Close(bool /*not_reusable*/) {
  session_error_ = ERR_ABORTED;
  SaveResponseStatus();
  // Note: the not_reusable flag has no meaning for QUIC streams.
  if (stream_)
    stream_->Reset(quic::QUIC_STREAM_CANCELLED);
  ResetStream();
}

bool QuicHttpStream::IsResponseBodyComplete() const {
  return next_state_ == STATE_OPEN && stream_->IsDoneReading();
}

bool QuicHttpStream::IsConnectionReused() const {
  // TODO(rch): do something smarter here.
  return stream_ && stream_->id() > 1;
}

int64_t QuicHttpStream::GetTotalReceivedBytes() const {
  // When QPACK is enabled, headers are sent and received on the stream, so
  // the headers bytes do not need to be accounted for independently.
  int64_t total_received_bytes =
      quic::VersionUsesHttp3(quic_session()->GetQuicVersion().transport_version)
          ? 0
          : headers_bytes_received_;
  if (stream_) {
    DCHECK_LE(stream_->NumBytesConsumed(), stream_->stream_bytes_read());
    // Only count the uniquely received bytes.
    total_received_bytes += stream_->NumBytesConsumed();
  } else {
    total_received_bytes += closed_stream_received_bytes_;
  }
  return total_received_bytes;
}

int64_t QuicHttpStream::GetTotalSentBytes() const {
  // When QPACK is enabled, headers are sent and received on the stream, so
  // the headers bytes do not need to be accounted for independently.
  int64_t total_sent_bytes =
      quic::VersionUsesHttp3(quic_session()->GetQuicVersion().transport_version)
          ? 0
          : headers_bytes_sent_;
  if (stream_) {
    total_sent_bytes += stream_->stream_bytes_written();
  } else {
    total_sent_bytes += closed_stream_sent_bytes_;
  }
  return total_sent_bytes;
}

bool QuicHttpStream::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  bool is_first_stream = closed_is_first_stream_;
  if (stream_)
    is_first_stream = stream_->IsFirstStream();
  if (is_first_stream) {
    load_timing_info->socket_reused = false;
    load_timing_info->connect_timing = connect_timing_;
  } else {
    load_timing_info->socket_reused = true;
  }
  return true;
}

bool QuicHttpStream::GetAlternativeService(
    AlternativeService* alternative_service) const {
  alternative_service->protocol = kProtoQUIC;
  const HostPortPair& destination = quic_session()->destination();
  alternative_service->host = destination.host();
  alternative_service->port = destination.port();
  return true;
}

void QuicHttpStream::PopulateNetErrorDetails(NetErrorDetails* details) {
  details->connection_info =
      ConnectionInfoFromQuicVersion(quic_session()->GetQuicVersion());
  quic_session()->PopulateNetErrorDetails(details);
  if (quic_session()->IsCryptoHandshakeConfirmed() && stream_ &&
      stream_->connection_error() != quic::QUIC_NO_ERROR)
    details->quic_connection_error = stream_->connection_error();
}

void QuicHttpStream::SetPriority(RequestPriority priority) {
  priority_ = priority;
}

void QuicHttpStream::OnReadResponseHeadersComplete(int rv) {
  DCHECK(callback_);
  DCHECK(!response_headers_received_);
  if (rv > 0) {
    headers_bytes_received_ += rv;
    rv = ProcessResponseHeaders(response_header_block_);
  }
  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    DoCallback(rv);
  }
}

void QuicHttpStream::ReadTrailingHeaders() {
  int rv = stream_->ReadTrailingHeaders(
      &trailing_header_block_,
      base::BindOnce(&QuicHttpStream::OnReadTrailingHeadersComplete,
                     weak_factory_.GetWeakPtr()));

  if (rv != ERR_IO_PENDING)
    OnReadTrailingHeadersComplete(rv);
}

void QuicHttpStream::OnReadTrailingHeadersComplete(int rv) {
  DCHECK(response_headers_received_);
  if (rv > 0)
    headers_bytes_received_ += rv;

  // QuicHttpStream ignores trailers.
  if (stream_->IsDoneReading()) {
    // Close the read side. If the write side has been closed, this will
    // invoke QuicHttpStream::OnClose to reset the stream.
    stream_->OnFinRead();
    SetResponseStatus(OK);
  }
}

void QuicHttpStream::OnIOComplete(int rv) {
  rv = DoLoop(rv);

  if (rv != ERR_IO_PENDING && !callback_.is_null()) {
    DoCallback(rv);
  }
}

void QuicHttpStream::DoCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(!callback_.is_null());
  CHECK(!in_loop_);

  // The client callback can do anything, including destroying this class,
  // so any pending callback must be issued after everything else is done.
  std::move(callback_).Run(MapStreamError(rv));
}

int QuicHttpStream::DoLoop(int rv) {
  CHECK(!in_loop_);
  base::AutoReset<bool> auto_reset_in_loop(&in_loop_, true);
  std::unique_ptr<quic::QuicConnection::ScopedPacketFlusher> packet_flusher =
      quic_session()->CreatePacketBundler();
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_HANDLE_PROMISE:
        CHECK_EQ(OK, rv);
        rv = DoHandlePromise();
        break;
      case STATE_HANDLE_PROMISE_COMPLETE:
        rv = DoHandlePromiseComplete(rv);
        break;
      case STATE_REQUEST_STREAM:
        CHECK_EQ(OK, rv);
        rv = DoRequestStream();
        break;
      case STATE_REQUEST_STREAM_COMPLETE:
        rv = DoRequestStreamComplete(rv);
        break;
      case STATE_SET_REQUEST_PRIORITY:
        CHECK_EQ(OK, rv);
        rv = DoSetRequestPriority();
        break;
      case STATE_SEND_HEADERS:
        CHECK_EQ(OK, rv);
        rv = DoSendHeaders();
        break;
      case STATE_SEND_HEADERS_COMPLETE:
        rv = DoSendHeadersComplete(rv);
        break;
      case STATE_READ_REQUEST_BODY:
        CHECK_EQ(OK, rv);
        rv = DoReadRequestBody();
        break;
      case STATE_READ_REQUEST_BODY_COMPLETE:
        rv = DoReadRequestBodyComplete(rv);
        break;
      case STATE_SEND_BODY:
        CHECK_EQ(OK, rv);
        rv = DoSendBody();
        break;
      case STATE_SEND_BODY_COMPLETE:
        rv = DoSendBodyComplete(rv);
        break;
      case STATE_OPEN:
        CHECK_EQ(OK, rv);
        break;
      default:
        NOTREACHED() << "next_state_: " << next_state_;
        break;
    }
  } while (next_state_ != STATE_NONE && next_state_ != STATE_OPEN &&
           rv != ERR_IO_PENDING);

  return rv;
}

int QuicHttpStream::DoRequestStream() {
  next_state_ = STATE_REQUEST_STREAM_COMPLETE;

  return quic_session()->RequestStream(
      !can_send_early_,
      base::BindOnce(&QuicHttpStream::OnIOComplete, weak_factory_.GetWeakPtr()),
      NetworkTrafficAnnotationTag(request_info_->traffic_annotation));
}

int QuicHttpStream::DoRequestStreamComplete(int rv) {
  DCHECK(rv == OK || !stream_);
  if (rv != OK) {
    session_error_ = rv;
    return GetResponseStatus();
  }

  stream_ = quic_session()->ReleaseStream();
  DCHECK(stream_);
  if (!stream_->IsOpen()) {
    session_error_ = ERR_CONNECTION_CLOSED;
    return GetResponseStatus();
  }

  if (request_info_->load_flags &
      LOAD_DISABLE_CONNECTION_MIGRATION_TO_CELLULAR) {
    stream_->DisableConnectionMigrationToCellularNetwork();
  }

  if (response_info_) {
    // This happens in the case of a asynchronous push rendezvous
    // that ultimately fails (e.g. vary failure).  |response_info_|
    // non-null implies that |DoRequestStream()| was called via
    // |SendRequest()|.
    next_state_ = STATE_SET_REQUEST_PRIORITY;
  }

  return OK;
}

int QuicHttpStream::DoSetRequestPriority() {
  // Set priority according to request
  DCHECK(stream_);
  DCHECK(response_info_);

  spdy::SpdyPriority priority = ConvertRequestPriorityToQuicPriority(priority_);
  spdy::SpdyStreamPrecedence precedence(priority);
  stream_->SetPriority(precedence);
  next_state_ = STATE_SEND_HEADERS;
  return OK;
}

int QuicHttpStream::DoSendHeaders() {
  // Log the actual request with the URL Request's net log.
  stream_net_log_.AddEvent(
      NetLogEventType::HTTP_TRANSACTION_QUIC_SEND_REQUEST_HEADERS,
      [&](NetLogCaptureMode capture_mode) {
        return QuicRequestNetLogParams(stream_->id(), &request_headers_,
                                       priority_, capture_mode);
      });
  DispatchRequestHeadersCallback(request_headers_);
  bool has_upload_data = request_body_stream_ != nullptr;

  next_state_ = STATE_SEND_HEADERS_COMPLETE;
  int rv = stream_->WriteHeaders(std::move(request_headers_), !has_upload_data,
                                 nullptr);
  if (rv > 0)
    headers_bytes_sent_ += rv;

  request_headers_ = spdy::SpdyHeaderBlock();
  return rv;
}

int QuicHttpStream::DoSendHeadersComplete(int rv) {
  if (rv < 0)
    return rv;

  next_state_ = request_body_stream_ ? STATE_READ_REQUEST_BODY : STATE_OPEN;

  return OK;
}

int QuicHttpStream::DoReadRequestBody() {
  next_state_ = STATE_READ_REQUEST_BODY_COMPLETE;
  return request_body_stream_->Read(
      raw_request_body_buf_.get(), raw_request_body_buf_->size(),
      base::BindOnce(&QuicHttpStream::OnIOComplete,
                     weak_factory_.GetWeakPtr()));
}

int QuicHttpStream::DoReadRequestBodyComplete(int rv) {
  // |rv| is the result of read from the request body from the last call to
  // DoSendBody().
  if (rv < 0) {
    stream_->Reset(quic::QUIC_ERROR_PROCESSING_STREAM);
    ResetStream();
    return rv;
  }

  request_body_buf_ =
      base::MakeRefCounted<DrainableIOBuffer>(raw_request_body_buf_, rv);
  if (rv == 0) {  // Reached the end.
    DCHECK(request_body_stream_->IsEOF());
  }

  next_state_ = STATE_SEND_BODY;
  return OK;
}

int QuicHttpStream::DoSendBody() {
  CHECK(request_body_stream_);
  CHECK(request_body_buf_.get());
  const bool eof = request_body_stream_->IsEOF();
  int len = request_body_buf_->BytesRemaining();
  if (len > 0 || eof) {
    next_state_ = STATE_SEND_BODY_COMPLETE;
    quic::QuicStringPiece data(request_body_buf_->data(), len);
    return stream_->WriteStreamData(
        data, eof,
        base::BindOnce(&QuicHttpStream::OnIOComplete,
                       weak_factory_.GetWeakPtr()));
  }

  next_state_ = STATE_OPEN;
  return OK;
}

int QuicHttpStream::DoSendBodyComplete(int rv) {
  if (rv < 0)
    return rv;

  request_body_buf_->DidConsume(request_body_buf_->BytesRemaining());

  if (!request_body_stream_->IsEOF()) {
    next_state_ = STATE_READ_REQUEST_BODY;
    return OK;
  }

  next_state_ = STATE_OPEN;
  return OK;
}

int QuicHttpStream::ProcessResponseHeaders(
    const spdy::SpdyHeaderBlock& headers) {
  if (!SpdyHeadersToHttpResponse(headers, response_info_)) {
    DLOG(WARNING) << "Invalid headers";
    return ERR_QUIC_PROTOCOL_ERROR;
  }
  // Put the peer's IP address and port into the response.
  IPEndPoint address;
  int rv = quic_session()->GetPeerAddress(&address);
  if (rv != OK)
    return rv;

  response_info_->remote_endpoint = address;
  response_info_->connection_info =
      ConnectionInfoFromQuicVersion(quic_session()->GetQuicVersion());
  response_info_->vary_data.Init(*request_info_,
                                 *response_info_->headers.get());
  response_info_->was_alpn_negotiated = true;
  response_info_->alpn_negotiated_protocol =
      HttpResponseInfo::ConnectionInfoToString(response_info_->connection_info);
  response_info_->response_time = base::Time::Now();
  response_info_->request_time = request_time_;
  response_headers_received_ = true;

  // Populate |connect_timing_| when response headers are received. This should
  // take care of 0-RTT where request is sent before handshake is confirmed.
  connect_timing_ = quic_session()->GetConnectTiming();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&QuicHttpStream::ReadTrailingHeaders,
                                weak_factory_.GetWeakPtr()));

  if (stream_->IsDoneReading()) {
    session_error_ = OK;
    SaveResponseStatus();
    stream_->OnFinRead();
  }

  return OK;
}

void QuicHttpStream::OnReadBodyComplete(int rv) {
  CHECK(callback_);
  user_buffer_ = nullptr;
  user_buffer_len_ = 0;
  rv = HandleReadComplete(rv);
  DoCallback(rv);
}

int QuicHttpStream::HandleReadComplete(int rv) {
  if (stream_->IsDoneReading()) {
    stream_->OnFinRead();
    SetResponseStatus(OK);
    ResetStream();
  }
  return rv;
}

void QuicHttpStream::ResetStream() {
  // If |request_body_stream_| is non-NULL, Reset it, to abort any in progress
  // read.
  if (request_body_stream_)
    request_body_stream_->Reset();

  if (!stream_)
    return;

  DCHECK_LE(stream_->NumBytesConsumed(), stream_->stream_bytes_read());
  // Only count the uniquely received bytes.
  closed_stream_received_bytes_ = stream_->NumBytesConsumed();
  closed_stream_sent_bytes_ = stream_->stream_bytes_written();
  closed_is_first_stream_ = stream_->IsFirstStream();
}

int QuicHttpStream::MapStreamError(int rv) {
  if (rv == ERR_QUIC_PROTOCOL_ERROR &&
      !quic_session()->IsCryptoHandshakeConfirmed()) {
    return ERR_QUIC_HANDSHAKE_FAILED;
  }
  return rv;
}

int QuicHttpStream::GetResponseStatus() {
  SaveResponseStatus();
  return response_status_;
}

void QuicHttpStream::SaveResponseStatus() {
  if (!has_response_status_)
    SetResponseStatus(ComputeResponseStatus());
}

void QuicHttpStream::SetResponseStatus(int response_status) {
  has_response_status_ = true;
  response_status_ = response_status;
}

int QuicHttpStream::ComputeResponseStatus() const {
  DCHECK(!has_response_status_);

  // If the handshake has failed this will be handled by the QuicStreamFactory
  // and HttpStreamFactory to mark QUIC as broken if TCP is actually working.
  if (!quic_session()->IsCryptoHandshakeConfirmed())
    return ERR_QUIC_HANDSHAKE_FAILED;

  // If the session was aborted by a higher layer, simply use that error code.
  if (session_error_ != ERR_UNEXPECTED)
    return session_error_;

  // If |response_info_| is null then the request has not been sent, so
  // return ERR_CONNECTION_CLOSED to permit HttpNetworkTransaction to
  // retry the request.
  if (!response_info_)
    return ERR_CONNECTION_CLOSED;

  // Explicit stream error are always fatal.
  if (stream_->stream_error() != quic::QUIC_STREAM_NO_ERROR &&
      stream_->stream_error() != quic::QUIC_STREAM_CONNECTION_ERROR) {
    return ERR_QUIC_PROTOCOL_ERROR;
  }

  return ERR_QUIC_PROTOCOL_ERROR;
}

}  // namespace net
