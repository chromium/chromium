// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_auth_handler.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/win/win_util.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "remoting/base/logging.h"
#include "remoting/host/chromoting_messages.h"
#include "remoting/host/client_session_details.h"
#include "remoting/host/security_key/security_key_ipc_constants.h"
#include "remoting/host/security_key/security_key_ipc_server.h"

namespace {

// The timeout used to disconnect a client from the IPC Server channel if it
// forgets to do so.  This ensures the server channel is not blocked forever.
constexpr base::TimeDelta kInitialRequestTimeout =
    base::TimeDelta::FromSeconds(5);

// This value represents the amount of time to wait for a security key request
// from the client before terminating the connection.
constexpr base::TimeDelta kSecurityKeyRequestTimeout =
    base::TimeDelta::FromSeconds(60);

}  // namespace

namespace remoting {

// Creates an IPC server channel which services IPC clients that want to start
// a security key forwarding session.  Once an IPC Client connects to the
// server, the SecurityKeyAuthHandlerWin class will create a new
// SecurityKeyIpcServer instance to service the next request.  This system
// allows multiple security key forwarding sessions to occur concurrently.
// TODO(joedow): Update SecurityKeyAuthHandler impls to run on a separate IO
// thread instead of the thread it was created on: crbug.com/591739
class SecurityKeyAuthHandlerWin : public SecurityKeyAuthHandler {
 public:
  explicit SecurityKeyAuthHandlerWin(
      ClientSessionDetails* client_session_details);
  ~SecurityKeyAuthHandlerWin() override;

 private:
  typedef std::map<int, std::unique_ptr<SecurityKeyIpcServer>> ActiveChannels;

  // SecurityKeyAuthHandler interface.
  void CreateSecurityKeyConnection() override;
  bool IsValidConnectionId(int security_key_connection_id) const override;
  void SendClientResponse(int security_key_connection_id,
                          const std::string& response) override;
  void SendErrorAndCloseConnection(int security_key_connection_id) override;
  void SetSendMessageCallback(const SendMessageCallback& callback) override;
  size_t GetActiveConnectionCountForTest() const override;
  void SetRequestTimeoutForTest(base::TimeDelta timeout) override;

  // Creates the IPC server channel and waits for a connection using
  // |ipc_server_channel_name_|.
  void StartIpcServerChannel();

  // Closes the IPC channel created for a security key forwarding session.
  void CloseSecurityKeyRequestIpcChannel(int connection_id);

  // Returns the IPC Channel instance created for |connection_id|.
  ActiveChannels::const_iterator GetChannelForConnectionId(
      int connection_id) const;

  void OnChannelConnected();

  // Represents the last id assigned to a new security key request IPC channel.
  int last_connection_id_ = 0;

  // Sends a security key extension message to the client when called.
  SendMessageCallback send_message_callback_;

  // Interface which provides details about the client session.
  ClientSessionDetails* client_session_details_ = nullptr;

  // Tracks the IPC channel created for each security key forwarding session.
  ActiveChannels active_channels_;

  // The amount of time to wait for a client to process the connection details
  // message and disconnect from the IPC server channel before disconnecting it.
  base::TimeDelta disconnect_timeout_;

  // Ensures SecurityKeyAuthHandlerWin methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SecurityKeyAuthHandlerWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SecurityKeyAuthHandlerWin);
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
    : client_session_details_(client_session_details),
      disconnect_timeout_(kInitialRequestTimeout) {
  DCHECK(client_session_details_);
}

SecurityKeyAuthHandlerWin::~SecurityKeyAuthHandlerWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void SecurityKeyAuthHandlerWin::CreateSecurityKeyConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  StartIpcServerChannel();
}

bool SecurityKeyAuthHandlerWin::IsValidConnectionId(int connection_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return connection_id != last_connection_id_ &&
         (GetChannelForConnectionId(connection_id) != active_channels_.end());
}

void SecurityKeyAuthHandlerWin::SendClientResponse(
    int connection_id,
    const std::string& response_data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ActiveChannels::const_iterator iter =
      GetChannelForConnectionId(connection_id);
  if (iter == active_channels_.end()) {
    HOST_LOG << "Invalid security key connection ID received: "
             << connection_id;
    return;
  }

  if (!iter->second->SendResponse(response_data)) {
    CloseSecurityKeyRequestIpcChannel(connection_id);
  }
}

void SecurityKeyAuthHandlerWin::SendErrorAndCloseConnection(int connection_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SendClientResponse(connection_id, kSecurityKeyConnectionError);
  CloseSecurityKeyRequestIpcChannel(connection_id);
}

void SecurityKeyAuthHandlerWin::SetSendMessageCallback(
    const SendMessageCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  send_message_callback_ = callback;
}

size_t SecurityKeyAuthHandlerWin::GetActiveConnectionCountForTest() const {
  if (active_channels_.empty()) {
    return 0u;
  }
  // One channel is waiting for a connection.
  return active_channels_.size() - 1;
}

void SecurityKeyAuthHandlerWin::SetRequestTimeoutForTest(
    base::TimeDelta timeout) {
  disconnect_timeout_ = timeout;
}

void SecurityKeyAuthHandlerWin::StartIpcServerChannel() {
  DCHECK(thread_checker_.CalledOnValidThread());

  int new_connection_id = ++last_connection_id_;
  std::unique_ptr<SecurityKeyIpcServer> ipc_server(SecurityKeyIpcServer::Create(
      new_connection_id, client_session_details_, disconnect_timeout_,
      send_message_callback_,
      base::Bind(&SecurityKeyAuthHandlerWin::OnChannelConnected,
                 base::Unretained(this)),
      base::Bind(&SecurityKeyAuthHandlerWin::CloseSecurityKeyRequestIpcChannel,
                 base::Unretained(this), new_connection_id)));
  ipc_server->CreateChannel(remoting::GetSecurityKeyIpcChannel(),
                            kSecurityKeyRequestTimeout);
  active_channels_[new_connection_id] = std::move(ipc_server);
}

void SecurityKeyAuthHandlerWin::CloseSecurityKeyRequestIpcChannel(
    int connection_id) {
  active_channels_.erase(connection_id);
}

SecurityKeyAuthHandlerWin::ActiveChannels::const_iterator
SecurityKeyAuthHandlerWin::GetChannelForConnectionId(int connection_id) const {
  return active_channels_.find(connection_id);
}

void SecurityKeyAuthHandlerWin::OnChannelConnected() {
  // Create another server to accept the next connection.
  StartIpcServerChannel();
}

}  // namespace remoting
