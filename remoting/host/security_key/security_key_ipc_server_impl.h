// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_SERVER_IMPL_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_SERVER_IMPL_H_

#include "remoting/host/security_key/security_key_ipc_server.h"

#include <cstdint>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ipc/ipc_listener.h"

namespace base {
class TimeDelta;
}  // base

namespace IPC {
class Channel;
class Message;
}  // IPC

namespace mojo {
class IsolatedConnection;
}

namespace remoting {

// Responsible for handing the server end of the IPC channel between the
// the network process and the local remote_security_key process.
class SecurityKeyIpcServerImpl : public SecurityKeyIpcServer,
                                 public IPC::Listener {
 public:
  SecurityKeyIpcServerImpl(
      int connection_id,
      ClientSessionDetails* client_session_details,
      base::TimeDelta initial_connect_timeout,
      const SecurityKeyAuthHandler::SendMessageCallback& message_callback,
      const base::Closure& connect_callback,
      const base::Closure& done_callback);
  ~SecurityKeyIpcServerImpl() override;

  // SecurityKeyIpcServer implementation.
  bool CreateChannel(const mojo::NamedPlatformChannel::ServerName& server_name,
                     base::TimeDelta request_timeout) override;
  bool SendResponse(const std::string& message_data) override;

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // Handles security key resquest IPC messages.
  void OnSecurityKeyRequest(const std::string& request);

  void CloseChannel();

  // The value assigned to identify the current IPC channel.
  int connection_id_;

  // Interface which provides details about the client session.
  ClientSessionDetails* client_session_details_ = nullptr;

  // Tracks whether the connection is in the process of being closed.
  bool connection_close_pending_ = false;

  // Timeout for disconnecting the IPC channel if there is no client activity.
  base::TimeDelta initial_connect_timeout_;

  // Timeout for disconnecting the IPC channel if there is no response from
  // the remote client after a security key request.
  base::TimeDelta security_key_request_timeout_;

  // Used to detect timeouts and disconnect the IPC channel.
  base::OneShotTimer timer_;

  // Used to signal that the IPC channel has been connected.
  base::Closure connect_callback_;

  // Used to signal that the IPC channel should be disconnected.
  base::Closure done_callback_;

  // Used to pass a security key request on to the remote client.
  SecurityKeyAuthHandler::SendMessageCallback message_callback_;

  // Used for sending/receiving security key messages between processes.
  std::unique_ptr<mojo::IsolatedConnection> mojo_connection_;
  std::unique_ptr<IPC::Channel> ipc_channel_;

  // Ensures SecurityKeyIpcServerImpl methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SecurityKeyIpcServerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SecurityKeyIpcServerImpl);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_SERVER_IMPL_H_
