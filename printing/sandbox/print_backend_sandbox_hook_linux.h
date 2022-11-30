// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_SANDBOX_PRINT_BACKEND_SANDBOX_HOOK_LINUX_H_
#define PRINTING_SANDBOX_PRINT_BACKEND_SANDBOX_HOOK_LINUX_H_

#include "base/component_export.h"
#include "sandbox/policy/linux/sandbox_linux.h"

namespace printing {

// Setup allowed commands and filesystem permissions for print backend service
// sandboxed process.
COMPONENT_EXPORT(PRINTING)
bool PrintBackendPreSandboxHook(sandbox::policy::SandboxLinux::Options options);

}  // namespace printing

#endif  // PRINTING_SANDBOX_PRINT_BACKEND_SANDBOX_HOOK_LINUX_H_
