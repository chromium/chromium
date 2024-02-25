// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_SOCKETS_TCP_SERVER_API_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_SOCKETS_TCP_SERVER_API_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "extensions/browser/api/socket/socket_api.h"
#include "extensions/common/api/sockets_tcp_server.h"

namespace extensions {
class ResumableTCPServerSocket;
}

namespace extensions {
namespace api {

class TCPServerSocketApiFunction : public SocketApiFunction {
 protected:
  ~TCPServerSocketApiFunction() override;

  std::unique_ptr<SocketResourceManagerInterface> CreateSocketResourceManager()
      override;

  ResumableTCPServerSocket* GetTcpSocket(int socket_id);
};

class SocketsTcpServerCreateFunction : public TCPServerSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.create",
                             SOCKETS_TCP_SERVER_CREATE)

  SocketsTcpServerCreateFunction();

 protected:
  ~SocketsTcpServerCreateFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketsTcpServerUnitTest, Create);
};

class SocketsTcpServerUpdateFunction : public TCPServerSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.update",
                             SOCKETS_TCP_SERVER_UPDATE)

  SocketsTcpServerUpdateFunction();

 protected:
  ~SocketsTcpServerUpdateFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsTcpServerSetPausedFunction : public TCPServerSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.setPaused",
                             SOCKETS_TCP_SERVER_SETPAUSED)

  SocketsTcpServerSetPausedFunction();

 protected:
  ~SocketsTcpServerSetPausedFunction() override;

  // SocketApiFunction
  ResponseAction Work() override;
};

class SocketsTcpServerListenFunction : public TCPServerSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.listen",
                             SOCKETS_TCP_SERVER_LISTEN)

  SocketsTcpServerListenFunction();

 protected:
  ~SocketsTcpServerListenFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;

 private:
  void OnCompleted(int result, const std::string& error_msg);

  std::optional<sockets_tcp_server::Listen::Params> params_;
  raw_ptr<TCPServerSocketEventDispatcher> socket_event_dispatcher_;
};

class SocketsTcpServerDisconnectFunction : public TCPServerSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.disconnect",
                             SOCKETS_TCP_SERVER_DISCONNECT)

  SocketsTcpServerDisconnectFunction();

 protected:
  ~SocketsTcpServerDisconnectFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsTcpServerCloseFunction : public TCPServerSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.close",
                             SOCKETS_TCP_SERVER_CLOSE)

  SocketsTcpServerCloseFunction();

 protected:
  ~SocketsTcpServerCloseFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsTcpServerGetInfoFunction : public TCPServerSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.getInfo",
                             SOCKETS_TCP_SERVER_GETINFO)

  SocketsTcpServerGetInfoFunction();

 protected:
  ~SocketsTcpServerGetInfoFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

class SocketsTcpServerGetSocketsFunction : public TCPServerSocketApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.getSockets",
                             SOCKETS_TCP_SERVER_GETSOCKETS)

  SocketsTcpServerGetSocketsFunction();

 protected:
  ~SocketsTcpServerGetSocketsFunction() override;

  // SocketApiFunction:
  ResponseAction Work() override;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_SOCKETS_TCP_SERVER_API_H_
