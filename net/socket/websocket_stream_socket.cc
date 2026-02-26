// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_stream_socket.h"

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback_helpers.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket.h"
#include "net/socket/stream_socket.h"
#include "net/socket/websocket_endpoint_lock_manager.h"

namespace net {

WebSocketStreamSocket::WebSocketStreamSocket(
    WebSocketEndpointLockManager& websocket_endpoint_lock_manager,
    const IPEndPoint& endpoint,
    std::unique_ptr<StreamSocket> stream_socket)
    : wrapped_socket_(std::move(stream_socket)),
      endpoint_lock_(&websocket_endpoint_lock_manager, endpoint) {}

WebSocketStreamSocket::~WebSocketStreamSocket() = default;

int WebSocketStreamSocket::Read(IOBuffer* buf,
                                int buf_len,
                                CompletionOnceCallback callback) {
  return wrapped_socket_->Read(buf, buf_len, std::move(callback));
}

int WebSocketStreamSocket::ReadIfReady(IOBuffer* buf,
                                       int buf_len,
                                       CompletionOnceCallback callback) {
  return wrapped_socket_->ReadIfReady(buf, buf_len, std::move(callback));
}

int WebSocketStreamSocket::CancelReadIfReady() {
  return wrapped_socket_->CancelReadIfReady();
}

int WebSocketStreamSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  return wrapped_socket_->Write(buf, buf_len, std::move(callback),
                                traffic_annotation);
}

int WebSocketStreamSocket::SetReceiveBufferSize(int32_t size) {
  return wrapped_socket_->SetReceiveBufferSize(size);
}

int WebSocketStreamSocket::SetSendBufferSize(int32_t size) {
  return wrapped_socket_->SetSendBufferSize(size);
}

void WebSocketStreamSocket::SetDnsAliases(std::set<std::string> aliases) {
  wrapped_socket_->SetDnsAliases(std::move(aliases));
}

const std::set<std::string>& WebSocketStreamSocket::GetDnsAliases() const {
  return wrapped_socket_->GetDnsAliases();
}

int WebSocketStreamSocket::Connect(CompletionOnceCallback callback) {
  auto [callback1, callback2] = base::SplitOnceCallback(std::move(callback));
  // Unretained is safe here because destroying the EndpointLock will abort any
  // pending lock request, and thus prevent the callback from being invoked.
  int rv = endpoint_lock_.LockEndpoint(
      base::BindOnce(&WebSocketStreamSocket::OnWebSocketEndpointLockObtained,
                     base::Unretained(this), std::move(callback1)));
  if (rv == ERR_IO_PENDING) {
    return rv;
  }
  return wrapped_socket_->Connect(std::move(callback2));
}

void WebSocketStreamSocket::Disconnect() {
  wrapped_socket_->Disconnect();
}

bool WebSocketStreamSocket::IsConnected() const {
  return wrapped_socket_->IsConnected();
}

bool WebSocketStreamSocket::IsConnectedAndIdle() const {
  return wrapped_socket_->IsConnectedAndIdle();
}

int WebSocketStreamSocket::GetPeerAddress(IPEndPoint* address) const {
  return wrapped_socket_->GetPeerAddress(address);
}

int WebSocketStreamSocket::GetLocalAddress(IPEndPoint* address) const {
  return wrapped_socket_->GetLocalAddress(address);
}

const NetLogWithSource& WebSocketStreamSocket::NetLog() const {
  return wrapped_socket_->NetLog();
}

bool WebSocketStreamSocket::WasEverUsed() const {
  return wrapped_socket_->WasEverUsed();
}

NextProto WebSocketStreamSocket::GetNegotiatedProtocol() const {
  return wrapped_socket_->GetNegotiatedProtocol();
}

bool WebSocketStreamSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return wrapped_socket_->GetSSLInfo(ssl_info);
}

int64_t WebSocketStreamSocket::GetTotalReceivedBytes() const {
  return wrapped_socket_->GetTotalReceivedBytes();
}

void WebSocketStreamSocket::ApplySocketTag(const SocketTag& tag) {
  wrapped_socket_->ApplySocketTag(tag);
}

void WebSocketStreamSocket::OnWebSocketEndpointLockObtained(
    CompletionOnceCallback callback) {
  auto [callback1, callback2] = base::SplitOnceCallback(std::move(callback));

  int rv = wrapped_socket_->Connect(std::move(callback1));
  if (rv != ERR_IO_PENDING) {
    std::move(callback2).Run(rv);
  }
}

}  // namespace net
