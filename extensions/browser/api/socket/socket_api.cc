// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/socket/socket_api.h"

#include <memory>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/api/socket/socket.h"
#include "extensions/browser/api/socket/tcp_socket.h"
#include "extensions/browser/api/socket/tls_socket.h"
#include "extensions/browser/api/socket/udp_socket.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_interfaces.h"
#include "net/base/network_isolation_key.h"
#include "net/base/url_util.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_with_source.h"

using extensions::mojom::APIPermissionID;

namespace extensions {

namespace {

const char kAddressKey[] = "address";
const char kPortKey[] = "port";
const char kBytesWrittenKey[] = "bytesWritten";
const char kDataKey[] = "data";
const char kResultCodeKey[] = "resultCode";
const char kSocketIdKey[] = "socketId";

const char kSocketNotFoundError[] = "Socket not found";
const char kDnsLookupFailedError[] = "DNS resolution failed";
const char kPermissionError[] = "App does not have permission";
const char kPortInvalidError[] = "Port must be a value between 0 and 65535.";
const char kNetworkListError[] = "Network lookup failed or unsupported";
const char kTCPSocketBindError[] =
    "TCP socket does not support bind. For TCP server please use listen.";
const char kMulticastSocketTypeError[] = "Only UDP socket supports multicast.";
const char kSecureSocketTypeError[] = "Only TCP sockets are supported for TLS.";
const char kSocketNotConnectedError[] = "Socket not connected";
const char kWildcardAddress[] = "*";
const uint16_t kWildcardPort = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kFirewallFailure[] = "Failed to open firewall port";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool IsPortValid(int port) {
  return port >= 0 && port <= 65535;
}

}  // namespace

using content::BrowserThread;
using content::SocketPermissionRequest;

SocketAsyncApiFunction::SocketAsyncApiFunction() {}

SocketAsyncApiFunction::~SocketAsyncApiFunction() {}

bool SocketAsyncApiFunction::PrePrepare() {
  manager_ = CreateSocketResourceManager();
  return manager_->SetBrowserContext(browser_context());
}

bool SocketAsyncApiFunction::Respond() { return error_.empty(); }

std::unique_ptr<SocketResourceManagerInterface>
SocketAsyncApiFunction::CreateSocketResourceManager() {
  return std::unique_ptr<SocketResourceManagerInterface>(
      new SocketResourceManager<Socket>());
}

int SocketAsyncApiFunction::AddSocket(Socket* socket) {
  return manager_->Add(socket);
}

Socket* SocketAsyncApiFunction::GetSocket(int api_resource_id) {
  return manager_->Get(extension_->id(), api_resource_id);
}

void SocketAsyncApiFunction::ReplaceSocket(int api_resource_id,
                                           Socket* socket) {
  manager_->Replace(extension_->id(), api_resource_id, socket);
}

std::unordered_set<int>* SocketAsyncApiFunction::GetSocketIds() {
  return manager_->GetResourceIds(extension_->id());
}

void SocketAsyncApiFunction::RemoveSocket(int api_resource_id) {
  manager_->Remove(extension_->id(), api_resource_id);
}

void SocketAsyncApiFunction::OpenFirewallHole(const std::string& address,
                                              int socket_id,
                                              Socket* socket) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!net::HostStringIsLocalhost(address)) {
    net::IPEndPoint local_address;
    if (!socket->GetLocalAddress(&local_address)) {
      NOTREACHED() << "Cannot get address of recently bound socket.";
      error_ = kFirewallFailure;
      SetResult(std::make_unique<base::Value>(-1));
      AsyncWorkCompleted();
      return;
    }

    AppFirewallHole::PortType type = socket->GetSocketType() == Socket::TYPE_TCP
                                         ? AppFirewallHole::PortType::TCP
                                         : AppFirewallHole::PortType::UDP;

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SocketAsyncApiFunction::OpenFirewallHoleOnUIThread,
                       this, type, local_address.port(), socket_id));
    return;
  }
#endif
  AsyncWorkCompleted();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

void SocketAsyncApiFunction::OpenFirewallHoleOnUIThread(
    AppFirewallHole::PortType type,
    uint16_t port,
    int socket_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  AppFirewallHoleManager* manager =
      AppFirewallHoleManager::Get(browser_context());
  std::unique_ptr<AppFirewallHole, BrowserThread::DeleteOnUIThread> hole(
      manager->Open(type, port, extension_id()).release());
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SocketAsyncApiFunction::OnFirewallHoleOpened,
                                this, socket_id, std::move(hole)));
}

void SocketAsyncApiFunction::OnFirewallHoleOpened(
    int socket_id,
    std::unique_ptr<AppFirewallHole, BrowserThread::DeleteOnUIThread> hole) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!hole) {
    error_ = kFirewallFailure;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  Socket* socket = GetSocket(socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  socket->set_firewall_hole(std::move(hole));
  AsyncWorkCompleted();
}

#endif  // IS_CHROMEOS_ASH

SocketExtensionWithDnsLookupFunction::SocketExtensionWithDnsLookupFunction() =
    default;

SocketExtensionWithDnsLookupFunction::~SocketExtensionWithDnsLookupFunction() {
}

bool SocketExtensionWithDnsLookupFunction::PrePrepare() {
  if (!SocketAsyncApiFunction::PrePrepare())
    return false;
  content::BrowserContext::GetDefaultStoragePartition(browser_context())
      ->GetNetworkContext()
      ->CreateHostResolver(
          base::nullopt,
          pending_host_resolver_.InitWithNewPipeAndPassReceiver());
  return true;
}

void SocketExtensionWithDnsLookupFunction::StartDnsLookup(
    const net::HostPortPair& host_port_pair) {
  DCHECK(pending_host_resolver_);
  DCHECK(!receiver_.is_bound());
  host_resolver_.Bind(std::move(pending_host_resolver_));
  url::Origin origin = url::Origin::Create(extension_->url());
  host_resolver_->ResolveHost(host_port_pair,
                              net::NetworkIsolationKey(origin, origin), nullptr,
                              receiver_.BindNewPipeAndPassRemote());
  receiver_.set_disconnect_handler(
      base::BindOnce(&SocketExtensionWithDnsLookupFunction::OnComplete,
                     base::Unretained(this), net::ERR_NAME_NOT_RESOLVED,
                     net::ResolveErrorInfo(net::ERR_FAILED), base::nullopt));

  // Balanced in OnComplete().
  AddRef();
}

void SocketExtensionWithDnsLookupFunction::OnComplete(
    int result,
    const net::ResolveErrorInfo& resolve_error_info,
    const base::Optional<net::AddressList>& resolved_addresses) {
  host_resolver_.reset();
  receiver_.reset();
  if (result == net::OK) {
    DCHECK(resolved_addresses && !resolved_addresses->empty());
    addresses_ = resolved_addresses.value();
  } else {
    error_ = kDnsLookupFailedError;
  }
  AfterDnsLookup(result);

  Release();  // Added in StartDnsLookup().
}

SocketCreateFunction::SocketCreateFunction()
    : socket_type_(kSocketTypeInvalid) {}

SocketCreateFunction::~SocketCreateFunction() {}

bool SocketCreateFunction::Prepare() {
  params_ = api::socket::Create::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());

  switch (params_->type) {
    case extensions::api::socket::SOCKET_TYPE_TCP:
      socket_type_ = kSocketTypeTCP;
      break;
    case extensions::api::socket::SOCKET_TYPE_UDP: {
      socket_type_ = kSocketTypeUDP;

      mojo::PendingRemote<network::mojom::UDPSocketListener> listener_remote;
      socket_listener_receiver_ =
          listener_remote.InitWithNewPipeAndPassReceiver();
      content::BrowserContext::GetDefaultStoragePartition(browser_context())
          ->GetNetworkContext()
          ->CreateUDPSocket(socket_.InitWithNewPipeAndPassReceiver(),
                            std::move(listener_remote));
      break;
    }
    case extensions::api::socket::SOCKET_TYPE_NONE:
      NOTREACHED();
      break;
  }

  return true;
}

void SocketCreateFunction::Work() {
  Socket* socket = nullptr;
  if (socket_type_ == kSocketTypeTCP) {
    socket = new TCPSocket(browser_context(), extension_->id());
  } else if (socket_type_ == kSocketTypeUDP) {
    socket =
        new UDPSocket(std::move(socket_), std::move(socket_listener_receiver_),
                      extension_->id());
  }
  DCHECK(socket);

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetInteger(kSocketIdKey, AddSocket(socket));
  SetResult(std::move(result));
}

bool SocketDestroyFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketDestroyFunction::Work() {
  RemoveSocket(socket_id_);
}

SocketConnectFunction::SocketConnectFunction()
    : socket_id_(0), hostname_(), port_(0) {
}

SocketConnectFunction::~SocketConnectFunction() {}

bool SocketConnectFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &hostname_));
  int port;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(2, &port));
  if (!IsPortValid(port)) {
    error_ = kPortInvalidError;
    return false;
  }
  port_ = static_cast<uint16_t>(port);
  return true;
}

void SocketConnectFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  socket->set_hostname(hostname_);

  SocketPermissionRequest::OperationType operation_type;
  switch (socket->GetSocketType()) {
    case Socket::TYPE_TCP:
      operation_type = SocketPermissionRequest::TCP_CONNECT;
      break;
    case Socket::TYPE_UDP:
      operation_type = SocketPermissionRequest::UDP_SEND_TO;
      break;
    default:
      NOTREACHED() << "Unknown socket type.";
      operation_type = SocketPermissionRequest::NONE;
      break;
  }

  SocketPermission::CheckParam param(operation_type, hostname_, port_);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  StartDnsLookup(net::HostPortPair(hostname_, port_));
}

void SocketConnectFunction::AfterDnsLookup(int lookup_result) {
  if (lookup_result == net::OK) {
    StartConnect();
  } else {
    SetResult(std::make_unique<base::Value>(lookup_result));
    AsyncWorkCompleted();
  }
}

void SocketConnectFunction::StartConnect() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  socket->Connect(addresses_,
                  base::BindOnce(&SocketConnectFunction::OnConnect, this));
}

void SocketConnectFunction::OnConnect(int result) {
  SetResult(std::make_unique<base::Value>(result));
  AsyncWorkCompleted();
}

bool SocketDisconnectFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  return true;
}

void SocketDisconnectFunction::Work() {
  Socket* socket = GetSocket(socket_id_);
  if (socket)
    socket->Disconnect(false /* socket_destroying */);
  else
    error_ = kSocketNotFoundError;
  SetResult(std::make_unique<base::Value>());
}

bool SocketBindFunction::Prepare() {
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &socket_id_));
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(1, &address_));
  int port;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(2, &port));
  if (!IsPortValid(port)) {
    error_ = kPortInvalidError;
    return false;
  }
  port_ = static_cast<uint16_t>(port);
  return true;
}

void SocketBindFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  if (socket->GetSocketType() == Socket::TYPE_TCP) {
    error_ = kTCPSocketBindError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  CHECK(socket->GetSocketType() == Socket::TYPE_UDP);
  SocketPermission::CheckParam param(SocketPermissionRequest::UDP_BIND,
                                     address_, port_);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  socket->Bind(address_, port_,
               base::BindOnce(&SocketBindFunction::OnCompleted, this));
}
void SocketBindFunction::OnCompleted(int net_result) {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  SetResult(std::make_unique<base::Value>(net_result));
  if (net_result != net::OK) {
    AsyncWorkCompleted();
    return;
  }

  OpenFirewallHole(address_, socket_id_, socket);
}

SocketListenFunction::SocketListenFunction() {}

SocketListenFunction::~SocketListenFunction() {}

bool SocketListenFunction::Prepare() {
  params_ = api::socket::Listen::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketListenFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  SocketPermission::CheckParam param(SocketPermissionRequest::TCP_LISTEN,
                                     params_->address, params_->port);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  socket->Listen(params_->address, params_->port,
                 params_->backlog.get() ? *params_->backlog : 5,
                 base::BindOnce(&SocketListenFunction::OnCompleted, this));
}

void SocketListenFunction::OnCompleted(int result,
                                       const std::string& error_msg) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }
  SetResult(std::make_unique<base::Value>(result));
  if (result != net::OK) {
    error_ = error_msg;
    AsyncWorkCompleted();
    return;
  }
  OpenFirewallHole(params_->address, params_->socket_id, socket);
}

SocketAcceptFunction::SocketAcceptFunction() {}

SocketAcceptFunction::~SocketAcceptFunction() {}

bool SocketAcceptFunction::Prepare() {
  params_ = api::socket::Accept::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketAcceptFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (socket) {
    socket->Accept(base::BindOnce(&SocketAcceptFunction::OnAccept, this));
  } else {
    error_ = kSocketNotFoundError;
    OnAccept(net::ERR_FAILED, mojo::NullRemote(), base::nullopt,
             mojo::ScopedDataPipeConsumerHandle(),
             mojo::ScopedDataPipeProducerHandle());
  }
}

void SocketAcceptFunction::OnAccept(
    int result_code,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> socket,
    const base::Optional<net::IPEndPoint>& remote_addr,
    mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
    mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetInteger(kResultCodeKey, result_code);
  if (result_code == net::OK) {
    Socket* client_socket =
        new TCPSocket(std::move(socket), std::move(receive_pipe_handle),
                      std::move(send_pipe_handle), remote_addr, extension_id());
    result->SetInteger(kSocketIdKey, AddSocket(client_socket));
  }
  SetResult(std::move(result));

  AsyncWorkCompleted();
}

SocketReadFunction::SocketReadFunction() {}

SocketReadFunction::~SocketReadFunction() {}

bool SocketReadFunction::Prepare() {
  params_ = api::socket::Read::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketReadFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1, nullptr, false /* socket_destroying */);
    return;
  }

  socket->Read(params_->buffer_size.get() ? *params_->buffer_size : 4096,
               base::BindOnce(&SocketReadFunction::OnCompleted, this));
}

void SocketReadFunction::OnCompleted(int bytes_read,
                                     scoped_refptr<net::IOBuffer> io_buffer,
                                     bool socket_destroying) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetIntKey(kResultCodeKey, bytes_read);
  base::span<const uint8_t> data_span;
  if (bytes_read > 0)
    data_span = base::as_bytes(base::make_span(io_buffer->data(), bytes_read));
  result.SetKey(kDataKey, base::Value(data_span));
  SetResult(base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(result))));

  AsyncWorkCompleted();
}

SocketWriteFunction::SocketWriteFunction()
    : socket_id_(0), io_buffer_(nullptr), io_buffer_size_(0) {}

SocketWriteFunction::~SocketWriteFunction() {}

bool SocketWriteFunction::Prepare() {
  const auto& list = args_->GetList();
  EXTENSION_FUNCTION_VALIDATE(list.size() >= 2);
  const auto& socket_id_value = list[0];
  const auto& data_value = list[1];
  EXTENSION_FUNCTION_VALIDATE(socket_id_value.is_int());
  EXTENSION_FUNCTION_VALIDATE(data_value.is_blob());

  socket_id_ = socket_id_value.GetInt();
  io_buffer_size_ = data_value.GetBlob().size();
  io_buffer_ = base::MakeRefCounted<net::WrappedIOBuffer>(
      reinterpret_cast<const char*>(data_value.GetBlob().data()));
  return true;
}

void SocketWriteFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(socket_id_);

  if (!socket) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1);
    return;
  }

  socket->Write(io_buffer_, io_buffer_size_,
                base::BindOnce(&SocketWriteFunction::OnCompleted, this));
}

void SocketWriteFunction::OnCompleted(int bytes_written) {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetInteger(kBytesWrittenKey, bytes_written);
  SetResult(std::move(result));

  AsyncWorkCompleted();
}

SocketRecvFromFunction::SocketRecvFromFunction() {}

SocketRecvFromFunction::~SocketRecvFromFunction() {}

bool SocketRecvFromFunction::Prepare() {
  params_ = api::socket::RecvFrom::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketRecvFromFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket || socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kSocketNotFoundError;
    OnCompleted(-1, nullptr, false /* socket_destroying*/, std::string(), 0);
    return;
  }

  socket->RecvFrom(params_->buffer_size.get() ? *params_->buffer_size : 4096,
                   base::BindOnce(&SocketRecvFromFunction::OnCompleted, this));
}

void SocketRecvFromFunction::OnCompleted(int bytes_read,
                                         scoped_refptr<net::IOBuffer> io_buffer,
                                         bool socket_destroying,
                                         const std::string& address,
                                         uint16_t port) {
  base::Value result(base::Value::Type::DICTIONARY);
  result.SetIntKey(kResultCodeKey, bytes_read);
  base::span<const uint8_t> data_span;
  if (bytes_read > 0)
    data_span = base::as_bytes(base::make_span(io_buffer->data(), bytes_read));
  result.SetKey(kDataKey, base::Value(data_span));
  result.SetStringKey(kAddressKey, address);
  result.SetIntKey(kPortKey, port);
  SetResult(base::DictionaryValue::From(
      base::Value::ToUniquePtrValue(std::move(result))));

  AsyncWorkCompleted();
}

SocketSendToFunction::SocketSendToFunction()
    : socket_id_(0), io_buffer_(nullptr), io_buffer_size_(0), port_(0) {}

SocketSendToFunction::~SocketSendToFunction() {}

bool SocketSendToFunction::Prepare() {
  const auto& list = args_->GetList();
  EXTENSION_FUNCTION_VALIDATE(list.size() >= 4);
  const auto& socket_id_value = list[0];
  const auto& data_value = list[1];
  const auto& hostname_value = list[2];
  const auto& port_value = list[3];
  EXTENSION_FUNCTION_VALIDATE(socket_id_value.is_int());
  EXTENSION_FUNCTION_VALIDATE(data_value.is_blob());
  EXTENSION_FUNCTION_VALIDATE(hostname_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(port_value.is_int());

  int port = port_value.GetInt();
  if (!IsPortValid(port)) {
    error_ = kPortInvalidError;
    return false;
  }
  port_ = static_cast<uint16_t>(port);
  socket_id_ = socket_id_value.GetInt();
  hostname_ = hostname_value.GetString();

  io_buffer_size_ = data_value.GetBlob().size();
  io_buffer_ = base::MakeRefCounted<net::WrappedIOBuffer>(
      reinterpret_cast<const char*>(data_value.GetBlob().data()));
  return true;
}

void SocketSendToFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  if (socket->GetSocketType() == Socket::TYPE_UDP) {
    SocketPermission::CheckParam param(
        SocketPermissionRequest::UDP_SEND_TO, hostname_, port_);
    if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
            APIPermissionID::kSocket, &param)) {
      error_ = kPermissionError;
      SetResult(std::make_unique<base::Value>(-1));
      AsyncWorkCompleted();
      return;
    }
  }

  StartDnsLookup(net::HostPortPair(hostname_, port_));
}

void SocketSendToFunction::AfterDnsLookup(int lookup_result) {
  if (lookup_result == net::OK) {
    StartSendTo();
  } else {
    SetResult(std::make_unique<base::Value>(lookup_result));
    AsyncWorkCompleted();
  }
}

void SocketSendToFunction::StartSendTo() {
  Socket* socket = GetSocket(socket_id_);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(-1));
    AsyncWorkCompleted();
    return;
  }

  socket->SendTo(io_buffer_, io_buffer_size_, addresses_.front(),
                 base::BindOnce(&SocketSendToFunction::OnCompleted, this));
}

void SocketSendToFunction::OnCompleted(int bytes_written) {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetInteger(kBytesWrittenKey, bytes_written);
  SetResult(std::move(result));

  AsyncWorkCompleted();
}

SocketSetKeepAliveFunction::SocketSetKeepAliveFunction() {}

SocketSetKeepAliveFunction::~SocketSetKeepAliveFunction() {}

bool SocketSetKeepAliveFunction::Prepare() {
  params_ = api::socket::SetKeepAlive::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketSetKeepAliveFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    SetResult(std::make_unique<base::Value>(false));
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }
  int delay = 0;
  if (params_->delay.get())
    delay = *params_->delay;
  socket->SetKeepAlive(
      params_->enable, delay,
      base::BindOnce(&SocketSetKeepAliveFunction::OnCompleted, this));
}

void SocketSetKeepAliveFunction::OnCompleted(bool success) {
  SetResult(std::make_unique<base::Value>(success));
  AsyncWorkCompleted();
}

SocketSetNoDelayFunction::SocketSetNoDelayFunction() {}

SocketSetNoDelayFunction::~SocketSetNoDelayFunction() {}

bool SocketSetNoDelayFunction::Prepare() {
  params_ = api::socket::SetNoDelay::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketSetNoDelayFunction::AsyncWorkStart() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(false));
    AsyncWorkCompleted();
    return;
  }
  socket->SetNoDelay(
      params_->no_delay,
      base::BindOnce(&SocketSetNoDelayFunction::OnCompleted, this));
}

void SocketSetNoDelayFunction::OnCompleted(bool success) {
  SetResult(std::make_unique<base::Value>(success));
  AsyncWorkCompleted();
}

SocketGetInfoFunction::SocketGetInfoFunction() {}

SocketGetInfoFunction::~SocketGetInfoFunction() {}

bool SocketGetInfoFunction::Prepare() {
  params_ = api::socket::GetInfo::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketGetInfoFunction::Work() {
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    return;
  }

  api::socket::SocketInfo info;
  // This represents what we know about the socket, and does not call through
  // to the system.
  if (socket->GetSocketType() == Socket::TYPE_TCP)
    info.socket_type = extensions::api::socket::SOCKET_TYPE_TCP;
  else
    info.socket_type = extensions::api::socket::SOCKET_TYPE_UDP;
  info.connected = socket->IsConnected();

  // Grab the peer address as known by the OS. This and the call below will
  // always succeed while the socket is connected, even if the socket has
  // been remotely closed by the peer; only reading the socket will reveal
  // that it should be closed locally.
  net::IPEndPoint peerAddress;
  if (socket->GetPeerAddress(&peerAddress)) {
    info.peer_address =
        std::make_unique<std::string>(peerAddress.ToStringWithoutPort());
    info.peer_port = std::make_unique<int>(peerAddress.port());
  }

  // Grab the local address as known by the OS.
  net::IPEndPoint localAddress;
  if (socket->GetLocalAddress(&localAddress)) {
    info.local_address =
        std::make_unique<std::string>(localAddress.ToStringWithoutPort());
    info.local_port = std::make_unique<int>(localAddress.port());
  }

  SetResult(info.ToValue());
}

ExtensionFunction::ResponseAction SocketGetNetworkListFunction::Run() {
  content::GetNetworkService()->GetNetworkList(
      net::INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
      base::BindOnce(&SocketGetNetworkListFunction::GotNetworkList, this));
  return RespondLater();
}

void SocketGetNetworkListFunction::GotNetworkList(
    const base::Optional<net::NetworkInterfaceList>& interface_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!interface_list.has_value()) {
    Respond(Error(kNetworkListError));
    return;
  }

  std::vector<api::socket::NetworkInterface> create_arg;
  create_arg.reserve(interface_list->size());
  for (const net::NetworkInterface& interface : interface_list.value()) {
    api::socket::NetworkInterface info;
    info.name = interface.name;
    info.address = interface.address.ToString();
    info.prefix_length = interface.prefix_length;
    create_arg.push_back(std::move(info));
  }

  Respond(
      ArgumentList(api::socket::GetNetworkList::Results::Create(create_arg)));
}

SocketJoinGroupFunction::SocketJoinGroupFunction() {}

SocketJoinGroupFunction::~SocketJoinGroupFunction() {}

bool SocketJoinGroupFunction::Prepare() {
  params_ = api::socket::JoinGroup::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketJoinGroupFunction::AsyncWorkStart() {
  int result = -1;
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(result));
    AsyncWorkCompleted();
    return;
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kMulticastSocketTypeError;
    SetResult(std::make_unique<base::Value>(result));
    AsyncWorkCompleted();
    return;
  }

  SocketPermission::CheckParam param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);

  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(std::make_unique<base::Value>(result));
    AsyncWorkCompleted();
    return;
  }

  static_cast<UDPSocket*>(socket)->JoinGroup(
      params_->address,
      base::BindOnce(&SocketJoinGroupFunction::OnCompleted, this));
}

void SocketJoinGroupFunction::OnCompleted(int result) {
  if (result != net::OK) {
    error_ = net::ErrorToString(result);
  }
  SetResult(std::make_unique<base::Value>(result));
  AsyncWorkCompleted();
}

SocketLeaveGroupFunction::SocketLeaveGroupFunction() {}

SocketLeaveGroupFunction::~SocketLeaveGroupFunction() {}

bool SocketLeaveGroupFunction::Prepare() {
  params_ = api::socket::LeaveGroup::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketLeaveGroupFunction::AsyncWorkStart() {
  int result = -1;
  Socket* socket = GetSocket(params_->socket_id);

  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(result));
    AsyncWorkCompleted();
    return;
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kMulticastSocketTypeError;
    SetResult(std::make_unique<base::Value>(result));
    AsyncWorkCompleted();
    return;
  }

  SocketPermission::CheckParam param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(std::make_unique<base::Value>(result));
    AsyncWorkCompleted();
    return;
  }

  static_cast<UDPSocket*>(socket)->LeaveGroup(
      params_->address,
      base::BindOnce(&SocketLeaveGroupFunction::OnCompleted, this));
}

void SocketLeaveGroupFunction::OnCompleted(int result) {
  if (result != net::OK)
    error_ = net::ErrorToString(result);
  SetResult(std::make_unique<base::Value>(result));
  AsyncWorkCompleted();
}

SocketSetMulticastTimeToLiveFunction::SocketSetMulticastTimeToLiveFunction() {}

SocketSetMulticastTimeToLiveFunction::~SocketSetMulticastTimeToLiveFunction() {}

bool SocketSetMulticastTimeToLiveFunction::Prepare() {
  params_ = api::socket::SetMulticastTimeToLive::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}
void SocketSetMulticastTimeToLiveFunction::Work() {
  int result = -1;
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(result));
    return;
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kMulticastSocketTypeError;
    SetResult(std::make_unique<base::Value>(result));
    return;
  }

  result =
      static_cast<UDPSocket*>(socket)->SetMulticastTimeToLive(params_->ttl);
  if (result != 0)
    error_ = net::ErrorToString(result);
  SetResult(std::make_unique<base::Value>(result));
}

SocketSetMulticastLoopbackModeFunction::
    SocketSetMulticastLoopbackModeFunction() {}

SocketSetMulticastLoopbackModeFunction::
    ~SocketSetMulticastLoopbackModeFunction() {}

bool SocketSetMulticastLoopbackModeFunction::Prepare() {
  params_ = api::socket::SetMulticastLoopbackMode::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketSetMulticastLoopbackModeFunction::Work() {
  int result = -1;
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(result));
    return;
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kMulticastSocketTypeError;
    SetResult(std::make_unique<base::Value>(result));
    return;
  }

  result = static_cast<UDPSocket*>(socket)
               ->SetMulticastLoopbackMode(params_->enabled);
  if (result != 0)
    error_ = net::ErrorToString(result);
  SetResult(std::make_unique<base::Value>(result));
}

SocketGetJoinedGroupsFunction::SocketGetJoinedGroupsFunction() {}

SocketGetJoinedGroupsFunction::~SocketGetJoinedGroupsFunction() {}

bool SocketGetJoinedGroupsFunction::Prepare() {
  params_ = api::socket::GetJoinedGroups::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

void SocketGetJoinedGroupsFunction::Work() {
  int result = -1;
  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    error_ = kSocketNotFoundError;
    SetResult(std::make_unique<base::Value>(result));
    return;
  }

  if (socket->GetSocketType() != Socket::TYPE_UDP) {
    error_ = kMulticastSocketTypeError;
    SetResult(std::make_unique<base::Value>(result));
    return;
  }

  SocketPermission::CheckParam param(
      SocketPermissionRequest::UDP_MULTICAST_MEMBERSHIP,
      kWildcardAddress,
      kWildcardPort);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermissionID::kSocket, &param)) {
    error_ = kPermissionError;
    SetResult(std::make_unique<base::Value>(result));
    return;
  }

  std::unique_ptr<base::ListValue> values(new base::ListValue());
  values->AppendStrings((std::vector<std::string>&)static_cast<UDPSocket*>(
                            socket)->GetJoinedGroups());
  SetResult(std::move(values));
}

SocketSecureFunction::SocketSecureFunction() {
}

SocketSecureFunction::~SocketSecureFunction() {
}

bool SocketSecureFunction::Prepare() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  params_ = api::socket::Secure::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_.get());
  return true;
}

// Override the regular implementation, which would call AsyncWorkCompleted
// immediately after Work().
void SocketSecureFunction::AsyncWorkStart() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  Socket* socket = GetSocket(params_->socket_id);
  if (!socket) {
    SetResult(std::make_unique<base::Value>(net::ERR_INVALID_ARGUMENT));
    error_ = kSocketNotFoundError;
    AsyncWorkCompleted();
    return;
  }

  // Make sure that the socket is a TCP client socket.
  if (socket->GetSocketType() != Socket::TYPE_TCP) {
    SetResult(std::make_unique<base::Value>(net::ERR_INVALID_ARGUMENT));
    error_ = kSecureSocketTypeError;
    AsyncWorkCompleted();
    return;
  }

  if (!socket->IsConnected()) {
    SetResult(std::make_unique<base::Value>(net::ERR_INVALID_ARGUMENT));
    error_ = kSocketNotConnectedError;
    AsyncWorkCompleted();
    return;
  }

  TCPSocket* tcp_socket = static_cast<TCPSocket*>(socket);
  tcp_socket->UpgradeToTLS(
      params_->options.get(),
      base::BindOnce(&SocketSecureFunction::TlsConnectDone, this));
}

void SocketSecureFunction::TlsConnectDone(
    int result,
    mojo::PendingRemote<network::mojom::TLSClientSocket> tls_socket,
    const net::IPEndPoint& local_addr,
    const net::IPEndPoint& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_pipe_handle,
    mojo::ScopedDataPipeProducerHandle send_pipe_handle) {
  if (result != net::OK) {
    RemoveSocket(params_->socket_id);
    error_ = net::ErrorToString(result);
    results_ = api::socket::Secure::Results::Create(result);
    AsyncWorkCompleted();
    return;
  }

  auto socket =
      std::make_unique<TLSSocket>(std::move(tls_socket), local_addr, peer_addr,
                                  std::move(receive_pipe_handle),
                                  std::move(send_pipe_handle), extension_id());
  ReplaceSocket(params_->socket_id, socket.release());
  results_ = api::socket::Secure::Results::Create(result);
  AsyncWorkCompleted();
}

}  // namespace extensions
