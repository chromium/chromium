// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This StreamSocket implementation is to be used with servers that
// accept connections on port 443 but don't really use SSL.  For
// example, the Google Talk servers do this to bypass proxies.  (The
// connection is upgraded to TLS as part of the XMPP negotiation, so
// security is preserved.)  A "fake" SSL handshake is done immediately
// after connection to fool proxies into thinking that this is a real
// SSL connection.
//
// NOTE: This StreamSocket implementation does *not* do a real SSL
// handshake nor does it do any encryption!

#ifndef COMPONENTS_WEBRTC_FAKE_SSL_CLIENT_SOCKET_H_
#define COMPONENTS_WEBRTC_FAKE_SSL_CLIENT_SOCKET_H_

#include <stdint.h>

#include <cstddef>
#include <memory>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {
class DrainableIOBuffer;
class SSLInfo;
}  // namespace net

namespace webrtc {

class FakeSSLClientSocket : public net::StreamSocket {
 public:
  explicit FakeSSLClientSocket(
      std::unique_ptr<net::StreamSocket> transport_socket);

  ~FakeSSLClientSocket() override;

  // Exposed for testing.
  static std::string_view GetSslClientHello();
  static std::string_view GetSslServerHello();

  // net::StreamSocket implementation.
  int Read(net::IOBuffer* buf,
           int buf_len,
           net::CompletionOnceCallback callback) override;
  int ReadIfReady(net::IOBuffer* buf,
                  int buf_len,
                  net::CompletionOnceCallback callback) override;
  int CancelReadIfReady() override;
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;
  int Connect(net::CompletionOnceCallback callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  const net::NetLogWithSource& NetLog() const override;
  bool WasEverUsed() const override;
  net::NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(net::SSLInfo* ssl_info) override;
  int64_t GetTotalReceivedBytes() const override;
  void ApplySocketTag(const net::SocketTag& tag) override;

 private:
  enum HandshakeState {
    STATE_NONE,
    STATE_CONNECT,
    STATE_SEND_CLIENT_HELLO,
    STATE_VERIFY_SERVER_HELLO,
  };

  int DoHandshakeLoop();
  void RunUserConnectCallback(int status);
  void DoHandshakeLoopWithUserConnectCallback();

  int DoConnect();
  void OnConnectDone(int status);
  void ProcessConnectDone();

  int DoSendClientHello();
  void OnSendClientHelloDone(int status);
  void ProcessSendClientHelloDone(size_t written);

  int DoVerifyServerHello();
  void OnVerifyServerHelloDone(int status);
  net::Error ProcessVerifyServerHelloDone(size_t read);

  std::unique_ptr<net::StreamSocket> transport_socket_;

  // During the handshake process, holds a value from HandshakeState.
  // STATE_NONE otherwise.
  HandshakeState next_handshake_state_;

  // True iff we're connected and we've finished the handshake.
  bool handshake_completed_;

  // The callback passed to Connect().
  net::CompletionOnceCallback user_connect_callback_;

  ::scoped_refptr<net::DrainableIOBuffer> write_buf_;
  ::scoped_refptr<net::DrainableIOBuffer> read_buf_;
};

}  // namespace webrtc

#endif  // COMPONENTS_WEBRTC_FAKE_SSL_CLIENT_SOCKET_H_
