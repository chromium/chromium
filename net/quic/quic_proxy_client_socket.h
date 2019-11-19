// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_PROXY_CLIENT_SOCKET_H_
#define NET_QUIC_QUIC_PROXY_CLIENT_SOCKET_H_

#include <cstdio>
#include <memory>
#include <string>

#include "net/base/completion_once_callback.h"
#include "net/base/load_timing_info.h"
#include "net/http/proxy_client_socket.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_chromium_client_stream.h"
#include "net/spdy/spdy_read_queue.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class HttpAuthController;

// QuicProxyClientSocket provides a socket interface to an underlying
// QuicChromiumClientStream. Bytes written to/read from a QuicProxyClientSocket
// are sent/received via STREAM frames in the underlying QUIC stream.
class NET_EXPORT_PRIVATE QuicProxyClientSocket : public ProxyClientSocket {
 public:
  // Create a socket on top of the |stream| by sending a HEADERS CONNECT
  // frame for |endpoint|.  After the response HEADERS frame is received, any
  // data read/written to the socket will be transferred in STREAM frames.
  QuicProxyClientSocket(
      std::unique_ptr<QuicChromiumClientStream::Handle> stream,
      std::unique_ptr<QuicChromiumClientSession::Handle> session,
      const std::string& user_agent,
      const HostPortPair& endpoint,
      const NetLogWithSource& net_log,
      HttpAuthController* auth_controller);

  // On destruction Disconnect() is called.
  ~QuicProxyClientSocket() override;

  // ProxyClientSocket methods:
  const HttpResponseInfo* GetConnectResponseInfo() const override;
  const scoped_refptr<HttpAuthController>& GetAuthController() const override;
  int RestartWithAuth(CompletionOnceCallback callback) override;
  bool IsUsingSpdy() const override;
  NextProto GetProxyNegotiatedProtocol() const override;
  void SetStreamPriority(RequestPriority priority) override;

  // StreamSocket implementation.
  int Connect(CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  const NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  bool WasAlpnNegotiated() const override;
  NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  void GetConnectionAttempts(ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override {}
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {}
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const SocketTag& tag) override;

  // Socket implementation.
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override;
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
    STATE_DISCONNECTED,
    STATE_GENERATE_AUTH_TOKEN,
    STATE_GENERATE_AUTH_TOKEN_COMPLETE,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_READ_REPLY,
    STATE_READ_REPLY_COMPLETE,
    STATE_CONNECT_COMPLETE
  };

  void OnIOComplete(int result);  // Callback used during connecting
  void OnReadComplete(int rv);
  void OnWriteComplete(int rv);

  // Callback for stream_->ReadInitialHeaders()
  void OnReadResponseHeadersComplete(int result);
  int ProcessResponseHeaders(const spdy::SpdyHeaderBlock& headers);

  int DoLoop(int last_io_result);
  int DoGenerateAuthToken();
  int DoGenerateAuthTokenComplete(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoReadReply();
  int DoReadReplyComplete(int result);

  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const;

  State next_state_;

  // Handle to the QUIC Stream that this sits on top of.
  std::unique_ptr<QuicChromiumClientStream::Handle> stream_;

  // Handle to the session that |stream_| belongs to.
  std::unique_ptr<QuicChromiumClientSession::Handle> session_;

  // Stores the callback for Connect().
  CompletionOnceCallback connect_callback_;
  // Stores the callback for Read().
  CompletionOnceCallback read_callback_;
  // Stores the read buffer pointer for Read().
  IOBuffer* read_buf_;
  // Stores the callback for Write().
  CompletionOnceCallback write_callback_;
  // Stores the write buffer length for Write().
  int write_buf_len_;

  // CONNECT request and response.
  HttpRequestInfo request_;
  HttpResponseInfo response_;

  spdy::SpdyHeaderBlock response_header_block_;

  // The hostname and port of the endpoint.  This is not necessarily the one
  // specified by the URL, due to Alternate-Protocol or fixed testing ports.
  const HostPortPair endpoint_;
  scoped_refptr<HttpAuthController> auth_;

  std::string user_agent_;

  // Session connect timing info.
  LoadTimingInfo::ConnectTiming connect_timing_;

  const NetLogWithSource net_log_;

  // The default weak pointer factory.
  base::WeakPtrFactory<QuicProxyClientSocket> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuicProxyClientSocket);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PROXY_CLIENT_SOCKET_H_
