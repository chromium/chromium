// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/win_util.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "remoting/base/logging.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/mojom/remote_security_key.mojom.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"

namespace remoting {

namespace {

// The timeout used to disconnect a client from the IPC Server if it forgets to
// send a request after it is connected.  This ensures the server channel is not
// blocked forever.
constexpr base::TimeDelta kInitialRequestTimeout = base::Seconds(5);

// This value represents the amount of time to wait for a security key request
// from the client before terminating the connection.
constexpr base::TimeDelta kSecurityKeyRequestTimeout = base::Seconds(60);

struct ActiveConnection {
  mojo::ReceiverId receiver_id;
  base::OneShotTimer disconnect_timer;
  mojom::SecurityKeyForwarder::OnSecurityKeyRequestCallback
      on_security_key_request_callback;
};

}  // namespace

// Implements the mojom::SecurityKeyForwarder interface and handles incoming SK
// requests from the IPC client. The caller is responsible for running the IPC
// server and passing in new connections through BindSecurityKeyForwarder().
// TODO(joedow): Update SecurityKeyAuthHandler impls to run on a separate IO
// thread instead of the thread it was created on: crbug.com/591739
class SecurityKeyAuthHandlerWin : public SecurityKeyAuthHandler,
                                  public mojom::SecurityKeyForwarder {
 public:
  explicit SecurityKeyAuthHandlerWin(
      ClientSessionDetails* client_session_details);

  SecurityKeyAuthHandlerWin(const SecurityKeyAuthHandlerWin&) = delete;
  SecurityKeyAuthHandlerWin& operator=(const SecurityKeyAuthHandlerWin&) =
      delete;

  ~SecurityKeyAuthHandlerWin() override;

 private:
  // On Windows, sizeof(int) != sizeof(ReceiverId), so we can't just use the
  // receiver ID as the connection ID.
  using ActiveConnections = std::map</* connection_id */ int, ActiveConnection>;

  // SecurityKeyAuthHandler interface.
  void BindSecurityKeyForwarder(
      mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver) override;
  void CreateSecurityKeyConnection() override;
  bool IsValidConnectionId(int security_key_connection_id) const override;
  void SendClientResponse(int security_key_connection_id,
                          const std::string& response) override;
  void SendErrorAndCloseConnection(int security_key_connection_id) override;
  void SetSendMessageCallback(const SendMessageCallback& callback) override;
  size_t GetActiveConnectionCountForTest() const override;
  void SetRequestTimeoutForTest(base::TimeDelta timeout) override;

  // mojom::SecurityKeyForwarder interface.
  void OnSecurityKeyRequest(const std::string& request_data,
                            OnSecurityKeyRequestCallback callback) override;

  void OnIpcPeerDisconnected();

  // Closes the connection created for a security key forwarding session.
  void CloseSecurityKeyRequestConnection(int connection_id);

  base::OnceClosure GetCloseConnectionClosure(int connection_id);

  // Represents the last id assigned to a new security key request connection.
  int last_connection_id_ = 0;

  // Sends a security key extension message to the client when called.
  SendMessageCallback send_message_callback_;

  // Interface which provides details about the client session.
  raw_ptr<ClientSessionDetails> client_session_details_ = nullptr;

  // Tracks the connection created for each security key forwarding session.
  ActiveConnections active_connections_;

  mojo::ReceiverSet<mojom::SecurityKeyForwarder, /* connection_id */ int>
      receiver_set_;

  // Ensures SecurityKeyAuthHandlerWin methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SecurityKeyAuthHandlerWin> weak_factory_{this};
};

std::unique_ptr<SecurityKeyAuthHandler> SecurityKeyAuthHandler::Create(
    ClientSessionDetails* client_session_details,
    const SendMessageCallback& send_message_callback,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner) {
  std::unique_ptr<SecurityKeyAuthHandler> auth_handler(
      new SecurityKeyAuthHandlerWin(client_session_details));
  auth_handler->SetSendMessageCallback(send_message_callback);
  return auth_handler;
}

SecurityKeyAuthHandlerWin::SecurityKeyAuthHandlerWin(
    ClientSessionDetails* client_session_details)
    : client_session_details_(client_session_details) {
  DCHECK(client_session_details_);
  receiver_set_.set_disconnect_handler(
      base::BindRepeating(&SecurityKeyAuthHandlerWin::OnIpcPeerDisconnected,
                          weak_factory_.GetWeakPtr()));
}

SecurityKeyAuthHandlerWin::~SecurityKeyAuthHandlerWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SecurityKeyAuthHandlerWin::BindSecurityKeyForwarder(
    mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver) {
  DCHECK(thread_checker_.CalledOnValidThread());
  int new_connection_id = ++last_connection_id_;
  // Note that this default-constructs the object.
  ActiveConnection& connection = active_connections_[new_connection_id];
  connection.receiver_id =
      receiver_set_.Add(this, std::move(receiver), new_connection_id);
  // Close the connection if the client doesn't send any requests within the
  // deadline.
  connection.disconnect_timer.Start(
      FROM_HERE, kInitialRequestTimeout,
      GetCloseConnectionClosure(new_connection_id));
}

void SecurityKeyAuthHandlerWin::CreateSecurityKeyConnection() {
  // No-op, since the caller maintains the IPC connection and passes pending
  // receivers via BindSecurityKeyForwarder().
}

bool SecurityKeyAuthHandlerWin::IsValidConnectionId(int connection_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return active_connections_.find(connection_id) != active_connections_.end();
}

void SecurityKeyAuthHandlerWin::SendClientResponse(
    int connection_id,
    const std::string& response_data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto iter = active_connections_.find(connection_id);
  if (iter == active_connections_.end()) {
    HOST_LOG << "Invalid security key connection ID received: "
             << connection_id;
    return;
  }
  ActiveConnection& connection = iter->second;
  std::move(connection.on_security_key_request_callback).Run(response_data);
  // Reset the timer to give the client a chance to send another request.
  connection.disconnect_timer.Start(FROM_HERE, kSecurityKeyRequestTimeout,
                                    GetCloseConnectionClosure(connection_id));
}

void SecurityKeyAuthHandlerWin::SendErrorAndCloseConnection(int connection_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SendClientResponse(connection_id, kSecurityKeyConnectionError);
  CloseSecurityKeyRequestConnection(connection_id);
}

void SecurityKeyAuthHandlerWin::SetSendMessageCallback(
    const SendMessageCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  send_message_callback_ = callback;
}

size_t SecurityKeyAuthHandlerWin::GetActiveConnectionCountForTest() const {
  return active_connections_.size();
}

void SecurityKeyAuthHandlerWin::SetRequestTimeoutForTest(
    base::TimeDelta timeout) {
  // SecurityKeyAuthHandlerWin tests don't override request timeout.
  NOTREACHED();
}

void SecurityKeyAuthHandlerWin::OnSecurityKeyRequest(
    const std::string& request_data,
    OnSecurityKeyRequestCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(send_message_callback_);

  int connection_id = receiver_set_.current_context();
  auto iter = active_connections_.find(connection_id);
  DCHECK(iter != active_connections_.end());
  ActiveConnection& connection = iter->second;
  if (connection.on_security_key_request_callback) {
    LOG(ERROR) << "Received security key request while waiting for a response";
    CloseSecurityKeyRequestConnection(connection_id);
    return;
  }
  // Reset the timer to give the client a chance to send the response.
  connection.disconnect_timer.Start(FROM_HERE, kSecurityKeyRequestTimeout,
                                    GetCloseConnectionClosure(connection_id));
  connection.on_security_key_request_callback = std::move(callback);
  send_message_callback_.Run(connection_id, request_data);
}

void SecurityKeyAuthHandlerWin::OnIpcPeerDisconnected() {
  DCHECK(thread_checker_.CalledOnValidThread());
  active_connections_.erase(receiver_set_.current_context());
}

void SecurityKeyAuthHandlerWin::CloseSecurityKeyRequestConnection(
    int connection_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto iter = active_connections_.find(connection_id);
  if (iter == active_connections_.end()) {
    LOG(ERROR) << "Connection ID " << connection_id << " doesn't exist.";
    return;
  }
  receiver_set_.Remove(iter->second.receiver_id);
  active_connections_.erase(iter);
}

base::OnceClosure SecurityKeyAuthHandlerWin::GetCloseConnectionClosure(
    int connection_id) {
  return base::BindOnce(
      &SecurityKeyAuthHandlerWin::CloseSecurityKeyRequestConnection,
      weak_factory_.GetWeakPtr(), connection_id);
}

}  // namespace remoting
