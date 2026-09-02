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
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return connection_->socket()->Write(buf, buf_len, std::move(callback),
                                      traffic_annotation);
}

void WebSocketClientSocketHandleAdapter::Disconnect() {
  connection_->socket()->Disconnect();
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

  if (!stream_)
    return stream_error_;

  read_callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int WebSocketSpdyStreamAdapter::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  CHECK(headers_sent_);

  if (end_stream_sent_) {
    return stream_error_;
  }

  DCHECK(!write_callback_);
  DCHECK(callback);
  DCHECK_LT(0, buf_len);

  if (!stream_)
    return stream_error_;

  stream_->SendData(buf, buf_len, MORE_DATA_TO_SEND);
  write_callback_ = std::move(callback);
  write_length_ = buf_len;
  return ERR_IO_PENDING;
}

void WebSocketSpdyStreamAdapter::Disconnect() {
  if (stream_) {
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
  if (!buffer) {
    // The server has half-closed the stream. RFC 8441 section 5 makes HTTP/2
    // stream closure analogous to TCP connection closure, with orderly closures
    // "represented as END_STREAM flags", and RFC 6455 section 7.1.1 requires
    // the transport to be closed once the closing handshake is complete. Close
    // our half as well: simply dropping the stream locally would stop it
    // counting against SETTINGS_MAX_CONCURRENT_STREAMS here while leaving the
    // peer in half-closed(local) indefinitely, leaking a stream per WebSocket.
    CHECK(headers_sent_);
    MaybeSendEndStream();
    return;
  }

  read_data_.Enqueue(std::move(buffer));
  if (read_callback_) {
    // Avoid UAF due to C++17 sequencing rules. See crbug.com/499194333.
    auto callback = std::move(read_callback_);
    int rv = CopySavedReadDataIntoBuffer();
    std::move(callback).Run(rv);
  }
}

void WebSocketSpdyStreamAdapter::MaybeSendEndStream() {
  if (end_stream_sent_ || !stream_) {
    return;
  }

  // If there's an existing write pending, then re-queue to execute next time.
  if (write_callback_) {
    write_callback_ = base::BindOnce(
        [](base::WeakPtr<WebSocketSpdyStreamAdapter> self,
           CompletionOnceCallback cb, int result) {
          if (self) {
            self->MaybeSendEndStream();
          }
          std::move(cb).Run(result);
        },
        weak_factory_.GetWeakPtr(), std::move(write_callback_));
    return;
  }

  // Send an empty END_STREAM. This will result in OnClose() being called which
  // informs our delegate.
  end_stream_sent_ = true;
  auto buffer = base::MakeRefCounted<IOBufferWithSize>(0);
  stream_->SendData(buffer.get(), 0, NO_MORE_DATA_TO_SEND);
}

void WebSocketSpdyStreamAdapter::OnDataSent() {
  if (end_stream_sent_) {
    CHECK(!write_callback_);
    return;
  }

  DCHECK(write_callback_);

  auto write_callback = std::move(write_callback_);
  std::move(write_callback).Run(write_length_);
}

void WebSocketSpdyStreamAdapter::OnTrailers(
    const quiche::HttpHeaderBlock& trailers) {}

void WebSocketSpdyStreamAdapter::OnClose(int status) {
  DCHECK_NE(ERR_IO_PENDING, status);
  DCHECK_LE(status, 0);

  if (status == OK) {
    status = ERR_CONNECTION_CLOSED;
  }

  stream_error_ = status;
  stream_ = nullptr;

  auto self = weak_factory_.GetWeakPtr();

  if (read_callback_) {
    DCHECK(read_data_.IsEmpty());
    // Might destroy |this|.
    std::move(read_callback_).Run(status);
    if (!self)
      return;
  }
  if (write_callback_) {
    // Might destroy |this|.
    std::move(write_callback_).Run(status);
    if (!self)
      return;
  }

  // Delay calling delegate_->OnClose() until all buffered data are read.
  if (read_data_.IsEmpty() && delegate_) {
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
  if (!websocket_quic_spdy_stream_) {
    return stream_error_;
  }

  // Consuming the last of the body can close the stream, and closing the
  // stream can delete `this` from a callback, so nothing below may touch
  // members without checking `weak_this` first.
  base::WeakPtr<WebSocketQuicStreamAdapter> weak_this =
      weak_factory_.GetWeakPtr();
  int rv = websocket_quic_spdy_stream_->Read(buf, buf_len);

  // Consume the peer's FIN once it has been read, which closes the read side
  // and lets the session retire the stream. Other byte stream readers over
  // QUIC do the same, see WebTransportStreamAdapter::Read(). Our own FIN is
  // not sent here: the peer's FIN only half-closes the stream, and the layer
  // above still owes the peer a WebSocket Close frame in response to the one
  // this read may have just delivered (RFC 6455 section 5.5.1). DetachDelegate
  // sends the FIN once that layer is done with the stream.
  if (weak_this && websocket_quic_spdy_stream_ &&
      websocket_quic_spdy_stream_->IsDoneReading() &&
      !websocket_quic_spdy_stream_->read_side_closed()) {
    websocket_quic_spdy_stream_->OnFinRead();
  }

  if (rv != ERR_IO_PENDING) {
    return rv;
  }

  // If we were torn down above then we can't store the callback.
  if (!weak_this) {
    return ERR_CONNECTION_CLOSED;
  }

  read_callback_ = std::move(callback);
  read_buffer_ = buf;
  read_length_ = buf_len;
  return ERR_IO_PENDING;
}

int WebSocketQuicStreamAdapter::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!write_callback_);
  CHECK_GT(buf_len, 0);
  DCHECK(callback);

  if (!websocket_quic_spdy_stream_) {
    return stream_error_;
  }

  // The send side must still be open. A peer STOP_SENDING closes the write
  // side while the stream stays alive and readable, and WriteOrBufferBody()
  // would then drop the data with only a log line, leaving this to report a
  // successful write of bytes that never left. A queued FIN cannot happen
  // here, since DetachDelegate() only queues one as it drops the stream, but
  // it is covered too because WriteOrBufferBody() would QUIC_BUG on it.
  if (websocket_quic_spdy_stream_->fin_buffered() ||
      websocket_quic_spdy_stream_->write_side_closed()) {
    return ERR_CONNECTION_CLOSED;
  }

  // Queue data to the QUIC stream. WriteOrBufferBody() either sends the data
  // immediately if flow control allows, or buffers it internally.
  // It can also synchronously close the connection on socket write errors.
  base::WeakPtr<WebSocketQuicStreamAdapter> weak_this =
      weak_factory_.GetWeakPtr();
  websocket_quic_spdy_stream_->WriteOrBufferBody(
      {buf->data(), static_cast<size_t>(buf_len)},
      /*fin=*/false);
  // If the adapter was destroyed by a callback during the write, return
  // safely without accessing member variables.
  if (!weak_this) {
    return ERR_CONNECTION_CLOSED;
  }
  if (!websocket_quic_spdy_stream_) {
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

  // Reset the member variables before calling `Read` to ensure any delete of
  // `this` is safe.
  CompletionOnceCallback read_callback = std::move(read_callback_);
  IOBuffer* read_buffer = std::exchange(read_buffer_, nullptr);
  int read_length = std::exchange(read_length_, 0);

  // CAUTION: `this` may have been deleted by the time this returns, so only
  // locals may be touched afterwards.
  int rv = Read(read_buffer, read_length, CompletionOnceCallback());

  // There are either bytes to read or a FIN to report as EOF, so `Read()`
  // cannot come back pending and never stores the null callback given above.
  CHECK_NE(ERR_IO_PENDING, rv);
  std::move(read_callback).Run(rv);
}

void WebSocketQuicStreamAdapter::OnClose(int status) {
  CHECK_LE(status, 0);
  if (status == OK) {
    status = ERR_CONNECTION_CLOSED;
  }
  stream_error_ = status;

  base::WeakPtr<WebSocketQuicStreamAdapter> weak_this =
      weak_factory_.GetWeakPtr();
  ClearStream();

  // Running a completion callback can delete the current
  // WebSocketQuicStreamAdapter. In that case, `weak_this` becomes invalid and
  // OnClose() must return before accessing more member variables.
  if (read_callback_) {
    std::move(read_callback_).Run(status);
    if (!weak_this) {
      return;
    }
  }
  if (write_callback_) {
    std::move(write_callback_).Run(status);
    if (!weak_this) {
      return;
    }
  }
  if (delegate_) {
    delegate_->OnClose(status);
  }
}

void WebSocketQuicStreamAdapter::ClearStream() {
  websocket_quic_spdy_stream_ = nullptr;
}

void WebSocketQuicStreamAdapter::OnCanWriteNewData() {
  if (write_callback_) {
    CHECK_GT(write_length_, 0);
    std::move(write_callback_).Run(write_length_);
  }
}

}  // namespace net
