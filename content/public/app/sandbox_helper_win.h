// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_APP_SANDBOX_HELPER_WIN_H_
#define CONTENT_PUBLIC_APP_SANDBOX_HELPER_WIN_H_

#include "sandbox/win/src/security_level.h"

namespace sandbox {
struct SandboxInterfaceInfo;
}

namespace content {

// Initialize the sandbox code Note: This function
// must be *statically* linked into the executable (along with the static
// sandbox library); it will not work correctly if it is exported from a
// DLL and linked in.
// starting_mitigations are the mitigations which were applied early in
// startup, but not at startup. If a wrong value is sent in, additional
// mitigations may fail to be applied.
// In particular, child processes must have a value of 0 here since they
// use mitigations applied in process creation. If you have additional
// mitigations you need at startup, please add them to
// GenerateConfigForSandboxedProcess in sandbox_win.cc
void InitializeSandboxInfo(sandbox::SandboxInterfaceInfo* sandbox_info,
                           sandbox::MitigationFlags starting_mitigations = 0);

}  // namespace content

#endif  // CONTENT_PUBLIC_APP_SANDBOX_HELPER_WIN_H_
