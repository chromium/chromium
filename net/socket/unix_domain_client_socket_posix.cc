// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/unix_domain_client_socket_posix.h"

#include <sys/socket.h>
#include <sys/un.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/sockaddr_storage.h"
#include "net/socket/socket_posix.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

UnixDomainClientSocket::UnixDomainClientSocket(const std::string& socket_path,
                                               bool use_abstract_namespace)
    : socket_path_(socket_path),
      use_abstract_namespace_(use_abstract_namespace) {
}

UnixDomainClientSocket::UnixDomainClientSocket(
    std::unique_ptr<SocketPosix> socket)
    : use_abstract_namespace_(false), socket_(std::move(socket)) {}

UnixDomainClientSocket::~UnixDomainClientSocket() {
  Disconnect();
}

// static
bool UnixDomainClientSocket::FillAddress(const std::string& socket_path,
                                         bool use_abstract_namespace,
                                         SockaddrStorage* address) {
  // Caller should provide a non-empty path for the socket address.
  if (socket_path.empty())
    return false;

  size_t path_max = address->addr_len - offsetof(struct sockaddr_un, sun_path);
  // Non abstract namespace pathname should be null-terminated. Abstract
  // namespace pathname must start with '\0'. So, the size is always greater
  // than socket_path size by 1.
  size_t path_size = socket_path.size() + 1;
  if (path_size > path_max)
    return false;

  struct sockaddr_un* socket_addr =
      reinterpret_cast<struct sockaddr_un*>(address->addr);
  memset(socket_addr, 0, address->addr_len);
  socket_addr->sun_family = AF_UNIX;
  address->addr_len = path_size + offsetof(struct sockaddr_un, sun_path);
  if (!use_abstract_namespace) {
    memcpy(socket_addr->sun_path, socket_path.c_str(), socket_path.size());
    return true;
  }

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Convert the path given into abstract socket name. It must start with
  // the '\0' character, so we are adding it. |addr_len| must specify the
  // length of the structure exactly, as potentially the socket name may
  // have '\0' characters embedded (although we don't support this).
  // Note that addr.sun_path is already zero initialized.
  memcpy(socket_addr->sun_path + 1, socket_path.c_str(), socket_path.size());
  return true;
#else
  return false;
#endif
}

int UnixDomainClientSocket::Connect(CompletionOnceCallback callback) {
  if (IsConnected())
    return OK;

  SockaddrStorage address;
  if (!FillAddress(socket_path_, use_abstract_namespace_, &address))
    return ERR_ADDRESS_INVALID;

  socket_ = std::make_unique<SocketPosix>();
  int rv = socket_->Open(AF_UNIX);
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv != OK)
    return rv;

  return socket_->Connect(address, std::move(callback));
}

void UnixDomainClientSocket::Disconnect() {
  socket_.reset();
}

bool UnixDomainClientSocket::IsConnected() const {
  return socket_ && socket_->IsConnected();
}

bool UnixDomainClientSocket::IsConnectedAndIdle() const {
  return socket_ && socket_->IsConnectedAndIdle();
}

int UnixDomainClientSocket::GetPeerAddress(IPEndPoint* address) const {
  // Unix domain sockets have no valid associated addr/port;
  // return either not connected or address invalid.
  DCHECK(address);

  if (!IsConnected())
    return ERR_SOCKET_NOT_CONNECTED;

  return ERR_ADDRESS_INVALID;
}

int UnixDomainClientSocket::GetLocalAddress(IPEndPoint* address) const {
  // Unix domain sockets have no valid associated addr/port;
  // return either not connected or address invalid.
  DCHECK(address);

  if (!socket_)
    return ERR_SOCKET_NOT_CONNECTED;

  return ERR_ADDRESS_INVALID;
}

const NetLogWithSource& UnixDomainClientSocket::NetLog() const {
  return net_log_;
}

bool UnixDomainClientSocket::WasEverUsed() const {
  return true;  // We don't care.
}

bool UnixDomainClientSocket::WasAlpnNegotiated() const {
  return false;
}

NextProto UnixDomainClientSocket::GetNegotiatedProtocol() const {
  return kProtoUnknown;
}

bool UnixDomainClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return false;
}

void UnixDomainClientSocket::GetConnectionAttempts(
    ConnectionAttempts* out) const {
  out->clear();
}

int64_t UnixDomainClientSocket::GetTotalReceivedBytes() const {
  NOTIMPLEMENTED();
  return 0;
}

void UnixDomainClientSocket::ApplySocketTag(const SocketTag& tag) {
  // Ignore socket tags as Unix domain sockets are local only.
}

int UnixDomainClientSocket::Read(IOBuffer* buf,
                                 int buf_len,
                                 CompletionOnceCallback callback) {
  DCHECK(socket_);
  return socket_->Read(buf, buf_len, std::move(callback));
}

int UnixDomainClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(socket_);
  return socket_->Write(buf, buf_len, std::move(callback), traffic_annotation);
}

int UnixDomainClientSocket::SetReceiveBufferSize(int32_t size) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int UnixDomainClientSocket::SetSendBufferSize(int32_t size) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

SocketDescriptor UnixDomainClientSocket::ReleaseConnectedSocket() {
  DCHECK(socket_);
  DCHECK(socket_->IsConnected());

  SocketDescriptor socket_fd = socket_->ReleaseConnectedSocket();
  socket_.reset();
  return socket_fd;
}

}  // namespace net
