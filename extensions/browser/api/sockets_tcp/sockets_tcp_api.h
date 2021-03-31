// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_TCP_SOCKETS_TCP_API_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_TCP_SOCKETS_TCP_API_H_

#include <stddef.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "extensions/browser/api/socket/socket_api.h"
#include "extensions/common/api/sockets_tcp.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"

namespace extensions {
class ResumableTCPSocket;
}

namespace extensions {
namespace api {

class TCPSocketEventDispatcher;

class TCPSocketAsyncApiFunction : public SocketAsyncApiFunction {
 protected:
  ~TCPSocketAsyncApiFunction() override;

  std::unique_ptr<SocketResourceManagerInterface> CreateSocketResourceManager()
      override;

  ResumableTCPSocket* GetTcpSocket(int socket_id);
};

class TCPSocketExtensionWithDnsLookupFunction
    : public SocketExtensionWithDnsLookupFunction {
 protected:
  ~TCPSocketExtensionWithDnsLookupFunction() override;

  std::unique_ptr<SocketResourceManagerInterface> CreateSocketResourceManager()
      override;

  ResumableTCPSocket* GetTcpSocket(int socket_id);
};

class SocketsTcpCreateFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.create", SOCKETS_TCP_CREATE)

  SocketsTcpCreateFunction();

 protected:
  ~SocketsTcpCreateFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketsTcpUnitTest, Create);
  std::unique_ptr<sockets_tcp::Create::Params> params_;
};

class SocketsTcpUpdateFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.update", SOCKETS_TCP_UPDATE)

  SocketsTcpUpdateFunction();

 protected:
  ~SocketsTcpUpdateFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_tcp::Update::Params> params_;
};

class SocketsTcpSetPausedFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.setPaused", SOCKETS_TCP_SETPAUSED)

  SocketsTcpSetPausedFunction();

 protected:
  ~SocketsTcpSetPausedFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_tcp::SetPaused::Params> params_;
  TCPSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsTcpSetKeepAliveFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.setKeepAlive",
                             SOCKETS_TCP_SETKEEPALIVE)

  SocketsTcpSetKeepAliveFunction();

 protected:
  ~SocketsTcpSetKeepAliveFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(bool success);

  std::unique_ptr<sockets_tcp::SetKeepAlive::Params> params_;
};

class SocketsTcpSetNoDelayFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.setNoDelay", SOCKETS_TCP_SETNODELAY)

  SocketsTcpSetNoDelayFunction();

 protected:
  ~SocketsTcpSetNoDelayFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(bool success);

  std::unique_ptr<sockets_tcp::SetNoDelay::Params> params_;
};

class SocketsTcpConnectFunction
    : public TCPSocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.connect", SOCKETS_TCP_CONNECT)

  SocketsTcpConnectFunction();

 protected:
  ~SocketsTcpConnectFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

  // SocketExtensionWithDnsLookupFunction:
  void AfterDnsLookup(int lookup_result) override;

 private:
  void StartConnect();
  void OnCompleted(int net_result);

  std::unique_ptr<sockets_tcp::Connect::Params> params_;
  TCPSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsTcpDisconnectFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.disconnect", SOCKETS_TCP_DISCONNECT)

  SocketsTcpDisconnectFunction();

 protected:
  ~SocketsTcpDisconnectFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_tcp::Disconnect::Params> params_;
};

class SocketsTcpSendFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.send", SOCKETS_TCP_SEND)

  SocketsTcpSendFunction();

 protected:
  ~SocketsTcpSendFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(int net_result);
  void SetSendResult(int net_result, int bytes_sent);

  std::unique_ptr<sockets_tcp::Send::Params> params_;
  scoped_refptr<net::IOBuffer> io_buffer_;
  size_t io_buffer_size_;
};

class SocketsTcpCloseFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.close", SOCKETS_TCP_CLOSE)

  SocketsTcpCloseFunction();

 protected:
  ~SocketsTcpCloseFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_tcp::Close::Params> params_;
};

class SocketsTcpGetInfoFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.getInfo", SOCKETS_TCP_GETINFO)

  SocketsTcpGetInfoFunction();

 protected:
  ~SocketsTcpGetInfoFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_tcp::GetInfo::Params> params_;
};

class SocketsTcpGetSocketsFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.getSockets", SOCKETS_TCP_GETSOCKETS)

  SocketsTcpGetSocketsFunction();

 protected:
  ~SocketsTcpGetSocketsFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;
};

class SocketsTcpSecureFunction : public TCPSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.secure", SOCKETS_TCP_SECURE)

  SocketsTcpSecureFunction();

 protected:
  ~SocketsTcpSecureFunction() override;
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void TlsConnectDone(
      int result,
      mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
      const net::IPEndPoint& local_addr,
      const net::IPEndPoint& peer_addr,
      mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
      mojo::ScopedDataPipeProducerHandle send_pipe_handle);

  bool paused_;
  bool persistent_;
  std::unique_ptr<sockets_tcp::Secure::Params> params_;

  DISALLOW_COPY_AND_ASSIGN(SocketsTcpSecureFunction);
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_TCP_SOCKETS_TCP_API_H_
