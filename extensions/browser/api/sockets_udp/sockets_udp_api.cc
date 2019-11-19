// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/sockets_udp/sockets_udp_api.h"

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "extensions/browser/api/sockets_udp/udp_socket_event_dispatcher.h"
#include "extensions/common/api/sockets/sockets_manifest_data.h"
#include "net/base/net_errors.h"

namespace extensions {
namespace api {

using content::SocketPermissionRequest;

const char kSocketNotFoundError[] = "Socket not found";
const char kPermissionError[] = "App does not have permission";
const char kWildcardAddress[] = "*";
const int kWildcardPort = 0;

UDPSocketAsyncApiFunction::~UDPSocketAsyncApiFunction() {}

std::unique_ptr<SocketResourceManagerInterface>
UDPSocketAsyncApiFunction::CreateSocketResourceManager() {
  return std::unique_ptr<SocketResourceManagerInterface>(
      new SocketResourceManager<ResumableUDPSocket>());
}

ResumableUDPSocket* UDPSocketAsyncApiFunction::GetUdpSocket(int socket_id) {
  return static_cast<ResumableUDPSocket*>(GetSocket(socket_id));
}

UDPSocketExtensionWithDnsLookupFunction::
    ~UDPSocketExtensionWithDnsLookupFunction() {}

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
    socket_info.name.reset(new std::string(socket->name()));
  }
  socket_info.persistent = socket->persistent();
  if (socket->buffer_size() > 0) {
    socket_info.buffer_size.reset(new int(socket->buffer_size()));
  }
  socket_info.paused = socket->paused();

  // Grab the local address as known by the OS.
  net::IPEndPoint localAddress;
  if (socket->GetLocalAddress(&localAddress)) {
    socket_info.local_address.reset(
        new std::string(localAddress.ToStringWithoutPort()));
    socket_info.local_port.reset(new int(localAddress.port()));
  }

  return socket_info;
}

void SetSocketProperties(ResumableUDPSocket* socket,
                         sockets_udp::SocketProperties* properties) {
  if (properties->name.get()) {
    socket->set_name(*properties->name);
  }
  if (properties->persistent.get()) {
    socket->set_persistent(*properties->persistent);
  }
  if (properties->buffer_size.get()) {
    socket->set_buffer_size(*properties->buffer_size);
  }
}

SocketsUdpCreateFunction::SocketsUdpCreateFunction() {}

SocketsUdpCreateFunction::~SocketsUdpCreateFunction() {}

bool SocketsUdpCreateFunction::Prepare() {
  params_ = sockets_udp::Create::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  mojo::PendingRemote<network::mojom::UDPSocketListener> listener_remote;
  socket_listener_receiver_ = listener_remote.InitWithNewPipeAndPassReceiver();
  content::BrowserContext::GetDefaultStoragePartition(browser_context())
      ->GetNetworkContext()
      ->CreateUDPSocket(socket_.InitWithNewPipeAndPassReceiver(),
                        std::move(listener_remote));
  return true;
}

void SocketsUdpCreateFunction::Work() {
  ResumableUDPSocket* socket = new ResumableUDPSocket(
      std::move(socket_), std::move(socket_listener_receiver_),
      extension_->id());

  sockets_udp::SocketProperties* properties = params_->properties.get();
  if (properties) {
    SetSocketProperties(socket, properties);
  }

  sockets_udp::CreateInfo create_info;
  create_info.socket_id = AddSocket(socket);
  results_ = sockets_udp::Create::Results::Create(create_info);
}

SocketsUdpUpdateFunction::SocketsUdpUpdateFunction() {}

SocketsUdpUpdateFunction::~SocketsUdpUpdateFunction() {}

bool SocketsUdpUpdateFunction::Prepare() {
  params_ = sockets_udp::Update::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsUdpUpdateFunction::Work() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  SetSocketProperties(socket, &params_->properties);
  results_ = sockets_udp::Update::Results::Create();
}

SocketsUdpSetPausedFunction::SocketsUdpSetPausedFunction()
    : socket_event_dispatcher_(NULL) {}

SocketsUdpSetPausedFunction::~SocketsUdpSetPausedFunction() {}

bool SocketsUdpSetPausedFunction::Prepare() {
  params_ = api::sockets_udp::SetPaused::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  socket_event_dispatcher_ = UDPSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "UDPSocketEventDispatcher.";
  return socket_event_dispatcher_ != NULL;
}

void SocketsUdpSetPausedFunction::Work() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
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

  results_ = sockets_udp::SetPaused::Results::Create();
}

SocketsUdpBindFunction::SocketsUdpBindFunction()
    : socket_event_dispatcher_(NULL) {}

SocketsUdpBindFunction::~SocketsUdpBindFunction() {}

bool SocketsUdpBindFunction::Prepare() {
  params_ = sockets_udp::Bind::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  socket_event_dispatcher_ = UDPSocketEventDispatcher::Get(browser_context());
  DCHECK(socket_event_dispatcher_)
      << "There is no socket event dispatcher. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "UDPSocketEventDispatcher.";
  return socket_event_dispatcher_ != NULL;
}

void SocketsUdpBindFunction::AsyncWorkStart() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  content::SocketPermissionRequest param(
      SocketPermissionRequest::UDP_BIND, params_->address, params_->port);
  if (!SocketsManifestData::CheckRequest(extension(), param)) {
    error_ = kPermissionError;
    AsyncWorkCompleted();
    return;
  }
  socket->Bind(params_->address, params_->port,
               base::BindOnce(&SocketsUdpBindFunction::OnCompleted, this));
}

void SocketsUdpBindFunction::OnCompleted(int net_result) {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }
  results_ = sockets_udp::Bind::Results::Create(net_result);
  if (net_result == net::OK) {
    socket_event_dispatcher_->OnSocketBind(extension_->id(),
                                           params_->socket_id);
  } else {
    error_ = net::ErrorToString(net_result);
    AsyncWorkCompleted();
    return;
  }

  OpenFirewallHole(params_->address, params_->socket_id, socket);
}

SocketsUdpSendFunction::SocketsUdpSendFunction() : io_buffer_size_(0) {}

SocketsUdpSendFunction::~SocketsUdpSendFunction() {}

bool SocketsUdpSendFunction::Prepare() {
  params_ = sockets_udp::Send::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  io_buffer_size_ = params_->data.size();
  io_buffer_ = base::MakeRefCounted<net::WrappedIOBuffer>(
      reinterpret_cast<const char*>(params_->data.data()));

  return true;
}

void SocketsUdpSendFunction::AsyncWorkStart() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  content::SocketPermissionRequest param(
      SocketPermissionRequest::UDP_SEND_TO, params_->address, params_->port);
  if (!SocketsManifestData::CheckRequest(extension(), param)) {
    error_ = kPermissionError;
    AsyncWorkCompleted();
    return;
  }

  StartDnsLookup(net::HostPortPair(params_->address, params_->port));
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
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  socket->SendTo(
      io_buffer_, io_buffer_size_, addresses_.front(),
      base::BindRepeating(&SocketsUdpSendFunction::OnCompleted, this));
}

void SocketsUdpSendFunction::OnCompleted(int net_result) {
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
    send_info.bytes_sent.reset(new int(bytes_sent));
  }

  if (net_result != net::OK)
    error_ = net::ErrorToString(net_result);
  results_ = sockets_udp::Send::Results::Create(send_info);
  AsyncWorkCompleted();
}

SocketsUdpCloseFunction::SocketsUdpCloseFunction() {}

SocketsUdpCloseFunction::~SocketsUdpCloseFunction() {}

bool SocketsUdpCloseFunction::Prepare() {
  params_ = sockets_udp::Close::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsUdpCloseFunction::Work() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  socket->Disconnect(false /* socket_destroying */);
  RemoveSocket(params_->socket_id);
  results_ = sockets_udp::Close::Results::Create();
}

SocketsUdpGetInfoFunction::SocketsUdpGetInfoFunction() {}

SocketsUdpGetInfoFunction::~SocketsUdpGetInfoFunction() {}

bool SocketsUdpGetInfoFunction::Prepare() {
  params_ = sockets_udp::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsUdpGetInfoFunction::Work() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  sockets_udp::SocketInfo socket_info =
      CreateSocketInfo(params_->socket_id, socket);
  results_ = sockets_udp::GetInfo::Results::Create(socket_info);
}

SocketsUdpGetSocketsFunction::SocketsUdpGetSocketsFunction() {}

SocketsUdpGetSocketsFunction::~SocketsUdpGetSocketsFunction() {}

bool SocketsUdpGetSocketsFunction::Prepare() { return true; }

void SocketsUdpGetSocketsFunction::Work() {
  std::vector<sockets_udp::SocketInfo> socket_infos;
  std::unordered_set<int>* resource_ids = GetSocketIds();
  if (resource_ids != NULL) {
    for (int socket_id : *resource_ids) {
      ResumableUDPSocket* socket = GetUdpSocket(socket_id);
      if (socket) {
        socket_infos.push_back(CreateSocketInfo(socket_id, socket));
      }
    }
  }
  results_ = sockets_udp::GetSockets::Results::Create(socket_infos);
}

SocketsUdpJoinGroupFunction::SocketsUdpJoinGroupFunction() {}

SocketsUdpJoinGroupFunction::~SocketsUdpJoinGroupFunction() {}

bool SocketsUdpJoinGroupFunction::Prepare() {
  params_ = sockets_udp::JoinGroup::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsUdpJoinGroupFunction::AsyncWorkStart() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  content::SocketPermissionRequest param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!SocketsManifestData::CheckRequest(extension(), param)) {
    error_ = kPermissionError;
    AsyncWorkCompleted();
    return;
  }

  socket->JoinGroup(
      params_->address,
      base::BindOnce(&SocketsUdpJoinGroupFunction::OnCompleted, this));
}

void SocketsUdpJoinGroupFunction::OnCompleted(int net_result) {
  if (net_result != net::OK)
    error_ = net::ErrorToString(net_result);
  results_ = sockets_udp::JoinGroup::Results::Create(net_result);
  AsyncWorkCompleted();
}

SocketsUdpLeaveGroupFunction::SocketsUdpLeaveGroupFunction() {}

SocketsUdpLeaveGroupFunction::~SocketsUdpLeaveGroupFunction() {}

bool SocketsUdpLeaveGroupFunction::Prepare() {
  params_ = api::sockets_udp::LeaveGroup::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsUdpLeaveGroupFunction::AsyncWorkStart() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  content::SocketPermissionRequest param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!SocketsManifestData::CheckRequest(extension(), param)) {
    error_ = kPermissionError;
    AsyncWorkCompleted();
    return;
  }
  socket->LeaveGroup(
      params_->address,
      base::BindOnce(&SocketsUdpLeaveGroupFunction::OnCompleted, this));
}

void SocketsUdpLeaveGroupFunction::OnCompleted(int result) {
  if (result != net::OK)
    error_ = net::ErrorToString(result);
  results_ = sockets_udp::LeaveGroup::Results::Create(result);
  AsyncWorkCompleted();
}

SocketsUdpSetMulticastTimeToLiveFunction::
    SocketsUdpSetMulticastTimeToLiveFunction() {}

SocketsUdpSetMulticastTimeToLiveFunction::
    ~SocketsUdpSetMulticastTimeToLiveFunction() {}

bool SocketsUdpSetMulticastTimeToLiveFunction::Prepare() {
  params_ = api::sockets_udp::SetMulticastTimeToLive::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsUdpSetMulticastTimeToLiveFunction::Work() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  int net_result = socket->SetMulticastTimeToLive(params_->ttl);
  if (net_result != net::OK)
    error_ = net::ErrorToString(net_result);
  results_ = sockets_udp::SetMulticastTimeToLive::Results::Create(net_result);
}

SocketsUdpSetMulticastLoopbackModeFunction::
    SocketsUdpSetMulticastLoopbackModeFunction() {}

SocketsUdpSetMulticastLoopbackModeFunction::
    ~SocketsUdpSetMulticastLoopbackModeFunction() {}

bool SocketsUdpSetMulticastLoopbackModeFunction::Prepare() {
  params_ = api::sockets_udp::SetMulticastLoopbackMode::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsUdpSetMulticastLoopbackModeFunction::Work() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  int net_result = socket->SetMulticastLoopbackMode(params_->enabled);
  if (net_result != net::OK)
    error_ = net::ErrorToString(net_result);
  results_ = sockets_udp::SetMulticastLoopbackMode::Results::Create(net_result);
}

SocketsUdpGetJoinedGroupsFunction::SocketsUdpGetJoinedGroupsFunction() {}

SocketsUdpGetJoinedGroupsFunction::~SocketsUdpGetJoinedGroupsFunction() {}

bool SocketsUdpGetJoinedGroupsFunction::Prepare() {
  params_ = api::sockets_udp::GetJoinedGroups::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsUdpGetJoinedGroupsFunction::Work() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  content::SocketPermissionRequest param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!SocketsManifestData::CheckRequest(extension(), param)) {
    error_ = kPermissionError;
    return;
  }

  const std::vector<std::string>& groups = socket->GetJoinedGroups();
  results_ = sockets_udp::GetJoinedGroups::Results::Create(groups);
}

SocketsUdpSetBroadcastFunction::SocketsUdpSetBroadcastFunction() {
}

SocketsUdpSetBroadcastFunction::~SocketsUdpSetBroadcastFunction() {
}

bool SocketsUdpSetBroadcastFunction::Prepare() {
  params_ = api::sockets_udp::SetBroadcast::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketsUdpSetBroadcastFunction::AsyncWorkStart() {
  ResumableUDPSocket* socket = GetUdpSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  socket->SetBroadcast(
      params_->enabled,
      base::BindOnce(&SocketsUdpSetBroadcastFunction::OnCompleted, this));
}

void SocketsUdpSetBroadcastFunction::OnCompleted(int net_result) {
  if (net_result != net::OK)
    error_ = net::ErrorToString(net_result);
  results_ = sockets_udp::SetBroadcast::Results::Create(net_result);
  AsyncWorkCompleted();
}

}  // namespace api
}  // namespace extensions
