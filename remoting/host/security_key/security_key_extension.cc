// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/security_key/security_key_extension.h"

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "remoting/host/security_key/security_key_extension_session.h"

namespace remoting {

// static
const char SecurityKeyExtension::kCapability[] = "securityKey";

SecurityKeyExtension::SecurityKeyExtension(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : file_task_runner_(file_task_runner) {}

SecurityKeyExtension::~SecurityKeyExtension() = default;

std::string SecurityKeyExtension::capability() const {
  return kCapability;
}

std::unique_ptr<HostExtensionSession>
SecurityKeyExtension::CreateExtensionSession(
    ClientSessionDetails* details,
    protocol::ClientStub* client_stub) {
  // TODO(joedow): Update this mechanism to allow for multiple sessions.  The
  //               extension will only send messages through the initial
  //               |client_stub| and |details| with the current design.
  return base::WrapUnique(
      new SecurityKeyExtensionSession(details, client_stub, file_task_runner_));
}

}  // namespace remoting
