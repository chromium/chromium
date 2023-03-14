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

UDPClientSocket::UDPClientSocket(DatagramSocket::BindType bind_type,
                                 net::NetLog* net_log,
                                 const net::NetLogSource& source,
                                 handles::NetworkHandle network)
    : socket_(bind_type, net_log, source), connect_using_network_(network) {}

UDPClientSocket::~UDPClientSocket() = default;

int UDPClientSocket::Connect(const IPEndPoint& address) {
  CHECK(!connect_called_);
  if (connect_using_network_ != handles::kInvalidNetworkHandle)
    return ConnectUsingNetwork(connect_using_network_, address);

  connect_called_ = true;
  int rv = socket_.Open(address.GetFamily());
  if (rv != OK)
    return rv;
  return socket_.Connect(address);
}

int UDPClientSocket::ConnectUsingNetwork(handles::NetworkHandle network,
                                         const IPEndPoint& address) {
  CHECK(!connect_called_);
  connect_called_ = true;
  if (!NetworkChangeNotifier::AreNetworkHandlesSupported())
    return ERR_NOT_IMPLEMENTED;
  int rv = socket_.Open(address.GetFamily());
  if (rv != OK)
    return rv;
  rv = socket_.BindToNetwork(network);
  if (rv != OK)
    return rv;
  network_ = network;
  return socket_.Connect(address);
}

int UDPClientSocket::ConnectUsingDefaultNetwork(const IPEndPoint& address) {
  CHECK(!connect_called_);
  connect_called_ = true;
  if (!NetworkChangeNotifier::AreNetworkHandlesSupported())
    return ERR_NOT_IMPLEMENTED;
  int rv;
  rv = socket_.Open(address.GetFamily());
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
    // |network| may have disconnected between the call to GetDefaultNetwork()
    // and the call to BindToNetwork(). Loop only if this is the case (|rv| will
    // be ERR_NETWORK_CHANGED).
    if (rv != ERR_NETWORK_CHANGED)
      break;
  }
  if (rv != OK)
    return rv;
  network_ = network;
  return socket_.Connect(address);
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

void UDPClientSocket::AdoptOpenedSocket(AddressFamily address_family,
                                        SocketDescriptor socket) {
  socket_.AdoptOpenedSocket(address_family, socket);
}

}  // namespace net
