// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_ADAPTERS_H_
#define NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_ADAPTERS_H_

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/spdy/spdy_read_queue.h"
#include "net/spdy/spdy_stream.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_basic_stream.h"
#include "net/websockets/websocket_quic_spdy_stream.h"

namespace quic {
class QuicHeaderList;
}  // namespace quic

namespace net {

class StreamSocketHandle;
class IOBuffer;
class SpdyBuffer;
struct NetworkTrafficAnnotationTag;

// Trivial adapter to make WebSocketBasicStream use a TCP/IP or TLS socket.
class NET_EXPORT_PRIVATE WebSocketClientSocketHandleAdapter
    : public WebSocketBasicStream::Adapter {
 public:
  WebSocketClientSocketHandleAdapter() = delete;
  explicit WebSocketClientSocketHandleAdapter(
      std::unique_ptr<StreamSocketHandle> connection);
  ~WebSocketClientSocketHandleAdapter() override;

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  void Disconnect() override;
  bool is_initialized() const override;

 private:
  std::unique_ptr<StreamSocketHandle> connection_;
};

// Adapter to make WebSocketBasicStream use an HTTP/2 stream.
// Sets itself as a delegate of the SpdyStream, and forwards headers-related
// methods to WebSocketHttp2HandshakeStream, which implements
// WebSocketSpdyStreamAdapter::Delegate.  After the handshake, ownership of this
// object can be passed to WebSocketBasicStream, which can read and write using
// a ClientSocketHandle-like interface.
class NET_EXPORT_PRIVATE WebSocketSpdyStreamAdapter
    : public WebSocketBasicStream::Adapter,
      public SpdyStream::Delegate {
 public:
  // Interface for forwarding SpdyStream::Delegate methods necessary for the
  // handshake.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnHeadersSent() = 0;
    virtual void OnHeadersReceived(
        const quiche::HttpHeaderBlock& response_headers) = 0;
    // Might destroy |this|.
    virtual void OnClose(int status) = 0;
  };

  // |delegate| must be valid until DetachDelegate() is called.
  WebSocketSpdyStreamAdapter(base::WeakPtr<SpdyStream> stream,
                             Delegate* delegate,
                             NetLogWithSource net_log);
  ~WebSocketSpdyStreamAdapter() override;

  // Called by WebSocketSpdyStreamAdapter::Delegate before it is destroyed.
  void DetachDelegate();

  // WebSocketBasicStream::Adapter methods.

  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;

  // Write() must not be called before Delegate::OnHeadersSent() is called.
  // Write() always returns asynchronously.
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;

  void Disconnect() override;
  bool is_initialized() const override;

  // SpdyStream::Delegate methods.

  void OnHeadersSent() override;
  void OnEarlyHintsReceived(const quiche::HttpHeaderBlock& headers) override;
  void OnHeadersReceived(
      const quiche::HttpHeaderBlock& response_headers) override;
  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override;
  void OnDataSent() override;
  void OnTrailers(const quiche::HttpHeaderBlock& trailers) override;
  void OnClose(int status) override;
  bool CanGreaseFrameType() const override;
  NetLogSource source_dependency() const override;

 private:
  // Copy data from read_data_ to read_buffer_.
  int CopySavedReadDataIntoBuffer();

  // Call WebSocketSpdyStreamAdapter::Delegate::OnClose().
  void CallDelegateOnClose();

  // True if SpdyStream::Delegate::OnHeadersSent() has been called.
  // SpdyStream::SendData() must not be called before that.
  bool headers_sent_ = false;

  // The underlying SpdyStream.
  base::WeakPtr<SpdyStream> stream_;

  // The error code with which SpdyStream was closed.
  int stream_error_ = ERR_CONNECTION_CLOSED;

  raw_ptr<Delegate> delegate_;

  // Buffer data pushed by SpdyStream until read through Read().
  SpdyReadQueue read_data_;

  // Read buffer and length used for both synchronous and asynchronous
  // read operations.
  raw_ptr<IOBuffer> read_buffer_ = nullptr;
  size_t read_length_ = 0u;

  // Read callback saved for asynchronous reads.
  // Whenever |read_data_| is not empty, |read_callback_| must be null.
  CompletionOnceCallback read_callback_;

  // Write length saved to be passed to |write_callback_|.  This is necessary
  // because SpdyStream::Delegate::OnDataSent() does not pass number of bytes
  // written.
  int write_length_ = 0;

  // Write callback saved for asynchronous writes (all writes are asynchronous).
  CompletionOnceCallback write_callback_;

  NetLogWithSource net_log_;

  base::WeakPtrFactory<WebSocketSpdyStreamAdapter> weak_factory_{this};
};

// Adapter to make WebSocketBasicStream use an HTTP/3 stream.
// Sets itself as a delegate of the WebSocketQuicSpdyStream. Forwards
// headers-related methods to Delegate.
class NET_EXPORT_PRIVATE WebSocketQuicStreamAdapter
    : public WebSocketBasicStream::Adapter,
      public WebSocketQuicSpdyStream::Delegate {
 public:
  // The Delegate interface is implemented by WebSocketHttp3HandshakeStream the
  // user of the WebSocketQuicStreamAdapter to receive events related to the
  // lifecycle of the Adapter.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnHeadersSent() = 0;
    virtual void OnHeadersReceived(
        const quiche::HttpHeaderBlock& response_headers) = 0;
    virtual void OnClose(int status) = 0;
  };

  explicit WebSocketQuicStreamAdapter(
      WebSocketQuicSpdyStream* websocket_quic_spdy_stream,
      Delegate* delegate);

  WebSocketQuicStreamAdapter(const WebSocketQuicStreamAdapter&) = delete;
  WebSocketQuicStreamAdapter& operator=(const WebSocketQuicStreamAdapter&) =
      delete;

  ~WebSocketQuicStreamAdapter() override;

  // Called by WebSocketQuicStreamAdapter::Delegate before it is destroyed.
  void clear_delegate() { delegate_ = nullptr; }

  size_t WriteHeaders(quiche::HttpHeaderBlock header_block, bool fin);

  // WebSocketBasicStream::Adapter methods.
  // TODO(momoka): Add functions that are needed to implement
  // WebSocketHttp3HandshakeStream.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  void Disconnect() override;
  bool is_initialized() const override;

  // WebSocketQuicSpdyStream::Delegate methods.
  void OnInitialHeadersComplete(
      bool fin,
      size_t frame_len,
      const quic::QuicHeaderList& header_list) override;
  void OnBodyAvailable() override;
  void ClearStream() override;

 private:
  //  `websocket_quic_spdy_stream_` notifies this object of its destruction,
  //  because they may be destroyed in any order.
  raw_ptr<WebSocketQuicSpdyStream> websocket_quic_spdy_stream_;

  raw_ptr<Delegate> delegate_;

  // Read buffer, length and callback used for asynchronous read operations.
  raw_ptr<IOBuffer> read_buffer_ = nullptr;
  int read_length_ = 0u;
  CompletionOnceCallback read_callback_;
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_ADAPTERS_H_
