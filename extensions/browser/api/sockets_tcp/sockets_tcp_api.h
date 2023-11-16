// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_TCP_SOCKETS_TCP_API_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_TCP_SOCKETS_TCP_API_H_

#include <stddef.h>

#include <memory>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
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

class TCPSocketApiFunction : public SocketApiFunction {
 protected:
  ~TCPSocketApiFunction() override;

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

class SocketsTcpCreateFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.create", SOCKETS_TCP_CREATE)

  SocketsTcpCreateFunction();

 protected:
  ~SocketsTcpCreateFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketsTcpUnitTest, Create);
};

class SocketsTcpUpdateFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.update", SOCKETS_TCP_UPDATE)

  SocketsTcpUpdateFunction();

 protected:
  ~SocketsTcpUpdateFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsTcpSetPausedFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.setPaused", SOCKETS_TCP_SETPAUSED)

  SocketsTcpSetPausedFunction();

 protected:
  ~SocketsTcpSetPausedFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;
};

class SocketsTcpSetKeepAliveFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.setKeepAlive",
                             SOCKETS_TCP_SETKEEPALIVE)

  SocketsTcpSetKeepAliveFunction();

 protected:
  ~SocketsTcpSetKeepAliveFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;

 private:
  void OnCompleted(bool success);
};

class SocketsTcpSetNoDelayFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.setNoDelay", SOCKETS_TCP_SETNODELAY)

  SocketsTcpSetNoDelayFunction();

 protected:
  ~SocketsTcpSetNoDelayFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;

 private:
  void OnCompleted(bool success);
};

class SocketsTcpConnectFunction
    : public TCPSocketExtensionWithDnsLookupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.connect", SOCKETS_TCP_CONNECT)

  SocketsTcpConnectFunction();

 protected:
  ~SocketsTcpConnectFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

  // SocketExtensionWithDnsLookupFunction:
  void AfterDnsLookup(int lookup_result) override;

 private:
  void StartConnect();
  void OnCompleted(int net_result);

  std::optional<sockets_tcp::Connect::Params> params_;
  raw_ptr<TCPSocketEventDispatcher> socket_event_dispatcher_ = nullptr;
};

class SocketsTcpDisconnectFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.disconnect", SOCKETS_TCP_DISCONNECT)

  SocketsTcpDisconnectFunction();

 protected:
  ~SocketsTcpDisconnectFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsTcpSendFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.send", SOCKETS_TCP_SEND)

  SocketsTcpSendFunction();

 protected:
  ~SocketsTcpSendFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  void OnCompleted(int net_result);
  void SetSendResult(int net_result, int bytes_sent);

  std::optional<sockets_tcp::Send::Params> params_;
};

class SocketsTcpCloseFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.close", SOCKETS_TCP_CLOSE)

  SocketsTcpCloseFunction();

 protected:
  ~SocketsTcpCloseFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsTcpGetInfoFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.getInfo", SOCKETS_TCP_GETINFO)

  SocketsTcpGetInfoFunction();

 protected:
  ~SocketsTcpGetInfoFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsTcpGetSocketsFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.getSockets", SOCKETS_TCP_GETSOCKETS)

  SocketsTcpGetSocketsFunction();

 protected:
  ~SocketsTcpGetSocketsFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsTcpSecureFunction : public TCPSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcp.secure", SOCKETS_TCP_SECURE)

  SocketsTcpSecureFunction();

  SocketsTcpSecureFunction(const SocketsTcpSecureFunction&) = delete;
  SocketsTcpSecureFunction& operator=(const SocketsTcpSecureFunction&) = delete;

 protected:
  ~SocketsTcpSecureFunction() override;
  ResponseAction Work() override;

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
  std::optional<sockets_tcp::Secure::Params> params_;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_TCP_SOCKETS_TCP_API_H_
