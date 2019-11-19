// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CLIENT_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CLIENT_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/isolated_connection.h"

namespace IPC {
class Channel;
class Message;
}  // IPC

namespace remoting {

// Responsible for handing the client end of the IPC channel between the
// the network process (server) and remote_security_key process (client).
// The public methods are virtual to allow for using fake objects for testing.
class SecurityKeyIpcClient : public IPC::Listener {
 public:
  SecurityKeyIpcClient();
  ~SecurityKeyIpcClient() override;

  // Used to send security key extension messages to the client.
  typedef base::Callback<void(const std::string& response_data)>
      ResponseCallback;

  // Used to indicate whether the channel can be used for request forwarding.
  typedef base::Callback<void(bool connection_usable)> ConnectedCallback;

  // Returns true if there is an active remoting session which supports
  // security key request forwarding.
  virtual bool CheckForSecurityKeyIpcServerChannel();

  // Begins the process of connecting to the IPC channel which will be used for
  // exchanging security key messages.
  // |connected_callback| is called when a channel has been established and
  // indicates whether security key requests can be sent using it.
  // |connection_error_callback| is stored and will be called back for any
  // unexpected errors that occur while establishing, or during, the session.
  virtual void EstablishIpcConnection(
      const ConnectedCallback& connected_callback,
      const base::Closure& connection_error_callback);

  // Sends a security key request message to the network process to be forwarded
  // to the remote client.
  virtual bool SendSecurityKeyRequest(
      const std::string& request_payload,
      const ResponseCallback& response_callback);

  // Closes the IPC channel if connected.
  virtual void CloseIpcConnection();

  // Allows tests to override the IPC channel.
  void SetIpcChannelHandleForTest(
      const mojo::NamedPlatformChannel::ServerName& server_name);

  // Allows tests to override the expected session ID.
  void SetExpectedIpcServerSessionIdForTest(uint32_t expected_session_id);

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

  // Handles the ConnectionReady IPC message.
  void OnConnectionReady();

  // Handles the InvalidSession IPC message.
  void OnInvalidSession();

  // Handles security key response IPC messages.
  void OnSecurityKeyResponse(const std::string& request_data);

  // Establishes a connection to the specified IPC Server channel.
  void ConnectToIpcChannel();

  // Used to validate the IPC Server process is running in the correct session.
  // '0' (default) corresponds to the session the network process runs in.
  uint32_t expected_ipc_server_session_id_ = 0;

  // Name of the initial IPC channel used to retrieve connection info.
  mojo::NamedPlatformChannel::ServerName named_channel_handle_;

  // A handle for the IPC channel used for exchanging security key messages.
  mojo::PlatformChannelEndpoint channel_handle_;

  // Signaled when the IPC connection is ready for security key requests.
  ConnectedCallback connected_callback_;

  // Signaled when an error occurs in either the IPC channel or communication.
  base::Closure connection_error_callback_;

  // Signaled when a security key response has been received.
  ResponseCallback response_callback_;

  // Used for sending/receiving security key messages between processes.
  mojo::IsolatedConnection mojo_connection_;
  std::unique_ptr<IPC::Channel> ipc_channel_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SecurityKeyIpcClient> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SecurityKeyIpcClient);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CLIENT_H_
