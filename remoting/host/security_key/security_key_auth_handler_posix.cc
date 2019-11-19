// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler.h"

#include <unistd.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#include "remoting/base/logging.h"
#include "remoting/host/security_key/security_key_socket.h"

namespace {

const int64_t kDefaultRequestTimeoutSeconds = 60;

// The name of the socket to listen for security key requests on.
base::LazyInstance<base::FilePath>::Leaky g_security_key_socket_name =
    LAZY_INSTANCE_INITIALIZER;

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

namespace remoting {

class SecurityKeyAuthHandlerPosix : public SecurityKeyAuthHandler {
 public:
  explicit SecurityKeyAuthHandlerPosix(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);
  ~SecurityKeyAuthHandlerPosix() override;

 private:
  typedef std::map<int, std::unique_ptr<SecurityKeySocket>> ActiveSockets;

  // SecurityKeyAuthHandler interface.
  void CreateSecurityKeyConnection() override;
  bool IsValidConnectionId(int security_key_connection_id) const override;
  void SendClientResponse(int security_key_connection_id,
                          const std::string& response) override;
  void SendErrorAndCloseConnection(int security_key_connection_id) override;
  void SetSendMessageCallback(const SendMessageCallback& callback) override;
  size_t GetActiveConnectionCountForTest() const override;
  void SetRequestTimeoutForTest(base::TimeDelta timeout) override;

  // Sets up the socket used for accepting new connections.
  void CreateSocket();

  // Starts listening for connection.
  void DoAccept();

  // Called when a connection is accepted.
  void OnAccepted(int result);

  // Called when a SecurityKeySocket has done reading.
  void OnReadComplete(int security_key_connection_id);

  // Gets an active socket iterator for |security_key_connection_id|.
  ActiveSockets::const_iterator GetSocketForConnectionId(
      int security_key_connection_id) const;

  // Send an error and closes an active socket.
  void SendErrorAndCloseActiveSocket(const ActiveSockets::const_iterator& iter);

  // A request timed out.
  void RequestTimedOut(int security_key_connection_id);

  // Ensures SecurityKeyAuthHandlerPosix methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Socket used to listen for authorization requests.
  std::unique_ptr<net::UnixDomainServerSocket> auth_socket_;

  // A temporary holder for an accepted connection.
  std::unique_ptr<net::StreamSocket> accept_socket_;

  // Used to pass security key extension messages to the client.
  SendMessageCallback send_message_callback_;

  // The last assigned security key connection id.
  int last_connection_id_ = 0;

  // Sockets by connection id used to process gnubbyd requests.
  ActiveSockets active_sockets_;

  // Used to perform blocking File IO.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;

  // Timeout used for a request.
  base::TimeDelta request_timeout_;

  base::WeakPtrFactory<SecurityKeyAuthHandlerPosix> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SecurityKeyAuthHandlerPosix);
};

std::unique_ptr<SecurityKeyAuthHandler> SecurityKeyAuthHandler::Create(
    ClientSessionDetails* client_session_details,
    const SendMessageCallback& send_message_callback,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
  std::unique_ptr<SecurityKeyAuthHandler> auth_handler(
      new SecurityKeyAuthHandlerPosix(file_task_runner));
  auth_handler->SetSendMessageCallback(send_message_callback);
  return auth_handler;
}

void SecurityKeyAuthHandler::SetSecurityKeySocketName(
    const base::FilePath& security_key_socket_name) {
  g_security_key_socket_name.Get() = security_key_socket_name;
}

SecurityKeyAuthHandlerPosix::SecurityKeyAuthHandlerPosix(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(file_task_runner),
      request_timeout_(
          base::TimeDelta::FromSeconds(kDefaultRequestTimeoutSeconds)) {}

SecurityKeyAuthHandlerPosix::~SecurityKeyAuthHandlerPosix() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (file_task_runner_) {
    // Attempt to clean up the socket before being destroyed.
    file_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(base::IgnoreResult(&base::DeleteFile),
                                  g_security_key_socket_name.Get(),
                                  /*recursive=*/false));
  }
}

void SecurityKeyAuthHandlerPosix::CreateSecurityKeyConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!g_security_key_socket_name.Get().empty());

  // We need to run the DeleteFile method on |file_task_runner_| as it is a
  // blocking function call which cannot be run on the main thread.  Once
  // that task has completed, the main thread will be called back and we will
  // resume setting up our security key auth socket there.
  file_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(base::IgnoreResult(&base::DeleteFile),
                            g_security_key_socket_name.Get(),
                            /*recursive=*/false),
      base::Bind(&SecurityKeyAuthHandlerPosix::CreateSocket,
                 weak_factory_.GetWeakPtr()));
}

void SecurityKeyAuthHandlerPosix::CreateSocket() {
  DCHECK(thread_checker_.CalledOnValidThread());
  HOST_LOG << "Listening for security key requests on "
           << g_security_key_socket_name.Get().value();

  auth_socket_.reset(
      new net::UnixDomainServerSocket(base::Bind(MatchUid), false));
  int rv = auth_socket_->BindAndListen(g_security_key_socket_name.Get().value(),
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
        base::Bind(&SecurityKeyAuthHandlerPosix::OnReadComplete,
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
  DCHECK(thread_checker_.CalledOnValidThread());
  int result = auth_socket_->Accept(
      &accept_socket_, base::Bind(&SecurityKeyAuthHandlerPosix::OnAccepted,
                                  base::Unretained(this)));
  if (result != net::ERR_IO_PENDING) {
    OnAccepted(result);
  }
}

void SecurityKeyAuthHandlerPosix::OnAccepted(int result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(net::ERR_IO_PENDING, result);

  if (result < 0) {
    LOG(ERROR) << "Error accepting new socket connection: " << result;
    return;
  }

  int security_key_connection_id = ++last_connection_id_;
  HOST_DLOG << "Creating new socket: " << security_key_connection_id;
  SecurityKeySocket* socket = new SecurityKeySocket(
      std::move(accept_socket_), request_timeout_,
      base::Bind(&SecurityKeyAuthHandlerPosix::RequestTimedOut,
                 base::Unretained(this), security_key_connection_id));
  active_sockets_[security_key_connection_id] = base::WrapUnique(socket);
  socket->StartReadingRequest(
      base::Bind(&SecurityKeyAuthHandlerPosix::OnReadComplete,
                 base::Unretained(this), security_key_connection_id));

  // Continue accepting new connections.
  DoAccept();
}

void SecurityKeyAuthHandlerPosix::OnReadComplete(int connection_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

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
