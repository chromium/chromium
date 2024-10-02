// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"

#include <algorithm>
#include <list>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/next_proto.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "url/scheme_host_port.h"

namespace net {

// Align our request body with |kMaxSpdyFrameChunkSize| to prevent unexpected
// buffer chunking. This is 16KB - frame header size.
const size_t SpdyHttpStream::kRequestBodyBufferSize = kMaxSpdyFrameChunkSize;

SpdyHttpStream::SpdyHttpStream(const base::WeakPtr<SpdySession>& spdy_session,
                               NetLogSource source_dependency,
                               std::set<std::string> dns_aliases)
    : MultiplexedHttpStream(
          std::make_unique<MultiplexedSessionHandle>(spdy_session)),
      spdy_session_(spdy_session),
      is_reused_(spdy_session_->IsReused()),
      source_dependency_(source_dependency),
      dns_aliases_(std::move(dns_aliases)) {
  DCHECK(spdy_session_.get());
}

SpdyHttpStream::~SpdyHttpStream() {
  if (stream_) {
    stream_->DetachDelegate();
    DCHECK(!stream_);
  }
}

void SpdyHttpStream::RegisterRequest(const HttpRequestInfo* request_info) {
  DCHECK(request_info);
  request_info_ = request_info;
}

int SpdyHttpStream::InitializeStream(bool can_send_early,
                                     RequestPriority priority,
                                     const NetLogWithSource& stream_net_log,
                                     CompletionOnceCallback callback) {
  DCHECK(!stream_);
  DCHECK(request_info_);
  if (!spdy_session_)
    return ERR_CONNECTION_CLOSED;

  priority_ = priority;
  int rv = stream_request_.StartRequest(
      SPDY_REQUEST_RESPONSE_STREAM, spdy_session_, request_info_->url,
      can_send_early, priority, request_info_->socket_tag, stream_net_log,
      base::BindOnce(&SpdyHttpStream::OnStreamCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      NetworkTrafficAnnotationTag{request_info_->traffic_annotation});

  if (rv == OK) {
    stream_ = stream_request_.ReleaseStream().get();
    InitializeStreamHelper();
  }

  return rv;
}

int SpdyHttpStream::ReadResponseHeaders(CompletionOnceCallback callback) {
  CHECK(!callback.is_null());
  if (stream_closed_)
    return closed_stream_status_;

  CHECK(stream_);

  // Check if we already have the response headers. If so, return synchronously.
  if (response_headers_complete_) {
    CHECK(!stream_->IsIdle());
    return OK;
  }

  // Still waiting for the response, return IO_PENDING.
  CHECK(response_callback_.is_null());
  response_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int SpdyHttpStream::ReadResponseBody(IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  if (stream_)
    CHECK(!stream_->IsIdle());

  CHECK(buf);
  CHECK(buf_len);
  CHECK(!callback.is_null());

  // If we have data buffered, complete the IO immediately.
  if (!response_body_queue_.IsEmpty()) {
    return response_body_queue_.Dequeue(buf->data(), buf_len);
  } else if (stream_closed_) {
    return closed_stream_status_;
  }

  CHECK(response_callback_.is_null());
  CHECK(!user_buffer_.get());
  CHECK_EQ(0, user_buffer_len_);

  response_callback_ = std::move(callback);
  user_buffer_ = buf;
  user_buffer_len_ = buf_len;
  return ERR_IO_PENDING;
}

void SpdyHttpStream::Close(bool not_reusable) {
  // Note: the not_reusable flag has no meaning for SPDY streams.

  Cancel();
  DCHECK(!stream_);
}

bool SpdyHttpStream::IsResponseBodyComplete() const {
  return stream_closed_;
}

bool SpdyHttpStream::IsConnectionReused() const {
  return is_reused_;
}

int64_t SpdyHttpStream::GetTotalReceivedBytes() const {
  if (stream_closed_)
    return closed_stream_received_bytes_;

  if (!stream_)
    return 0;

  return stream_->raw_received_bytes();
}

int64_t SpdyHttpStream::GetTotalSentBytes() const {
  if (stream_closed_)
    return closed_stream_sent_bytes_;

  if (!stream_)
    return 0;

  return stream_->raw_sent_bytes();
}

bool SpdyHttpStream::GetAlternativeService(
    AlternativeService* alternative_service) const {
  return false;
}

bool SpdyHttpStream::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  if (stream_closed_) {
    if (!closed_stream_has_load_timing_info_)
      return false;
    *load_timing_info = closed_stream_load_timing_info_;
  } else {
    // If |stream_| has yet to be created, or does not yet have an ID, fail.
    // The reused flag can only be correctly set once a stream has an ID.
    // Streams get their IDs once the request has been successfully sent, so
    // this does not behave that differently from other stream types.
    if (!stream_ || stream_->stream_id() == 0)
      return false;

    if (!stream_->GetLoadTimingInfo(load_timing_info))
      return false;
  }

  // If the request waited for handshake confirmation, shift |ssl_end| to
  // include that time.
  if (!load_timing_info->connect_timing.ssl_end.is_null() &&
      !stream_request_.confirm_handshake_end().is_null()) {
    load_timing_info->connect_timing.ssl_end =
        stream_request_.confirm_handshake_end();
    load_timing_info->connect_timing.connect_end =
        stream_request_.confirm_handshake_end();
  }

  return true;
}

int SpdyHttpStream::SendRequest(const HttpRequestHeaders& request_headers,
                                HttpResponseInfo* response,
                                CompletionOnceCallback callback) {
  if (stream_closed_) {
    return closed_stream_status_;
  }

  base::Time request_time = base::Time::Now();
  CHECK(stream_);

  stream_->SetRequestTime(request_time);
  // This should only get called in the case of a request occurring
  // during server push that has already begun but hasn't finished,
  // so we set the response's request time to be the actual one
  if (response_info_)
    response_info_->request_time = request_time;

  CHECK(!request_body_buf_.get());
  if (HasUploadData()) {
    request_body_buf_ =
        base::MakeRefCounted<IOBufferWithSize>(kRequestBodyBufferSize);
    // The request body buffer is empty at first.
    request_body_buf_size_ = 0;
  }

  CHECK(!callback.is_null());
  CHECK(response);
  DCHECK(!response_info_);

  response_info_ = response;

  // Put the peer's IP address and port into the response.
  IPEndPoint address;
  int result = stream_->GetPeerAddress(&address);
  if (result != OK)
    return result;
  response_info_->remote_endpoint = address;

  quiche::HttpHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(*request_info_, priority_, request_headers,
                                   &headers);
  DispatchRequestHeadersCallback(headers);

  bool will_send_data =
      HasUploadData() || spdy_session_->EndStreamWithDataFrame();
  result = stream_->SendRequestHeaders(
      std::move(headers),
      will_send_data ? MORE_DATA_TO_SEND : NO_MORE_DATA_TO_SEND);

  if (result == ERR_IO_PENDING) {
    CHECK(request_callback_.is_null());
    request_callback_ = std::move(callback);
  }
  return result;
}

void SpdyHttpStream::Cancel() {
  request_callback_.Reset();
  response_callback_.Reset();
  if (stream_) {
    stream_->Cancel(ERR_ABORTED);
    DCHECK(!stream_);
  }
}

void SpdyHttpStream::OnHeadersSent() {
  if (HasUploadData()) {
    ReadAndSendRequestBodyData();
  } else if (spdy_session_->EndStreamWithDataFrame()) {
    SendEmptyBody();
  } else {
    MaybePostRequestCallback(OK);
  }
}

void SpdyHttpStream::OnEarlyHintsReceived(
    const quiche::HttpHeaderBlock& headers) {
  DCHECK(!response_headers_complete_);
  DCHECK(response_info_);
  DCHECK_EQ(stream_->type(), SPDY_REQUEST_RESPONSE_STREAM);

  const int rv = SpdyHeadersToHttpResponse(headers, response_info_);
  CHECK_NE(rv, ERR_INCOMPLETE_HTTP2_HEADERS);

  if (!response_callback_.is_null()) {
    DoResponseCallback(OK);
  }
}

void SpdyHttpStream::OnHeadersReceived(
    const quiche::HttpHeaderBlock& response_headers) {
  DCHECK(!response_headers_complete_);
  DCHECK(response_info_);
  response_headers_complete_ = true;

  const int rv = SpdyHeadersToHttpResponse(response_headers, response_info_);
  DCHECK_NE(rv, ERR_INCOMPLETE_HTTP2_HEADERS);

  if (rv == ERR_RESPONSE_HEADERS_MULTIPLE_LOCATION) {
    // Cancel will call OnClose, which might call callbacks and might destroy
    // `this`.
    stream_->Cancel(rv);
    return;
  }

  response_info_->response_time = stream_->response_time();
  // Don't store the SSLInfo in the response here, HttpNetworkTransaction
  // will take care of that part.
  CHECK_EQ(stream_->GetNegotiatedProtocol(), kProtoHTTP2);
  response_info_->was_alpn_negotiated = true;
  response_info_->request_time = stream_->GetRequestTime();
  response_info_->connection_info = HttpConnectionInfo::kHTTP2;
  response_info_->alpn_negotiated_protocol =
      HttpConnectionInfoToString(response_info_->connection_info);

  // Invalidate HttpRequestInfo pointer. This is to allow |this| to be
  // shared across multiple consumers at the cache layer which might require
  // this stream to outlive the request_info_'s owner.
  if (!upload_stream_in_progress_)
    request_info_ = nullptr;

  if (!response_callback_.is_null()) {
    DoResponseCallback(OK);
  }
}

void SpdyHttpStream::OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) {
  DCHECK(response_headers_complete_);

  // Note that data may be received for a SpdyStream prior to the user calling
  // ReadResponseBody(), therefore user_buffer_ may be NULL.  This may often
  // happen for server initiated streams.
  DCHECK(stream_);
  DCHECK(!stream_->IsClosed());
  if (buffer) {
    response_body_queue_.Enqueue(std::move(buffer));
    MaybeScheduleBufferedReadCallback();
  }
}

void SpdyHttpStream::OnDataSent() {
  if (request_info_ && HasUploadData()) {
    request_body_buf_size_ = 0;
    ReadAndSendRequestBodyData();
  } else {
    CHECK(spdy_session_->EndStreamWithDataFrame());
    MaybePostRequestCallback(OK);
  }
}

// TODO(xunjieli): Maybe do something with the trailers. crbug.com/422958.
void SpdyHttpStream::OnTrailers(const quiche::HttpHeaderBlock& trailers) {}

void SpdyHttpStream::OnClose(int status) {
  DCHECK(stream_);

  // Cancel any pending reads from the upload data stream.
  if (request_info_ && request_info_->upload_data_stream)
    request_info_->upload_data_stream->Reset();

  stream_closed_ = true;
  closed_stream_status_ = status;
  closed_stream_id_ = stream_->stream_id();
  closed_stream_has_load_timing_info_ =
      stream_->GetLoadTimingInfo(&closed_stream_load_timing_info_);
  closed_stream_received_bytes_ = stream_->raw_received_bytes();
  closed_stream_sent_bytes_ = stream_->raw_sent_bytes();
  stream_ = nullptr;

  // Callbacks might destroy |this|.
  base::WeakPtr<SpdyHttpStream> self = weak_factory_.GetWeakPtr();

  if (!request_callback_.is_null()) {
    DoRequestCallback(status);
    if (!self)
      return;
  }

  if (status == OK) {
    // We need to complete any pending buffered read now.
    DoBufferedReadCallback();
    if (!self)
      return;
  }

  if (!response_callback_.is_null()) {
    DoResponseCallback(status);
  }
}

bool SpdyHttpStream::CanGreaseFrameType() const {
  return true;
}

NetLogSource SpdyHttpStream::source_dependency() const {
  return source_dependency_;
}

bool SpdyHttpStream::HasUploadData() const {
  CHECK(request_info_);
  return
      request_info_->upload_data_stream &&
      ((request_info_->upload_data_stream->size() > 0) ||
       request_info_->upload_data_stream->is_chunked());
}

void SpdyHttpStream::OnStreamCreated(CompletionOnceCallback callback, int rv) {
  if (rv == OK) {
    stream_ = stream_request_.ReleaseStream().get();
    InitializeStreamHelper();
  }
  std::move(callback).Run(rv);
}

void SpdyHttpStream::ReadAndSendRequestBodyData() {
  CHECK(HasUploadData());
  upload_stream_in_progress_ = true;

  CHECK_EQ(request_body_buf_size_, 0);
  if (request_info_->upload_data_stream->IsEOF()) {
    MaybePostRequestCallback(OK);

    // Invalidate HttpRequestInfo pointer. This is to allow |this| to be
    // shared across multiple consumers at the cache layer which might require
    // this stream to outlive the request_info_'s owner.
    upload_stream_in_progress_ = false;
    if (response_headers_complete_)
      request_info_ = nullptr;
    return;
  }

  // Read the data from the request body stream.
  const int rv = request_info_->upload_data_stream->Read(
      request_body_buf_.get(), request_body_buf_->size(),
      base::BindOnce(&SpdyHttpStream::OnRequestBodyReadCompleted,
                     weak_factory_.GetWeakPtr()));

  if (rv != ERR_IO_PENDING)
    OnRequestBodyReadCompleted(rv);
}

void SpdyHttpStream::SendEmptyBody() {
  CHECK(!HasUploadData());
  CHECK(spdy_session_->EndStreamWithDataFrame());

  auto buffer = base::MakeRefCounted<IOBufferWithSize>(/* buffer_size = */ 0);
  stream_->SendData(buffer.get(), /* length = */ 0, NO_MORE_DATA_TO_SEND);
}

void SpdyHttpStream::InitializeStreamHelper() {
  stream_->SetDelegate(this);
}

void SpdyHttpStream::ResetStream(int error) {
  spdy_session_->ResetStream(stream()->stream_id(), error, std::string());
}

void SpdyHttpStream::OnRequestBodyReadCompleted(int status) {
  if (status < 0) {
    DCHECK_NE(ERR_IO_PENDING, status);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SpdyHttpStream::ResetStream,
                                  weak_factory_.GetWeakPtr(), status));

    return;
  }

  CHECK_GE(status, 0);
  request_body_buf_size_ = status;
  const bool eof = request_info_->upload_data_stream->IsEOF();
  // Only the final frame may have a length of 0.
  if (eof) {
    CHECK_GE(request_body_buf_size_, 0);
  } else {
    CHECK_GT(request_body_buf_size_, 0);
  }
  stream_->SendData(request_body_buf_.get(),
                    request_body_buf_size_,
                    eof ? NO_MORE_DATA_TO_SEND : MORE_DATA_TO_SEND);
}

void SpdyHttpStream::MaybeScheduleBufferedReadCallback() {
  DCHECK(!stream_closed_);

  if (!user_buffer_.get())
    return;

  // If enough data was received to fill the user buffer, invoke
  // DoBufferedReadCallback() with no delay.
  //
  // Note: DoBufferedReadCallback() is invoked asynchronously to preserve
  // historical behavior. It would be interesting to evaluate whether it can be
  // invoked synchronously to avoid the overhead of posting a task. A long time
  // ago, the callback was invoked synchronously
  // https://codereview.chromium.org/652209/diff/2018/net/spdy/spdy_stream.cc.
  if (response_body_queue_.GetTotalSize() >=
      static_cast<size_t>(user_buffer_len_)) {
    buffered_read_timer_.Start(FROM_HERE, base::TimeDelta() /* no delay */,
                               this, &SpdyHttpStream::DoBufferedReadCallback);
    return;
  }

  // Handing small chunks of data to the caller creates measurable overhead.
  // Wait 1ms to allow handing off multiple chunks of data received within a
  // short time span at once.
  buffered_read_timer_.Start(FROM_HERE, base::Milliseconds(1), this,
                             &SpdyHttpStream::DoBufferedReadCallback);
}

void SpdyHttpStream::DoBufferedReadCallback() {
  buffered_read_timer_.Stop();

  // If the transaction is cancelled or errored out, we don't need to complete
  // the read.
  if (stream_closed_ && closed_stream_status_ != OK) {
    if (response_callback_)
      DoResponseCallback(closed_stream_status_);
    return;
  }

  if (!user_buffer_.get())
    return;

  if (!response_body_queue_.IsEmpty()) {
    int rv =
        response_body_queue_.Dequeue(user_buffer_->data(), user_buffer_len_);
    user_buffer_ = nullptr;
    user_buffer_len_ = 0;
    DoResponseCallback(rv);
    return;
  }

  if (stream_closed_ && response_callback_)
    DoResponseCallback(closed_stream_status_);
}

void SpdyHttpStream::DoRequestCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(!request_callback_.is_null());
  // Since Run may result in being called back, reset request_callback_ in
  // advance.
  std::move(request_callback_).Run(rv);
}

void SpdyHttpStream::MaybeDoRequestCallback(int rv) {
  CHECK_NE(ERR_IO_PENDING, rv);
  if (request_callback_)
    std::move(request_callback_).Run(rv);
}

void SpdyHttpStream::MaybePostRequestCallback(int rv) {
  CHECK_NE(ERR_IO_PENDING, rv);
  if (request_callback_)
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&SpdyHttpStream::MaybeDoRequestCallback,
                                  weak_factory_.GetWeakPtr(), rv));
}

void SpdyHttpStream::DoResponseCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(!response_callback_.is_null());

  // Since Run may result in being called back, reset response_callback_ in
  // advance.
  std::move(response_callback_).Run(rv);
}

int SpdyHttpStream::GetRemoteEndpoint(IPEndPoint* endpoint) {
  if (!spdy_session_)
    return ERR_SOCKET_NOT_CONNECTED;

  return spdy_session_->GetPeerAddress(endpoint);
}

void SpdyHttpStream::PopulateNetErrorDetails(NetErrorDetails* details) {
  details->connection_info = HttpConnectionInfo::kHTTP2;
  return;
}

void SpdyHttpStream::SetPriority(RequestPriority priority) {
  priority_ = priority;
  if (stream_) {
    stream_->SetPriority(priority);
  }
}

const std::set<std::string>& SpdyHttpStream::GetDnsAliases() const {
  return dns_aliases_;
}

std::string_view SpdyHttpStream::GetAcceptChViaAlps() const {
  if (!request_info_) {
    return {};
  }

  return session()->GetAcceptChViaAlps(url::SchemeHostPort(request_info_->url));
}

}  // namespace net
