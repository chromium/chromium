// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_SOCKETS_TCP_SERVER_API_H_
#define EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_SOCKETS_TCP_SERVER_API_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "extensions/browser/api/socket/socket_api.h"
#include "extensions/common/api/sockets_tcp_server.h"

namespace extensions {
class ResumableTCPServerSocket;
}

namespace extensions {
namespace api {

class TCPServerSocketAsyncApiFunction : public SocketAsyncApiFunction {
 protected:
  ~TCPServerSocketAsyncApiFunction() override;

  std::unique_ptr<SocketResourceManagerInterface> CreateSocketResourceManager()
      override;

  ResumableTCPServerSocket* GetTcpSocket(int socket_id);
};

class SocketsTcpServerCreateFunction : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.create",
                             SOCKETS_TCP_SERVER_CREATE)

  SocketsTcpServerCreateFunction();

 protected:
  ~SocketsTcpServerCreateFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SocketsTcpServerUnitTest, Create);
  std::unique_ptr<sockets_tcp_server::Create::Params> params_;
};

class SocketsTcpServerUpdateFunction : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.update",
                             SOCKETS_TCP_SERVER_UPDATE)

  SocketsTcpServerUpdateFunction();

 protected:
  ~SocketsTcpServerUpdateFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_tcp_server::Update::Params> params_;
};

class SocketsTcpServerSetPausedFunction
    : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.setPaused",
                             SOCKETS_TCP_SERVER_SETPAUSED)

  SocketsTcpServerSetPausedFunction();

 protected:
  ~SocketsTcpServerSetPausedFunction() override;

  // AsyncApiFunction
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_tcp_server::SetPaused::Params> params_;
  TCPServerSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsTcpServerListenFunction : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.listen",
                             SOCKETS_TCP_SERVER_LISTEN)

  SocketsTcpServerListenFunction();

 protected:
  ~SocketsTcpServerListenFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void AsyncWorkStart() override;

 private:
  void OnCompleted(int result, const std::string& error_msg);

  std::unique_ptr<sockets_tcp_server::Listen::Params> params_;
  TCPServerSocketEventDispatcher* socket_event_dispatcher_;
};

class SocketsTcpServerDisconnectFunction
    : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.disconnect",
                             SOCKETS_TCP_SERVER_DISCONNECT)

  SocketsTcpServerDisconnectFunction();

 protected:
  ~SocketsTcpServerDisconnectFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_tcp_server::Disconnect::Params> params_;
};

class SocketsTcpServerCloseFunction : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.close",
                             SOCKETS_TCP_SERVER_CLOSE)

  SocketsTcpServerCloseFunction();

 protected:
  ~SocketsTcpServerCloseFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_tcp_server::Close::Params> params_;
};

class SocketsTcpServerGetInfoFunction : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.getInfo",
                             SOCKETS_TCP_SERVER_GETINFO)

  SocketsTcpServerGetInfoFunction();

 protected:
  ~SocketsTcpServerGetInfoFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;

 private:
  std::unique_ptr<sockets_tcp_server::GetInfo::Params> params_;
};

class SocketsTcpServerGetSocketsFunction
    : public TCPServerSocketAsyncApiFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("sockets.tcpServer.getSockets",
                             SOCKETS_TCP_SERVER_GETSOCKETS)

  SocketsTcpServerGetSocketsFunction();

 protected:
  ~SocketsTcpServerGetSocketsFunction() override;

  // AsyncApiFunction:
  bool Prepare() override;
  void Work() override;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKETS_TCP_SERVER_SOCKETS_TCP_SERVER_API_H_
