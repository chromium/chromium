// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "remoting/host/mojom/remote_security_key.mojom.h"

namespace remoting {

class ClientSessionDetails;

// Class responsible for proxying authentication data between a local gnubbyd
// and the client.
class SecurityKeyAuthHandler {
 public:
  virtual ~SecurityKeyAuthHandler() = default;

  // Used to send security key extension messages to the client.
  using SendMessageCallback =
      base::RepeatingCallback<void(int connection_id, const std::string& data)>;

  static void set_use_mojo_handler(bool use_mojo_handler);

  using CreateHandlerCallbackForTesting =
      base::RepeatingCallback<std::unique_ptr<SecurityKeyAuthHandler>(
          ClientSessionDetails* client_session_details)>;
  static void SetCreateHandlerCallbackForTesting(
      CreateHandlerCallbackForTesting callback);

  // Creates a platform-specific SecurityKeyAuthHandler.
  // |client_session_details| will be valid until this instance is destroyed.
  static std::unique_ptr<SecurityKeyAuthHandler> Create(
      ClientSessionDetails* client_session_details);

  // Binds a SecurityKeyForwarder receiver for receiving SK forwarding requests.
  virtual void BindSecurityKeyForwarder(
      mojo::PendingReceiver<mojom::SecurityKeyForwarder> receiver);

  // Sets the callback used to send messages to the client, associated with
  // the given |client_id| (typically the transport's 'this' pointer).
  virtual void SetSendMessageCallback(const SendMessageCallback& callback,
                                      const void* client_id) = 0;

  // Clears the callback if the registered |client_id| matches the caller.
  virtual void ClearSendMessageCallback(const void* client_id) = 0;

  // Returns a weak pointer to this handler.
  virtual base::WeakPtr<SecurityKeyAuthHandler> GetWeakPtr() = 0;

  // Creates the platform specific connection to handle security key requests.
  virtual void CreateSecurityKeyConnection() = 0;

  // Returns true if |security_key_connection_id| represents a valid connection.
  virtual bool IsValidConnectionId(int security_key_connection_id) const = 0;

  // Sends security key response from client to local security key agent.
  virtual void SendClientResponse(int security_key_connection_id,
                                  const std::string& response) = 0;

  // Closes key connection represented by |security_key_connection_id|.
  virtual void SendErrorAndCloseConnection(int security_key_connection_id) = 0;

  // Returns the number of active security key connections.
  virtual size_t GetActiveConnectionCountForTest() const = 0;

  // Sets the timeout used when waiting for a security key response.
  virtual void SetRequestTimeoutForTest(base::TimeDelta timeout) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_AUTH_HANDLER_H_
