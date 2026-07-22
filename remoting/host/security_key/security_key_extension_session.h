// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_SESSION_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_SESSION_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "remoting/host/host_extension_session.h"
#include "remoting/host/mojom/remote_security_key.mojom.h"

namespace remoting {

class SecurityKeyAuthHandler;

namespace protocol {
class ClientStub;
}

// A HostExtensionSession implementation that enables Security Key support.
class SecurityKeyExtensionSession : public HostExtensionSession {
 public:
  SecurityKeyExtensionSession(
      base::WeakPtr<SecurityKeyAuthHandler> auth_handler,
      protocol::ClientStub* client_stub);

  SecurityKeyExtensionSession(const SecurityKeyExtensionSession&) = delete;
  SecurityKeyExtensionSession& operator=(const SecurityKeyExtensionSession&) =
      delete;

  ~SecurityKeyExtensionSession() override;

  // HostExtensionSession interface.
  bool OnExtensionMessage(protocol::ClientStub* client_stub,
                          const protocol::ExtensionMessage& message) override;

 private:
  // These methods process specific security key extension message types.
  void ProcessControlMessage(const base::DictValue& message_data) const;
  void ProcessDataMessage(const base::DictValue& message_data) const;
  void ProcessErrorMessage(const base::DictValue& message_data) const;

  void SendMessageToClient(int connection_id, const std::string& data);

  // Ensures SecurityKeyExtensionSession methods are called on the same thread.
  base::ThreadChecker thread_checker_;

  // Interface through which messages can be sent to the client.
  raw_ptr<protocol::ClientStub> client_stub_ = nullptr;

  // Handles platform specific security key operations.
  base::WeakPtr<SecurityKeyAuthHandler> auth_handler_;

  base::WeakPtrFactory<SecurityKeyExtensionSession> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_SESSION_H_
