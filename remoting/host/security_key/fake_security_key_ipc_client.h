// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_CLIENT_H_
#define REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_CLIENT_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "remoting/host/mojom/remote_security_key.mojom.h"
#include "remoting/host/security_key/security_key_ipc_client.h"

namespace remoting {

// Simulates the SecurityKeyIpcClient and provides access to data members
// for testing.  This class is used for scenarios which require an IPC
// connection as well as for tests which only need callbacks activated.
class FakeSecurityKeyIpcClient : public SecurityKeyIpcClient {
 public:
  explicit FakeSecurityKeyIpcClient(
      const base::RepeatingClosure& connection_event_callback);

  FakeSecurityKeyIpcClient(const FakeSecurityKeyIpcClient&) = delete;
  FakeSecurityKeyIpcClient& operator=(const FakeSecurityKeyIpcClient&) = delete;

  ~FakeSecurityKeyIpcClient() override;

  // SecurityKeyIpcClient interface.
  bool CheckForSecurityKeyIpcServerChannel() override;
  void EstablishIpcConnection(
      ConnectedCallback connected_callback,
      base::OnceClosure connection_error_callback) override;
  bool SendSecurityKeyRequest(const std::string& request_payload,
                              ResponseCallback response_callback) override;
  void CloseIpcConnection() override;

  // Returns a pending receiver that can be bound to receive requests from the
  // fake client.
  mojo::PendingReceiver<mojom::SecurityKeyForwarder>
  BindNewPipeAndPassReceiver();

  // Override of SendSecurityKeyRequest() interface method for tests which use
  // an IPC connection for testing.
  void SendSecurityKeyRequestViaIpc(const std::string& request_payload);

  base::WeakPtr<FakeSecurityKeyIpcClient> AsWeakPtr();

  const std::string& last_message_received() const {
    return last_message_received_;
  }

  bool ipc_connected() { return ipc_connected_; }

  bool connection_ready() { return connection_ready_; }

  void set_check_for_ipc_channel_return_value(bool return_value) {
    check_for_ipc_channel_return_value_ = return_value;
  }

  void set_establish_ipc_connection_should_succeed(bool should_succeed) {
    establish_ipc_connection_should_succeed_ = should_succeed;
  }

  void set_send_security_request_should_succeed(bool should_succeed) {
    send_security_request_should_succeed_ = should_succeed;
  }

  void set_security_key_response_payload(const std::string& response_payload) {
    security_key_response_payload_ = response_payload;
  }

 private:
  void OnQueryVersionResult(uint32_t unused_version);

  // Handles security key response IPC messages.
  void OnSecurityKeyResponse(const std::string& request_data);

  // Called when a change in the IPC connection state has occurred.
  base::RepeatingClosure connection_event_callback_;

  mojo::Remote<mojom::SecurityKeyForwarder> security_key_forwarder_;

  // Provides the contents of the last IPC message received.
  std::string last_message_received_;

  // Determines whether EstablishIpcConnection() returns success or failure.
  bool establish_ipc_connection_should_succeed_ = true;

  // Determines whether SendSecurityKeyRequest() returns success or failure.
  bool send_security_request_should_succeed_ = true;

  // Value returned by CheckForSecurityKeyIpcServerChannel() method.
  bool check_for_ipc_channel_return_value_ = true;

  // Stores whether a connection to the server IPC connection is active.
  bool ipc_connected_ = false;

  // Tracks whether a ConnectionReady message has been received.
  bool connection_ready_ = false;

  // Value returned by SendSecurityKeyRequest() method.
  std::string security_key_response_payload_;

  base::WeakPtrFactory<FakeSecurityKeyIpcClient> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_FAKE_SECURITY_KEY_IPC_CLIENT_H_
