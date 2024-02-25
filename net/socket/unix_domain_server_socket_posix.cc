// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/unix_domain_server_socket_posix.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "net/base/sockaddr_storage.h"
#include "net/base/sockaddr_util_posix.h"
#include "net/socket/socket_posix.h"
#include "net/socket/unix_domain_client_socket_posix.h"

namespace net {

UnixDomainServerSocket::UnixDomainServerSocket(
    const AuthCallback& auth_callback,
    bool use_abstract_namespace)
    : auth_callback_(auth_callback),
      use_abstract_namespace_(use_abstract_namespace) {
  DCHECK(!auth_callback_.is_null());
}

UnixDomainServerSocket::~UnixDomainServerSocket() = default;

// static
bool UnixDomainServerSocket::GetPeerCredentials(SocketDescriptor socket,
                                                Credentials* credentials) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_FUCHSIA)
  struct ucred user_cred;
  socklen_t len = sizeof(user_cred);
  if (getsockopt(socket, SOL_SOCKET, SO_PEERCRED, &user_cred, &len) < 0)
    return false;
  credentials->process_id = user_cred.pid;
  credentials->user_id = user_cred.uid;
  credentials->group_id = user_cred.gid;
  return true;
#else
  return getpeereid(
      socket, &credentials->user_id, &credentials->group_id) == 0;
#endif
}

int UnixDomainServerSocket::Listen(const IPEndPoint& address,
                                   int backlog,
                                   std::optional<bool> ipv6_only) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int UnixDomainServerSocket::ListenWithAddressAndPort(
    const std::string& address_string,
    uint16_t port,
    int backlog) {
  NOTIMPLEMENTED();
  return ERR_NOT_IMPLEMENTED;
}

int UnixDomainServerSocket::BindAndListen(const std::string& socket_path,
                                          int backlog) {
  DCHECK(!listen_socket_);

  SockaddrStorage address;
  if (!FillUnixAddress(socket_path, use_abstract_namespace_, &address)) {
    return ERR_ADDRESS_INVALID;
  }

  auto socket = std::make_unique<SocketPosix>();
  int rv = socket->Open(AF_UNIX);
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv != OK)
    return rv;

  rv = socket->Bind(address);
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv != OK) {
    PLOG(ERROR)
        << "Could not bind unix domain socket to " << socket_path
        << (use_abstract_namespace_ ? " (with abstract namespace)" : "");
    return rv;
  }

  rv = socket->Listen(backlog);
  DCHECK_NE(ERR_IO_PENDING, rv);
  if (rv != OK)
    return rv;

  listen_socket_.swap(socket);
  return rv;
}

int UnixDomainServerSocket::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(address);

  // Unix domain sockets have no valid associated addr/port;
  // return address invalid.
  return ERR_ADDRESS_INVALID;
}

int UnixDomainServerSocket::Accept(std::unique_ptr<StreamSocket>* socket,
                                   CompletionOnceCallback callback) {
  DCHECK(socket);
  DCHECK(callback);
  DCHECK(!callback_ && !out_socket_.stream && !out_socket_.descriptor);

  out_socket_ = {socket, nullptr};
  int rv = DoAccept();
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);
  else
    CancelCallback();
  return rv;
}

int UnixDomainServerSocket::AcceptSocketDescriptor(
    SocketDescriptor* socket,
    CompletionOnceCallback callback) {
  DCHECK(socket);
  DCHECK(callback);
  DCHECK(!callback_ && !out_socket_.stream && !out_socket_.descriptor);

  out_socket_ = {nullptr, socket};
  int rv = DoAccept();
  if (rv == ERR_IO_PENDING)
    callback_ = std::move(callback);
  else
    CancelCallback();
  return rv;
}

int UnixDomainServerSocket::DoAccept() {
  DCHECK(listen_socket_);
  DCHECK(!accept_socket_);

  while (true) {
    int rv = listen_socket_->Accept(
        &accept_socket_,
        base::BindOnce(&UnixDomainServerSocket::AcceptCompleted,
                       base::Unretained(this)));
    if (rv != OK)
      return rv;
    if (AuthenticateAndGetStreamSocket())
      return OK;
    // Accept another socket because authentication error should be transparent
    // to the caller.
  }
}

void UnixDomainServerSocket::AcceptCompleted(int rv) {
  DCHECK(!callback_.is_null());

  if (rv != OK) {
    RunCallback(rv);
    return;
  }

  if (AuthenticateAndGetStreamSocket()) {
    RunCallback(OK);
    return;
  }

  // Accept another socket because authentication error should be transparent
  // to the caller.
  rv = DoAccept();
  if (rv != ERR_IO_PENDING)
    RunCallback(rv);
}

bool UnixDomainServerSocket::AuthenticateAndGetStreamSocket() {
  DCHECK(accept_socket_);

  Credentials credentials;
  if (!GetPeerCredentials(accept_socket_->socket_fd(), &credentials) ||
      !auth_callback_.Run(credentials)) {
    accept_socket_.reset();
    return false;
  }

  SetSocketResult(std::move(accept_socket_));
  return true;
}

void UnixDomainServerSocket::SetSocketResult(
    std::unique_ptr<SocketPosix> accepted_socket) {
  // Exactly one of the output pointers should be set.
  DCHECK_NE(!!out_socket_.stream, !!out_socket_.descriptor);

  // Pass ownership of |accepted_socket|.
  if (out_socket_.descriptor) {
    *out_socket_.descriptor = accepted_socket->ReleaseConnectedSocket();
    return;
  }
  *out_socket_.stream =
      std::make_unique<UnixDomainClientSocket>(std::move(accepted_socket));
}

void UnixDomainServerSocket::RunCallback(int rv) {
  out_socket_ = SocketDestination();
  std::move(callback_).Run(rv);
}

void UnixDomainServerSocket::CancelCallback() {
  out_socket_ = SocketDestination();
  callback_.Reset();
}

}  // namespace net
