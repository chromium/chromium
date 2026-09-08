// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_basic_stream_adapters.h"

#include <cstring>
#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/stream_socket.h"
#include "net/socket/stream_socket_handle.h"
#include "net/spdy/spdy_buffer.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/quic_header_list.h"
#include "net/third_party/quiche/src/quiche/quic/core/http/spdy_utils.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_error_codes.h"
#include "net/websockets/websocket_quic_spdy_stream.h"

namespace net {
struct NetworkTrafficAnnotationTag;

WebSocketClientSocketHandleAdapter::WebSocketClientSocketHandleAdapter(
    std::unique_ptr<StreamSocketHandle> connection)
    : connection_(std::move(connection)) {}

WebSocketClientSocketHandleAdapter::~WebSocketClientSocketHandleAdapter() =
    default;

int WebSocketClientSocketHandleAdapter::Read(IOBuffer* buf,
                                             int buf_len,
                                             CompletionOnceCallback callback) {
  return connection_->socket()->Read(buf, buf_len, std::move(callback));
}

int WebSocketClientSocketHandleAdapter::Write(
    IOBuffer* buf,
    int buf_len,
    bool is_final_write,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  CHECK(!write_callback_);
  CHECK_GT(buf_len, 0);

  // `is_final_write` is ignored: StreamSocket offers no half-close, and RFC
  // 6455 section 7.1.1 has the server close the TCP connection once the closing
  // handshake is complete, with the client closing its end afterwards. Close()
  // does that by disconnecting the socket.
  int result = DrainWriteBuffer(base::MakeRefCounted<DrainableIOBuffer>(
                                    buf, base::checked_cast<size_t>(buf_len)),
                                traffic_annotation);
  if (result == ERR_IO_PENDING) {
    write_callback_ = std::move(callback);
  }
  return result;
}

int WebSocketClientSocketHandleAdapter::DrainWriteBuffer(
    scoped_refptr<DrainableIOBuffer> buffer,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  while (buffer->BytesRemaining() > 0) {
    // The use of base::Unretained() here is safe because this object owns the
    // socket, and destroying it prevents any further callbacks.
    int result = connection_->socket()->Write(
        buffer.get(), buffer->BytesRemaining(),
        base::BindOnce(&WebSocketClientSocketHandleAdapter::OnWriteComplete,
                       base::Unretained(this), buffer, traffic_annotation),
        traffic_annotation);
    if (result < 0) {
      return result;
    }
    CHECK_NE(0, result);
    buffer->DidConsume(result);
  }
  return buffer->BytesConsumed();
}

void WebSocketClientSocketHandleAdapter::OnWriteComplete(
    scoped_refptr<DrainableIOBuffer> buffer,
    const NetworkTrafficAnnotationTag& traffic_annotation,
    int result) {
  CHECK(write_callback_);
  CHECK_NE(ERR_IO_PENDING, result);
  CHECK_NE(0, result);

  if (result > 0) {
    buffer->DidConsume(result);
    result = DrainWriteBuffer(std::move(buffer), traffic_annotation);
  }

  if (result != ERR_IO_PENDING) {
    // Might destroy `this`.
    auto callback = std::move(write_callback_);
    std::move(callback).Run(result);
  }
}

void WebSocketClientSocketHandleAdapter::Disconnect() {
  connection_->socket()->Disconnect();
  // The socket makes no further callbacks after Disconnect(), so drop any
  // stored write callback.
  write_callback_.Reset();
}

bool WebSocketClientSocketHandleAdapter::is_initialized() const {
  return connection_->is_initialized();
}

WebSocketSpdyStreamAdapter::WebSocketSpdyStreamAdapter(
    base::WeakPtr<SpdyStream> stream,
    Delegate* delegate,
    NetLogWithSource net_log)
    : stream_(std::move(stream)),
      delegate_(delegate),
      net_log_(std::move(net_log)) {
  stream_->SetDelegate(this);
}

WebSocketSpdyStreamAdapter::~WebSocketSpdyStreamAdapter() {
  if (stream_) {
    // DetachDelegate() also cancels the stream.
    stream_->DetachDelegate();
  }
}

void WebSocketSpdyStreamAdapter::DetachDelegate() {
  delegate_ = nullptr;
}

int WebSocketSpdyStreamAdapter::Read(IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  base::AutoReset disallow_callback(&in_callback_disallowed_scope_, true);

  DCHECK(!read_callback_);
  DCHECK_LT(0, buf_len);

  DCHECK(!read_buffer_);
  read_buffer_ = buf;
  // |read_length_| is size_t and |buf_len| is a non-negative int, therefore
  // conversion is always valid.
  DCHECK(!read_length_);
  read_length_ = buf_len;

  if (!read_data_.IsEmpty())
    return CopySavedReadDataIntoBuffer();

  if (!stream_ || end_stream_received_) {
    read_buffer_ = nullptr;
    read_length_ = 0u;
    return stream_error_;
  }

  read_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int WebSocketSpdyStreamAdapter::Write(
    IOBuffer* buf,
    int buf_len,
    bool is_final_write,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  base::AutoReset disallow_callback(&in_callback_disallowed_scope_, true);

  CHECK(headers_sent_);
  DCHECK(!write_callback_);
  DCHECK(callback);
  DCHECK_LT(0, buf_len);

  if (!stream_ || end_stream_sent_) {
    return stream_error_;
  }

  write_callback_ = std::move(callback);
  write_length_ = buf_len;
  end_stream_sent_ = is_final_write;
  stream_->SendData(buf, buf_len,
                    is_final_write ? NO_MORE_DATA_TO_SEND : MORE_DATA_TO_SEND);
  return ERR_IO_PENDING;
}

void WebSocketSpdyStreamAdapter::Disconnect() {
  if (stream_) {
    // A stream still alive here has not completed the orderly close of RFC
    // 8441 section 5: either our END_STREAM was never sent, or the peer has
    // not sent theirs and its read side is being abandoned. Both are the RST
    // exception, and DetachDelegate() cancels the stream. An orderly close
    // needs no work here, because the END_STREAM rides on the Close frame
    // written through Write() and the stream is gone by the time the layer
    // above disconnects.
    stream_->DetachDelegate();
    stream_ = nullptr;
  }
}

bool WebSocketSpdyStreamAdapter::is_initialized() const {
  return true;
}

// SpdyStream::Delegate methods.
void WebSocketSpdyStreamAdapter::OnHeadersSent() {
  headers_sent_ = true;
  if (delegate_)
    delegate_->OnHeadersSent();
}

void WebSocketSpdyStreamAdapter::OnEarlyHintsReceived(
    const quiche::HttpHeaderBlock& headers) {
  // This callback should not be called for a WebSocket handshake.
  NOTREACHED();
}

void WebSocketSpdyStreamAdapter::OnHeadersReceived(
    const quiche::HttpHeaderBlock& response_headers) {
  if (delegate_)
    delegate_->OnHeadersReceived(response_headers);
}

void WebSocketSpdyStreamAdapter::OnDataReceived(
    std::unique_ptr<SpdyBuffer> buffer) {
  std::optional<int> result;

  if (!buffer) {
    // The peer half-closed the stream with an END_STREAM, which RFC 8441
    // section 5 makes the orderly closure. Report it to the reader as the end
    // of the stream once anything already buffered has been read; the layer
    // above answers with a Close frame, and the END_STREAM that closes our
    // half rides on that write. Nothing is sent from here, both because a
    // write may be in flight and because doing so would finish the send side
    // before that Close frame could be written.
    CHECK(headers_sent_);
    end_stream_received_ = true;
    if (read_callback_ && read_data_.IsEmpty()) {
      read_buffer_ = nullptr;
      read_length_ = 0u;
      result = ERR_CONNECTION_CLOSED;
    }
  } else {
    read_data_.Enqueue(std::move(buffer));
    if (read_callback_) {
      result = CopySavedReadDataIntoBuffer();
    }
  }

  if (result.has_value()) {
    CHECK(!in_callback_disallowed_scope_);
    auto callback = std::move(read_callback_);
    std::move(callback).Run(*result);
  }
}

void WebSocketSpdyStreamAdapter::OnDataSent() {
  CHECK(write_callback_);
  CHECK(!in_callback_disallowed_scope_);
  auto callback = std::move(write_callback_);
  std::move(callback).Run(write_length_);
}

void WebSocketSpdyStreamAdapter::OnTrailers(
    const quiche::HttpHeaderBlock& trailers) {}

void WebSocketSpdyStreamAdapter::OnClose(int status) {
  CHECK_NE(ERR_IO_PENDING, status);
  CHECK_LE(status, 0);

  if (status == OK) {
    status = ERR_CONNECTION_CLOSED;
  }

  stream_error_ = status;
  stream_ = nullptr;

  auto weak_this = weak_factory_.GetWeakPtr();

  if (in_callback_disallowed_scope_) {
    // We don't want to call the delegate while we're in Write or Read, simply
    // re-queue to handle delegate tear-down later.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&WebSocketSpdyStreamAdapter::OnClose,
                                  std::move(weak_this), status));
    return;
  }

  auto read_callback = std::move(read_callback_);
  auto write_callback = std::move(write_callback_);
  if (read_callback) {
    // Might destroy |this|.
    std::move(read_callback).Run(status);
  }
  if (weak_this && write_callback) {
    // Might destroy |this|.
    std::move(write_callback).Run(status);
  }

  // Delay calling delegate_->OnClose() until all buffered data are read.
  if (weak_this && read_data_.IsEmpty() && delegate_) {
    // Might destroy |this|.
    delegate_->OnClose(status);
  }
}

bool WebSocketSpdyStreamAdapter::CanGreaseFrameType() const {
  return false;
}

NetLogSource WebSocketSpdyStreamAdapter::source_dependency() const {
  return net_log_.source();
}

int WebSocketSpdyStreamAdapter::CopySavedReadDataIntoBuffer() {
  DCHECK(read_buffer_);
  DCHECK(read_length_);
  int rv = read_data_.Dequeue(read_buffer_->first(read_length_));
  read_buffer_ = nullptr;
  read_length_ = 0u;

  // Stream has been destroyed earlier but delegate_->OnClose() call was
  // delayed until all buffered data are read.  PostTask so that Read() can
  // return beforehand.
  if (!stream_ && delegate_ && read_data_.IsEmpty()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&WebSocketSpdyStreamAdapter::CallDelegateOnClose,
                       weak_factory_.GetWeakPtr()));
  }

  return rv;
}

void WebSocketSpdyStreamAdapter::CallDelegateOnClose() {
  if (delegate_)
    delegate_->OnClose(stream_error_);
}

WebSocketQuicStreamAdapter::WebSocketQuicStreamAdapter(
    WebSocketQuicSpdyStream* websocket_quic_spdy_stream,
    Delegate* delegate)
    : websocket_quic_spdy_stream_(websocket_quic_spdy_stream),
      delegate_(delegate) {
  websocket_quic_spdy_stream_->set_delegate(this);
}

WebSocketQuicStreamAdapter::~WebSocketQuicStreamAdapter() {
  if (websocket_quic_spdy_stream_) {
    websocket_quic_spdy_stream_->DetachDelegate();
  }
}

size_t WebSocketQuicStreamAdapter::WriteHeaders(
    quiche::HttpHeaderBlock header_block,
    bool fin) {
  return websocket_quic_spdy_stream_->WriteHeaders(std::move(header_block), fin,
                                                   nullptr);
}

void WebSocketQuicStreamAdapter::SetPriority(
    const quic::QuicStreamPriority& priority) {
  if (websocket_quic_spdy_stream_) {
    websocket_quic_spdy_stream_->SetPriority(priority);
  }
}

// WebSocketBasicStream::Adapter methods.
int WebSocketQuicStreamAdapter::Read(IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  base::AutoReset disallow_callback(&in_callback_disallowed_scope_, true);

  if (!websocket_quic_spdy_stream_) {
    return stream_error_;
  }

  int rv = websocket_quic_spdy_stream_->Read(buf, buf_len);

  // Consume the peer's FIN once it has been read, which closes the read side
  // and lets the session retire the stream. Other byte stream readers over
  // QUIC do the same, see WebTransportStreamAdapter::Read(). Our own FIN is
  // not sent here: the peer's FIN only half-closes the stream, and the layer
  // above still owes the peer a WebSocket Close frame in response to the one
  // this read may have just delivered (RFC 6455 section 5.5.1). DetachDelegate
  // sends the FIN once that layer is done with the stream.
  if (websocket_quic_spdy_stream_ &&
      websocket_quic_spdy_stream_->IsDoneReading() &&
      !websocket_quic_spdy_stream_->read_side_closed()) {
    websocket_quic_spdy_stream_->OnFinRead();
  }

  if (rv != ERR_IO_PENDING) {
    return rv;
  }

  read_callback_ = std::move(callback);
  read_buffer_ = buf;
  read_length_ = buf_len;
  return ERR_IO_PENDING;
}

int WebSocketQuicStreamAdapter::Write(
    IOBuffer* buf,
    int buf_len,
    bool is_final_write,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  base::AutoReset disallow_callback(&in_callback_disallowed_scope_, true);

  DCHECK(!write_callback_);
  CHECK_GT(buf_len, 0);
  DCHECK(callback);

  if (!websocket_quic_spdy_stream_) {
    return stream_error_;
  }

  // The send side must still be open. It is finished once a FIN has been
  // queued, on which WriteOrBufferBody() would QUIC_BUG, and a peer
  // STOP_SENDING closes the write side while the stream stays alive and
  // readable, on which WriteOrBufferBody() would drop the data with only a log
  // line and leave this to report a successful write of bytes that never left.
  if (websocket_quic_spdy_stream_->fin_buffered() ||
      websocket_quic_spdy_stream_->write_side_closed()) {
    return ERR_CONNECTION_CLOSED;
  }

  // Queue data to the QUIC stream. WriteOrBufferBody() either sends the data
  // immediately if flow control allows, or buffers it internally.
  // It can also synchronously close the connection on socket write errors.
  websocket_quic_spdy_stream_->WriteOrBufferBody(
      {buf->data(), static_cast<size_t>(buf_len)}, is_final_write);

  if (!websocket_quic_spdy_stream_) {
    // The write may have synchronously called OnClose(), so provide the
    // correct write bytes in this case.
    if (is_final_write && stream_error_ == ERR_CONNECTION_CLOSED) {
      return buf_len;
    }
    return stream_error_;
  }

  // Check CanWriteNewData() after queuing rather than before. This is necessary
  // because WriteOrBufferBody() may have caused the send buffer to cross its
  // threshold or exhausted the flow control window, blocking further writes.
  // If the stream can still accept new data, complete the write synchronously.
  // Otherwise, save |callback| to invoke later when OnCanWriteNewData() is
  // called (triggered when buffered data is sent and buffer size drops below
  // the threshold, allowing more data to be accepted).
  if (websocket_quic_spdy_stream_->CanWriteNewData()) {
    return buf_len;
  }

  write_length_ = buf_len;
  write_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

void WebSocketQuicStreamAdapter::Disconnect() {
  if (websocket_quic_spdy_stream_) {
    websocket_quic_spdy_stream_->DetachDelegate();
    ClearStream();
  }
}

bool WebSocketQuicStreamAdapter::is_initialized() const {
  return true;
}

uint64_t WebSocketQuicStreamAdapter::stream_bytes_read() const {
  return websocket_quic_spdy_stream_
             ? websocket_quic_spdy_stream_->stream_bytes_read()
             : 0;
}

uint64_t WebSocketQuicStreamAdapter::stream_bytes_written() const {
  return websocket_quic_spdy_stream_
             ? websocket_quic_spdy_stream_->stream_bytes_written()
             : 0;
}

// WebSocketQuicSpdyStream::Delegate methods.

void WebSocketQuicStreamAdapter::OnInitialHeadersComplete(
    bool fin,
    size_t frame_len,
    const quic::QuicHeaderList& quic_header_list) {
  int64_t content_length = -1;
  quiche::HttpHeaderBlock response_headers;
  if (!quic::SpdyUtils::CopyAndValidateHeaders(
          quic_header_list, &content_length, &response_headers)) {
    DLOG(ERROR) << "Failed to parse header list: "
                << quic_header_list.DebugString();
    websocket_quic_spdy_stream_->ConsumeHeaderList();
    websocket_quic_spdy_stream_->Reset(quic::QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }
  websocket_quic_spdy_stream_->ConsumeHeaderList();
  delegate_->OnHeadersReceived(response_headers);
}

void WebSocketQuicStreamAdapter::OnBodyAvailable() {
  if (!websocket_quic_spdy_stream_->FinishedReadingHeaders()) {
    // Buffer the data in the sequencer until the headers have been read.
    return;
  }

  // Handle in the case there's bytes to read *or* an empty FIN body arrived.
  if (!websocket_quic_spdy_stream_->HasBytesToRead() &&
      !websocket_quic_spdy_stream_->IsDoneReading()) {
    return;
  }

  if (!read_callback_) {
    // Wait for Read() to be called.
    return;
  }

  DCHECK(read_buffer_);
  CHECK_GT(read_length_, 0);

  IOBuffer* read_buffer = std::exchange(read_buffer_, nullptr);
  int read_length = std::exchange(read_length_, 0);
  int result = Read(read_buffer, read_length, CompletionOnceCallback());

  // There are either bytes to read or a FIN to report as EOF, so `Read()`
  // cannot come back pending and never stores the null callback given above.
  CHECK_NE(ERR_IO_PENDING, result);
  CHECK(!in_callback_disallowed_scope_);
  auto callback = std::move(read_callback_);
  std::move(callback).Run(result);
}

void WebSocketQuicStreamAdapter::OnClose(int status) {
  CHECK_LE(status, 0);

  if (status == OK) {
    status = ERR_CONNECTION_CLOSED;
  }
  stream_error_ = status;

  ClearStream();

  base::WeakPtr<WebSocketQuicStreamAdapter> weak_this =
      weak_factory_.GetWeakPtr();

  if (in_callback_disallowed_scope_) {
    // We don't want to call the delegate while we're in Write or Read, simply
    // re-queue to handle delegate tear-down later.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&WebSocketQuicStreamAdapter::OnClose,
                                  std::move(weak_this), status));
    return;
  }

  auto read_callback = std::move(read_callback_);
  auto write_callback = std::move(write_callback_);
  if (read_callback) {
    // Might destroy |this|.
    std::move(read_callback).Run(status);
  }
  if (weak_this && write_callback) {
    // Might destroy |this|.
    std::move(write_callback).Run(status);
  }

  if (weak_this && delegate_) {
    delegate_->OnClose(status);
  }
}

void WebSocketQuicStreamAdapter::ClearStream() {
  websocket_quic_spdy_stream_ = nullptr;
}

void WebSocketQuicStreamAdapter::OnCanWriteNewData() {
  if (write_callback_) {
    CHECK_GT(write_length_, 0);
    CHECK(!in_callback_disallowed_scope_);
    auto callback = std::move(write_callback_);
    std::move(callback).Run(write_length_);
  }
}

}  // namespace net
