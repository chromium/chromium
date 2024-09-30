// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_tcp/sockets_tcp_api.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/types/optional_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/socket/tls_socket.h"
#include "extensions/browser/api/sockets_tcp/tcp_socket_event_dispatcher.h"
#include "extensions/common/api/sockets_tcp.h"
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
    socket_info.name = socket->name();
  }
  socket_info.persistent = socket->persistent();
  if (socket->buffer_size() > 0) {
    socket_info.buffer_size = socket->buffer_size();
  }
  socket_info.paused = socket->paused();
  socket_info.connected = socket->IsConnected();

  // Grab the local address as known by the OS.
  net::IPEndPoint localAddress;
  if (socket->GetLocalAddress(&localAddress)) {
    socket_info.local_address = localAddress.ToStringWithoutPort();
    socket_info.local_port = localAddress.port();
  }

  // Grab the peer address as known by the OS. This and the call below will
  // always succeed while the socket is connected, even if the socket has
  // been remotely closed by the peer; only reading the socket will reveal
  // that it should be closed locally.
  net::IPEndPoint peerAddress;
  if (socket->GetPeerAddress(&peerAddress)) {
    socket_info.peer_address = peerAddress.ToStringWithoutPort();
    socket_info.peer_port = peerAddress.port();
  }

  return socket_info;
}

void SetSocketProperties(ResumableTCPSocket* socket,
                         SocketProperties* properties) {
  if (properties->name) {
    socket->set_name(*properties->name);
  }
  if (properties->persistent) {
    socket->set_persistent(*properties->persistent);
  }
  if (properties->buffer_size) {
    // buffer size is validated when issuing the actual Recv operation
    // on the socket.
    socket->set_buffer_size(*properties->buffer_size);
  }
}

}  // namespace

namespace extensions {
namespace api {

using content::SocketPermissionRequest;

TCPSocketApiFunction::~TCPSocketApiFunction() = default;

std::unique_ptr<SocketResourceManagerInterface>
TCPSocketApiFunction::CreateSocketResourceManager() {
  return std::unique_ptr<SocketResourceManagerInterface>(
      new SocketResourceManager<ResumableTCPSocket>());
}

ResumableTCPSocket* TCPSocketApiFunction::GetTcpSocket(int socket_id) {
  return static_cast<ResumableTCPSocket*>(GetSocket(socket_id));
}

TCPSocketExtensionWithDnsLookupFunction::
    ~TCPSocketExtensionWithDnsLookupFunction() = default;

std::unique_ptr<SocketResourceManagerInterface>
TCPSocketExtensionWithDnsLookupFunction::CreateSocketResourceManager() {
  return std::unique_ptr<SocketResourceManagerInterface>(
      new SocketResourceManager<ResumableTCPSocket>());
}

ResumableTCPSocket* TCPSocketExtensionWithDnsLookupFunction::GetTcpSocket(
    int socket_id) {
  return static_cast<ResumableTCPSocket*>(GetSocket(socket_id));
}

SocketsTcpCreateFunction::SocketsTcpCreateFunction() = default;

SocketsTcpCreateFunction::~SocketsTcpCreateFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpCreateFunction::Work() {
  std::optional<sockets_tcp::Create::Params> params =
      sockets_tcp::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableTCPSocket* socket =
      new ResumableTCPSocket(browser_context(), GetOriginId());

  sockets_tcp::SocketProperties* properties =
      base::OptionalToPtr(params->properties);
  if (properties) {
    SetSocketProperties(socket, properties);
  }

  sockets_tcp::CreateInfo create_info;
  create_info.socket_id = AddSocket(socket);
  return RespondNow(
      ArgumentList(sockets_tcp::Create::Results::Create(create_info)));
}

SocketsTcpUpdateFunction::SocketsTcpUpdateFunction() = default;

SocketsTcpUpdateFunction::~SocketsTcpUpdateFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpUpdateFunction::Work() {
  std::optional<sockets_tcp::Update::Params> params =
      sockets_tcp::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableTCPSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  SetSocketProperties(socket, &params->properties);
  return RespondNow(NoArguments());
}

SocketsTcpSetPausedFunction::SocketsTcpSetPausedFunction() = default;

SocketsTcpSetPausedFunction::~SocketsTcpSetPausedFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpSetPausedFunction::Work() {
  std::optional<sockets_tcp::SetPaused::Params> params =
      api::sockets_tcp::SetPaused::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  TCPSocketEventDispatcher* socket_event_dispatcher =
      TCPSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "TCPSocketEventDispatcher.";

  ResumableTCPSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  if (socket->paused() != params->paused) {
    socket->set_paused(params->paused);
    if (socket->IsConnected() && !params->paused) {
      socket_event_dispatcher->OnSocketResume(GetOriginId(), params->socket_id);
    }
  }

  return RespondNow(NoArguments());
}

SocketsTcpSetKeepAliveFunction::SocketsTcpSetKeepAliveFunction() = default;

SocketsTcpSetKeepAliveFunction::~SocketsTcpSetKeepAliveFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpSetKeepAliveFunction::Work() {
  std::optional<sockets_tcp::SetKeepAlive::Params> params =
      api::sockets_tcp::SetKeepAlive::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableTCPSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(ErrorWithCode(net::ERR_FAILED, kSocketNotFoundError));
  }

  int delay = params->delay ? *params->delay : 0;

  socket->SetKeepAlive(
      params->enable, delay,
      base::BindOnce(&SocketsTcpSetKeepAliveFunction::OnCompleted, this));
  return RespondLater();
}

void SocketsTcpSetKeepAliveFunction::OnCompleted(bool success) {
  if (success) {
    Respond(WithArguments(net::OK));
  } else {
    Respond(
        ErrorWithCode(net::ERR_FAILED, net::ErrorToString(net::ERR_FAILED)));
  }
}

SocketsTcpSetNoDelayFunction::SocketsTcpSetNoDelayFunction() = default;

SocketsTcpSetNoDelayFunction::~SocketsTcpSetNoDelayFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpSetNoDelayFunction::Work() {
  std::optional<sockets_tcp::SetNoDelay::Params> params =
      api::sockets_tcp::SetNoDelay::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableTCPSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(ErrorWithCode(net::ERR_FAILED, kSocketNotFoundError));
  }
  socket->SetNoDelay(
      params->no_delay,
      base::BindOnce(&SocketsTcpSetNoDelayFunction::OnCompleted, this));
  return RespondLater();
}

void SocketsTcpSetNoDelayFunction::OnCompleted(bool success) {
  if (success) {
    Respond(WithArguments(net::OK));
  } else {
    Respond(
        ErrorWithCode(net::ERR_FAILED, net::ErrorToString(net::ERR_FAILED)));
  }
}

SocketsTcpConnectFunction::SocketsTcpConnectFunction() = default;

SocketsTcpConnectFunction::~SocketsTcpConnectFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpConnectFunction::Work() {
  params_ = sockets_tcp::Connect::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  socket_event_dispatcher_ = TCPSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "TCPSocketEventDispatcher.";

  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  socket->set_hostname(params_->peer_address);

  net::DnsQueryType dns_query_type;
  switch (params_->dns_query_type) {
    case extensions::api::sockets_tcp::DnsQueryType::kNone:
    case extensions::api::sockets_tcp::DnsQueryType::kAny:
      dns_query_type = net::DnsQueryType::UNSPECIFIED;
      break;
    case extensions::api::sockets_tcp::DnsQueryType::kIpv4:
      dns_query_type = net::DnsQueryType::A;
      break;
    case extensions::api::sockets_tcp::DnsQueryType::kIpv6:
      dns_query_type = net::DnsQueryType::AAAA;
      break;
  }

  content::SocketPermissionRequest param(SocketPermissionRequest::TCP_CONNECT,
                                         params_->peer_address,
                                         params_->peer_port);
  if (!CheckRequest(param)) {
    return RespondNow(Error(kPermissionError));
  }

  StartDnsLookup(net::HostPortPair(params_->peer_address, params_->peer_port),
                 dns_query_type);
  return RespondLater();
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
    Respond(Error(kSocketNotFoundError));
    return;
  }

  socket->Connect(
      addresses_,
      base::BindOnce(&SocketsTcpConnectFunction::OnCompleted, this));
}

void SocketsTcpConnectFunction::OnCompleted(int net_result) {
  if (net_result == net::OK) {
    socket_event_dispatcher_->OnSocketConnect(GetOriginId(),
                                              params_->socket_id);
  }

  if (net_result == net::OK) {
    Respond(WithArguments(net_result));
  } else {
    Respond(ErrorWithCode(net_result, net::ErrorToString(net_result)));
  }
}

SocketsTcpDisconnectFunction::SocketsTcpDisconnectFunction() = default;

SocketsTcpDisconnectFunction::~SocketsTcpDisconnectFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpDisconnectFunction::Work() {
  std::optional<sockets_tcp::Disconnect::Params> params =
      sockets_tcp::Disconnect::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableTCPSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  socket->Disconnect(false /* socket_destroying */);
  return RespondNow(NoArguments());
}

SocketsTcpSendFunction::SocketsTcpSendFunction() = default;

SocketsTcpSendFunction::~SocketsTcpSendFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpSendFunction::Work() {
  std::optional<sockets_tcp::Send::Params> params =
      sockets_tcp::Send::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);
  size_t io_buffer_size = params->data.size();
  if (!TakeWriteQuota(io_buffer_size)) {
    return RespondNow(Error(kExceedWriteQuotaError));
  }

  auto io_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(params->data.size());
  base::ranges::copy(params->data, io_buffer->data());

  ResumableTCPSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  socket->Write(io_buffer, io_buffer_size,
                base::BindOnce(&SocketsTcpSendFunction::OnCompleted, this));
  return RespondLater();
}

void SocketsTcpSendFunction::OnCompleted(int net_result) {
  ReturnWriteQuota();

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
    send_info.bytes_sent = bytes_sent;
  }

  auto args = sockets_tcp::Send::Results::Create(send_info);
  if (net_result == net::OK) {
    Respond(ArgumentList(std::move(args)));
  } else {
    Respond(
        ErrorWithArguments(std::move(args), net::ErrorToString(net_result)));
  }
}

SocketsTcpCloseFunction::SocketsTcpCloseFunction() = default;

SocketsTcpCloseFunction::~SocketsTcpCloseFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpCloseFunction::Work() {
  std::optional<sockets_tcp::Close::Params> params =
      sockets_tcp::Close::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableTCPSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  RemoveSocket(params->socket_id);
  return RespondNow(NoArguments());
}

SocketsTcpGetInfoFunction::SocketsTcpGetInfoFunction() = default;

SocketsTcpGetInfoFunction::~SocketsTcpGetInfoFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpGetInfoFunction::Work() {
  std::optional<sockets_tcp::GetInfo::Params> params =
      sockets_tcp::GetInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableTCPSocket* socket = GetTcpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  sockets_tcp::SocketInfo socket_info =
      CreateSocketInfo(params->socket_id, socket);
  return RespondNow(
      ArgumentList(sockets_tcp::GetInfo::Results::Create(socket_info)));
}

SocketsTcpGetSocketsFunction::SocketsTcpGetSocketsFunction() = default;

SocketsTcpGetSocketsFunction::~SocketsTcpGetSocketsFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpGetSocketsFunction::Work() {
  std::vector<sockets_tcp::SocketInfo> socket_infos;
  std::unordered_set<int>* resource_ids = GetSocketIds();
  if (resource_ids) {
    for (int socket_id : *resource_ids) {
      ResumableTCPSocket* socket = GetTcpSocket(socket_id);
      if (socket) {
        socket_infos.push_back(CreateSocketInfo(socket_id, socket));
      }
    }
  }
  return RespondNow(
      ArgumentList(sockets_tcp::GetSockets::Results::Create(socket_infos)));
}

SocketsTcpSecureFunction::SocketsTcpSecureFunction() = default;

SocketsTcpSecureFunction::~SocketsTcpSecureFunction() = default;

ExtensionFunction::ResponseAction SocketsTcpSecureFunction::Work() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  params_ = api::sockets_tcp::Secure::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  ResumableTCPSocket* socket = GetTcpSocket(params_->socket_id);
  if (!socket) {
    return RespondNow(
        ErrorWithCode(net::ERR_INVALID_ARGUMENT, kSocketNotFoundError));
  }

  paused_ = socket->paused();
  persistent_ = socket->persistent();

  // Make sure it's a connected TCP client socket. Error out if it's already
  // secure()'d.
  if (socket->GetSocketType() != Socket::TYPE_TCP) {
    return RespondNow(
        ErrorWithCode(net::ERR_INVALID_ARGUMENT, kInvalidSocketStateError));
  }

  if (!socket->IsConnected()) {
    return RespondNow(
        ErrorWithCode(net::ERR_INVALID_ARGUMENT, kSocketNotConnectedError));
  }

  // UpgradeSocketToTLS() uses the older API's SecureOptions. Copy over the
  // only values inside -- TLSVersionConstraints's |min| and |max|,
  api::socket::SecureOptions legacy_params;
  if (params_->options && params_->options->tls_version) {
    legacy_params.tls_version.emplace();
    legacy_params.tls_version->min = params_->options->tls_version->min;
    legacy_params.tls_version->max = params_->options->tls_version->max;
  }

  socket->UpgradeToTLS(
      &legacy_params,
      base::BindOnce(&SocketsTcpSecureFunction::TlsConnectDone, this));
  return RespondLater();
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
    Respond(ErrorWithCode(result, net::ErrorToString(result)));
    return;
  }
  auto socket =
      std::make_unique<TLSSocket>(std::move(tls_socket), local_addr, peer_addr,
                                  std::move(receive_pipe_handle),
                                  std::move(send_pipe_handle), GetOriginId());
  socket->set_persistent(persistent_);
  socket->set_paused(paused_);
  ReplaceSocket(params_->socket_id, socket.release());
  Respond(WithArguments(result));
}

}  // namespace api
}  // namespace extensions
