// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "remoting/host/host_extension.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class ClientSessionDetails;
class HostExtensionSession;

// SecurityKeyExtension extends HostExtension to enable Security Key support.
class SecurityKeyExtension : public HostExtension {
 public:
  static const char kCapability[];

  explicit SecurityKeyExtension(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner);

  SecurityKeyExtension(const SecurityKeyExtension&) = delete;
  SecurityKeyExtension& operator=(const SecurityKeyExtension&) = delete;

  ~SecurityKeyExtension() override;

  // HostExtension interface.
  std::string capability() const override;
  std::unique_ptr<HostExtensionSession> CreateExtensionSession(
      ClientSessionDetails* client_session_details,
      protocol::ClientStub* client_stub) override;

 private:
  // Allows underlying auth handler to perform blocking file IO.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_EXTENSION_H_
