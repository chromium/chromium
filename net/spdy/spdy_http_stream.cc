// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"

#include <algorithm>
#include <list>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_log_util.h"
#include "net/spdy/spdy_session.h"
#include "net/third_party/spdy/core/spdy_header_block.h"
#include "net/third_party/spdy/core/spdy_protocol.h"

namespace net {

namespace {

bool ValidatePushedHeaders(const HttpRequestInfo& request_info,
                           const spdy::SpdyHeaderBlock& pushed_request_headers,
                           const spdy::SpdyHeaderBlock& pushed_response_headers,
                           const HttpResponseInfo& pushed_response_info) {
  spdy::SpdyHeaderBlock::const_iterator status_it =
      pushed_response_headers.find(spdy::kHttp2StatusHeader);
  DCHECK(status_it != pushed_response_headers.end());
  // 206 Partial Content and 416 Requested Range Not Satisfiable are range
  // responses.
  if (status_it->second == "206" || status_it->second == "416") {
    std::string client_request_range;
    if (!request_info.extra_headers.GetHeader(HttpRequestHeaders::kRange,
                                              &client_request_range)) {
      // Client initiated request is not a range request.
      SpdySession::RecordSpdyPushedStreamFateHistogram(
          SpdyPushedStreamFate::kClientRequestNotRange);
      return false;
    }
    spdy::SpdyHeaderBlock::const_iterator pushed_request_range_it =
        pushed_request_headers.find("range");
    if (pushed_request_range_it == pushed_request_headers.end()) {
      // Pushed request is not a range request.
      SpdySession::RecordSpdyPushedStreamFateHistogram(
          SpdyPushedStreamFate::kPushedRequestNotRange);
      return false;
    }
    if (client_request_range != pushed_request_range_it->second) {
      // Client and pushed request ranges do not match.
      SpdySession::RecordSpdyPushedStreamFateHistogram(
          SpdyPushedStreamFate::kRangeMismatch);
      return false;
    }
  }

  HttpRequestInfo pushed_request_info;
  ConvertHeaderBlockToHttpRequestHeaders(pushed_request_headers,
                                         &pushed_request_info.extra_headers);
  HttpVaryData vary_data;
  if (!vary_data.Init(pushed_request_info,
                      *pushed_response_info.headers.get())) {
    // Pushed response did not contain non-empty Vary header.
    SpdySession::RecordSpdyPushedStreamFateHistogram(
        SpdyPushedStreamFate::kAcceptedNoVary);
    return true;
  }

  if (vary_data.MatchesRequest(request_info,
                               *pushed_response_info.headers.get())) {
    SpdySession::RecordSpdyPushedStreamFateHistogram(
        SpdyPushedStreamFate::kAcceptedMatchingVary);
    return true;
  }

  SpdySession::RecordSpdyPushedStreamFateHistogram(
      SpdyPushedStreamFate::kVaryMismatch);
  return false;
}

}  // anonymous namespace

const size_t SpdyHttpStream::kRequestBodyBufferSize = 1 << 14;  // 16KB

SpdyHttpStream::SpdyHttpStream(const base::WeakPtr<SpdySession>& spdy_session,
                               spdy::SpdyStreamId pushed_stream_id,
                               NetLogSource source_dependency)
    : MultiplexedHttpStream(
          std::make_unique<MultiplexedSessionHandle>(spdy_session)),
      spdy_session_(spdy_session),
      pushed_stream_id_(pushed_stream_id),
      is_reused_(spdy_session_->IsReused()),
      source_dependency_(source_dependency),
      stream_(nullptr),
      stream_closed_(false),
      closed_stream_status_(ERR_FAILED),
      closed_stream_id_(0),
      closed_stream_received_bytes_(0),
      closed_stream_sent_bytes_(0),
      request_info_(NULL),
      response_info_(NULL),
      response_headers_complete_(false),
      upload_stream_in_progress_(false),
      user_buffer_len_(0),
      request_body_buf_size_(0),
      buffered_read_callback_pending_(false),
      more_read_data_pending_(false),
      was_alpn_negotiated_(false),
      weak_factory_(this) {
  DCHECK(spdy_session_.get());
}

SpdyHttpStream::~SpdyHttpStream() {
  if (stream_) {
    stream_->DetachDelegate();
    DCHECK(!stream_);
  }
}

int SpdyHttpStream::InitializeStream(const HttpRequestInfo* request_info,
                                     bool can_send_early,
                                     RequestPriority priority,
                                     const NetLogWithSource& stream_net_log,
                                     CompletionOnceCallback callback) {
  DCHECK(!stream_);
  if (!spdy_session_)
    return ERR_CONNECTION_CLOSED;

  request_info_ = request_info;
  if (pushed_stream_id_ != kNoPushedStreamFound) {
    int error = spdy_session_->GetPushedStream(
        request_info_->url, pushed_stream_id_, priority, &stream_);
    if (error != OK)
      return error;

    // |stream_| may be NULL even if OK was returned.
    if (stream_) {
      DCHECK_EQ(stream_->type(), SPDY_PUSH_STREAM);
      InitializeStreamHelper();
      return OK;
    }
  }

  int rv = stream_request_.StartRequest(
      SPDY_REQUEST_RESPONSE_STREAM, spdy_session_, request_info_->url, priority,
      request_info_->socket_tag, stream_net_log,
      base::BindOnce(&SpdyHttpStream::OnStreamCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      NetworkTrafficAnnotationTag(request_info->traffic_annotation));

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
    return true;
  }

  // If |stream_| has yet to be created, or does not yet have an ID, fail.
  // The reused flag can only be correctly set once a stream has an ID.  Streams
  // get their IDs once the request has been successfully sent, so this does not
  // behave that differently from other stream types.
  if (!stream_ || stream_->stream_id() == 0)
    return false;

  return stream_->GetLoadTimingInfo(load_timing_info);
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

  // SendRequest can be called in two cases.
  //
  // a) A client initiated request. In this case, |response_info_| should be
  //    NULL to start with.
  // b) A client request which matches a response that the server has already
  //    pushed.
  if (push_response_info_.get()) {
    *response = *(push_response_info_.get());
    push_response_info_.reset();
  } else {
    DCHECK_EQ(static_cast<HttpResponseInfo*>(NULL), response_info_);
  }

  response_info_ = response;

  // Put the peer's IP address and port into the response.
  IPEndPoint address;
  int result = stream_->GetPeerAddress(&address);
  if (result != OK)
    return result;
  response_info_->socket_address = HostPortPair::FromIPEndPoint(address);

  if (stream_->type() == SPDY_PUSH_STREAM) {
    // Pushed streams do not send any data, and should always be
    // idle. However, we still want to return ERR_IO_PENDING to mimic
    // non-push behavior. The callback will be called when the
    // response is received.
    CHECK(response_callback_.is_null());
    response_callback_ = std::move(callback);
    return ERR_IO_PENDING;
  }

  spdy::SpdyHeaderBlock headers;
  CreateSpdyHeadersFromHttpRequest(*request_info_, request_headers, &headers);
  stream_->net_log().AddEvent(
      NetLogEventType::HTTP_TRANSACTION_HTTP2_SEND_REQUEST_HEADERS,
      base::Bind(&SpdyHeaderBlockNetLogCallback, &headers));
  DispatchRequestHeadersCallback(headers);
  result = stream_->SendRequestHeaders(
      std::move(headers),
      HasUploadData() ? MORE_DATA_TO_SEND : NO_MORE_DATA_TO_SEND);

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
  } else {
    MaybePostRequestCallback(OK);
  }
}

void SpdyHttpStream::OnHeadersReceived(
    const spdy::SpdyHeaderBlock& response_headers,
    const spdy::SpdyHeaderBlock* pushed_request_headers) {
  DCHECK(!response_headers_complete_);
  response_headers_complete_ = true;

  if (!response_info_) {
    DCHECK_EQ(stream_->type(), SPDY_PUSH_STREAM);
    push_response_info_ = std::make_unique<HttpResponseInfo>();
    response_info_ = push_response_info_.get();
  }

  const bool headers_valid =
      SpdyHeadersToHttpResponse(response_headers, response_info_);
  DCHECK(headers_valid);

  if (pushed_request_headers &&
      !ValidatePushedHeaders(*request_info_, *pushed_request_headers,
                             response_headers, *response_info_)) {
    // Cancel will call OnClose, which might call callbacks and might destroy
    // |this|.
    stream_->Cancel(ERR_SPDY_PUSHED_RESPONSE_DOES_NOT_MATCH);

    return;
  }

  response_info_->response_time = stream_->response_time();
  // Don't store the SSLInfo in the response here, HttpNetworkTransaction
  // will take care of that part.
  response_info_->was_alpn_negotiated = was_alpn_negotiated_;
  response_info_->request_time = stream_->GetRequestTime();
  response_info_->connection_info = HttpResponseInfo::CONNECTION_INFO_HTTP2;
  response_info_->alpn_negotiated_protocol =
      HttpResponseInfo::ConnectionInfoToString(response_info_->connection_info);
  response_info_->vary_data
      .Init(*request_info_, *response_info_->headers.get());

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
  DCHECK(!stream_->IsClosed() || stream_->type() == SPDY_PUSH_STREAM);
  if (buffer) {
    response_body_queue_.Enqueue(std::move(buffer));

    if (user_buffer_.get()) {
      // Handing small chunks of data to the caller creates measurable overhead.
      // We buffer data in short time-spans and send a single read notification.
      ScheduleBufferedReadCallback();
    }
  }
}

void SpdyHttpStream::OnDataSent() {
  request_body_buf_size_ = 0;
  ReadAndSendRequestBodyData();
}

// TODO(xunjieli): Maybe do something with the trailers. crbug.com/422958.
void SpdyHttpStream::OnTrailers(const spdy::SpdyHeaderBlock& trailers) {}

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

void SpdyHttpStream::InitializeStreamHelper() {
  stream_->SetDelegate(this);
  was_alpn_negotiated_ = stream_->WasAlpnNegotiated();
}

void SpdyHttpStream::ResetStream(int error) {
  spdy_session_->ResetStream(stream()->stream_id(), error, std::string());
}

void SpdyHttpStream::OnRequestBodyReadCompleted(int status) {
  if (status < 0) {
    DCHECK_NE(ERR_IO_PENDING, status);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
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

void SpdyHttpStream::ScheduleBufferedReadCallback() {
  // If there is already a scheduled DoBufferedReadCallback, don't issue
  // another one.  Mark that we have received more data and return.
  if (buffered_read_callback_pending_) {
    more_read_data_pending_ = true;
    return;
  }

  more_read_data_pending_ = false;
  buffered_read_callback_pending_ = true;
  const base::TimeDelta kBufferTime = base::TimeDelta::FromMilliseconds(1);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SpdyHttpStream::DoBufferedReadCallback,
                     weak_factory_.GetWeakPtr()),
      kBufferTime);
}

// Checks to see if we should wait for more buffered data before notifying
// the caller.  Returns true if we should wait, false otherwise.
bool SpdyHttpStream::ShouldWaitForMoreBufferedData() const {
  // If the response is complete, there is no point in waiting.
  if (stream_closed_)
    return false;

  DCHECK_GT(user_buffer_len_, 0);
  return response_body_queue_.GetTotalSize() <
      static_cast<size_t>(user_buffer_len_);
}

void SpdyHttpStream::DoBufferedReadCallback() {
  buffered_read_callback_pending_ = false;

  // If the transaction is cancelled or errored out, we don't need to complete
  // the read.
  if (stream_closed_ && closed_stream_status_ != OK) {
    if (response_callback_)
      DoResponseCallback(closed_stream_status_);
    return;
  }

  // When more_read_data_pending_ is true, it means that more data has
  // arrived since we started waiting.  Wait a little longer and continue
  // to buffer.
  if (more_read_data_pending_ && ShouldWaitForMoreBufferedData()) {
    ScheduleBufferedReadCallback();
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
  base::ResetAndReturn(&request_callback_).Run(rv);
}

void SpdyHttpStream::MaybeDoRequestCallback(int rv) {
  CHECK_NE(ERR_IO_PENDING, rv);
  if (request_callback_)
    base::ResetAndReturn(&request_callback_).Run(rv);
}

void SpdyHttpStream::MaybePostRequestCallback(int rv) {
  CHECK_NE(ERR_IO_PENDING, rv);
  if (request_callback_)
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SpdyHttpStream::MaybeDoRequestCallback,
                                  weak_factory_.GetWeakPtr(), rv));
}

void SpdyHttpStream::DoResponseCallback(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  CHECK(!response_callback_.is_null());

  // Since Run may result in being called back, reset response_callback_ in
  // advance.
  base::ResetAndReturn(&response_callback_).Run(rv);
}

bool SpdyHttpStream::GetRemoteEndpoint(IPEndPoint* endpoint) {
  if (!spdy_session_)
    return false;

  return spdy_session_->GetPeerAddress(endpoint) == OK;
}

void SpdyHttpStream::PopulateNetErrorDetails(NetErrorDetails* details) {
  details->connection_info = HttpResponseInfo::CONNECTION_INFO_HTTP2;
  return;
}

void SpdyHttpStream::SetPriority(RequestPriority priority) {
  if (stream_) {
    stream_->SetPriority(priority);
  }
}

}  // namespace net
