// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler_posix.h"

#include <unistd.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#include "remoting/base/logging.h"
#include "remoting/host/security_key/security_key_socket.h"

namespace remoting {

namespace {

const int64_t kDefaultRequestTimeoutSeconds = 60;

base::FilePath& GetMutableSecurityKeySocketName() {
  static base::NoDestructor<base::FilePath> path{
      SecurityKeyAuthHandlerPosix::GetDefaultSecurityKeySocketName()};
  return *path;
}

// Socket authentication function that only allows connections from callers with
// the current uid.
bool MatchUid(const net::UnixDomainServerSocket::Credentials& credentials) {
  bool allowed = credentials.user_id == getuid();
  if (!allowed) {
    HOST_LOG << "Refused socket connection from uid " << credentials.user_id;
  }
  return allowed;
}

// Returns the command code (the first byte of the data) if it exists, or -1 if
// the data is empty.
unsigned int GetCommandCode(const std::string& data) {
  return data.empty() ? -1 : static_cast<unsigned int>(data[0]);
}

}  // namespace

// static
base::FilePath SecurityKeyAuthHandlerPosix::GetDefaultSecurityKeySocketName() {
#if BUILDFLAG(IS_LINUX)
  // LINT.IfChange(ssh_auth_sock_name)
  const char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (xdg_runtime_dir) {
    return base::FilePath(xdg_runtime_dir).Append("crd_ssh_auth_sock");
  }
  // LINT.ThenChange(//remoting/host/linux/linux_me2me_host.py:ssh_auth_sock_name)
  LOG(WARNING) << "Cannot find the XDG_RUNTIME_DIR environment variable.";
#else
  NOTIMPLEMENTED();
#endif
  return {};
}

// static
const base::FilePath& SecurityKeyAuthHandlerPosix::GetSecurityKeySocketName() {
  return GetMutableSecurityKeySocketName();
}

// static
void SecurityKeyAuthHandlerPosix::SetSecurityKeySocketName(
    const base::FilePath& security_key_socket_name) {
  GetMutableSecurityKeySocketName() = security_key_socket_name;
}

SecurityKeyAuthHandlerPosix::SecurityKeyAuthHandlerPosix(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(file_task_runner),
      request_timeout_(base::Seconds(kDefaultRequestTimeoutSeconds)) {}

SecurityKeyAuthHandlerPosix::~SecurityKeyAuthHandlerPosix() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (file_task_runner_) {
    // Attempt to clean up the socket before being destroyed.
    file_task_runner_->PostTask(
        FROM_HERE, base::GetDeleteFileCallback(GetSecurityKeySocketName()));
  }
}

void SecurityKeyAuthHandlerPosix::CreateSecurityKeyConnection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!GetSecurityKeySocketName().empty());

  // We need to run the DeleteFile method on |file_task_runner_| as it is a
  // blocking function call which cannot be run on the main thread.  Once
  // that task has completed, the main thread will be called back and we will
  // resume setting up our security key auth socket there.
  file_task_runner_->PostTask(
      FROM_HERE, base::GetDeleteFileCallback(
                     GetSecurityKeySocketName(),
                     base::BindOnce(&SecurityKeyAuthHandlerPosix::CreateSocket,
                                    weak_factory_.GetWeakPtr())));
}

void SecurityKeyAuthHandlerPosix::CreateSocket(bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  HOST_LOG << "Listening for security key requests on "
           << GetSecurityKeySocketName().value();

  if (!success) {
    LOG(ERROR) << "Delete g_security_key_socket_name failed";
    return;
  }

  auth_socket_ = std::make_unique<net::UnixDomainServerSocket>(
      base::BindRepeating(MatchUid), false);
  int rv = auth_socket_->BindAndListen(GetSecurityKeySocketName().value(),
                                       /*backlog=*/1);
  if (rv != net::OK) {
    LOG(ERROR) << "Failed to open socket for auth requests: '" << rv << "'";
    return;
  }
  DoAccept();
}

bool SecurityKeyAuthHandlerPosix::IsValidConnectionId(int connection_id) const {
  return GetSocketForConnectionId(connection_id) != active_sockets_.end();
}

void SecurityKeyAuthHandlerPosix::SendClientResponse(
    int connection_id,
    const std::string& response) {
  auto iter = GetSocketForConnectionId(connection_id);
  if (iter != active_sockets_.end()) {
    HOST_DLOG << "Sending client response to socket: " << connection_id;
    iter->second->SendResponse(response);
    iter->second->StartReadingRequest(
        base::BindOnce(&SecurityKeyAuthHandlerPosix::OnReadComplete,
                       base::Unretained(this), connection_id));
  } else {
    LOG(WARNING) << "Unknown gnubby-auth connection id: " << connection_id;
  }
}

void SecurityKeyAuthHandlerPosix::SendErrorAndCloseConnection(int id) {
  auto iter = GetSocketForConnectionId(id);
  if (iter != active_sockets_.end()) {
    HOST_DLOG << "Sending error and closing socket: " << id;
    SendErrorAndCloseActiveSocket(iter);
  } else {
    LOG(WARNING) << "Unknown gnubby-auth connection id: " << id;
  }
}

void SecurityKeyAuthHandlerPosix::SetSendMessageCallback(
    const SendMessageCallback& callback) {
  send_message_callback_ = callback;
}

size_t SecurityKeyAuthHandlerPosix::GetActiveConnectionCountForTest() const {
  return active_sockets_.size();
}

void SecurityKeyAuthHandlerPosix::SetRequestTimeoutForTest(
    base::TimeDelta timeout) {
  request_timeout_ = timeout;
}

void SecurityKeyAuthHandlerPosix::DoAccept() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  int result = auth_socket_->Accept(
      &accept_socket_, base::BindOnce(&SecurityKeyAuthHandlerPosix::OnAccepted,
                                      base::Unretained(this)));
  if (result != net::ERR_IO_PENDING) {
    OnAccepted(result);
  }
}

void SecurityKeyAuthHandlerPosix::OnAccepted(int result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(net::ERR_IO_PENDING, result);

  if (result < 0) {
    LOG(ERROR) << "Error accepting new socket connection: " << result;
    return;
  }

  int security_key_connection_id = ++last_connection_id_;
  HOST_DLOG << "Creating new socket: " << security_key_connection_id;
  SecurityKeySocket* socket = new SecurityKeySocket(
      std::move(accept_socket_), request_timeout_,
      base::BindOnce(&SecurityKeyAuthHandlerPosix::RequestTimedOut,
                     base::Unretained(this), security_key_connection_id));
  active_sockets_[security_key_connection_id] = base::WrapUnique(socket);
  socket->StartReadingRequest(
      base::BindOnce(&SecurityKeyAuthHandlerPosix::OnReadComplete,
                     base::Unretained(this), security_key_connection_id));

  // Continue accepting new connections.
  DoAccept();
}

void SecurityKeyAuthHandlerPosix::OnReadComplete(int connection_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  ActiveSockets::const_iterator iter = active_sockets_.find(connection_id);
  DCHECK(iter != active_sockets_.end());
  std::string request_data;
  if (!iter->second->GetAndClearRequestData(&request_data)) {
    HOST_DLOG << "Closing socket: " << connection_id;
    if (iter->second->socket_read_error()) {
      iter->second->SendSshError();
    }
    active_sockets_.erase(iter);
    return;
  }

  HOST_LOG << "Received request from socket: " << connection_id
           << ", code: " << GetCommandCode(request_data);
  send_message_callback_.Run(connection_id, request_data);
}

SecurityKeyAuthHandlerPosix::ActiveSockets::const_iterator
SecurityKeyAuthHandlerPosix::GetSocketForConnectionId(int connection_id) const {
  return active_sockets_.find(connection_id);
}

void SecurityKeyAuthHandlerPosix::SendErrorAndCloseActiveSocket(
    const ActiveSockets::const_iterator& iter) {
  iter->second->SendSshError();
  active_sockets_.erase(iter);
}

void SecurityKeyAuthHandlerPosix::RequestTimedOut(int connection_id) {
  HOST_LOG << "SecurityKey request timed out for socket: " << connection_id;
  ActiveSockets::const_iterator iter = active_sockets_.find(connection_id);
  if (iter != active_sockets_.end()) {
    SendErrorAndCloseActiveSocket(iter);
  }
}

}  // namespace remoting
