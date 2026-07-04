// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_MOJO_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_MOJO_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "remoting/host/mojom/remote_security_key.mojom.h"
#include "remoting/host/security_key/security_key_auth_handler.h"

namespace remoting {

class ClientSessionDetails;

// Implements the mojom::SecurityKeyForwarder interface and handles incoming SK
// requests from the IPC client. The caller is responsible for running the IPC
// server and passing in new connections through BindSecurityKeyForwarder().
class SecurityKeyAuthHandlerMojo : public SecurityKeyAuthHandler,
                                   public mojom::SecurityKeyForwarder {
 public:
  explicit SecurityKeyAuthHandlerMojo(
      ClientSessionDetails* client_session_details);

  SecurityKeyAuthHandlerMojo(const SecurityKeyAuthHandlerMojo&) = delete;
  SecurityKeyAuthHandlerMojo& operator=(const SecurityKeyAuthHandlerMojo&) =
      delete;

  ~SecurityKeyAuthHandlerMojo() override;

  // SecurityKeyAuthHandler interface.
  void BindSecurityKeyForwarder(
      mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver) override;
  void CreateSecurityKeyConnection() override;
  bool IsValidConnectionId(int security_key_connection_id) const override;
  void SendClientResponse(int security_key_connection_id,
                          const std::string& response) override;
  void SendErrorAndCloseConnection(int security_key_connection_id) override;
  void SetSendMessageCallback(const SendMessageCallback& callback,
                              const void* client_id) override;
  void ClearSendMessageCallback(const void* client_id) override;
  base::WeakPtr<SecurityKeyAuthHandler> GetWeakPtr() override;
  size_t GetActiveConnectionCountForTest() const override;
  void SetRequestTimeoutForTest(base::TimeDelta timeout) override;

  // mojom::SecurityKeyForwarder interface.
  void OnSecurityKeyRequest(const std::string& request_data,
                            OnSecurityKeyRequestCallback callback) override;

 private:
  struct ActiveConnection {
    ActiveConnection();
    ~ActiveConnection();

    mojo::ReceiverId receiver_id;
    base::OneShotTimer disconnect_timer;
    mojom::SecurityKeyForwarder::OnSecurityKeyRequestCallback
        on_security_key_request_callback;
  };

  using ActiveConnections = std::map</* connection_id */ int, ActiveConnection>;

  void OnIpcPeerDisconnected();

  // Closes the connection created for a security key forwarding session.
  void CloseSecurityKeyRequestConnection(int connection_id);

  base::OnceClosure GetCloseConnectionClosure(int connection_id);

  // Represents the last id assigned to a new security key request connection.
  int last_connection_id_ = 0;

  // The callback used to send messages to the client.
  SendMessageCallback send_message_callback_;

  // The id of the client that registered the callback.
  RAW_PTR_EXCLUSION const void* active_client_id_ = nullptr;

  // Interface which provides details about the client session.
  raw_ptr<ClientSessionDetails> client_session_details_ = nullptr;

  // Tracks the connection created for each security key forwarding session.
  ActiveConnections active_connections_;

  mojo::ReceiverSet<mojom::SecurityKeyForwarder, /* connection_id */ int>
      receiver_set_;

  // Ensures SecurityKeyAuthHandlerMojo methods are called on the same thread.
  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<SecurityKeyAuthHandlerMojo> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_MOJO_H_
