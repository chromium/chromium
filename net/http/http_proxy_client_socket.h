// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_H_
#define NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/proxy_chain.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/proxy_client_socket.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/ssl_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class GrowableIOBuffer;
class HttpStreamParser;
class IOBuffer;
class ProxyDelegate;
class StreamSocket;

// Tunnels a stream socket over an HTTP/1.1 connection.
//
// This class handles only _tunneled_ proxy connections, using `CONNECT`.
// Un-tunneled proxy requests are handled in the HTTP stream layer,
// specifically in `HttpBasicStream`.
class NET_EXPORT_PRIVATE HttpProxyClientSocket : public ProxyClientSocket {
 public:
  // Takes ownership of |socket|, which should already be connected by the time
  // Connect() is called. |socket| is assumed to be a fresh socket.
  HttpProxyClientSocket(std::unique_ptr<StreamSocket> socket,
                        const std::string& user_agent,
                        const HostPortPair& endpoint,
                        const ProxyChain& proxy_chain,
                        size_t proxy_chain_index,
                        scoped_refptr<HttpAuthController> http_auth_controller,
                        ProxyDelegate* proxy_delegate,
                        const NetworkTrafficAnnotationTag& traffic_annotation);

  HttpProxyClientSocket(const HttpProxyClientSocket&) = delete;
  HttpProxyClientSocket& operator=(const HttpProxyClientSocket&) = delete;

  // On destruction Disconnect() is called.
  ~HttpProxyClientSocket() override;

  // ProxyClientSocket implementation.
  const HttpResponseInfo* GetConnectResponseInfo() const override;
  int RestartWithAuth(CompletionOnceCallback callback) override;
  const scoped_refptr<HttpAuthController>& GetAuthController() const override;

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

 private:
  enum State {
    STATE_NONE,
    STATE_GENERATE_AUTH_TOKEN,
    STATE_GENERATE_AUTH_TOKEN_COMPLETE,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_DRAIN_BODY,
    STATE_DRAIN_BODY_COMPLETE,
    STATE_DONE,
  };

  // The size in bytes of the buffer we use to drain the response body that
  // we want to throw away.  The response body is typically a small error
  // page just a few hundred bytes long.
  static const int kDrainBodyBufferSize = 1024;

  int PrepareForAuthRestart();
  int DidDrainBodyForAuthRestart();

  void DoCallback(int result);
  void OnIOComplete(int result);

  int DoLoop(int last_io_result);
  int DoGenerateAuthToken();
  int DoGenerateAuthTokenComplete(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoDrainBody();
  int DoDrainBodyComplete(int result);

  // Returns whether |next_state_| is STATE_DONE.
  bool CheckDone();

  CompletionRepeatingCallback io_callback_;
  State next_state_ = STATE_NONE;

  // Stores the callback provided by the caller of async operations.
  CompletionOnceCallback user_callback_;

  // Bogus request info. Only its url and method fields are used.
  HttpRequestInfo request_;

  const std::string user_agent_;
  HttpResponseInfo response_;

  scoped_refptr<GrowableIOBuffer> parser_buf_;
  scoped_refptr<IOBuffer> drain_buf_;
  std::unique_ptr<StreamSocket> socket_;
  std::unique_ptr<HttpStreamParser> http_stream_parser_;

  // Whether or not |socket_| has been previously used. Once auth credentials
  // are sent, set to true.
  bool is_reused_ = false;

  // The hostname and port of the endpoint.  This is not necessarily the one
  // specified by the URL, due to Alternate-Protocol or fixed testing ports.
  const HostPortPair endpoint_;
  scoped_refptr<HttpAuthController> auth_;

  std::string request_line_;
  HttpRequestHeaders request_headers_;

  const ProxyChain proxy_chain_;
  const size_t proxy_chain_index_;

  // This delegate must outlive this proxy client socket.
  raw_ptr<ProxyDelegate> proxy_delegate_;

  // Network traffic annotation for handshaking and setup.
  const NetworkTrafficAnnotationTag traffic_annotation_;

  const NetLogWithSource net_log_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_PROXY_CLIENT_SOCKET_H_
