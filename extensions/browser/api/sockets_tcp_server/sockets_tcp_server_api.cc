// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_tcp_server/sockets_tcp_server_api.h"

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/functional/bind.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/sockets_tcp_server/tcp_server_socket_event_dispatcher.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"

using content::SocketPermissionRequest;
using extensions::ResumableTCPServerSocket;
using extensions::api::sockets_tcp_server::SocketInfo;
using extensions::api::sockets_tcp_server::SocketProperties;

namespace {

const char kSocketNotFoundError[] = "Socket not found";
const char kPermissionError[] = "Does not have permission";
const int kDefaultListenBacklog = SOMAXCONN;

SocketInfo CreateSocketInfo(int socket_id, ResumableTCPServerSocket* socket) {
  SocketInfo socket_info;
  // This represents what we know about the socket, and does not call through
  // to the system.
  socket_info.socket_id = socket_id;
  if (!socket->name().empty()) {
    socket_info.name = socket->name();
  }
  socket_info.persistent = socket->persistent();
  socket_info.paused = socket->paused();

  // Grab the local address as known by the OS.
  net::IPEndPoint localAddress;
  if (socket->GetLocalAddress(&localAddress)) {
    socket_info.local_address = localAddress.ToStringWithoutPort();
    socket_info.local_port = localAddress.port();
  }

  return socket_info;
}

void SetSocketProperties(ResumableTCPServerSocket* socket,
                         const SocketProperties& properties) {
  if (properties.name) {
    socket->set_name(*properties.name);
  }
  if (properties.persistent) {
    socket->set_persistent(*properties.persistent);
  }
}

}  // namespace

namespace extensions {
namespace api {

TCPServerSocketApiFunction::~TCPServerSocketApiFunction() = default;

std::unique_ptr<SocketResourceManagerInterface>
TCPServerSocketApiFunction::CreateSocketResourceManager() {
  return std::unique_ptr<SocketResourceManagerInterface>(
      new SocketResourceManager<ResumableTCPServerSocket>());
}

ResumableTCPServerSocket* TCPServerSocketApiFunction::GetTcpSocket(
    int socket_id) {
  return static_cast<ResumableTCPServerSocket*>(GetSocket(socket_id));
}

SocketsTcpServerCreateFunction::SocketsTcpServerCreateFunction() = default;

SocketsTcpServerCreateFunction::~SocketsTcpServerCreateFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpServerCreateFunction::Work() {
  std::optional<sockets_tcp_server::Create::Params> params =
      sockets_tcp_server::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* socket = new ResumableTCPServerSocket(browser_context(), GetOriginId());

  if (params->properties) {
    SetSocketProperties(socket, *params->properties);
  }

  sockets_tcp_server::CreateInfo create_info;
  create_info.socket_id = AddSocket(socket);
  return RespondNow(
      ArgumentList(sockets_tcp_server::Create::Results::Create(create_info)));
}

SocketsTcpServerUpdateFunction::SocketsTcpServerUpdateFunction() = default;

SocketsTcpServerUpdateFunction::~SocketsTcpServerUpdateFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpServerUpdateFunction::Work() {
  std::optional<sockets_tcp_server::Update::Params> params =
      sockets_tcp_server::Update::Params::Create(args());

  ResumableTCPServerSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  SetSocketProperties(socket, params->properties);
  return RespondNow(NoArguments());
}

SocketsTcpServerSetPausedFunction::SocketsTcpServerSetPausedFunction() =
    default;

SocketsTcpServerSetPausedFunction::~SocketsTcpServerSetPausedFunction() =
    default;

ExtensionFunction::ResponseAction SocketsTcpServerSetPausedFunction::Work() {
  std::optional<sockets_tcp_server::SetPaused::Params> params =
      api::sockets_tcp_server::SetPaused::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  TCPServerSocketEventDispatcher* socket_event_dispatcher =
      TCPServerSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "TCPServerSocketEventDispatcher.";

  ResumableTCPServerSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  if (socket->paused() != params->paused) {
    socket->set_paused(params->paused);
    if (socket->IsConnected() && !params->paused) {
      socket_event_dispatcher->OnServerSocketResume(GetOriginId(),
                                                    params->socket_id);
    }
  }

  return RespondNow(NoArguments());
}

SocketsTcpServerListenFunction::SocketsTcpServerListenFunction() = default;

SocketsTcpServerListenFunction::~SocketsTcpServerListenFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpServerListenFunction::Work() {
  params_ = api::sockets_tcp_server::Listen::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  socket_event_dispatcher_ =
      TCPServerSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "TCPServerSocketEventDispatcher.";

  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  SocketPermissionRequest param(
      SocketPermissionRequest::TCP_LISTEN, params_->address, params_->port);
  if (!CheckRequest(param)) {
    return RespondNow(Error(kPermissionError));
  }

  socket->Listen(
      params_->address, params_->port,
      params_->backlog.value_or(kDefaultListenBacklog),
      base::BindOnce(&SocketsTcpServerListenFunction::OnCompleted, this));
  return RespondLater();
}

void SocketsTcpServerListenFunction::OnCompleted(
    int net_result,
    const std::string& /* error_msg */) {
  DCHECK_NE(net::ERR_IO_PENDING, net_result);

  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    Respond(Error(kSocketNotFoundError));
    return;
  }
  if (net_result == net::OK) {
    socket_event_dispatcher_->OnServerSocketListen(GetOriginId(),
                                                   params_->socket_id);
  } else {
    Respond(ErrorWithCode(net_result, net::ErrorToString(net_result)));
    return;
  }

  OpenFirewallHole(params_->address, params_->socket_id, socket);
  if (!did_respond()) {
    Respond(WithArguments(net_result));
  }
}

SocketsTcpServerDisconnectFunction::SocketsTcpServerDisconnectFunction() =
    default;

SocketsTcpServerDisconnectFunction::~SocketsTcpServerDisconnectFunction() =
    default;

ExtensionFunction::ResponseAction SocketsTcpServerDisconnectFunction::Work() {
  std::optional<sockets_tcp_server::Disconnect::Params> params =
      sockets_tcp_server::Disconnect::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableTCPServerSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  socket->Disconnect(false /* socket_destroying */);
  return RespondNow(NoArguments());
}

SocketsTcpServerCloseFunction::SocketsTcpServerCloseFunction() = default;

SocketsTcpServerCloseFunction::~SocketsTcpServerCloseFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpServerCloseFunction::Work() {
  std::optional<sockets_tcp_server::Close::Params> params =
      sockets_tcp_server::Close::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableTCPServerSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  RemoveSocket(params->socket_id);
  return RespondNow(NoArguments());
}

SocketsTcpServerGetInfoFunction::SocketsTcpServerGetInfoFunction() = default;

SocketsTcpServerGetInfoFunction::~SocketsTcpServerGetInfoFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpServerGetInfoFunction::Work() {
  std::optional<sockets_tcp_server::GetInfo::Params> params =
      sockets_tcp_server::GetInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableTCPServerSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  sockets_tcp_server::SocketInfo socket_info =
      CreateSocketInfo(params->socket_id, socket);
  return RespondNow(
      ArgumentList(sockets_tcp_server::GetInfo::Results::Create(socket_info)));
}

SocketsTcpServerGetSocketsFunction::SocketsTcpServerGetSocketsFunction() =
    default;

SocketsTcpServerGetSocketsFunction::~SocketsTcpServerGetSocketsFunction() =
    default;

ExtensionFunction::ResponseAction SocketsTcpServerGetSocketsFunction::Work() {
  std::vector<sockets_tcp_server::SocketInfo> socket_infos;
  std::unordered_set<int>* resource_ids = GetSocketIds();
  if (resource_ids) {
    for (int socket_id : *resource_ids) {
      ResumableTCPServerSocket* socket = GetTcpSocket(socket_id);
      if (socket) {
        socket_infos.push_back(CreateSocketInfo(socket_id, socket));
      }
    }
  }
  return RespondNow(ArgumentList(
      sockets_tcp_server::GetSockets::Results::Create(socket_infos)));
}

}  // namespace api
}  // namespace extensions
