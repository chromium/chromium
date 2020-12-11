// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_SESSION_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_SESSION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "remoting/host/host_extension_session.h"

namespace base {
class DictionaryValue;
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ClientSessionDetails;
class SecurityKeyAuthHandler;

namespace protocol {
class ClientStub;
}

// A HostExtensionSession implementation that enables Security Key support.
class SecurityKeyExtensionSession : public HostExtensionSession {
 public:
  SecurityKeyExtensionSession(
      ClientSessionDetails* client_session_details,
      protocol::ClientStub* client_stub,
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);
  ~SecurityKeyExtensionSession() override;

  // HostExtensionSession interface.
  bool OnExtensionMessage(ClientSessionDetails* client_session_details,
                          protocol::ClientStub* client_stub,
                          const protocol::ExtensionMessage& message) override;

  // Allows overriding SecurityKeyAuthHandler for unit testing.
  void SetSecurityKeyAuthHandlerForTesting(
      std::unique_ptr<SecurityKeyAuthHandler> security_key_auth_handler);

 private:
  // These methods process specific security key extension message types.
  void ProcessControlMessage(base::DictionaryValue* message_data) const;
  void ProcessDataMessage(base::DictionaryValue* message_data) const;
  void ProcessErrorMessage(base::DictionaryValue* message_data) const;

  void SendMessageToClient(int connection_id, const std::string& data) const;

  // Ensures SecurityKeyExtensionSession methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Interface through which messages can be sent to the client.
  protocol::ClientStub* client_stub_ = nullptr;

  // Handles platform specific security key operations.
  std::unique_ptr<SecurityKeyAuthHandler> security_key_auth_handler_;

  DISALLOW_COPY_AND_ASSIGN(SecurityKeyExtensionSession);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_SESSION_H_
