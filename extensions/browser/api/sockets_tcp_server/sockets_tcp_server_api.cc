// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_tcp_server/sockets_tcp_server_api.h"

#include <memory>
#include <unordered_set>
#include <vector>

#include "base/bind.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/sockets_tcp_server/tcp_server_socket_event_dispatcher.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "net/base/net_errors.h"

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
    socket_info.name = std::make_unique<std::string>(socket->name());
  }
  socket_info.persistent = socket->persistent();
  socket_info.paused = socket->paused();

  // Grab the local address as known by the OS.
  net::IPEndPoint localAddress;
  if (socket->GetLocalAddress(&localAddress)) {
    socket_info.local_address =
        std::make_unique<std::string>(localAddress.ToStringWithoutPort());
    socket_info.local_port = std::make_unique<int>(localAddress.port());
  }

  return socket_info;
}

void SetSocketProperties(ResumableTCPServerSocket* socket,
                         SocketProperties* properties) {
  if (properties->name.get()) {
    socket->set_name(*properties->name);
  }
  if (properties->persistent.get()) {
    socket->set_persistent(*properties->persistent);
  }
}

}  // namespace

namespace extensions {
namespace api {

TCPServerSocketAsyncApiFunction::~TCPServerSocketAsyncApiFunction() {}

std::unique_ptr<SocketResourceManagerInterface>
TCPServerSocketAsyncApiFunction::CreateSocketResourceManager() {
  return std::unique_ptr<SocketResourceManagerInterface>(
      new SocketResourceManager<ResumableTCPServerSocket>());
}

ResumableTCPServerSocket* TCPServerSocketAsyncApiFunction::GetTcpSocket(
    int socket_id) {
  return static_cast<ResumableTCPServerSocket*>(GetSocket(socket_id));
}

SocketsTcpServerCreateFunction::SocketsTcpServerCreateFunction() {}

SocketsTcpServerCreateFunction::~SocketsTcpServerCreateFunction() {}

bool SocketsTcpServerCreateFunction::Prepare() {
  params_ = sockets_tcp_server::Create::Params::Create(*args_);
  browser_context_ = browser_context();
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpServerCreateFunction::Work() {
  // TODO(crbug.com/1191472): |browser_context_| is unsafe to access when
  // DestroyProfileOnBrowserClose is enabled, since it could've been deleted by
  // now. Fix this by creating the TCPSocket on the UI thread instead.
  auto* socket =
      new ResumableTCPServerSocket(browser_context_, extension_->id());

  sockets_tcp_server::SocketProperties* properties = params_->properties.get();
  if (properties) {
    SetSocketProperties(socket, properties);
  }

  sockets_tcp_server::CreateInfo create_info;
  create_info.socket_id = AddSocket(socket);
  results_ = std::make_unique<base::ListValue>(
      sockets_tcp_server::Create::Results::Create(create_info));
}

SocketsTcpServerUpdateFunction::SocketsTcpServerUpdateFunction() {}

SocketsTcpServerUpdateFunction::~SocketsTcpServerUpdateFunction() {}

bool SocketsTcpServerUpdateFunction::Prepare() {
  params_ = sockets_tcp_server::Update::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpServerUpdateFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  SetSocketProperties(socket, &params_->properties);
  results_ = std::make_unique<base::ListValue>(
      sockets_tcp_server::Update::Results::Create());
}

SocketsTcpServerSetPausedFunction::SocketsTcpServerSetPausedFunction()
    : socket_event_dispatcher_(nullptr) {}

SocketsTcpServerSetPausedFunction::~SocketsTcpServerSetPausedFunction() {}

bool SocketsTcpServerSetPausedFunction::Prepare() {
  params_ = api::sockets_tcp_server::SetPaused::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  socket_event_dispatcher_ =
      TCPServerSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "TCPServerSocketEventDispatcher.";
  return !!socket_event_dispatcher_;
}

void SocketsTcpServerSetPausedFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  if (socket->paused() != params_->paused) {
    socket->set_paused(params_->paused);
    if (socket->IsConnected() && !params_->paused) {
      socket_event_dispatcher_->OnServerSocketResume(extension_->id(),
                                                     params_->socket_id);
    }
  }

  results_ = std::make_unique<base::ListValue>(
      sockets_tcp_server::SetPaused::Results::Create());
}

SocketsTcpServerListenFunction::SocketsTcpServerListenFunction()
    : socket_event_dispatcher_(nullptr) {}

SocketsTcpServerListenFunction::~SocketsTcpServerListenFunction() {}

bool SocketsTcpServerListenFunction::Prepare() {
  params_ = api::sockets_tcp_server::Listen::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  socket_event_dispatcher_ =
      TCPServerSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "TCPServerSocketEventDispatcher.";
  return !!socket_event_dispatcher_;
}

void SocketsTcpServerListenFunction::AsyncWorkStart() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  SocketPermissionRequest param(
      SocketPermissionRequest::TCP_LISTEN, params_->address, params_->port);
  if (!SocketsManifestData::CheckRequest(extension(), param)) {
    error_ = kPermissionError;
    AsyncWorkCompleted();
    return;
  }

  socket->Listen(
      params_->address, params_->port,
      params_->backlog.get() ? *params_->backlog : kDefaultListenBacklog,
      base::BindOnce(&SocketsTcpServerListenFunction::OnCompleted, this));
}

void SocketsTcpServerListenFunction::OnCompleted(
    int net_result,
    const std::string& /* error_msg */) {
  DCHECK_NE(net::ERR_IO_PENDING, net_result);

  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }
  results_ = std::make_unique<base::ListValue>(
      sockets_tcp_server::Listen::Results::Create(net_result));
  if (net_result == net::OK) {
    socket_event_dispatcher_->OnServerSocketListen(extension_->id(),
                                                   params_->socket_id);
  } else {
    error_ = net::ErrorToString(net_result);
    AsyncWorkCompleted();
    return;
  }

  OpenFirewallHole(params_->address, params_->socket_id, socket);
}

SocketsTcpServerDisconnectFunction::SocketsTcpServerDisconnectFunction() {}

SocketsTcpServerDisconnectFunction::~SocketsTcpServerDisconnectFunction() {}

bool SocketsTcpServerDisconnectFunction::Prepare() {
  params_ = sockets_tcp_server::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpServerDisconnectFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  socket->Disconnect(false /* socket_destroying */);
  results_ = std::make_unique<base::ListValue>(
      sockets_tcp_server::Disconnect::Results::Create());
}

SocketsTcpServerCloseFunction::SocketsTcpServerCloseFunction() {}

SocketsTcpServerCloseFunction::~SocketsTcpServerCloseFunction() {}

bool SocketsTcpServerCloseFunction::Prepare() {
  params_ = sockets_tcp_server::Close::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpServerCloseFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  RemoveSocket(params_->socket_id);
  results_ = std::make_unique<base::ListValue>(
      sockets_tcp_server::Close::Results::Create());
}

SocketsTcpServerGetInfoFunction::SocketsTcpServerGetInfoFunction() {}

SocketsTcpServerGetInfoFunction::~SocketsTcpServerGetInfoFunction() {}

bool SocketsTcpServerGetInfoFunction::Prepare() {
  params_ = sockets_tcp_server::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpServerGetInfoFunction::Work() {
  ResumableTCPServerSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  sockets_tcp_server::SocketInfo socket_info =
      CreateSocketInfo(params_->socket_id, socket);
  results_ = std::make_unique<base::ListValue>(
      sockets_tcp_server::GetInfo::Results::Create(socket_info));
}

SocketsTcpServerGetSocketsFunction::SocketsTcpServerGetSocketsFunction() {}

SocketsTcpServerGetSocketsFunction::~SocketsTcpServerGetSocketsFunction() {}

bool SocketsTcpServerGetSocketsFunction::Prepare() { return true; }

void SocketsTcpServerGetSocketsFunction::Work() {
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
  results_ = std::make_unique<base::ListValue>(
      sockets_tcp_server::GetSockets::Results::Create(socket_infos));
}

}  // namespace api
}  // namespace extensions
