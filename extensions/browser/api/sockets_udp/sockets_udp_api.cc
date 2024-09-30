// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_udp/sockets_udp_api.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/types/optional_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "extensions/browser/api/sockets_udp/udp_socket_event_dispatcher.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "net/base/net_errors.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace extensions {
namespace api {

using content::SocketPermissionRequest;

const char kSocketNotFoundError[] = "Socket not found";
const char kPermissionError[] = "App does not have permission";
const char kWildcardAddress[] = "*";
const int kWildcardPort = 0;

UDPSocketApiFunction::~UDPSocketApiFunction() = default;

std::unique_ptr<SocketResourceManagerInterface>
UDPSocketApiFunction::CreateSocketResourceManager() {
  return std::unique_ptr<SocketResourceManagerInterface>(
      new SocketResourceManager<ResumableUDPSocket>());
}

ResumableUDPSocket* UDPSocketApiFunction::GetUdpSocket(int socket_id) {
  return static_cast<ResumableUDPSocket*>(GetSocket(socket_id));
}

UDPSocketExtensionWithDnsLookupFunction::
    ~UDPSocketExtensionWithDnsLookupFunction() = default;

std::unique_ptr<SocketResourceManagerInterface>
UDPSocketExtensionWithDnsLookupFunction::CreateSocketResourceManager() {
  return std::unique_ptr<SocketResourceManagerInterface>(
      new SocketResourceManager<ResumableUDPSocket>());
}

ResumableUDPSocket* UDPSocketExtensionWithDnsLookupFunction::GetUdpSocket(
    int socket_id) {
  return static_cast<ResumableUDPSocket*>(GetSocket(socket_id));
}

sockets_udp::SocketInfo CreateSocketInfo(int socket_id,
                                         ResumableUDPSocket* socket) {
  sockets_udp::SocketInfo socket_info;
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

  // Grab the local address as known by the OS.
  net::IPEndPoint localAddress;
  if (socket->GetLocalAddress(&localAddress)) {
    socket_info.local_address = localAddress.ToStringWithoutPort();
    socket_info.local_port = localAddress.port();
  }

  return socket_info;
}

void SetSocketProperties(ResumableUDPSocket* socket,
                         sockets_udp::SocketProperties* properties) {
  if (properties->name) {
    socket->set_name(*properties->name);
  }
  if (properties->persistent) {
    socket->set_persistent(*properties->persistent);
  }
  if (properties->buffer_size) {
    socket->set_buffer_size(*properties->buffer_size);
  }
}

SocketsUdpCreateFunction::SocketsUdpCreateFunction() = default;

SocketsUdpCreateFunction::~SocketsUdpCreateFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpCreateFunction::Work() {
  std::optional<sockets_udp::Create::Params> params =
      sockets_udp::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  mojo::PendingRemote<network::mojom::UDPSocketListener> listener_remote;
  mojo::PendingReceiver<network::mojom::UDPSocketListener>
      socket_listener_receiver =
          listener_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<network::mojom::UDPSocket> udp_socket;
  browser_context()
      ->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->CreateUDPSocket(udp_socket.InitWithNewPipeAndPassReceiver(),
                        std::move(listener_remote));

  ResumableUDPSocket* socket = new ResumableUDPSocket(
      std::move(udp_socket), std::move(socket_listener_receiver),
      GetOriginId());

  sockets_udp::SocketProperties* properties =
      base::OptionalToPtr(params->properties);
  if (properties) {
    SetSocketProperties(socket, properties);
  }

  sockets_udp::CreateInfo create_info;
  create_info.socket_id = AddSocket(socket);
  return RespondNow(
      ArgumentList(sockets_udp::Create::Results::Create(create_info)));
}

SocketsUdpUpdateFunction::SocketsUdpUpdateFunction() = default;

SocketsUdpUpdateFunction::~SocketsUdpUpdateFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpUpdateFunction::Work() {
  std::optional<sockets_udp::Update::Params> params =
      sockets_udp::Update::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableUDPSocket* socket = GetUdpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  SetSocketProperties(socket, &params->properties);
  return RespondNow(NoArguments());
}

SocketsUdpSetPausedFunction::SocketsUdpSetPausedFunction() = default;

SocketsUdpSetPausedFunction::~SocketsUdpSetPausedFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpSetPausedFunction::Work() {
  std::optional<sockets_udp::SetPaused::Params> params =
      api::sockets_udp::SetPaused::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  UDPSocketEventDispatcher* socket_event_dispatcher =
      UDPSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "UDPSocketEventDispatcher.";

  ResumableUDPSocket* socket = GetUdpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  if (socket->paused() != params->paused) {
    socket->set_paused(params->paused);
    if (socket->IsConnectedOrBound() && !params->paused) {
      socket_event_dispatcher->OnSocketResume(GetOriginId(), params->socket_id);
    }
  }

  return RespondNow(NoArguments());
}

SocketsUdpBindFunction::SocketsUdpBindFunction() = default;

SocketsUdpBindFunction::~SocketsUdpBindFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpBindFunction::Work() {
  params_ = sockets_udp::Bind::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);

  socket_event_dispatcher_ = UDPSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "UDPSocketEventDispatcher.";

  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  content::SocketPermissionRequest param(
      SocketPermissionRequest::UDP_BIND, params_->address, params_->port);
  if (!CheckRequest(param)) {
    return RespondNow(Error(kPermissionError));
  }
  socket->Bind(params_->address, params_->port,
               base::BindOnce(&SocketsUdpBindFunction::OnCompleted, this));
  return RespondLater();
}

void SocketsUdpBindFunction::OnCompleted(int net_result) {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    Respond(Error(kSocketNotFoundError));
    return;
  }
  if (net_result == net::OK) {
    socket_event_dispatcher_->OnSocketBind(GetOriginId(), params_->socket_id);
  } else {
    Respond(ErrorWithCode(net_result, net::ErrorToString(net_result)));
    return;
  }

  OpenFirewallHole(params_->address, params_->socket_id, socket);
  if (!did_respond()) {
    Respond(WithArguments(net_result));
  }
}

SocketsUdpSendFunction::SocketsUdpSendFunction() = default;

SocketsUdpSendFunction::~SocketsUdpSendFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpSendFunction::Work() {
  params_ = sockets_udp::Send::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params_);
  io_buffer_size_ = params_->data.size();
  if (!TakeWriteQuota(io_buffer_size_)) {
    return RespondNow(Error(kExceedWriteQuotaError));
  }

  io_buffer_ =
      base::MakeRefCounted<net::IOBufferWithSize>(params_->data.size());
  base::ranges::copy(params_->data, io_buffer_->data());

  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  net::DnsQueryType dns_query_type;
  switch (params_->dns_query_type) {
    case extensions::api::sockets_udp::DnsQueryType::kNone:
    case extensions::api::sockets_udp::DnsQueryType::kAny:
      dns_query_type = net::DnsQueryType::UNSPECIFIED;
      break;
    case extensions::api::sockets_udp::DnsQueryType::kIpv4:
      dns_query_type = net::DnsQueryType::A;
      break;
    case extensions::api::sockets_udp::DnsQueryType::kIpv6:
      dns_query_type = net::DnsQueryType::AAAA;
      break;
  }

  content::SocketPermissionRequest param(
      SocketPermissionRequest::UDP_SEND_TO, params_->address, params_->port);
  if (!CheckRequest(param)) {
    return RespondNow(Error(kPermissionError));
  }

  StartDnsLookup(net::HostPortPair(params_->address, params_->port),
                 dns_query_type);
  return RespondLater();
}

void SocketsUdpSendFunction::AfterDnsLookup(int lookup_result) {
  if (lookup_result == net::OK) {
    StartSendTo();
  } else {
    SetSendResult(lookup_result, -1);
  }
}

void SocketsUdpSendFunction::StartSendTo() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    Respond(Error(kSocketNotFoundError));
    return;
  }

  socket->SendTo(io_buffer_, io_buffer_size_, addresses_.front(),
                 base::BindOnce(&SocketsUdpSendFunction::OnCompleted, this));
}

void SocketsUdpSendFunction::OnCompleted(int net_result) {
  ReturnWriteQuota();

  if (net_result >= net::OK) {
    SetSendResult(net::OK, net_result);
  } else {
    SetSendResult(net_result, -1);
  }
}

void SocketsUdpSendFunction::SetSendResult(int net_result, int bytes_sent) {
  CHECK(net_result <= net::OK) << "Network status code must be < 0";

  sockets_udp::SendInfo send_info;
  send_info.result_code = net_result;
  if (net_result == net::OK) {
    send_info.bytes_sent = bytes_sent;
  }

  auto args = sockets_udp::Send::Results::Create(send_info);
  if (net_result == net::OK) {
    Respond(ArgumentList(std::move(args)));
  } else {
    Respond(
        ErrorWithArguments(std::move(args), net::ErrorToString(net_result)));
  }
}

SocketsUdpCloseFunction::SocketsUdpCloseFunction() = default;

SocketsUdpCloseFunction::~SocketsUdpCloseFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpCloseFunction::Work() {
  std::optional<sockets_udp::Close::Params> params =
      sockets_udp::Close::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableUDPSocket* socket = GetUdpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  socket->Disconnect(false /* socket_destroying */);
  RemoveSocket(params->socket_id);
  return RespondNow(NoArguments());
}

SocketsUdpGetInfoFunction::SocketsUdpGetInfoFunction() = default;

SocketsUdpGetInfoFunction::~SocketsUdpGetInfoFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpGetInfoFunction::Work() {
  std::optional<sockets_udp::GetInfo::Params> params =
      sockets_udp::GetInfo::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableUDPSocket* socket = GetUdpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  sockets_udp::SocketInfo socket_info =
      CreateSocketInfo(params->socket_id, socket);
  return RespondNow(
      ArgumentList(sockets_udp::GetInfo::Results::Create(socket_info)));
}

SocketsUdpGetSocketsFunction::SocketsUdpGetSocketsFunction() = default;

SocketsUdpGetSocketsFunction::~SocketsUdpGetSocketsFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpGetSocketsFunction::Work() {
  std::vector<sockets_udp::SocketInfo> socket_infos;
  std::unordered_set<int>* resource_ids = GetSocketIds();
  if (resource_ids) {
    for (int socket_id : *resource_ids) {
      ResumableUDPSocket* socket = GetUdpSocket(socket_id);
      if (socket) {
        socket_infos.push_back(CreateSocketInfo(socket_id, socket));
      }
    }
  }
  return RespondNow(
      ArgumentList(sockets_udp::GetSockets::Results::Create(socket_infos)));
}

SocketsUdpJoinGroupFunction::SocketsUdpJoinGroupFunction() = default;

SocketsUdpJoinGroupFunction::~SocketsUdpJoinGroupFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpJoinGroupFunction::Work() {
  std::optional<sockets_udp::JoinGroup::Params> params =
      sockets_udp::JoinGroup::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableUDPSocket* socket = GetUdpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  content::SocketPermissionRequest param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!CheckRequest(param)) {
    return RespondNow(Error(kPermissionError));
  }

  socket->JoinGroup(
      params->address,
      base::BindOnce(&SocketsUdpJoinGroupFunction::OnCompleted, this));
  return RespondLater();
}

void SocketsUdpJoinGroupFunction::OnCompleted(int net_result) {
  if (net_result == net::OK) {
    Respond(WithArguments(net_result));
  } else {
    Respond(ErrorWithCode(net_result, net::ErrorToString(net_result)));
  }
}

SocketsUdpLeaveGroupFunction::SocketsUdpLeaveGroupFunction() = default;

SocketsUdpLeaveGroupFunction::~SocketsUdpLeaveGroupFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpLeaveGroupFunction::Work() {
  std::optional<sockets_udp::LeaveGroup::Params> params =
      api::sockets_udp::LeaveGroup::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableUDPSocket* socket = GetUdpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  content::SocketPermissionRequest param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!CheckRequest(param)) {
    return RespondNow(Error(kPermissionError));
  }
  socket->LeaveGroup(
      params->address,
      base::BindOnce(&SocketsUdpLeaveGroupFunction::OnCompleted, this));
  return RespondLater();
}

void SocketsUdpLeaveGroupFunction::OnCompleted(int result) {
  if (result == net::OK) {
    Respond(WithArguments(result));
  } else {
    Respond(ErrorWithCode(result, net::ErrorToString(result)));
  }
}

SocketsUdpSetMulticastTimeToLiveFunction::
    SocketsUdpSetMulticastTimeToLiveFunction() = default;

SocketsUdpSetMulticastTimeToLiveFunction::
    ~SocketsUdpSetMulticastTimeToLiveFunction() = default;

ExtensionFunction::ResponseAction
SocketsUdpSetMulticastTimeToLiveFunction::Work() {
  std::optional<sockets_udp::SetMulticastTimeToLive::Params> params =
      api::sockets_udp::SetMulticastTimeToLive::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableUDPSocket* socket = GetUdpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  int net_result = socket->SetMulticastTimeToLive(params->ttl);
  if (net_result == net::OK) {
    return RespondNow(WithArguments(net_result));
  }
  return RespondNow(ErrorWithCode(net_result, net::ErrorToString(net_result)));
}

SocketsUdpSetMulticastLoopbackModeFunction::
    SocketsUdpSetMulticastLoopbackModeFunction() = default;

SocketsUdpSetMulticastLoopbackModeFunction::
    ~SocketsUdpSetMulticastLoopbackModeFunction() = default;

ExtensionFunction::ResponseAction
SocketsUdpSetMulticastLoopbackModeFunction::Work() {
  std::optional<sockets_udp::SetMulticastLoopbackMode::Params> params =
      api::sockets_udp::SetMulticastLoopbackMode::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableUDPSocket* socket = GetUdpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  int net_result = socket->SetMulticastLoopbackMode(params->enabled);
  if (net_result == net::OK) {
    return RespondNow(WithArguments(net_result));
  }
  return RespondNow(ErrorWithCode(net_result, net::ErrorToString(net_result)));
}

SocketsUdpGetJoinedGroupsFunction::SocketsUdpGetJoinedGroupsFunction() =
    default;

SocketsUdpGetJoinedGroupsFunction::~SocketsUdpGetJoinedGroupsFunction() =
    default;

ExtensionFunction::ResponseAction SocketsUdpGetJoinedGroupsFunction::Work() {
  std::optional<sockets_udp::GetJoinedGroups::Params> params =
      api::sockets_udp::GetJoinedGroups::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableUDPSocket* socket = GetUdpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  content::SocketPermissionRequest param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!CheckRequest(param)) {
    return RespondNow(Error(kPermissionError));
  }

  const std::vector<std::string>& groups = socket->GetJoinedGroups();
  return RespondNow(
      ArgumentList(sockets_udp::GetJoinedGroups::Results::Create(groups)));
}

SocketsUdpSetBroadcastFunction::SocketsUdpSetBroadcastFunction() = default;

SocketsUdpSetBroadcastFunction::~SocketsUdpSetBroadcastFunction() = default;

ExtensionFunction::ResponseAction SocketsUdpSetBroadcastFunction::Work() {
  std::optional<sockets_udp::SetBroadcast::Params> params =
      api::sockets_udp::SetBroadcast::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  ResumableUDPSocket* socket = GetUdpSocket(params->socket_id);
  if (!socket) {
    return RespondNow(Error(kSocketNotFoundError));
  }

  socket->SetBroadcast(
      params->enabled,
      base::BindOnce(&SocketsUdpSetBroadcastFunction::OnCompleted, this));
  return RespondLater();
}

void SocketsUdpSetBroadcastFunction::OnCompleted(int net_result) {
  if (net_result == net::OK) {
    Respond(WithArguments(net_result));
    return;
  }
  Respond(ErrorWithCode(net_result, net::ErrorToString(net_result)));
}

}  // namespace api
}  // namespace extensions
