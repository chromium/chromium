// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CLIENT_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/chromoting_host_services_provider.h"
#include "remoting/host/mojom/remote_security_key.mojom.h"

namespace remoting {

// Responsible for handing the client end of the IPC channel between the
// the network process (server) and remote_security_key process (client).
// The public methods are virtual to allow for using fake objects for testing.
class SecurityKeyIpcClient {
 public:
  SecurityKeyIpcClient();

  SecurityKeyIpcClient(const SecurityKeyIpcClient&) = delete;
  SecurityKeyIpcClient& operator=(const SecurityKeyIpcClient&) = delete;

  virtual ~SecurityKeyIpcClient();

  // Used to send security key extension messages to the client.
  using ResponseCallback =
      base::RepeatingCallback<void(const std::string& response_data)>;

  // Used to indicate when the channel can be used for request forwarding.
  using ConnectedCallback = base::OnceCallback<void()>;

  // Returns true if there is an active remoting session which supports
  // security key request forwarding.
  virtual bool CheckForSecurityKeyIpcServerChannel();

  // Begins the process of connecting to the IPC channel which will be used for
  // exchanging security key messages.
  // |connected_callback| is called when a channel has been established.
  // |connection_error_callback| is stored and will be called back for any
  // unexpected errors that occur while establishing, or during, the session.
  virtual void EstablishIpcConnection(
      ConnectedCallback connected_callback,
      base::OnceClosure connection_error_callback);

  // Sends a security key request message to the network process to be forwarded
  // to the remote client.
  virtual bool SendSecurityKeyRequest(const std::string& request_payload,
                                      ResponseCallback response_callback);

  // Closes the IPC channel if connected.
  virtual void CloseIpcConnection();

 protected:
  friend class SecurityKeyIpcClientTest;

  explicit SecurityKeyIpcClient(
      std::unique_ptr<ChromotingHostServicesProvider> service_provider);

 private:
  void OnQueryVersionResult(uint32_t unused_version);
  void OnChannelError();

  // Handles security key response IPC messages.
  void OnSecurityKeyResponse(const std::string& request_data);

  // Establishes a connection to the specified IPC Server channel.
  void ConnectToIpcChannel();

  // Signaled when the IPC connection is ready for security key requests.
  ConnectedCallback connected_callback_;

  // Signaled when an error occurs in either the IPC channel or communication.
  base::OnceClosure connection_error_callback_;

  // Signaled when a security key response has been received.
  ResponseCallback response_callback_;

  std::unique_ptr<ChromotingHostServicesProvider> service_provider_;

  // Used for forwarding security key requests to the remote client.
  mojo::Remote<mojom::SecurityKeyForwarder> security_key_forwarder_;

  base::ThreadChecker thread_checker_;

  base::WeakPtrFactory<SecurityKeyIpcClient> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_IPC_CLIENT_H_
