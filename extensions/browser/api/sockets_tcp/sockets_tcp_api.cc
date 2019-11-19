// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_tcp/sockets_tcp_api.h"

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/socket/tls_socket.h"
#include "extensions/browser/api/sockets_tcp/tcp_socket_event_dispatcher.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "net/base/net_errors.h"

using extensions::ResumableTCPSocket;
using extensions::api::sockets_tcp::SocketInfo;
using extensions::api::sockets_tcp::SocketProperties;

namespace {

const char kSocketNotFoundError[] = "Socket not found";
const char kPermissionError[] = "Does not have permission";
const char kInvalidSocketStateError[] =
    "Socket must be a connected client TCP socket.";
const char kSocketNotConnectedError[] = "Socket not connected";

SocketInfo CreateSocketInfo(int socket_id, ResumableTCPSocket* socket) {
  SocketInfo socket_info;
  // This represents what we know about the socket, and does not call through
  // to the system.
  socket_info.socket_id = socket_id;
  if (!socket->name().empty()) {
    socket_info.name.reset(new std::string(socket->name()));
  }
  socket_info.persistent = socket->persistent();
  if (socket->buffer_size() > 0) {
    socket_info.buffer_size.reset(new int(socket->buffer_size()));
  }
  socket_info.paused = socket->paused();
  socket_info.connected = socket->IsConnected();

  // Grab the local address as known by the OS.
  net::IPEndPoint localAddress;
  if (socket->GetLocalAddress(&localAddress)) {
    socket_info.local_address.reset(
        new std::string(localAddress.ToStringWithoutPort()));
    socket_info.local_port.reset(new int(localAddress.port()));
  }

  // Grab the peer address as known by the OS. This and the call below will
  // always succeed while the socket is connected, even if the socket has
  // been remotely closed by the peer; only reading the socket will reveal
  // that it should be closed locally.
  net::IPEndPoint peerAddress;
  if (socket->GetPeerAddress(&peerAddress)) {
    socket_info.peer_address.reset(
        new std::string(peerAddress.ToStringWithoutPort()));
    socket_info.peer_port.reset(new int(peerAddress.port()));
  }

  return socket_info;
}

void SetSocketProperties(ResumableTCPSocket* socket,
                         SocketProperties* properties) {
  if (properties->name.get()) {
    socket->set_name(*properties->name);
  }
  if (properties->persistent.get()) {
    socket->set_persistent(*properties->persistent);
  }
  if (properties->buffer_size.get()) {
    // buffer size is validated when issuing the actual Recv operation
    // on the socket.
    socket->set_buffer_size(*properties->buffer_size);
  }
}

}  // namespace

namespace extensions {
namespace api {

using content::SocketPermissionRequest;

TCPSocketAsyncApiFunction::~TCPSocketAsyncApiFunction() {}

std::unique_ptr<SocketResourceManagerInterface>
TCPSocketAsyncApiFunction::CreateSocketResourceManager() {
  return std::unique_ptr<SocketResourceManagerInterface>(
      new SocketResourceManager<ResumableTCPSocket>());
}

ResumableTCPSocket* TCPSocketAsyncApiFunction::GetTcpSocket(int socket_id) {
  return static_cast<ResumableTCPSocket*>(GetSocket(socket_id));
}

TCPSocketExtensionWithDnsLookupFunction::
    ~TCPSocketExtensionWithDnsLookupFunction() {}

std::unique_ptr<SocketResourceManagerInterface>
TCPSocketExtensionWithDnsLookupFunction::CreateSocketResourceManager() {
  return std::unique_ptr<SocketResourceManagerInterface>(
      new SocketResourceManager<ResumableTCPSocket>());
}

ResumableTCPSocket* TCPSocketExtensionWithDnsLookupFunction::GetTcpSocket(
    int socket_id) {
  return static_cast<ResumableTCPSocket*>(GetSocket(socket_id));
}

SocketsTcpCreateFunction::SocketsTcpCreateFunction() {}

SocketsTcpCreateFunction::~SocketsTcpCreateFunction() {}

bool SocketsTcpCreateFunction::Prepare() {
  params_ = sockets_tcp::Create::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpCreateFunction::Work() {
  ResumableTCPSocket* socket =
      new ResumableTCPSocket(browser_context(), extension_->id());

  sockets_tcp::SocketProperties* properties = params_->properties.get();
  if (properties) {
    SetSocketProperties(socket, properties);
  }

  sockets_tcp::CreateInfo create_info;
  create_info.socket_id = AddSocket(socket);
  results_ = sockets_tcp::Create::Results::Create(create_info);
}

SocketsTcpUpdateFunction::SocketsTcpUpdateFunction() {}

SocketsTcpUpdateFunction::~SocketsTcpUpdateFunction() {}

bool SocketsTcpUpdateFunction::Prepare() {
  params_ = sockets_tcp::Update::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpUpdateFunction::Work() {
  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  SetSocketProperties(socket, &params_->properties);
  results_ = sockets_tcp::Update::Results::Create();
}

SocketsTcpSetPausedFunction::SocketsTcpSetPausedFunction()
    : socket_event_dispatcher_(NULL) {}

SocketsTcpSetPausedFunction::~SocketsTcpSetPausedFunction() {}

bool SocketsTcpSetPausedFunction::Prepare() {
  params_ = api::sockets_tcp::SetPaused::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  socket_event_dispatcher_ = TCPSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "TCPSocketEventDispatcher.";
  return socket_event_dispatcher_ != NULL;
}

void SocketsTcpSetPausedFunction::Work() {
  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  if (socket->paused() != params_->paused) {
    socket->set_paused(params_->paused);
    if (socket->IsConnected() && !params_->paused) {
      socket_event_dispatcher_->OnSocketResume(extension_->id(),
                                               params_->socket_id);
    }
  }

  results_ = sockets_tcp::SetPaused::Results::Create();
}

SocketsTcpSetKeepAliveFunction::SocketsTcpSetKeepAliveFunction() {}

SocketsTcpSetKeepAliveFunction::~SocketsTcpSetKeepAliveFunction() {}

bool SocketsTcpSetKeepAliveFunction::Prepare() {
  params_ = api::sockets_tcp::SetKeepAlive::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpSetKeepAliveFunction::AsyncWorkStart() {
  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    results_ = sockets_tcp::SetKeepAlive::Results::Create(net::ERR_FAILED);
    AsyncWorkCompleted();
    return;
  }

  int delay = params_->delay ? *params_->delay : 0;

  socket->SetKeepAlive(
      params_->enable, delay,
      base::BindOnce(&SocketsTcpSetKeepAliveFunction::OnCompleted, this));
}

void SocketsTcpSetKeepAliveFunction::OnCompleted(bool success) {
  int net_result = (success ? net::OK : net::ERR_FAILED);
  results_ = sockets_tcp::SetKeepAlive::Results::Create(net_result);
  if (net_result != net::OK)
    error_ = net::ErrorToString(net_result);
  AsyncWorkCompleted();
}

SocketsTcpSetNoDelayFunction::SocketsTcpSetNoDelayFunction() {}

SocketsTcpSetNoDelayFunction::~SocketsTcpSetNoDelayFunction() {}

bool SocketsTcpSetNoDelayFunction::Prepare() {
  params_ = api::sockets_tcp::SetNoDelay::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpSetNoDelayFunction::AsyncWorkStart() {
  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    results_ = sockets_tcp::SetNoDelay::Results::Create(net::ERR_FAILED);
    AsyncWorkCompleted();
    return;
  }
  socket->SetNoDelay(
      params_->no_delay,
      base::BindOnce(&SocketsTcpSetNoDelayFunction::OnCompleted, this));
}

void SocketsTcpSetNoDelayFunction::OnCompleted(bool success) {
  int net_result = (success ? net::OK : net::ERR_FAILED);
  results_ = sockets_tcp::SetNoDelay::Results::Create(net_result);
  if (net_result != net::OK)
    error_ = net::ErrorToString(net_result);
  AsyncWorkCompleted();
}

SocketsTcpConnectFunction::SocketsTcpConnectFunction()
    : socket_event_dispatcher_(NULL) {}

SocketsTcpConnectFunction::~SocketsTcpConnectFunction() {}

bool SocketsTcpConnectFunction::Prepare() {
  params_ = sockets_tcp::Connect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  socket_event_dispatcher_ = TCPSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "TCPSocketEventDispatcher.";
  return socket_event_dispatcher_ != NULL;
}

void SocketsTcpConnectFunction::AsyncWorkStart() {
  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  socket->set_hostname(params_->peer_address);

  content::SocketPermissionRequest param(SocketPermissionRequest::TCP_CONNECT,
                                         params_->peer_address,
                                         params_->peer_port);
  if (!SocketsManifestData::CheckRequest(extension(), param)) {
    error_ = kPermissionError;
    AsyncWorkCompleted();
    return;
  }

  StartDnsLookup(net::HostPortPair(params_->peer_address, params_->peer_port));
}

void SocketsTcpConnectFunction::AfterDnsLookup(int lookup_result) {
  if (lookup_result == net::OK) {
    StartConnect();
  } else {
    OnCompleted(lookup_result);
  }
}

void SocketsTcpConnectFunction::StartConnect() {
  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  socket->Connect(
      addresses_,
      base::BindOnce(&SocketsTcpConnectFunction::OnCompleted, this));
}

void SocketsTcpConnectFunction::OnCompleted(int net_result) {
  if (net_result == net::OK) {
    socket_event_dispatcher_->OnSocketConnect(extension_->id(),
                                              params_->socket_id);
  }

  if (net_result != net::OK)
    error_ = net::ErrorToString(net_result);
  results_ = sockets_tcp::Connect::Results::Create(net_result);
  AsyncWorkCompleted();
}

SocketsTcpDisconnectFunction::SocketsTcpDisconnectFunction() {}

SocketsTcpDisconnectFunction::~SocketsTcpDisconnectFunction() {}

bool SocketsTcpDisconnectFunction::Prepare() {
  params_ = sockets_tcp::Disconnect::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpDisconnectFunction::Work() {
  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  socket->Disconnect(false /* socket_destroying */);
  results_ = sockets_tcp::Disconnect::Results::Create();
}

SocketsTcpSendFunction::SocketsTcpSendFunction() : io_buffer_size_(0) {}

SocketsTcpSendFunction::~SocketsTcpSendFunction() {}

bool SocketsTcpSendFunction::Prepare() {
  params_ = sockets_tcp::Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  io_buffer_size_ = params_->data.size();
  io_buffer_ = base::MakeRefCounted<net::WrappedIOBuffer>(
      reinterpret_cast<const char*>(params_->data.data()));
  return true;
}

void SocketsTcpSendFunction::AsyncWorkStart() {
  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  socket->Write(io_buffer_, io_buffer_size_,
                base::BindOnce(&SocketsTcpSendFunction::OnCompleted, this));
}

void SocketsTcpSendFunction::OnCompleted(int net_result) {
  if (net_result >= net::OK) {
    SetSendResult(net::OK, net_result);
  } else {
    SetSendResult(net_result, -1);
  }
}

void SocketsTcpSendFunction::SetSendResult(int net_result, int bytes_sent) {
  CHECK(net_result <= net::OK) << "Network status code must be <= net::OK";

  sockets_tcp::SendInfo send_info;
  send_info.result_code = net_result;
  if (net_result == net::OK) {
    send_info.bytes_sent.reset(new int(bytes_sent));
  }

  if (net_result != net::OK)
    error_ = net::ErrorToString(net_result);
  results_ = sockets_tcp::Send::Results::Create(send_info);
  AsyncWorkCompleted();
}

SocketsTcpCloseFunction::SocketsTcpCloseFunction() {}

SocketsTcpCloseFunction::~SocketsTcpCloseFunction() {}

bool SocketsTcpCloseFunction::Prepare() {
  params_ = sockets_tcp::Close::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpCloseFunction::Work() {
  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  RemoveSocket(params_->socket_id);
  results_ = sockets_tcp::Close::Results::Create();
}

SocketsTcpGetInfoFunction::SocketsTcpGetInfoFunction() {}

SocketsTcpGetInfoFunction::~SocketsTcpGetInfoFunction() {}

bool SocketsTcpGetInfoFunction::Prepare() {
  params_ = sockets_tcp::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsTcpGetInfoFunction::Work() {
  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  sockets_tcp::SocketInfo socket_info =
      CreateSocketInfo(params_->socket_id, socket);
  results_ = sockets_tcp::GetInfo::Results::Create(socket_info);
}

SocketsTcpGetSocketsFunction::SocketsTcpGetSocketsFunction() {}

SocketsTcpGetSocketsFunction::~SocketsTcpGetSocketsFunction() {}

bool SocketsTcpGetSocketsFunction::Prepare() { return true; }

void SocketsTcpGetSocketsFunction::Work() {
  std::vector<sockets_tcp::SocketInfo> socket_infos;
  std::unordered_set<int>* resource_ids = GetSocketIds();
  if (resource_ids != NULL) {
    for (int socket_id : *resource_ids) {
      ResumableTCPSocket* socket = GetTcpSocket(socket_id);
      if (socket) {
        socket_infos.push_back(CreateSocketInfo(socket_id, socket));
      }
    }
  }
  results_ = sockets_tcp::GetSockets::Results::Create(socket_infos);
}

SocketsTcpSecureFunction::SocketsTcpSecureFunction() {
}

SocketsTcpSecureFunction::~SocketsTcpSecureFunction() {
}

bool SocketsTcpSecureFunction::Prepare() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  params_ = api::sockets_tcp::Secure::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

// Override the regular implementation, which would call AsyncWorkCompleted
// immediately after Work().
void SocketsTcpSecureFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    SetResult(std::make_unique<base::Value>(net::ERR_INVALID_ARGUMENT));
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  paused_ = socket->paused();
  persistent_ = socket->persistent();

  // Make sure it's a connected TCP client socket. Error out if it's already
  // secure()'d.
  if (socket->GetSocketType() != Socket::TYPE_TCP) {
    SetResult(std::make_unique<base::Value>(net::ERR_INVALID_ARGUMENT));
    error_ = kInvalidSocketStateError;
    AsyncWorkCompleted();
    return;
  }

  if (!socket->IsConnected()) {
    SetResult(std::make_unique<base::Value>(net::ERR_INVALID_ARGUMENT));
    error_ = kSocketNotConnectedError;
    AsyncWorkCompleted();
    return;
  }

  // UpgradeSocketToTLS() uses the older API's SecureOptions. Copy over the
  // only values inside -- TLSVersionConstraints's |min| and |max|,
  api::socket::SecureOptions legacy_params;
  if (params_->options.get() && params_->options->tls_version.get()) {
    legacy_params.tls_version.reset(new api::socket::TLSVersionConstraints);
    if (params_->options->tls_version->min.get()) {
      legacy_params.tls_version->min.reset(
          new std::string(*params_->options->tls_version->min));
    }
    if (params_->options->tls_version->max.get()) {
      legacy_params.tls_version->max.reset(
          new std::string(*params_->options->tls_version->max));
    }
  }

  socket->UpgradeToTLS(
      &legacy_params,
      base::BindOnce(&SocketsTcpSecureFunction::TlsConnectDone, this));
}

void SocketsTcpSecureFunction::TlsConnectDone(
    int result,
    mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
    const net::IPEndPoint& local_addr,
    const net::IPEndPoint& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
    mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
  if (result != net::OK) {
    RemoveSocket(params_->socket_id);
    error_ = net::ErrorToString(result);
    results_ = api::sockets_tcp::Secure::Results::Create(result);
    AsyncWorkCompleted();
    return;
  }
  auto socket =
      std::make_unique<TLSSocket>(std::move(tls_socket), local_addr, peer_addr,
                                  std::move(receive_pipe_handle),
                                  std::move(send_pipe_handle), extension_id());
  socket->set_persistent(persistent_);
  socket->set_paused(paused_);
  ReplaceSocket(params_->socket_id, socket.release());
  results_ = api::sockets_tcp::Secure::Results::Create(result);
  AsyncWorkCompleted();
}

}  // namespace api
}  // namespace extensions
