// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler_mojo.h"

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
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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

}  // namespace

SecurityKeyAuthHandlerMojo::SecurityKeyAuthHandlerMojo(
    ClientSessionDetails* client_session_details)
    : client_session_details_(client_session_details) {
  DCHECK(client_session_details_);
  receiver_set_.set_disconnect_handler(
      base::BindRepeating(&SecurityKeyAuthHandlerMojo::OnIpcPeerDisconnected,
                          weak_factory_.GetWeakPtr()));
}

SecurityKeyAuthHandlerMojo::~SecurityKeyAuthHandlerMojo() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

SecurityKeyAuthHandlerMojo::ActiveConnection::ActiveConnection() = default;

SecurityKeyAuthHandlerMojo::ActiveConnection::~ActiveConnection() = default;

void SecurityKeyAuthHandlerMojo::BindSecurityKeyForwarder(
    mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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

void SecurityKeyAuthHandlerMojo::CreateSecurityKeyConnection() {
  // No-op, since the caller maintains the IPC connection and passes pending
  // receivers via BindSecurityKeyForwarder().
}

bool SecurityKeyAuthHandlerMojo::IsValidConnectionId(int connection_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return active_connections_.find(connection_id) != active_connections_.end();
}

void SecurityKeyAuthHandlerMojo::SendClientResponse(
    int connection_id,
    const std::string& response_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

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

void SecurityKeyAuthHandlerMojo::SendErrorAndCloseConnection(
    int connection_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  SendClientResponse(connection_id, kSecurityKeyConnectionError);
  CloseSecurityKeyRequestConnection(connection_id);
}

void SecurityKeyAuthHandlerMojo::SetSendMessageCallback(
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

void SecurityKeyAuthHandlerMojo::ClearSendMessageCallback(
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

base::WeakPtr<SecurityKeyAuthHandler> SecurityKeyAuthHandlerMojo::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

size_t SecurityKeyAuthHandlerMojo::GetActiveConnectionCountForTest() const {
  return active_connections_.size();
}

void SecurityKeyAuthHandlerMojo::SetRequestTimeoutForTest(
    base::TimeDelta timeout) {
  // SecurityKeyAuthHandlerMojo tests don't override request timeout.
  NOTREACHED();
}

void SecurityKeyAuthHandlerMojo::OnSecurityKeyRequest(
    const std::string& request_data,
    OnSecurityKeyRequestCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  int connection_id = receiver_set_.current_context();
  auto iter = active_connections_.find(connection_id);
  DCHECK(iter != active_connections_.end());
  ActiveConnection& connection = iter->second;
  if (connection.on_security_key_request_callback) {
    LOG(ERROR) << "Received security key request while waiting for a response";
    CloseSecurityKeyRequestConnection(connection_id);
    return;
  }
  if (send_message_callback_.is_null()) {
    LOG(ERROR) << "No callback registered, dropping request.";
    CloseSecurityKeyRequestConnection(connection_id);
    return;
  }

  // Reset the timer to give the client a chance to send the response.
  connection.disconnect_timer.Start(FROM_HERE, kSecurityKeyRequestTimeout,
                                    GetCloseConnectionClosure(connection_id));
  connection.on_security_key_request_callback = std::move(callback);
  send_message_callback_.Run(connection_id, request_data);
}

void SecurityKeyAuthHandlerMojo::OnIpcPeerDisconnected() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  active_connections_.erase(receiver_set_.current_context());
}

void SecurityKeyAuthHandlerMojo::CloseSecurityKeyRequestConnection(
    int connection_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto iter = active_connections_.find(connection_id);
  if (iter == active_connections_.end()) {
    LOG(ERROR) << "Connection ID " << connection_id << " doesn't exist.";
    return;
  }
  receiver_set_.Remove(iter->second.receiver_id);
  active_connections_.erase(iter);
}

base::OnceClosure SecurityKeyAuthHandlerMojo::GetCloseConnectionClosure(
    int connection_id) {
  return base::BindOnce(
      &SecurityKeyAuthHandlerMojo::CloseSecurityKeyRequestConnection,
      weak_factory_.GetWeakPtr(), connection_id);
}

}  // namespace remoting
