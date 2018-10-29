// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/values.h"
#include "net/log/net_log.h"
#include "net/log/net_log_capture_mode.h"
#include "net/log/net_log_event_type.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"

namespace net {

namespace {

std::unique_ptr<base::Value> NetLogSpdyStreamErrorCallback(
    spdy::SpdyStreamId stream_id,
    int net_error,
    const std::string* description,
    NetLogCaptureMode /* capture_mode */) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetInteger("stream_id", static_cast<int>(stream_id));
  dict->SetString("net_error", ErrorToShortString(net_error));
  dict->SetString("description", *description);
  return std::move(dict);
}

std::unique_ptr<base::Value> NetLogSpdyStreamWindowUpdateCallback(
    spdy::SpdyStreamId stream_id,
    int32_t delta,
    int32_t window_size,
    NetLogCaptureMode /* capture_mode */) {
  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetInteger("stream_id", stream_id);
  dict->SetInteger("delta", delta);
  dict->SetInteger("window_size", window_size);
  return std::move(dict);
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
      NOTREACHED();
      return std::unique_ptr<SpdyBuffer>();
    }
    DCHECK_GT(stream_->stream_id(), 0u);
    return std::make_unique<SpdyBuffer>(stream_->ProduceHeadersFrame());
  }
  size_t EstimateMemoryUsage() const override { return 0; }

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
                       const NetworkTrafficAnnotationTag& traffic_annotation)
    : type_(type),
      stream_id_(0),
      url_(url),
      priority_(priority),
      send_stalled_by_flow_control_(false),
      send_window_size_(initial_send_window_size),
      max_recv_window_size_(max_recv_window_size),
      recv_window_size_(max_recv_window_size),
      unacked_recv_window_bytes_(0),
      session_(session),
      delegate_(NULL),
      request_headers_valid_(false),
      pending_send_status_(MORE_DATA_TO_SEND),
      request_time_(base::Time::Now()),
      response_state_(READY_FOR_HEADERS),
      io_state_(STATE_IDLE),
      response_status_(OK),
      net_log_(net_log),
      raw_received_bytes_(0),
      raw_sent_bytes_(0),
      send_bytes_(0),
      recv_bytes_(0),
      write_handler_guard_(false),
      traffic_annotation_(traffic_annotation),
      weak_ptr_factory_(this) {
  CHECK(type_ == SPDY_BIDIRECTIONAL_STREAM ||
        type_ == SPDY_REQUEST_RESPONSE_STREAM ||
        type_ == SPDY_PUSH_STREAM);
  CHECK_GE(priority_, MINIMUM_PRIORITY);
  CHECK_LE(priority_, MAXIMUM_PRIORITY);
}

SpdyStream::~SpdyStream() {
  CHECK(!write_handler_guard_);
  UpdateHistograms();
}

void SpdyStream::SetDelegate(Delegate* delegate) {
  CHECK(!delegate_);
  CHECK(delegate);
  delegate_ = delegate;

  CHECK(io_state_ == STATE_IDLE ||
        io_state_ == STATE_HALF_CLOSED_LOCAL_UNCLAIMED ||
        io_state_ == STATE_RESERVED_REMOTE);

  if (io_state_ == STATE_HALF_CLOSED_LOCAL_UNCLAIMED) {
    DCHECK_EQ(type_, SPDY_PUSH_STREAM);
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&SpdyStream::PushedStreamReplay, GetWeakPtr()));
  }
}

void SpdyStream::PushedStreamReplay() {
  DCHECK_EQ(type_, SPDY_PUSH_STREAM);
  DCHECK_NE(stream_id_, 0u);
  CHECK_EQ(stream_id_ % 2, 0u);

  CHECK_EQ(io_state_, STATE_HALF_CLOSED_LOCAL_UNCLAIMED);
  io_state_ = STATE_HALF_CLOSED_LOCAL;

  // The delegate methods called below may delete |this|, so use
  // |weak_this| to detect that.
  base::WeakPtr<SpdyStream> weak_this = GetWeakPtr();

  CHECK(delegate_);
  delegate_->OnHeadersReceived(response_headers_, &request_headers_);

  // OnHeadersReceived() may have closed |this|.
  if (!weak_this)
    return;

  while (!pending_recv_data_.empty()) {
    // Take ownership of the first element of |pending_recv_data_|.
    std::unique_ptr<SpdyBuffer> buffer = std::move(pending_recv_data_.at(0));
    pending_recv_data_.erase(pending_recv_data_.begin());

    bool eof = (buffer == NULL);

    CHECK(delegate_);
    delegate_->OnDataReceived(std::move(buffer));

    // OnDataReceived() may have closed |this|.
    if (!weak_this)
      return;

    if (eof) {
      DCHECK(pending_recv_data_.empty());
      session_->CloseActiveStream(stream_id_, OK);
      DCHECK(!weak_this);
      // |pending_recv_data_| is invalid at this point.
      break;
    }
  }
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
  delegate_ = NULL;
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

  net_log_.AddEvent(
      NetLogEventType::HTTP2_STREAM_UPDATE_SEND_WINDOW,
      base::Bind(&NetLogSpdyStreamWindowUpdateCallback, stream_id_,
                 delta_window_size, send_window_size_));

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
    session_->ResetStream(stream_id_, ERR_SPDY_FLOW_CONTROL_ERROR, desc);
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

  net_log_.AddEvent(
      NetLogEventType::HTTP2_STREAM_UPDATE_SEND_WINDOW,
      base::Bind(&NetLogSpdyStreamWindowUpdateCallback, stream_id_,
                 -delta_window_size, send_window_size_));
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
  net_log_.AddEvent(
      NetLogEventType::HTTP2_STREAM_UPDATE_RECV_WINDOW,
      base::Bind(&NetLogSpdyStreamWindowUpdateCallback, stream_id_,
                 delta_window_size, recv_window_size_));

  unacked_recv_window_bytes_ += delta_window_size;
  if (unacked_recv_window_bytes_ > max_recv_window_size_ / 2) {
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
        stream_id_, ERR_SPDY_FLOW_CONTROL_ERROR,
        "delta_window_size is " + base::IntToString(delta_window_size) +
            " in DecreaseRecvWindowSize, which is larger than the receive " +
            "window size of " + base::IntToString(recv_window_size_));
    return;
  }

  recv_window_size_ -= delta_window_size;
  net_log_.AddEvent(
      NetLogEventType::HTTP2_STREAM_UPDATE_RECV_WINDOW,
      base::Bind(&NetLogSpdyStreamWindowUpdateCallback, stream_id_,
                 -delta_window_size, recv_window_size_));
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
    const spdy::SpdyHeaderBlock& response_headers,
    base::Time response_time,
    base::TimeTicks recv_first_byte_time) {
  switch (response_state_) {
    case READY_FOR_HEADERS:
      // No header block has been received yet.
      DCHECK(response_headers_.empty());

      {
        spdy::SpdyHeaderBlock::const_iterator it =
            response_headers.find(spdy::kHttp2StatusHeader);
        if (it == response_headers.end()) {
          const std::string error("Response headers do not include :status.");
          LogStreamError(ERR_SPDY_PROTOCOL_ERROR, error);
          session_->ResetStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR, error);
          return;
        }

        int status;
        if (!StringToInt(it->second, &status)) {
          const std::string error("Cannot parse :status.");
          LogStreamError(ERR_SPDY_PROTOCOL_ERROR, error);
          session_->ResetStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR, error);
          return;
        }

        base::UmaHistogramSparse("Net.SpdyResponseCode", status);

        // Ignore informational headers like 103 Early Hints.
        // TODO(bnc): Add support for 103 Early Hints, https://crbug.com/671310.
        // However, do not ignore 101 Switching Protocols, because broken
        // servers might send this as a response to a WebSocket request,
        // in which case it needs to pass through so that the WebSocket layer
        // can signal an error.
        if (status / 100 == 1 && status != 101) {
          return;
        }
      }

      response_state_ = READY_FOR_DATA_OR_TRAILERS;

      switch (type_) {
        case SPDY_BIDIRECTIONAL_STREAM:
        case SPDY_REQUEST_RESPONSE_STREAM:
          // A bidirectional stream or a request/response stream is ready for
          // the response headers only after request headers are sent.
          if (io_state_ == STATE_IDLE) {
            const std::string error("Response received before request sent.");
            LogStreamError(ERR_SPDY_PROTOCOL_ERROR, error);
            session_->ResetStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR, error);
            return;
          }
          break;

        case SPDY_PUSH_STREAM:
          // Push streams transition to a locally half-closed state upon
          // headers.  We must continue to buffer data while waiting for a call
          // to SetDelegate() (which may not ever happen).
          DCHECK_EQ(io_state_, STATE_RESERVED_REMOTE);
          if (!delegate_) {
            io_state_ = STATE_HALF_CLOSED_LOCAL_UNCLAIMED;
          } else {
            io_state_ = STATE_HALF_CLOSED_LOCAL;
          }
          break;
      }

      DCHECK_NE(io_state_, STATE_IDLE);

      response_time_ = response_time;
      recv_first_byte_time_ = recv_first_byte_time;
      SaveResponseHeaders(response_headers);

      break;

    case READY_FOR_DATA_OR_TRAILERS:
      // Second header block is trailers.
      if (type_ == SPDY_PUSH_STREAM) {
        const std::string error("Trailers not supported for push stream.");
        LogStreamError(ERR_SPDY_PROTOCOL_ERROR, error);
        session_->ResetStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR, error);
        return;
      }

      response_state_ = TRAILERS_RECEIVED;
      delegate_->OnTrailers(response_headers);
      break;

    case TRAILERS_RECEIVED:
      // No further header blocks are allowed after trailers.
      const std::string error("Header block received after trailers.");
      LogStreamError(ERR_SPDY_PROTOCOL_ERROR, error);
      session_->ResetStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR, error);
      break;
  }
}

bool SpdyStream::ShouldRetryRSTPushStream() const {
  // Retry if the stream is a pushed stream, has been claimed, but did not yet
  // receive response headers
  return (response_headers_.empty() && type_ == SPDY_PUSH_STREAM && delegate_);
}

void SpdyStream::OnPushPromiseHeadersReceived(spdy::SpdyHeaderBlock headers,
                                              GURL url) {
  CHECK(!request_headers_valid_);
  CHECK_EQ(io_state_, STATE_IDLE);
  CHECK_EQ(type_, SPDY_PUSH_STREAM);
  DCHECK(!delegate_);

  io_state_ = STATE_RESERVED_REMOTE;
  request_headers_ = std::move(headers);
  request_headers_valid_ = true;
}

void SpdyStream::OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) {
  DCHECK(session_->IsStreamActive(stream_id_));

  if (response_state_ == READY_FOR_HEADERS) {
    const std::string error("DATA received before headers.");
    LogStreamError(ERR_SPDY_PROTOCOL_ERROR, error);
    session_->ResetStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR, error);
    return;
  }

  if (response_state_ == TRAILERS_RECEIVED && buffer) {
    const std::string error("DATA received after trailers.");
    LogStreamError(ERR_SPDY_PROTOCOL_ERROR, error);
    session_->ResetStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR, error);
    return;
  }

  if (io_state_ == STATE_HALF_CLOSED_REMOTE) {
    const std::string error("DATA received on half-closed (remove) stream.");
    LogStreamError(ERR_SPDY_STREAM_CLOSED, error);
    session_->ResetStream(stream_id_, ERR_SPDY_STREAM_CLOSED, error);
    return;
  }

  // Track our bandwidth.
  recv_bytes_ += buffer ? buffer->GetRemainingSize() : 0;
  recv_last_byte_time_ = base::TimeTicks::Now();

  // If we're still buffering data for a push stream, we will do the check for
  // data received with incomplete headers in PushedStreamReplay().
  if (io_state_ == STATE_HALF_CLOSED_LOCAL_UNCLAIMED) {
    DCHECK_EQ(type_, SPDY_PUSH_STREAM);
    // It should be valid for this to happen in the server push case.
    // We'll return received data when delegate gets attached to the stream.
    if (buffer) {
      pending_recv_data_.push_back(std::move(buffer));
    } else {
      pending_recv_data_.push_back(NULL);
      // Note: we leave the stream open in the session until the stream
      //       is claimed.
    }
    return;
  }

  CHECK(!IsClosed());

  if (!buffer) {
    if (io_state_ == STATE_OPEN) {
      io_state_ = STATE_HALF_CLOSED_REMOTE;
    } else if (io_state_ == STATE_HALF_CLOSED_LOCAL) {
      io_state_ = STATE_CLOSED;
      // Deletes |this|.
      session_->CloseActiveStream(stream_id_, OK);
    } else {
      NOTREACHED() << io_state_;
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
      base::Bind(&SpdyStream::OnReadBufferConsumed, GetWeakPtr()));

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
  // PRIORITY writes are allowed at any time and do not trigger a state update.
  if (frame_type == spdy::SpdyFrameType::PRIORITY) {
    return;
  }

  // Frame types reserved in
  // https://tools.ietf.org/html/draft-bishop-httpbis-grease-00 ought to be
  // ignored.
  if (static_cast<uint8_t>(frame_type) % 0x1f == 0x0b)
    return;

  DCHECK_NE(type_, SPDY_PUSH_STREAM);
  CHECK(frame_type == spdy::SpdyFrameType::HEADERS ||
        frame_type == spdy::SpdyFrameType::DATA)
      << frame_type;

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
      NOTREACHED() << io_state_;
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

  send_bytes_ += frame_payload_size;

  // If more data is available to send, dispatch it and
  // return that the write operation is still ongoing.
  pending_send_data_->DidConsume(frame_payload_size);
  if (pending_send_data_->BytesRemaining() > 0) {
    QueueNextDataFrame();
    return ERR_IO_PENDING;
  } else {
    pending_send_data_ = NULL;
    return OK;
  }
}

void SpdyStream::LogStreamError(int error, const std::string& description) {
  net_log_.AddEvent(NetLogEventType::HTTP2_STREAM_ERROR,
                    base::Bind(&NetLogSpdyStreamErrorCallback, stream_id_,
                               error, &description));
}

void SpdyStream::OnClose(int status) {
  // In most cases, the stream should already be CLOSED. The exception is when a
  // SpdySession is shutting down while the stream is in an intermediate state.
  io_state_ = STATE_CLOSED;
  if (status == ERR_SPDY_RST_STREAM_NO_ERROR_RECEIVED) {
    if (response_state_ == READY_FOR_HEADERS) {
      status = ERR_SPDY_PROTOCOL_ERROR;
    } else {
      status = OK;
    }
  }
  response_status_ = status;
  Delegate* delegate = delegate_;
  delegate_ = NULL;
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

int SpdyStream::SendRequestHeaders(spdy::SpdyHeaderBlock request_headers,
                                   SpdySendStatus send_status) {
  CHECK_NE(type_, SPDY_PUSH_STREAM);
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
  CHECK_NE(type_, SPDY_PUSH_STREAM);
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

bool SpdyStream::WasAlpnNegotiated() const {
  return session_->WasAlpnNegotiated();
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
  net_log_.AddEvent(NetLogEventType::HTTP2_STREAM_FLOW_CONTROL_UNSTALLED,
                    NetLog::IntCallback("stream_id", stream_id_));
  send_stalled_by_flow_control_ = false;
  QueueNextDataFrame();
  return DoNotRequeue;
}

bool SpdyStream::IsClosed() const {
  return io_state_ == STATE_CLOSED;
}

bool SpdyStream::IsLocallyClosed() const {
  return io_state_ == STATE_HALF_CLOSED_LOCAL_UNCLAIMED ||
      io_state_ == STATE_HALF_CLOSED_LOCAL ||
      io_state_ == STATE_CLOSED;
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
  if (type_ == SPDY_PUSH_STREAM) {
    load_timing_info->push_start = recv_first_byte_time_;
    bool done_receiving = IsClosed() || (!pending_recv_data_.empty() &&
                                         !pending_recv_data_.back());
    if (done_receiving)
      load_timing_info->push_end = recv_last_byte_time_;
  }
  return result;
}

size_t SpdyStream::EstimateMemoryUsage() const {
  // TODO(xunjieli): https://crbug.com/669108. Estimate |pending_send_data_|
  // once scoped_refptr support is in.
  return base::trace_event::EstimateMemoryUsage(url_) +
         base::trace_event::EstimateMemoryUsage(request_headers_) +
         base::trace_event::EstimateMemoryUsage(pending_recv_data_) +
         base::trace_event::EstimateMemoryUsage(response_headers_);
}

void SpdyStream::UpdateHistograms() {
  // We need at least the receive timers to be filled in, as otherwise
  // metrics can be bogus.
  if (recv_first_byte_time_.is_null() || recv_last_byte_time_.is_null())
    return;

  base::TimeTicks effective_send_time;
  if (type_ == SPDY_PUSH_STREAM) {
    // Push streams shouldn't have |send_time_| filled in.
    DCHECK(send_time_.is_null());
    effective_send_time = recv_first_byte_time_;
  } else {
    // For non-push streams, we also need |send_time_| to be filled
    // in.
    if (send_time_.is_null())
      return;
    effective_send_time = send_time_;
  }

  UMA_HISTOGRAM_TIMES("Net.SpdyStreamTimeToFirstByte",
                      recv_first_byte_time_ - effective_send_time);
  UMA_HISTOGRAM_TIMES("Net.SpdyStreamDownloadTime",
                      recv_last_byte_time_ - recv_first_byte_time_);
  UMA_HISTOGRAM_TIMES("Net.SpdyStreamTime",
                      recv_last_byte_time_ - effective_send_time);

  UMA_HISTOGRAM_COUNTS_1M("Net.SpdySendBytes", send_bytes_);
  UMA_HISTOGRAM_COUNTS_1M("Net.SpdyRecvBytes", recv_bytes_);
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
  std::unique_ptr<SpdyBuffer> data_buffer(
      session_->CreateDataBuffer(stream_id_, pending_send_data_.get(),
                                 pending_send_data_->BytesRemaining(), flags));
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
    data_buffer->AddConsumeCallback(base::Bind(
        &SpdyStream::OnWriteBufferConsumed, GetWeakPtr(), payload_size));
  }

  session_->EnqueueStreamWrite(
      GetWeakPtr(), spdy::SpdyFrameType::DATA,
      std::make_unique<SimpleBufferProducer>(std::move(data_buffer)));
}

void SpdyStream::SaveResponseHeaders(
    const spdy::SpdyHeaderBlock& response_headers) {
  DCHECK(response_headers_.empty());
  if (response_headers.find("transfer-encoding") != response_headers.end()) {
    session_->ResetStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR,
                          "Received transfer-encoding header");
    return;
  }

  for (spdy::SpdyHeaderBlock::const_iterator it = response_headers.begin();
       it != response_headers.end(); ++it) {
    response_headers_.insert(*it);
  }

  // If delegate is not yet attached, OnHeadersReceived() will be called after
  // the delegate gets attached to the stream.
  if (!delegate_)
    return;

  if (type_ == SPDY_PUSH_STREAM) {
    // OnPushPromiseHeadersReceived() must have been called before
    // OnHeadersReceived().
    DCHECK(request_headers_valid_);
    delegate_->OnHeadersReceived(response_headers_, &request_headers_);
  } else {
    delegate_->OnHeadersReceived(response_headers_, nullptr);
  }
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
    STATE_CASE(STATE_HALF_CLOSED_LOCAL_UNCLAIMED);
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
