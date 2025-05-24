// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_PROXY_CLIENT_SOCKET_H_
#define NET_SPDY_SPDY_PROXY_CLIENT_SOCKET_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/proxy_chain.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/proxy_client_socket.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/spdy/spdy_http_stream.h"
#include "net/spdy/spdy_read_queue.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_protocol.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class IOBuffer;
class ProxyDelegate;
class SpdyStream;

// Tunnels a stream socket over an HTTP/2 connection.
class NET_EXPORT_PRIVATE SpdyProxyClientSocket : public ProxyClientSocket,
                                                 public SpdyStream::Delegate {
 public:
  // Create a socket on top of the |spdy_stream| by sending a HEADERS CONNECT
  // frame for |endpoint|.  After the response HEADERS frame is received, any
  // data read/written to the socket will be transferred in data frames. This
  // object will set itself as |spdy_stream|'s delegate.
  SpdyProxyClientSocket(const base::WeakPtr<SpdyStream>& spdy_stream,
                        const ProxyChain& proxy_chain,
                        size_t proxy_chain_index,
                        const std::string& user_agent,
                        const HostPortPair& endpoint,
                        const NetLogWithSource& source_net_log,
                        scoped_refptr<HttpAuthController> auth_controller,
                        ProxyDelegate* proxy_delegate);

  SpdyProxyClientSocket(const SpdyProxyClientSocket&) = delete;
  SpdyProxyClientSocket& operator=(const SpdyProxyClientSocket&) = delete;

  // On destruction Disconnect() is called.
  ~SpdyProxyClientSocket() override;

  // ProxyClientSocket methods:
  const HttpResponseInfo* GetConnectResponseInfo() const override;
  const scoped_refptr<HttpAuthController>& GetAuthController() const override;
  int RestartWithAuth(CompletionOnceCallback callback) override;
  void SetStreamPriority(RequestPriority priority) override;

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  const NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const SocketTag& tag) override;

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;

  // SpdyStream::Delegate implementation.
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
  enum State {
    STATE_DISCONNECTED,
    STATE_GENERATE_AUTH_TOKEN,
    STATE_GENERATE_AUTH_TOKEN_COMPLETE,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_READ_REPLY_COMPLETE,
    STATE_OPEN,
    STATE_CLOSED
  };

  // Calls `write_callback_(result)`. Used to run a callback posted to the
  // message loop.
  void RunWriteCallback(int result);

  void OnIOComplete(int result);

  int DoLoop(int last_io_result);
  int DoGenerateAuthToken();
  int DoGenerateAuthTokenComplete(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoReadReplyComplete(int result);

  // Populates |user_buffer_| with as much read data as possible
  // and returns the number of bytes read.
  size_t PopulateUserReadBuffer(char* out, size_t len);

  // Called when the peer sent END_STREAM.
  void MaybeSendEndStream();

  State next_state_ = STATE_DISCONNECTED;

  // Pointer to the SPDY Stream that this sits on top of.
  base::WeakPtr<SpdyStream> spdy_stream_;

  // Stores the callback to the layer above, called on completing Read() or
  // Connect().
  CompletionOnceCallback read_callback_;
  // Stores the callback to the layer above, called on completing Write().
  CompletionOnceCallback write_callback_;

  // CONNECT request and response.
  HttpRequestInfo request_;
  HttpResponseInfo response_;

  // The hostname and port of the endpoint.  This is not necessarily the one
  // specified by the URL, due to Alternate-Protocol or fixed testing ports.
  const HostPortPair endpoint_;
  scoped_refptr<HttpAuthController> auth_;

  const ProxyChain proxy_chain_;
  const size_t proxy_chain_index_;

  // This delegate must outlive this proxy client socket.
  const raw_ptr<ProxyDelegate> proxy_delegate_;

  std::string user_agent_;

  // We buffer the response body as it arrives asynchronously from the stream.
  SpdyReadQueue read_buffer_queue_;

  // User provided buffer for the Read() response.
  scoped_refptr<IOBuffer> user_buffer_;
  size_t user_buffer_len_ = 0;

  // User specified number of bytes to be written.
  int write_buffer_len_ = 0;

  // True if the transport socket has ever sent data.
  bool was_ever_used_ = false;

  const NetLogWithSource net_log_;
  const NetLogSource source_dependency_;

  // State for handling END_STREAM. When the peer sends a DATA frame with
  // END_STREAM, it should be treated as being equivalent to the TCP FIN bit.
  // We should send a DATA frame with END_STREAM after receiving END_STREAM
  // as the spec requires.
  enum class EndStreamState {
    kNone,
    kEndStreamReceived,
    kEndStreamSent,
  };
  EndStreamState end_stream_state_ = EndStreamState::kNone;

  base::WeakPtrFactory<SpdyProxyClientSocket> weak_factory_{this};
};

}  // namespace net

#endif  // NET_SPDY_SPDY_PROXY_CLIENT_SOCKET_H_
