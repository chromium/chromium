// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/mojo_ipc/mojo_caller_security_checker.h"

namespace remoting {

// An implementation that unconditionally returns true in the CheckCaller()
// method. This is to allow platforms that don't have MojoCallerSecurityChecker
// implemented to still be able to handle incoming IPCs.
// TODO(yuweih): Implement MojoCallerSecurityChecker for Windows.
bool IsTrustedMojoEndpoint(base::ProcessId caller_pid) {
  return true;
}

}  // namespace remoting
