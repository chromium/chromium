// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_proxy_datagram_client_socket.h"

#include "net/base/net_errors.h"

namespace net {

// TODO(crbug.com/1524411) Add net log begin event to constructor.
QuicProxyDatagramClientSocket::QuicProxyDatagramClientSocket(
    const NetLogWithSource& source_net_log,
    url::SchemeHostPort destination)
    : net_log_(NetLogWithSource::Make(
          source_net_log.net_log(),
          NetLogSourceType::QUIC_PROXY_DATAGRAM_CLIENT_SOCKET)),
      destination_(std::move(destination)) {}

QuicProxyDatagramClientSocket::~QuicProxyDatagramClientSocket() {}

int QuicProxyDatagramClientSocket::ConnectViaStream(
    const IPEndPoint& local_address,
    const IPEndPoint& proxy_peer_address,
    std::unique_ptr<QuicChromiumClientStream::Handle> stream,
    CompletionOnceCallback callback) {
  CHECK(!stream_);
  local_address_ = local_address;
  proxy_peer_address_ = proxy_peer_address;
  stream_ = std::move(stream);
  // TODO(crbug.com/1524411) Implement method.
  return OK;
}

int QuicProxyDatagramClientSocket::Connect(const IPEndPoint& address) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::ConnectAsync(
    const IPEndPoint& address,
    CompletionOnceCallback callback) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::ConnectUsingDefaultNetworkAsync(
    const IPEndPoint& address,
    CompletionOnceCallback callback) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::ConnectUsingNetwork(
    handles::NetworkHandle network,
    const IPEndPoint& address) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::ConnectUsingDefaultNetwork(
    const IPEndPoint& address) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::ConnectUsingNetworkAsync(
    handles::NetworkHandle network,
    const IPEndPoint& address,
    CompletionOnceCallback callback) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

// TODO(crbug.com/1524411) Implement method.
handles::NetworkHandle QuicProxyDatagramClientSocket::GetBoundNetwork() const {
  return handles::kInvalidNetworkHandle;
}

// TODO(crbug.com/1524411): Implement method.
void QuicProxyDatagramClientSocket::ApplySocketTag(const SocketTag& tag) {}

int QuicProxyDatagramClientSocket::SetMulticastInterface(
    uint32_t interface_index) {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

void QuicProxyDatagramClientSocket::SetIOSNetworkServiceType(
    int ios_network_service_type) {}

void QuicProxyDatagramClientSocket::Close() {}

int QuicProxyDatagramClientSocket::GetPeerAddress(IPEndPoint* address) const {
  *address = proxy_peer_address_;
  return OK;
}

int QuicProxyDatagramClientSocket::GetLocalAddress(IPEndPoint* address) const {
  *address = local_address_;
  return OK;
}

void QuicProxyDatagramClientSocket::UseNonBlockingIO() {
  NOTREACHED();
}

int QuicProxyDatagramClientSocket::SetDoNotFragment() {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

int QuicProxyDatagramClientSocket::SetRecvTos() {
  NOTREACHED();
  return ERR_NOT_IMPLEMENTED;
}

void QuicProxyDatagramClientSocket::SetMsgConfirm(bool confirm) {
  NOTREACHED();
}

const NetLogWithSource& QuicProxyDatagramClientSocket::NetLog() const {
  return net_log_;
}

// TODO(crbug.com/1524411): Implement method.
int QuicProxyDatagramClientSocket::Read(IOBuffer* buf,
                                        int buf_len,
                                        CompletionOnceCallback callback) {
  return ERR_NOT_IMPLEMENTED;
}

// TODO(crbug.com/1524411): Implement method.
int QuicProxyDatagramClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return ERR_NOT_IMPLEMENTED;
}

// TODO(crbug.com/1524411): Implement method.
int QuicProxyDatagramClientSocket::SetReceiveBufferSize(int32_t size) {
  return ERR_NOT_IMPLEMENTED;
}

// TODO(crbug.com/1524411): Implement method.
int QuicProxyDatagramClientSocket::SetSendBufferSize(int32_t size) {
  return ERR_NOT_IMPLEMENTED;
}

}  // namespace net
