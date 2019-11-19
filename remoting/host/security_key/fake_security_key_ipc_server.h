// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_SERVER_H_
#define REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_SERVER_H_

#include "remoting/host/security_key/security_key_ipc_server.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Channel;
class Message;
}  // IPC

namespace mojo {
class IsolatedConnection;
}

namespace remoting {

// Used to send/receive security key messages for testing.  It provides a
// WeakPtr reference to itself which allows tests to verify its lifetime is
// managed properly without interfering with it.
class FakeSecurityKeyIpcServer : public SecurityKeyIpcServer,
                                 public IPC::Listener {
 public:
  FakeSecurityKeyIpcServer(
      int connection_id,
      ClientSessionDetails* client_session_details,
      base::TimeDelta initial_connect_timeout,
      const SecurityKeyAuthHandler::SendMessageCallback& send_message_callback,
      const base::Closure& connect_callback,
      const base::Closure& channel_closed_callback);
  ~FakeSecurityKeyIpcServer() override;

  // SecurityKeyIpcServer interface.
  bool CreateChannel(const mojo::NamedPlatformChannel::ServerName& server_name,
                     base::TimeDelta request_timeout) override;
  bool SendResponse(const std::string& message_data) override;

  // Simulates receipt of a security key request message.
  void SendRequest(const std::string& message_data);

  // Simulates the IPC channel being closed.
  void CloseChannel();

  // Sends a ConnectionReady message to the client.
  void SendConnectionReadyMessage();

  // Sends an InvalidSession message to the client.
  void SendInvalidSessionMessage();

  // Returns a WeakPtr reference to this instance.
  base::WeakPtr<FakeSecurityKeyIpcServer> AsWeakPtr();

  // Returns the payload for the last message received.
  const std::string& last_message_received() const {
    return last_message_received_;
  }

  // Signaled when a security key response message is received.
  // NOTE: Ths callback will be used instead of the IPC channel for response
  // notifications if it is set.
  void set_send_response_callback(const base::Closure& send_response_callback) {
    send_response_callback_ = send_response_callback;
  }

 private:
  // IPC::Listener interface.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;

  // The id assigned to this IPC connection.
  int connection_id_;

  // The payload for the last message received.
  std::string last_message_received_;

  // Used to forward security key requests to the remote client.
  SecurityKeyAuthHandler::SendMessageCallback send_message_callback_;

  // Signaled when the IPC channel is connected.
  base::Closure connect_callback_;

  // Signaled when the IPC channel is closed.
  base::Closure channel_closed_callback_;

  // Signaled when a security key response message is received.
  base::Closure send_response_callback_;

  // Used for sending/receiving security key messages between processes.
  std::unique_ptr<mojo::IsolatedConnection> mojo_connection_;
  std::unique_ptr<IPC::Channel> ipc_channel_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FakeSecurityKeyIpcServer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeSecurityKeyIpcServer);
};

// Used to create FakeSecurityKeyIpcServer instances for testing.
// Provides a method which will return a WeakPtr reference to each instance
// this factory creates.  This allows tests to inject/retrieve messages and
// verify the backing instance is destroyed at the appropriate time.
class FakeSecurityKeyIpcServerFactory : public SecurityKeyIpcServerFactory {
 public:
  FakeSecurityKeyIpcServerFactory();
  ~FakeSecurityKeyIpcServerFactory() override;

  // SecurityKeyIpcServerFactory implementation.
  std::unique_ptr<SecurityKeyIpcServer> Create(
      int connection_id,
      ClientSessionDetails* client_session_details,
      base::TimeDelta initial_connect_timeout,
      const SecurityKeyAuthHandler::SendMessageCallback& message_callback,
      const base::Closure& connect_callback,
      const base::Closure& done_callback) override;

  // Provide a WeakPtr reference to the FakeSecurityKeyIpcServer object
  // created for the |connection_id| IPC channel.
  base::WeakPtr<FakeSecurityKeyIpcServer> GetIpcServerObject(int connection_id);

 private:
  // Tracks each FakeSecurityKeyIpcServer instance created by this
  // factory which allows them to be retrieved and queried for tests.
  std::map<int, base::WeakPtr<FakeSecurityKeyIpcServer>> ipc_server_map_;

  DISALLOW_COPY_AND_ASSIGN(FakeSecurityKeyIpcServerFactory);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_SERVER_H_
