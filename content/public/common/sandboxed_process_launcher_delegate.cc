// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandboxed_process_launcher_delegate.h"

#include "build/build_config.h"
#include "content/public/common/zygote/zygote_buildflags.h"

namespace content {

#if defined(OS_WIN)
bool SandboxedProcessLauncherDelegate::DisableDefaultPolicy() {
  return false;
}

bool SandboxedProcessLauncherDelegate::GetAppContainerId(
    std::string* appcontainer_id) {
  return false;
}

bool SandboxedProcessLauncherDelegate::PreSpawnTarget(
    sandbox::TargetPolicy* policy) {
  return true;
}

void SandboxedProcessLauncherDelegate::PostSpawnTarget(
    base::ProcessHandle process) {}

bool SandboxedProcessLauncherDelegate::ShouldLaunchElevated() {
  return false;
}
#endif  // defined(OS_WIN)

#if BUILDFLAG(USE_ZYGOTE_HANDLE)
ZygoteHandle SandboxedProcessLauncherDelegate::GetZygote() {
  // Default to the sandboxed zygote. If a more lax sandbox is needed, then the
  // child class should override this method and use the unsandboxed zygote.
  return GetGenericZygote();
}
#endif  // BUILDFLAG(USE_ZYGOTE_HANDLE)

#if defined(OS_POSIX)
base::EnvironmentMap SandboxedProcessLauncherDelegate::GetEnvironment() {
  return base::EnvironmentMap();
}
#endif  // defined(OS_POSIX)

#if defined(OS_MAC)

bool SandboxedProcessLauncherDelegate::DisclaimResponsibility() {
  return false;
}

#if defined(ARCH_CPU_ARM64)
bool SandboxedProcessLauncherDelegate::LaunchX86_64() {
  return false;
}
#endif  // ARCH_CPU_ARM64

#endif  // OS_MAC

}  // namespace content
