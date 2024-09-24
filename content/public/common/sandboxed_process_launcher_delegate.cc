// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/sandboxed_process_launcher_delegate.h"

#include "build/build_config.h"
#include "content/public/common/zygote/zygote_buildflags.h"

namespace content {

#if BUILDFLAG(IS_WIN)
std::string SandboxedProcessLauncherDelegate::GetSandboxTag() {
  // This implies that policies will not share backing data.
  return "";
}

bool SandboxedProcessLauncherDelegate::DisableDefaultPolicy() {
  return false;
}

bool SandboxedProcessLauncherDelegate::GetAppContainerId(
    std::string* appcontainer_id) {
  return false;
}

bool SandboxedProcessLauncherDelegate::InitializeConfig(
    sandbox::TargetConfig* config) {
  return true;
}

bool SandboxedProcessLauncherDelegate::PreSpawnTarget(
    sandbox::TargetPolicy* policy) {
  return true;
}

void SandboxedProcessLauncherDelegate::PostSpawnTarget(
    base::ProcessHandle process) {}

bool SandboxedProcessLauncherDelegate::ShouldUnsandboxedRunInJob() {
  return false;
}

bool SandboxedProcessLauncherDelegate::CetCompatible() {
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
bool SandboxedProcessLauncherDelegate::ShouldLaunchElevated() {
  return false;
}

bool SandboxedProcessLauncherDelegate::ShouldUseUntrustedMojoInvitation() {
  return false;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_ZYGOTE)
ZygoteCommunication* SandboxedProcessLauncherDelegate::GetZygote() {
  // Default to the sandboxed zygote. If a more lax sandbox is needed, then the
  // child class should override this method and use the unsandboxed zygote.
  return GetGenericZygote();
}
#endif  // BUILDFLAG(USE_ZYGOTE)

#if BUILDFLAG(IS_POSIX)
base::EnvironmentMap SandboxedProcessLauncherDelegate::GetEnvironment() {
  return base::EnvironmentMap();
}
#endif  // BUILDFLAG(IS_POSIX)

#if BUILDFLAG(IS_MAC)

bool SandboxedProcessLauncherDelegate::DisclaimResponsibility() {
  return false;
}

bool SandboxedProcessLauncherDelegate::EnableCpuSecurityMitigations() {
  return false;
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace content
