// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "remoting/host/host_extension.h"

namespace remoting {

class SecurityKeyAuthHandler;
class HostExtensionSession;

// SecurityKeyExtension extends HostExtension to enable Security Key support.
class SecurityKeyExtension : public HostExtension {
 public:
  static const char kCapability[];

  explicit SecurityKeyExtension(
      base::WeakPtr<SecurityKeyAuthHandler> auth_handler);

  SecurityKeyExtension(const SecurityKeyExtension&) = delete;
  SecurityKeyExtension& operator=(const SecurityKeyExtension&) = delete;

  ~SecurityKeyExtension() override;

  // HostExtension interface.
  std::string capability() const override;
  std::unique_ptr<HostExtensionSession> CreateExtensionSession(
      protocol::ClientStub* client_stub) override;

 private:
  base::WeakPtr<SecurityKeyAuthHandler> auth_handler_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_H_
