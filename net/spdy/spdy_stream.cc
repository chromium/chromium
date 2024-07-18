// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"

#include <algorithm>
#include <limits>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_log_util.h"
#include "net/spdy/spdy_session.h"

namespace net {

namespace {

base::Value::Dict NetLogSpdyStreamErrorParams(spdy::SpdyStreamId stream_id,
                                              int net_error,
                                              std::string_view description) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(stream_id))
      .Set("net_error", ErrorToShortString(net_error))
      .Set("description", description);
}

base::Value::Dict NetLogSpdyStreamWindowUpdateParams(
    spdy::SpdyStreamId stream_id,
    int32_t delta,
    int32_t window_size) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(stream_id))
      .Set("delta", delta)
      .Set("window_size", window_size);
}

base::Value::Dict NetLogSpdyDataParams(spdy::SpdyStreamId stream_id,
                                       int size,
                                       bool fin) {
  return base::Value::Dict()
      .Set("stream_id", static_cast<int>(stream_id))
      .Set("size", size)
      .Set("fin", fin);
}

}  // namespace

// A wrapper around a stream that calls into ProduceHeadersFrame().
class SpdyStream::HeadersBufferProducer : public SpdyBufferProducer {
 public:
  explicit HeadersBufferProducer(const base::WeakPtr<SpdyStream>& stream)
      : stream_(stream) {
    DCHECK(stream_.get());
  }

  ~HeadersBufferProducer() override = default;

  std::unique_ptr<SpdyBuffer> ProduceBuffer() override {
    if (!stream_.get()) {
      NOTREACHED_IN_MIGRATION();
      return nullptr;
    }
    DCHECK_GT(stream_->stream_id(), 0u);
    return std::make_unique<SpdyBuffer>(stream_->ProduceHeadersFrame());
  }

 private:
  const base::WeakPtr<SpdyStream> stream_;
};

SpdyStream::SpdyStream(SpdyStreamType type,
                       const base::WeakPtr<SpdySession>& session,
                       const GURL& url,
                       RequestPriority priority,
                       int32_t initial_send_window_size,
                       int32_t max_recv_window_size,
                       const NetLogWithSource& net_log,
                       const NetworkTrafficAnnotationTag& traffic_annotation,
                       bool detect_broken_connection)
    : type_(type),
      url_(url),
      priority_(priority),
      send_window_size_(initial_send_window_size),
      max_recv_window_size_(max_recv_window_size),
      recv_window_size_(max_recv_window_size),
      last_recv_window_update_(base::TimeTicks::Now()),
      session_(session),
      request_time_(base::Time::Now()),
      net_log_(net_log),
      traffic_annotation_(traffic_annotation),
      detect_broken_connection_(detect_broken_connection) {
  CHECK(type_ == SPDY_BIDIRECTIONAL_STREAM ||
        type_ == SPDY_REQUEST_RESPONSE_STREAM);
  CHECK_GE(priority_, MINIMUM_PRIORITY);
  CHECK_LE(priority_, MAXIMUM_PRIORITY);
}

SpdyStream::~SpdyStream() {
  CHECK(!write_handler_guard_);
}

void SpdyStream::SetDelegate(Delegate* delegate) {
  CHECK(!delegate_);
  CHECK(delegate);
  delegate_ = delegate;

  CHECK(io_state_ == STATE_IDLE || io_state_ == STATE_RESERVED_REMOTE);
}

std::unique_ptr<spdy::SpdySerializedFrame> SpdyStream::ProduceHeadersFrame() {
  CHECK_EQ(io_state_, STATE_IDLE);
  CHECK(request_headers_valid_);
  CHECK_GT(stream_id_, 0u);

  spdy::SpdyControlFlags flags = (pending_send_status_ == NO_MORE_DATA_TO_SEND)
                                     ? spdy::CONTROL_FLAG_FIN
                                     : spdy::CONTROL_FLAG_NONE;
  std::unique_ptr<spdy::SpdySerializedFrame> frame(session_->CreateHeaders(
      stream_id_, priority_, flags, std::move(request_headers_),
      delegate_->source_dependency()));
  request_headers_valid_ = false;
  send_time_ = base::TimeTicks::Now();
  return frame;
}

void SpdyStream::DetachDelegate() {
  DCHECK(!IsClosed());
  delegate_ = nullptr;
  Cancel(ERR_ABORTED);
}

void SpdyStream::SetPriority(RequestPriority priority) {
  if (priority_ == priority) {
    return;
  }

  session_->UpdateStreamPriority(this, /* old_priority = */ priority_,
                                 /* new_priority = */ priority);

  priority_ = priority;
}

bool SpdyStream::AdjustSendWindowSize(int32_t delta_window_size) {
  if (IsClosed())
    return true;

  if (delta_window_size > 0) {
    if (send_window_size_ >
        std::numeric_limits<int32_t>::max() - delta_window_size) {
      return false;
    }
  } else {
    // Minimum allowed value for spdy::SETTINGS_INITIAL_WINDOW_SIZE is 0 and
    // maximum is 2^31-1.  Data are not sent when |send_window_size_ < 0|, that
    // is, |send_window_size_ | can only decrease by a change in
    // spdy::SETTINGS_INITIAL_WINDOW_SIZE.  Therefore |send_window_size_| should
    // never be able to become less than -(2^31-1).
    DCHECK_LE(std::numeric_limits<int32_t>::min() - delta_window_size,
              send_window_size_);
  }

  send_window_size_ += delta_window_size;

  net_log_.AddEvent(NetLogEventType::HTTP2_STREAM_UPDATE_SEND_WINDOW, [&] {
    return NetLogSpdyStreamWindowUpdateParams(stream_id_, delta_window_size,
                                              send_window_size_);
  });

  PossiblyResumeIfSendStalled();
  return true;
}

void SpdyStream::OnWriteBufferConsumed(
    size_t frame_payload_size,
    size_t consume_size,
    SpdyBuffer::ConsumeSource consume_source) {
  if (consume_source == SpdyBuffer::DISCARD) {
    // If we're discarding a frame or part of it, increase the send
    // window by the number of discarded bytes. (Although if we're
    // discarding part of a frame, it's probably because of a write
    // error and we'll be tearing down the stream soon.)
    size_t remaining_payload_bytes = std::min(consume_size, frame_payload_size);
    DCHECK_GT(remaining_payload_bytes, 0u);
    IncreaseSendWindowSize(static_cast<int32_t>(remaining_payload_bytes));
  }
  // For consumed bytes, the send window is increased when we receive
  // a WINDOW_UPDATE frame.
}

void SpdyStream::IncreaseSendWindowSize(int32_t delta_window_size) {
  DCHECK_GE(delta_window_size, 1);

  if (!AdjustSendWindowSize(delta_window_size)) {
    std::string desc = base::StringPrintf(
        "Received WINDOW_UPDATE [delta: %d] for stream %d overflows "
        "send_window_size_ [current: %d]",
        delta_window_size, stream_id_, send_window_size_);
    session_->ResetStream(stream_id_, ERR_HTTP2_FLOW_CONTROL_ERROR, desc);
  }
}

void SpdyStream::DecreaseSendWindowSize(int32_t delta_window_size) {
  if (IsClosed())
    return;

  // We only call this method when sending a frame. Therefore,
  // |delta_window_size| should be within the valid frame size range.
  DCHECK_GE(delta_window_size, 1);
  DCHECK_LE(delta_window_size, kMaxSpdyFrameChunkSize);

  // |send_window_size_| should have been at least |delta_window_size| for
  // this call to happen.
  DCHECK_GE(send_window_size_, delta_window_size);

  send_window_size_ -= delta_window_size;

  net_log_.AddEvent(NetLogEventType::HTTP2_STREAM_UPDATE_SEND_WINDOW, [&] {
    return NetLogSpdyStreamWindowUpdateParams(stream_id_, -delta_window_size,
                                              send_window_size_);
  });
}

void SpdyStream::OnReadBufferConsumed(
    size_t consume_size,
    SpdyBuffer::ConsumeSource consume_source) {
  DCHECK_GE(consume_size, 1u);
  DCHECK_LE(consume_size,
            static_cast<size_t>(std::numeric_limits<int32_t>::max()));
  IncreaseRecvWindowSize(static_cast<int32_t>(consume_size));
}

void SpdyStream::IncreaseRecvWindowSize(int32_t delta_window_size) {
  // By the time a read is processed by the delegate, this stream may
  // already be inactive.
  if (!session_->IsStreamActive(stream_id_))
    return;

  DCHECK_GE(unacked_recv_window_bytes_, 0);
  DCHECK_GE(recv_window_size_, unacked_recv_window_bytes_);
  DCHECK_GE(delta_window_size, 1);
  // Check for overflow.
  DCHECK_LE(delta_window_size,
            std::numeric_limits<int32_t>::max() - recv_window_size_);

  recv_window_size_ += delta_window_size;
  net_log_.AddEvent(NetLogEventType::HTTP2_STREAM_UPDATE_RECV_WINDOW, [&] {
    return NetLogSpdyStreamWindowUpdateParams(stream_id_, delta_window_size,
                                              recv_window_size_);
  });

  // Update the receive window once half of the buffer is ready to be acked
  // to prevent excessive window updates on fast downloads. Also send an update
  // if too much time has elapsed since the last update to deal with
  // slow-reading clients so the server doesn't think the stream is idle.
  unacked_recv_window_bytes_ += delta_window_size;
  const base::TimeDelta elapsed =
      base::TimeTicks::Now() - last_recv_window_update_;
  if (unacked_recv_window_bytes_ > max_recv_window_size_ / 2 ||
      elapsed >= session_->TimeToBufferSmallWindowUpdates()) {
    last_recv_window_update_ = base::TimeTicks::Now();
    session_->SendStreamWindowUpdate(
        stream_id_, static_cast<uint32_t>(unacked_recv_window_bytes_));
    unacked_recv_window_bytes_ = 0;
  }
}

void SpdyStream::DecreaseRecvWindowSize(int32_t delta_window_size) {
  DCHECK(session_->IsStreamActive(stream_id_));
  DCHECK_GE(delta_window_size, 1);

  // The receiving window size as the peer knows it is
  // |recv_window_size_ - unacked_recv_window_bytes_|, if more data are sent by
  // the peer, that means that the receive window is not being respected.
  if (delta_window_size > recv_window_size_ - unacked_recv_window_bytes_) {
    session_->ResetStream(
        stream_id_, ERR_HTTP2_FLOW_CONTROL_ERROR,
        "delta_window_size is " + base::NumberToString(delta_window_size) +
            " in DecreaseRecvWindowSize, which is larger than the receive " +
            "window size of " + base::NumberToString(recv_window_size_));
    return;
  }

  recv_window_size_ -= delta_window_size;
  net_log_.AddEvent(NetLogEventType::HTTP2_STREAM_UPDATE_RECV_WINDOW, [&] {
    return NetLogSpdyStreamWindowUpdateParams(stream_id_, -delta_window_size,
                                              recv_window_size_);
  });
}

int SpdyStream::GetPeerAddress(IPEndPoint* address) const {
  return session_->GetPeerAddress(address);
}

int SpdyStream::GetLocalAddress(IPEndPoint* address) const {
  return session_->GetLocalAddress(address);
}

bool SpdyStream::WasEverUsed() const {
  return session_->WasEverUsed();
}

base::Time SpdyStream::GetRequestTime() const {
  return request_time_;
}

void SpdyStream::SetRequestTime(base::Time t) {
  request_time_ = t;
}

void SpdyStream::OnHeadersReceived(
    const quiche::HttpHeaderBlock& response_headers,
    base::Time response_time,
    base::TimeTicks recv_first_byte_time) {
  switch (response_state_) {
    case READY_FOR_HEADERS: {
      // No header block has been received yet.
      DCHECK(response_headers_.empty());

      quiche::HttpHeaderBlock::const_iterator it =
          response_headers.find(spdy::kHttp2StatusHeader);
      if (it == response_headers.end()) {
        const std::string error("Response headers do not include :status.");
        LogStreamError(ERR_HTTP2_PROTOCOL_ERROR, error);
        session_->ResetStream(stream_id_, ERR_HTTP2_PROTOCOL_ERROR, error);
        return;
      }

      int status;
      if (!base::StringToInt(it->second, &status)) {
        const std::string error("Cannot parse :status.");
        LogStreamError(ERR_HTTP2_PROTOCOL_ERROR, error);
        session_->ResetStream(stream_id_, ERR_HTTP2_PROTOCOL_ERROR, error);
        return;
      }

      base::UmaHistogramSparse("Net.SpdyResponseCode", status);

      // Include informational responses (1xx) in the TTFB as per the resource
      // timing spec for responseStart.
      if (recv_first_byte_time_.is_null())
        recv_first_byte_time_ = recv_first_byte_time;
      // Also record the TTFB of non-informational responses.
      if (status / 100 != 1) {
        DCHECK(recv_first_byte_time_for_non_informational_response_.is_null());
        recv_first_byte_time_for_non_informational_response_ =
            recv_first_byte_time;
      }

      // Handle informational responses (1xx):
      // * Pass through 101 Switching Protocols, because broken servers might
      //   send this as a response to a WebSocket request, in which case it
      //   needs to pass through so that the WebSocket layer can signal an
      //   error.
      // * Plumb 103 Early Hints to the delegate.
      // * Ignore other informational responses.
      if (status / 100 == 1 && status != HTTP_SWITCHING_PROTOCOLS) {
        if (status == HTTP_EARLY_HINTS)
          OnEarlyHintsReceived(response_headers, recv_first_byte_time);
        return;
      }

      response_state_ = READY_FOR_DATA_OR_TRAILERS;

      switch (type_) {
        case SPDY_BIDIRECTIONAL_STREAM:
        case SPDY_REQUEST_RESPONSE_STREAM:
          // A bidirectional stream or a request/response stream is ready for
          // the response headers only after request headers are sent.
          if (io_state_ == STATE_IDLE) {
            const std::string error("Response received before request sent.");
            LogStreamError(ERR_HTTP2_PROTOCOL_ERROR, error);
            session_->ResetStream(stream_id_, ERR_HTTP2_PROTOCOL_ERROR, error);
            return;
          }
          break;
      }

      DCHECK_NE(io_state_, STATE_IDLE);

      response_time_ = response_time;
      SaveResponseHeaders(response_headers, status);

      break;
    }
    case READY_FOR_DATA_OR_TRAILERS:
      // Second header block is trailers.
      response_state_ = TRAILERS_RECEIVED;
      delegate_->OnTrailers(response_headers);
      break;

    case TRAILERS_RECEIVED:
      // No further header blocks are allowed after trailers.
      const std::string error("Header block received after trailers.");
      LogStreamError(ERR_HTTP2_PROTOCOL_ERROR, error);
      session_->ResetStream(stream_id_, ERR_HTTP2_PROTOCOL_ERROR, error);
      break;
  }
}

void SpdyStream::OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) {
  DCHECK(session_->IsStreamActive(stream_id_));

  if (response_state_ == READY_FOR_HEADERS) {
    const std::string error("DATA received before headers.");
    LogStreamError(ERR_HTTP2_PROTOCOL_ERROR, error);
    session_->ResetStream(stream_id_, ERR_HTTP2_PROTOCOL_ERROR, error);
    return;
  }

  if (response_state_ == TRAILERS_RECEIVED && buffer) {
    const std::string error("DATA received after trailers.");
    LogStreamError(ERR_HTTP2_PROTOCOL_ERROR, error);
    session_->ResetStream(stream_id_, ERR_HTTP2_PROTOCOL_ERROR, error);
    return;
  }

  if (io_state_ == STATE_HALF_CLOSED_REMOTE) {
    const std::string error("DATA received on half-closed (remove) stream.");
    LogStreamError(ERR_HTTP2_STREAM_CLOSED, error);
    session_->ResetStream(stream_id_, ERR_HTTP2_STREAM_CLOSED, error);
    return;
  }

  // Track our bandwidth.
  recv_bytes_ += buffer ? buffer->GetRemainingSize() : 0;
  recv_last_byte_time_ = base::TimeTicks::Now();

  CHECK(!IsClosed());

  if (!buffer) {
    if (io_state_ == STATE_OPEN) {
      io_state_ = STATE_HALF_CLOSED_REMOTE;
      // Inform the delegate of EOF. This may delete |this|.
      delegate_->OnDataReceived(nullptr);
    } else if (io_state_ == STATE_HALF_CLOSED_LOCAL) {
      io_state_ = STATE_CLOSED;
      // Deletes |this|.
      session_->CloseActiveStream(stream_id_, OK);
    } else {
      NOTREACHED_IN_MIGRATION() << io_state_;
    }
    return;
  }

  size_t length = buffer->GetRemainingSize();
  DCHECK_LE(length, spdy::kHttp2DefaultFramePayloadLimit);
  base::WeakPtr<SpdyStream> weak_this = GetWeakPtr();
  // May close the stream.
  DecreaseRecvWindowSize(static_cast<int32_t>(length));
  if (!weak_this)
    return;
  buffer->AddConsumeCallback(
      base::BindRepeating(&SpdyStream::OnReadBufferConsumed, GetWeakPtr()));

  // May close |this|.
  delegate_->OnDataReceived(std::move(buffer));
}

void SpdyStream::OnPaddingConsumed(size_t len) {
  // Decrease window size because padding bytes are received.
  // Increase window size because padding bytes are consumed (by discarding).
  // Net result: |unacked_recv_window_bytes_| increases by |len|,
  // |recv_window_size_| does not change.
  base::WeakPtr<SpdyStream> weak_this = GetWeakPtr();
  // May close the stream.
  DecreaseRecvWindowSize(static_cast<int32_t>(len));
  if (!weak_this)
    return;
  IncreaseRecvWindowSize(static_cast<int32_t>(len));
}

void SpdyStream::OnFrameWriteComplete(spdy::SpdyFrameType frame_type,
                                      size_t frame_size) {
  if (frame_type != spdy::SpdyFrameType::HEADERS &&
      frame_type != spdy::SpdyFrameType::DATA) {
    return;
  }

  int result = (frame_type == spdy::SpdyFrameType::HEADERS)
                   ? OnHeadersSent()
                   : OnDataSent(frame_size);
  if (result == ERR_IO_PENDING) {
    // The write operation hasn't completed yet.
    return;
  }

  if (pending_send_status_ == NO_MORE_DATA_TO_SEND) {
    if (io_state_ == STATE_OPEN) {
      io_state_ = STATE_HALF_CLOSED_LOCAL;
    } else if (io_state_ == STATE_HALF_CLOSED_REMOTE) {
      io_state_ = STATE_CLOSED;
    } else {
      NOTREACHED_IN_MIGRATION() << io_state_;
    }
  }
  // Notify delegate of write completion. Must not destroy |this|.
  CHECK(delegate_);
  {
    base::WeakPtr<SpdyStream> weak_this = GetWeakPtr();
    write_handler_guard_ = true;
    if (frame_type == spdy::SpdyFrameType::HEADERS) {
      delegate_->OnHeadersSent();
    } else {
      delegate_->OnDataSent();
    }
    CHECK(weak_this);
    write_handler_guard_ = false;
  }

  if (io_state_ == STATE_CLOSED) {
    // Deletes |this|.
    session_->CloseActiveStream(stream_id_, OK);
  }
}

int SpdyStream::OnHeadersSent() {
  CHECK_EQ(io_state_, STATE_IDLE);
  CHECK_NE(stream_id_, 0u);

  io_state_ = STATE_OPEN;
  return OK;
}

int SpdyStream::OnDataSent(size_t frame_size) {
  CHECK(io_state_ == STATE_OPEN ||
        io_state_ == STATE_HALF_CLOSED_REMOTE) << io_state_;

  size_t frame_payload_size = frame_size - spdy::kDataFrameMinimumSize;

  CHECK_GE(frame_size, spdy::kDataFrameMinimumSize);
  CHECK_LE(frame_payload_size, spdy::kHttp2DefaultFramePayloadLimit);

  // If more data is available to send, dispatch it and
  // return that the write operation is still ongoing.
  pending_send_data_->DidConsume(frame_payload_size);
  if (pending_send_data_->BytesRemaining() > 0) {
    QueueNextDataFrame();
    return ERR_IO_PENDING;
  } else {
    pending_send_data_ = nullptr;
    return OK;
  }
}

void SpdyStream::LogStreamError(int error, std::string_view description) {
  net_log_.AddEvent(NetLogEventType::HTTP2_STREAM_ERROR, [&] {
    return NetLogSpdyStreamErrorParams(stream_id_, error, description);
  });
}

void SpdyStream::OnClose(int status) {
  // In most cases, the stream should already be CLOSED. The exception is when a
  // SpdySession is shutting down while the stream is in an intermediate state.
  io_state_ = STATE_CLOSED;
  if (status == ERR_HTTP2_RST_STREAM_NO_ERROR_RECEIVED) {
    if (response_state_ == READY_FOR_HEADERS) {
      status = ERR_HTTP2_PROTOCOL_ERROR;
    } else {
      status = OK;
    }
  }
  Delegate* delegate = delegate_;
  delegate_ = nullptr;
  if (delegate)
    delegate->OnClose(status);
  // Unset |stream_id_| last so that the delegate can look it up.
  stream_id_ = 0;
}

void SpdyStream::Cancel(int error) {
  // We may be called again from a delegate's OnClose().
  if (io_state_ == STATE_CLOSED)
    return;

  if (stream_id_ != 0) {
    session_->ResetStream(stream_id_, error, std::string());
  } else {
    session_->CloseCreatedStream(GetWeakPtr(), error);
  }
  // |this| is invalid at this point.
}

void SpdyStream::Close() {
  // We may be called again from a delegate's OnClose().
  if (io_state_ == STATE_CLOSED)
    return;

  if (stream_id_ != 0) {
    session_->CloseActiveStream(stream_id_, OK);
  } else {
    session_->CloseCreatedStream(GetWeakPtr(), OK);
  }
  // |this| is invalid at this point.
}

base::WeakPtr<SpdyStream> SpdyStream::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

int SpdyStream::SendRequestHeaders(quiche::HttpHeaderBlock request_headers,
                                   SpdySendStatus send_status) {
  net_log_.AddEvent(
      NetLogEventType::HTTP_TRANSACTION_HTTP2_SEND_REQUEST_HEADERS,
      [&](NetLogCaptureMode capture_mode) {
        return HttpHeaderBlockNetLogParams(&request_headers, capture_mode);
      });
  CHECK_EQ(pending_send_status_, MORE_DATA_TO_SEND);
  CHECK(!request_headers_valid_);
  CHECK(!pending_send_data_.get());
  CHECK_EQ(io_state_, STATE_IDLE);
  request_headers_ = std::move(request_headers);
  request_headers_valid_ = true;
  pending_send_status_ = send_status;
  session_->EnqueueStreamWrite(
      GetWeakPtr(), spdy::SpdyFrameType::HEADERS,
      std::make_unique<HeadersBufferProducer>(GetWeakPtr()));
  return ERR_IO_PENDING;
}

void SpdyStream::SendData(IOBuffer* data,
                          int length,
                          SpdySendStatus send_status) {
  CHECK_EQ(pending_send_status_, MORE_DATA_TO_SEND);
  CHECK(io_state_ == STATE_OPEN ||
        io_state_ == STATE_HALF_CLOSED_REMOTE) << io_state_;
  CHECK(!pending_send_data_.get());
  pending_send_data_ = base::MakeRefCounted<DrainableIOBuffer>(data, length);
  pending_send_status_ = send_status;
  QueueNextDataFrame();
}

bool SpdyStream::GetSSLInfo(SSLInfo* ssl_info) const {
  return session_->GetSSLInfo(ssl_info);
}

NextProto SpdyStream::GetNegotiatedProtocol() const {
  return session_->GetNegotiatedProtocol();
}

SpdyStream::ShouldRequeueStream SpdyStream::PossiblyResumeIfSendStalled() {
  if (IsLocallyClosed() || !send_stalled_by_flow_control_)
    return DoNotRequeue;
  if (session_->IsSendStalled() || send_window_size_ <= 0) {
    return Requeue;
  }
  net_log_.AddEventWithIntParams(
      NetLogEventType::HTTP2_STREAM_FLOW_CONTROL_UNSTALLED, "stream_id",
      stream_id_);
  send_stalled_by_flow_control_ = false;
  QueueNextDataFrame();
  return DoNotRequeue;
}

bool SpdyStream::IsClosed() const {
  return io_state_ == STATE_CLOSED;
}

bool SpdyStream::IsLocallyClosed() const {
  return io_state_ == STATE_HALF_CLOSED_LOCAL || io_state_ == STATE_CLOSED;
}

bool SpdyStream::IsIdle() const {
  return io_state_ == STATE_IDLE;
}

bool SpdyStream::IsOpen() const {
  return io_state_ == STATE_OPEN;
}

bool SpdyStream::IsReservedRemote() const {
  return io_state_ == STATE_RESERVED_REMOTE;
}

void SpdyStream::AddRawReceivedBytes(size_t received_bytes) {
  raw_received_bytes_ += received_bytes;
}

void SpdyStream::AddRawSentBytes(size_t sent_bytes) {
  raw_sent_bytes_ += sent_bytes;
}

bool SpdyStream::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  if (stream_id_ == 0)
    return false;
  bool result = session_->GetLoadTimingInfo(stream_id_, load_timing_info);
  // TODO(acomminos): recv_first_byte_time_ is actually the time after all
  // headers have been parsed. We should add support for reporting the time the
  // first bytes of the HEADERS frame were received to BufferedSpdyFramer
  // (https://crbug.com/568024).
  load_timing_info->receive_headers_start = recv_first_byte_time_;
  load_timing_info->receive_non_informational_headers_start =
      recv_first_byte_time_for_non_informational_response_;
  load_timing_info->first_early_hints_time = first_early_hints_time_;
  return result;
}

void SpdyStream::QueueNextDataFrame() {
  // Until the request has been completely sent, we cannot be sure
  // that our stream_id is correct.
  CHECK(io_state_ == STATE_OPEN ||
        io_state_ == STATE_HALF_CLOSED_REMOTE) << io_state_;
  CHECK_GT(stream_id_, 0u);
  CHECK(pending_send_data_.get());
  // Only the final fame may have a length of 0.
  if (pending_send_status_ == NO_MORE_DATA_TO_SEND) {
    CHECK_GE(pending_send_data_->BytesRemaining(), 0);
  } else {
    CHECK_GT(pending_send_data_->BytesRemaining(), 0);
  }

  spdy::SpdyDataFlags flags = (pending_send_status_ == NO_MORE_DATA_TO_SEND)
                                  ? spdy::DATA_FLAG_FIN
                                  : spdy::DATA_FLAG_NONE;
  int effective_len;
  bool end_stream;
  std::unique_ptr<SpdyBuffer> data_buffer(
      session_->CreateDataBuffer(stream_id_, pending_send_data_.get(),
                                 pending_send_data_->BytesRemaining(), flags,
                                 &effective_len, &end_stream));
  // We'll get called again by PossiblyResumeIfSendStalled().
  if (!data_buffer)
    return;

  DCHECK_GE(data_buffer->GetRemainingSize(), spdy::kDataFrameMinimumSize);
  size_t payload_size =
      data_buffer->GetRemainingSize() - spdy::kDataFrameMinimumSize;
  DCHECK_LE(payload_size, spdy::kHttp2DefaultFramePayloadLimit);

  // Send window size is based on payload size, so nothing to do if this is
  // just a FIN with no payload.
  if (payload_size != 0) {
    DecreaseSendWindowSize(static_cast<int32_t>(payload_size));
    // This currently isn't strictly needed, since write frames are
    // discarded only if the stream is about to be closed. But have it
    // here anyway just in case this changes.
    data_buffer->AddConsumeCallback(base::BindRepeating(
        &SpdyStream::OnWriteBufferConsumed, GetWeakPtr(), payload_size));
  }

  if (session_->GreasedFramesEnabled() && delegate_ &&
      delegate_->CanGreaseFrameType()) {
    session_->EnqueueGreasedFrame(GetWeakPtr());
  }

  session_->net_log().AddEvent(NetLogEventType::HTTP2_SESSION_SEND_DATA, [&] {
    return NetLogSpdyDataParams(stream_id_, effective_len, end_stream);
  });

  session_->EnqueueStreamWrite(
      GetWeakPtr(), spdy::SpdyFrameType::DATA,
      std::make_unique<SimpleBufferProducer>(std::move(data_buffer)));
}

void SpdyStream::OnEarlyHintsReceived(
    const quiche::HttpHeaderBlock& response_headers,
    base::TimeTicks recv_first_byte_time) {
  // Record the timing of the 103 Early Hints response for the experiment
  // (https://crbug.com/1093693).
  if (first_early_hints_time_.is_null())
    first_early_hints_time_ = recv_first_byte_time;

  // Transfer-encoding is a connection specific header.
  if (response_headers.find("transfer-encoding") != response_headers.end()) {
    const char error[] = "Received transfer-encoding header";
    LogStreamError(ERR_HTTP2_PROTOCOL_ERROR, error);
    session_->ResetStream(stream_id_, ERR_HTTP2_PROTOCOL_ERROR, error);
    return;
  }

  if (type_ != SPDY_REQUEST_RESPONSE_STREAM || io_state_ == STATE_IDLE) {
    const char error[] = "Early Hints received before request sent.";
    LogStreamError(ERR_HTTP2_PROTOCOL_ERROR, error);
    session_->ResetStream(stream_id_, ERR_HTTP2_PROTOCOL_ERROR, error);
    return;
  }

  // `delegate_` must be attached at this point when `type_` is
  // SPDY_REQUEST_RESPONSE_STREAM.
  CHECK(delegate_);
  delegate_->OnEarlyHintsReceived(response_headers);
}

void SpdyStream::SaveResponseHeaders(
    const quiche::HttpHeaderBlock& response_headers,
    int status) {
  if (response_headers.contains("transfer-encoding")) {
    session_->ResetStream(stream_id_, ERR_HTTP2_PROTOCOL_ERROR,
                          "Received transfer-encoding header");
    return;
  }

  DCHECK(response_headers_.empty());
  response_headers_ = response_headers.Clone();

  // If delegate is not yet attached, OnHeadersReceived() will be called after
  // the delegate gets attached to the stream.
  if (!delegate_)
    return;

  delegate_->OnHeadersReceived(response_headers_);
}

#define STATE_CASE(s)                                       \
  case s:                                                   \
    description = base::StringPrintf("%s (0x%08X)", #s, s); \
    break

std::string SpdyStream::DescribeState(State state) {
  std::string description;
  switch (state) {
    STATE_CASE(STATE_IDLE);
    STATE_CASE(STATE_OPEN);
    STATE_CASE(STATE_HALF_CLOSED_LOCAL);
    STATE_CASE(STATE_CLOSED);
    default:
      description =
          base::StringPrintf("Unknown state 0x%08X (%u)", state, state);
      break;
  }
  return description;
}

#undef STATE_CASE

}  // namespace net
