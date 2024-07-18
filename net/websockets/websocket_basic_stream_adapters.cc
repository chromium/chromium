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
#include "net/third_party/quiche/src/quiche/quic/core/quic_ack_listener_interface.h"
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
    : stream_(stream), delegate_(delegate), net_log_(net_log) {
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
  NOTREACHED_IN_MIGRATION();
}

void WebSocketSpdyStreamAdapter::OnHeadersReceived(
    const quiche::HttpHeaderBlock& response_headers) {
  if (delegate_)
    delegate_->OnHeadersReceived(response_headers);
}

void WebSocketSpdyStreamAdapter::OnDataReceived(
    std::unique_ptr<SpdyBuffer> buffer) {
  if (!buffer) {
    // This is slightly wrong semantically, as it's still possible to write to
    // the stream at this point. However, if the server closes the stream
    // without waiting for a close frame from us, that means it is not
    // interested in a clean shutdown. In which case we don't need to worry
    // about sending any remaining data we might have buffered. This results in
    // a call to OnClose() which then informs our delegate.
    stream_->Close();
    return;
  }

  read_data_.Enqueue(std::move(buffer));
  if (read_callback_)
    std::move(read_callback_).Run(CopySavedReadDataIntoBuffer());
}

void WebSocketSpdyStreamAdapter::OnDataSent() {
  DCHECK(write_callback_);

  std::move(write_callback_).Run(write_length_);
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
  int rv = read_data_.Dequeue(read_buffer_->data(), read_length_);
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
    websocket_quic_spdy_stream_->set_delegate(nullptr);
  }
}

size_t WebSocketQuicStreamAdapter::WriteHeaders(
    quiche::HttpHeaderBlock header_block,
    bool fin) {
  return websocket_quic_spdy_stream_->WriteHeaders(std::move(header_block), fin,
                                                   nullptr);
}

// WebSocketBasicStream::Adapter methods.
int WebSocketQuicStreamAdapter::Read(IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  if (!websocket_quic_spdy_stream_) {
    return ERR_UNEXPECTED;
  }

  int rv = websocket_quic_spdy_stream_->Read(buf, buf_len);
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
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  // TODO(momoka): Write implementation.
  return OK;
}

void WebSocketQuicStreamAdapter::Disconnect() {
  if (websocket_quic_spdy_stream_) {
    websocket_quic_spdy_stream_->Reset(quic::QUIC_STREAM_CANCELLED);
  }
}

bool WebSocketQuicStreamAdapter::is_initialized() const {
  return true;
}

// WebSocketQuicSpdyStream::Delegate methods.

void WebSocketQuicStreamAdapter::OnInitialHeadersComplete(
    bool fin,
    size_t frame_len,
    const quic::QuicHeaderList& quic_header_list) {
  quiche::HttpHeaderBlock response_headers;
  if (!quic::SpdyUtils::CopyAndValidateHeaders(quic_header_list, nullptr,
                                               &response_headers)) {
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

  if (!websocket_quic_spdy_stream_->HasBytesToRead()) {
    return;
  }

  if (!read_callback_) {
    // Wait for Read() to be called.
    return;
  }

  DCHECK(read_buffer_);
  DCHECK_GT(read_length_, 0);

  int rv = websocket_quic_spdy_stream_->Read(read_buffer_, read_length_);

  if (rv == ERR_IO_PENDING) {
    return;
  }

  read_buffer_ = nullptr;
  read_length_ = 0;
  std::move(read_callback_).Run(rv);
}

void WebSocketQuicStreamAdapter::ClearStream() {
  if (websocket_quic_spdy_stream_) {
    websocket_quic_spdy_stream_ = nullptr;
  }
}

}  // namespace net
