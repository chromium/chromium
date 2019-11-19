// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_ADAPTERS_H_
#define NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_ADAPTERS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_export.h"
#include "net/spdy/spdy_read_queue.h"
#include "net/spdy/spdy_stream.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/websockets/websocket_basic_stream.h"

namespace net {

class ClientSocketHandle;
class IOBuffer;
class SpdyBuffer;

// Trivial adapter to make WebSocketBasicStream use a TCP/IP or TLS socket.
class NET_EXPORT_PRIVATE WebSocketClientSocketHandleAdapter
    : public WebSocketBasicStream::Adapter {
 public:
  WebSocketClientSocketHandleAdapter() = delete;
  explicit WebSocketClientSocketHandleAdapter(
      std::unique_ptr<ClientSocketHandle> connection);
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
  std::unique_ptr<ClientSocketHandle> connection_;
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
        const spdy::SpdyHeaderBlock& response_headers) = 0;
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
  void OnHeadersReceived(
      const spdy::SpdyHeaderBlock& response_headers,
      const spdy::SpdyHeaderBlock* pushed_request_headers) override;
  void OnDataReceived(std::unique_ptr<SpdyBuffer> buffer) override;
  void OnDataSent() override;
  void OnTrailers(const spdy::SpdyHeaderBlock& trailers) override;
  void OnClose(int status) override;
  NetLogSource source_dependency() const override;

 private:
  // Copy data from read_data_ to read_buffer_.
  int CopySavedReadDataIntoBuffer();

  // Call WebSocketSpdyStreamAdapter::Delegate::OnClose().
  void CallDelegateOnClose();

  // True if SpdyStream::Delegate::OnHeadersSent() has been called.
  // SpdyStream::SendData() must not be called before that.
  bool headers_sent_;

  // The underlying SpdyStream.
  base::WeakPtr<SpdyStream> stream_;

  // The error code with which SpdyStream was closed.
  int stream_error_;

  Delegate* delegate_;

  // Buffer data pushed by SpdyStream until read through Read().
  SpdyReadQueue read_data_;

  // Read buffer and length used for both synchronous and asynchronous
  // read operations.
  IOBuffer* read_buffer_;
  size_t read_length_;

  // Read callback saved for asynchronous reads.
  // Whenever |read_data_| is not empty, |read_callback_| must be null.
  CompletionOnceCallback read_callback_;

  // Write length saved to be passed to |write_callback_|.  This is necessary
  // because SpdyStream::Delegate::OnDataSent() does not pass number of bytes
  // written.
  int write_length_;

  // Write callback saved for asynchronous writes (all writes are asynchronous).
  CompletionOnceCallback write_callback_;

  NetLogWithSource net_log_;

  base::WeakPtrFactory<WebSocketSpdyStreamAdapter> weak_factory_{this};
};

}  // namespace net

#endif  // NET_WEBSOCKETS_WEBSOCKET_BASIC_STREAM_ADAPTERS_H_
