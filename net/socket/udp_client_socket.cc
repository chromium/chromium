// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/udp_client_socket.h"

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

namespace {

base::Value::Dict CreateNetLogUDPConnectParams(const IPEndPoint& address,
                                               int net_error) {
  DCHECK_NE(ERR_IO_PENDING, net_error);
  auto params = base::Value::Dict().Set("address", address.ToString());
  if (net_error < 0) {
    params.Set("net_error", net_error);
  }
  return params;
}

base::Value::Dict CreateNetLogUDPBindToNetworkParams(
    handles::NetworkHandle network,
    int net_error) {
  DCHECK_NE(ERR_IO_PENDING, net_error);
  auto params = base::Value::Dict().Set("network", static_cast<int>(network));
  if (net_error < 0) {
    params.Set("net_error", net_error);
  }
  return params;
}

}  // namespace

UDPClientSocket::UDPClientSocket(DatagramSocket::BindType bind_type,
                                 net::NetLog* net_log,
                                 const net::NetLogSource& source,
                                 handles::NetworkHandle network)
    : net_log_(
          NetLogWithSource::Make(net_log, NetLogSourceType::UDP_CLIENT_SOCKET)),
      socket_(bind_type, net_log, net_log_.source()),
      connect_using_network_(network) {
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE, source);
}

UDPClientSocket::UDPClientSocket(DatagramSocket::BindType bind_type,
                                 NetLogWithSource source_net_log,
                                 handles::NetworkHandle network)
    : net_log_(NetLogWithSource::Make(source_net_log.net_log(),
                                      NetLogSourceType::UDP_CLIENT_SOCKET)),
      socket_(bind_type, net_log_),
      connect_using_network_(network) {
  net_log_.BeginEventReferencingSource(NetLogEventType::SOCKET_ALIVE,
                                       source_net_log.source());
}

UDPClientSocket::~UDPClientSocket() {
  net_log_.EndEvent(NetLogEventType::SOCKET_ALIVE);
}

int UDPClientSocket::Connect(const IPEndPoint& address) {
  CHECK(!connect_called_);
  if (connect_using_network_ != handles::kInvalidNetworkHandle)
    return ConnectUsingNetwork(connect_using_network_, address);

  connect_called_ = true;
  int rv = OK;
  if (!adopted_opened_socket_) {
    rv = socket_.Open(address.GetFamily());
    net_log_.AddEventWithNetErrorCode(NetLogEventType::SOCKET_OPEN, rv);
  }
  if (rv != OK)
    return rv;
  rv = socket_.Connect(address);
  net_log_.AddEvent(NetLogEventType::SOCKET_CONNECT,
                    [&] { return CreateNetLogUDPConnectParams(address, rv); });
  return rv;
}

int UDPClientSocket::ConnectUsingNetwork(handles::NetworkHandle network,
                                         const IPEndPoint& address) {
  CHECK(!connect_called_);
  connect_called_ = true;
  if (!NetworkChangeNotifier::AreNetworkHandlesSupported())
    return ERR_NOT_IMPLEMENTED;
  int rv = OK;
  if (!adopted_opened_socket_) {
    rv = socket_.Open(address.GetFamily());
    net_log_.AddEventWithNetErrorCode(NetLogEventType::SOCKET_OPEN, rv);
  }
  if (rv != OK) {
    return rv;
  }
  rv = socket_.BindToNetwork(network);
  net_log_.AddEvent(NetLogEventType::SOCKET_BIND_TO_NETWORK, [&] {
    return CreateNetLogUDPBindToNetworkParams(network, rv);
  });
  if (rv != OK)
    return rv;
  network_ = network;
  rv = socket_.Connect(address);
  net_log_.AddEvent(NetLogEventType::SOCKET_CONNECT,
                    [&] { return CreateNetLogUDPConnectParams(address, rv); });
  return rv;
}

int UDPClientSocket::ConnectUsingDefaultNetwork(const IPEndPoint& address) {
  CHECK(!connect_called_);
  connect_called_ = true;
  if (!NetworkChangeNotifier::AreNetworkHandlesSupported())
    return ERR_NOT_IMPLEMENTED;
  int rv = OK;
  if (!adopted_opened_socket_) {
    rv = socket_.Open(address.GetFamily());
    net_log_.AddEventWithNetErrorCode(NetLogEventType::SOCKET_OPEN, rv);
  }
  if (rv != OK)
    return rv;
  // Calling connect() will bind a socket to the default network, however there
  // is no way to determine what network the socket got bound to.  The
  // alternative is to query what the default network is and bind the socket to
  // that network explicitly, however this is racy because the default network
  // can change in between when we query it and when we bind to it.  This is
  // rare but should be accounted for.  Since changes of the default network
  // should not come in quick succession, we can simply try again.
  handles::NetworkHandle network;
  for (int attempt = 0; attempt < 2; attempt++) {
    network = NetworkChangeNotifier::GetDefaultNetwork();
    if (network == handles::kInvalidNetworkHandle)
      return ERR_INTERNET_DISCONNECTED;
    rv = socket_.BindToNetwork(network);
    net_log_.AddEvent(NetLogEventType::SOCKET_BIND_TO_NETWORK, [&] {
      return CreateNetLogUDPBindToNetworkParams(network, rv);
    });
    // |network| may have disconnected between the call to GetDefaultNetwork()
    // and the call to BindToNetwork(). Loop only if this is the case (|rv| will
    // be ERR_NETWORK_CHANGED).
    if (rv != ERR_NETWORK_CHANGED)
      break;
  }
  if (rv != OK)
    return rv;
  network_ = network;
  rv = socket_.Connect(address);
  net_log_.AddEvent(NetLogEventType::SOCKET_CONNECT,
                    [&] { return CreateNetLogUDPConnectParams(address, rv); });
  return rv;
}

int UDPClientSocket::ConnectAsync(const IPEndPoint& address,
                                  CompletionOnceCallback callback) {
  DCHECK(callback);
  return Connect(address);
}

int UDPClientSocket::ConnectUsingNetworkAsync(handles::NetworkHandle network,
                                              const IPEndPoint& address,
                                              CompletionOnceCallback callback) {
  DCHECK(callback);
  return ConnectUsingNetwork(network, address);
}

int UDPClientSocket::ConnectUsingDefaultNetworkAsync(
    const IPEndPoint& address,
    CompletionOnceCallback callback) {
  DCHECK(callback);
  return ConnectUsingDefaultNetwork(address);
}

handles::NetworkHandle UDPClientSocket::GetBoundNetwork() const {
  return network_;
}

void UDPClientSocket::ApplySocketTag(const SocketTag& tag) {
  socket_.ApplySocketTag(tag);
}

int UDPClientSocket::Read(IOBuffer* buf,
                          int buf_len,
                          CompletionOnceCallback callback) {
  return socket_.Read(buf, buf_len, std::move(callback));
}

int UDPClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return socket_.Write(buf, buf_len, std::move(callback), traffic_annotation);
}

void UDPClientSocket::Close() {
  socket_.Close();
  adopted_opened_socket_ = false;
}

int UDPClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return socket_.GetPeerAddress(address);
}

int UDPClientSocket::GetLocalAddress(IPEndPoint* address) const {
  return socket_.GetLocalAddress(address);
}

int UDPClientSocket::SetReceiveBufferSize(int32_t size) {
  return socket_.SetReceiveBufferSize(size);
}

int UDPClientSocket::SetSendBufferSize(int32_t size) {
  return socket_.SetSendBufferSize(size);
}

int UDPClientSocket::SetDoNotFragment() {
  return socket_.SetDoNotFragment();
}

int UDPClientSocket::SetRecvTos() {
  return socket_.SetRecvTos();
}

int UDPClientSocket::SetTos(DiffServCodePoint dscp, EcnCodePoint ecn) {
  return socket_.SetTos(dscp, ecn);
}

void UDPClientSocket::SetMsgConfirm(bool confirm) {
  socket_.SetMsgConfirm(confirm);
}

const NetLogWithSource& UDPClientSocket::NetLog() const {
  return socket_.NetLog();
}

void UDPClientSocket::UseNonBlockingIO() {
#if BUILDFLAG(IS_WIN)
  socket_.UseNonBlockingIO();
#endif
}

int UDPClientSocket::SetMulticastInterface(uint32_t interface_index) {
  return socket_.SetMulticastInterface(interface_index);
}

void UDPClientSocket::EnableRecvOptimization() {
#if BUILDFLAG(IS_POSIX)
  socket_.enable_experimental_recv_optimization();
#endif
}

void UDPClientSocket::SetIOSNetworkServiceType(int ios_network_service_type) {
#if BUILDFLAG(IS_POSIX)
  socket_.SetIOSNetworkServiceType(ios_network_service_type);
#endif
}

int UDPClientSocket::AdoptOpenedSocket(AddressFamily address_family,
                                       SocketDescriptor socket) {
  int rv = socket_.AdoptOpenedSocket(address_family, socket);
  if (rv == OK) {
    adopted_opened_socket_ = true;
  }
  return rv;
}

DscpAndEcn UDPClientSocket::GetLastTos() const {
  return socket_.GetLastTos();
}

}  // namespace net
