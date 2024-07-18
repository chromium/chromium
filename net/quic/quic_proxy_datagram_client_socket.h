// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_PROXY_DATAGRAM_CLIENT_SOCKET_H_
#define NET_QUIC_QUIC_PROXY_DATAGRAM_CLIENT_SOCKET_H_

#include <stdint.h>

#include <queue>
#include <string_view>

#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/log/net_log.h"
#include "net/log/net_log_with_source.h"
#include "net/quic/quic_chromium_client_session.h"
#include "net/quic/quic_chromium_client_stream.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/udp_socket.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/third_party/quiche/src/quiche/common/http/http_header_block.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/scheme_host_port.h"

namespace net {

class ProxyDelegate;

// A client socket that uses a QUIC proxy as the transport layer.
//
// Given that DatagramClientSocket class contains numerous methods tailored for
// UDP, many methods from DatagramClientSocket are left as stubs.
// ConnectViaStream is used to connect this socket over the provided QUIC stream
// to send and receive datagrams.
class NET_EXPORT_PRIVATE QuicProxyDatagramClientSocket
    : public DatagramClientSocket,
      public quic::QuicSpdyStream::Http3DatagramVisitor {
 public:
  // Initializes a QuicProxyDatagramClientSocket with the provided network
  // log (source_net_log) and destination URL. The destination URL is
  // derived from a URI Template containing the variables "target_host"
  // and "target_port". These variables need to be prepopulated by the caller of
  // this constructor. Datagrams will be sent to this target server.
  //
  // The `proxy_chain` describes the connection to the proxies over which
  // this socket carries data, which thus must have at least one proxy.
  QuicProxyDatagramClientSocket(const GURL& url,
                                const ProxyChain& proxy_chain,
                                const std::string& user_agent,
                                const NetLogWithSource& source_net_log,
                                ProxyDelegate* proxy_delegate);

  QuicProxyDatagramClientSocket(const QuicProxyDatagramClientSocket&) = delete;
  QuicProxyDatagramClientSocket& operator=(
      const QuicProxyDatagramClientSocket&) = delete;

  // On destruction Close() is called.
  ~QuicProxyDatagramClientSocket() override;

  // Connect this socket over the given QUIC stream, using the `url_`
  // and local and proxy peer addresses. The socket has no true peer
  // address since it is connected over a proxy and the proxy performs the
  // hostname resolution. Instead `proxy_peer_address_` is the peer to which the
  // underlying socket is connected.
  //
  // The passed stream is a connection to the last proxy in `proxy_chain`.
  int ConnectViaStream(const IPEndPoint& local_address,
                       const IPEndPoint& proxy_peer_address,
                       std::unique_ptr<QuicChromiumClientStream::Handle> stream,
                       CompletionOnceCallback callback);

  // DatagramClientSocket implementation.
  int Connect(const IPEndPoint& address) override;
  int ConnectUsingNetwork(handles::NetworkHandle network,
                          const IPEndPoint& address) override;
  int ConnectUsingDefaultNetwork(const IPEndPoint& address) override;
  int ConnectAsync(const IPEndPoint& address,
                   CompletionOnceCallback callback) override;
  int ConnectUsingNetworkAsync(handles::NetworkHandle network,
                               const IPEndPoint& address,
                               CompletionOnceCallback callback) override;
  int ConnectUsingDefaultNetworkAsync(const IPEndPoint& address,
                                      CompletionOnceCallback callback) override;
  handles::NetworkHandle GetBoundNetwork() const override;
  void ApplySocketTag(const SocketTag& tag) override;
  int SetMulticastInterface(uint32_t interface_index) override;
  void SetIOSNetworkServiceType(int ios_network_service_type) override;

  // DatagramSocket implementation.
  void Close() override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  void UseNonBlockingIO() override;
  int SetDoNotFragment() override;
  int SetRecvTos() override;
  int SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) override;
  void SetMsgConfirm(bool confirm) override;
  const NetLogWithSource& NetLog() const override;
  DscpAndEcn GetLastTos() const override;

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

  // Http3DatagramVisitor implementation.
  void OnHttp3Datagram(quic::QuicStreamId stream_id,
                       std::string_view payload) override;
  void OnUnknownCapsule(quic::QuicStreamId stream_id,
                        const quiche::UnknownCapsule& capsule) override;

  const HttpResponseInfo* GetConnectResponseInfo() const;
  bool IsConnected() const;

  const std::queue<std::string>& GetDatagramsForTesting() { return datagrams_; }

  static constexpr char kMaxQueueSizeHistogram[] =
      "Net.QuicProxyDatagramClientSocket.MaxQueueSizeReached";

  // Upper bound for datagrams in queue.
  static constexpr size_t kMaxDatagramQueueSize = 16;

 private:
  enum State {
    STATE_DISCONNECTED,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_READ_REPLY,
    STATE_READ_REPLY_COMPLETE,
    STATE_CONNECT_COMPLETE
  };

  // Callback used during connecting
  void OnIOComplete(int result);

  // Callback for stream_->ReadInitialHeaders()
  void OnReadResponseHeadersComplete(int result);
  int ProcessResponseHeaders(const quiche::HttpHeaderBlock& headers);

  int DoLoop(int last_io_result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoReadReply();
  int DoReadReplyComplete(int result);

  // ProxyDelegate operates in terms of a full proxy chain and an
  // index into that chain identifying the "current" proxy. Emulate
  // this by simply using the current chain and indexing the last proxy in
  // that chain.
  const ProxyChain& proxy_chain() { return proxy_chain_; }
  int proxy_chain_index() { return proxy_chain_.length() - 1; }

  State next_state_ = STATE_DISCONNECTED;

  // Stores the callback for Connect().
  CompletionOnceCallback connect_callback_;
  // Stores the callback for Read().
  CompletionOnceCallback read_callback_;
  // Stores the buffer pointer for Read().
  raw_ptr<IOBuffer> read_buf_ = nullptr;
  // Stores the buffer length for Read().
  int read_buf_len_ = 0;

  // Handle to the QUIC Stream that this sits on top of.
  std::unique_ptr<QuicChromiumClientStream::Handle> stream_handle_;

  // Queue for storing incoming datagrams received over QUIC. This queue acts as
  // a buffer, allowing datagrams to be stored when received and processed
  // asynchronously at a later time.
  std::queue<std::string> datagrams_;
  // Visitor on stream is registered to receive HTTP/3 datagrams.
  bool datagram_visitor_registered_ = false;

  // CONNECT request and response.
  HttpRequestInfo request_;
  HttpResponseInfo response_;

  quiche::HttpHeaderBlock response_header_block_;

  // Local address of socket.
  IPEndPoint local_address_;
  // The peer IP of sockets underlying connection.
  IPEndPoint proxy_peer_address_;

  // The URL generated from the expanded URI Template.
  // This URI Template includes variables for "target_host" and "target_port",
  // which have been replaced with their actual values to form the complete URL.
  GURL url_;

  // The proxy chain this socket represents: `stream_` is a connection to the
  // last proxy in this chain.
  const ProxyChain proxy_chain_;

  // This delegate must outlive this proxy client socket.
  const raw_ptr<ProxyDelegate> proxy_delegate_;

  std::string user_agent_;

  NetLogWithSource net_log_;

  // The default weak pointer factory.
  base::WeakPtrFactory<QuicProxyDatagramClientSocket> weak_factory_{this};
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PROXY_DATAGRAM_CLIENT_SOCKET_H_
