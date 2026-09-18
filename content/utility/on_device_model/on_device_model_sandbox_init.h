// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SANDBOX_INIT_H_
#define CONTENT_UTILITY_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SANDBOX_INIT_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "sandbox/policy/linux/sandbox_linux.h"
#endif

namespace on_device_model {

// Must be called in the service's process before sandbox initialization.
// These are defined separately in pre_sandbox_init.cc for explicit security
// review coverage.
[[nodiscard]] bool PreSandboxInit();

// Must be called in the service's process after the run loop finished.
[[nodiscard]] bool Shutdown();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void AddSandboxLinuxOptions(sandbox::policy::SandboxLinux::Options& options);

[[nodiscard]] bool PreSandboxHook(
    sandbox::policy::SandboxLinux::Options options);
#endif

}  // namespace on_device_model

#endif  // CONTENT_UTILITY_ON_DEVICE_MODEL_ON_DEVICE_MODEL_SANDBOX_INIT_H_
