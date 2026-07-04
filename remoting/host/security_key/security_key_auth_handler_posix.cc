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
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#include "remoting/base/logging.h"
#include "remoting/base/security_key_socket_name.h"
#include "remoting/host/security_key/security_key_socket.h"

namespace remoting {

namespace {

const int64_t kDefaultRequestTimeoutSeconds = 60;

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

base::Lock& GetGlobalResourceLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

base::FilePath& GetMutableSecurityKeySocketName() {
  static base::NoDestructor<base::FilePath> socket_name;
  return *socket_name;
}

scoped_refptr<base::SequencedTaskRunner>& GetFileTaskRunnerRef() {
  static base::NoDestructor<scoped_refptr<base::SequencedTaskRunner>> runner;
  return *runner;
}

scoped_refptr<base::SequencedTaskRunner> GetFileTaskRunner() {
  base::AutoLock l(GetGlobalResourceLock());
  auto& runner = GetFileTaskRunnerRef();
  if (!runner) {
    // We use a single, shared, global task runner to serialize all socket file
    // operations (creation and deletion) across multiple sessions. This
    // prevents concurrency race conditions, such as one session deleting the
    // socket file currently in use by another concurrent session.
    runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(),
         // We use USER_VISIBLE priority to ensure prompt socket file cleanup
         // during session termination. Using BEST_EFFORT could delay cleanup
         // under load, causing subsequent quick reconnects to fail with
         // "Address already in use" (EADDRINUSE) if the old file still exists.
         base::TaskPriority::USER_VISIBLE,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  }
  return runner;
}

}  // namespace

// static
base::FilePath SecurityKeyAuthHandlerPosix::GetSecurityKeySocketName() {
  base::AutoLock l(GetGlobalResourceLock());
  if (GetMutableSecurityKeySocketName().empty()) {
    GetMutableSecurityKeySocketName() = GetDefaultSecurityKeySocketName();
  }
  return GetMutableSecurityKeySocketName();
}

// static
void SecurityKeyAuthHandlerPosix::SetSecurityKeySocketName(
    const base::FilePath& security_key_socket_name) {
  base::AutoLock l(GetGlobalResourceLock());
  GetMutableSecurityKeySocketName() = security_key_socket_name;
}

// static
void SecurityKeyAuthHandlerPosix::ResetTaskRunnerForTesting() {
  base::AutoLock l(GetGlobalResourceLock());
  GetFileTaskRunnerRef() = nullptr;
}

// static
std::unique_ptr<SecurityKeyAuthHandlerPosix>
SecurityKeyAuthHandlerPosix::CreateForTesting(
    const base::FilePath& socket_name,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner) {
  return base::WrapUnique(new SecurityKeyAuthHandlerPosix(
      socket_name, std::move(file_task_runner)));
}

SecurityKeyAuthHandlerPosix::SecurityKeyAuthHandlerPosix()
    : socket_name_(GetSecurityKeySocketName()),
      file_task_runner_(GetFileTaskRunner()),
      request_timeout_(base::Seconds(kDefaultRequestTimeoutSeconds)) {
  DCHECK(!socket_name_.empty());
  DCHECK(file_task_runner_);
}

SecurityKeyAuthHandlerPosix::SecurityKeyAuthHandlerPosix(
    const base::FilePath& socket_name,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : socket_name_(socket_name),
      file_task_runner_(std::move(file_task_runner)),
      request_timeout_(base::Seconds(kDefaultRequestTimeoutSeconds)) {
  DCHECK(!socket_name_.empty());
  DCHECK(file_task_runner_);
}

SecurityKeyAuthHandlerPosix::~SecurityKeyAuthHandlerPosix() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!socket_name_.empty() && file_task_runner_) {
    // Attempt to clean up the socket before being destroyed.
    file_task_runner_->PostTask(FROM_HERE,
                                base::GetDeleteFileCallback(socket_name_));
  }
}

void SecurityKeyAuthHandlerPosix::CreateSecurityKeyConnection() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!socket_name_.empty());

  if (socket_created_) {
    return;
  }
  socket_created_ = true;

  // We need to run the DeleteFile method on the ThreadPool as it is a
  // blocking function call which cannot be run on the main thread.  Once
  // that task has completed, the main thread will be called back and we will
  // resume setting up our security key auth socket there.
  file_task_runner_->PostTask(
      FROM_HERE, base::GetDeleteFileCallback(
                     socket_name_,
                     base::BindOnce(&SecurityKeyAuthHandlerPosix::CreateSocket,
                                    weak_factory_.GetWeakPtr())));
}

void SecurityKeyAuthHandlerPosix::CreateSocket(bool success) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  HOST_LOG << "Listening for security key requests on " << socket_name_.value();

  if (!success) {
    LOG(ERROR) << "Delete socket file failed: " << socket_name_.value();
    return;
  }

  auth_socket_ = std::make_unique<net::UnixDomainServerSocket>(
      base::BindRepeating(MatchUid), false);
  int rv = auth_socket_->BindAndListen(socket_name_.value(),
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
    const SendMessageCallback& callback,
    const void* client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (active_client_id_ && active_client_id_ != client_id) {
    VLOG(1) << "Overwriting active client: " << active_client_id_
            << " with: " << client_id;
  }
  send_message_callback_ = callback;
  active_client_id_ = client_id;
}

void SecurityKeyAuthHandlerPosix::ClearSendMessageCallback(
    const void* client_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (active_client_id_ == client_id) {
    send_message_callback_.Reset();
    active_client_id_ = nullptr;
  } else if (active_client_id_) {
    VLOG(1) << "Ignoring request to clear callback for client: " << client_id
            << " (active client is: " << active_client_id_ << ")";
  }
}

base::WeakPtr<SecurityKeyAuthHandler>
SecurityKeyAuthHandlerPosix::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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

  if (send_message_callback_.is_null()) {
    LOG(ERROR) << "No callback registered, dropping request.";
    active_sockets_.erase(iter);
    return;
  }
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
