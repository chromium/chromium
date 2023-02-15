// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ppapi_plugin_sandboxed_process_launcher_delegate.h"

#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

#if BUILDFLAG(IS_WIN)
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox_policy.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/font_render_params.h"
#endif

namespace content {
#if BUILDFLAG(IS_WIN)
std::string PpapiPluginSandboxedProcessLauncherDelegate::GetSandboxTag() {
  return sandbox::policy::SandboxWin::GetSandboxTagForDelegate(
      "ppapi", GetSandboxType());
}

bool PpapiPluginSandboxedProcessLauncherDelegate::PreSpawnTarget(
    sandbox::TargetPolicy* policy) {
  sandbox::TargetConfig* config = policy->GetConfig();
  if (config->IsConfigured())
    return true;

  // The Pepper process is as locked-down as a renderer except that it can
  // create the server side of Chrome pipes.
  sandbox::ResultCode result;
#if !defined(NACL_WIN64)
  result = sandbox::policy::SandboxWin::AddWin32kLockdownPolicy(config);
  if (result != sandbox::SBOX_ALL_OK) {
    return false;
  }
#endif  // !defined(NACL_WIN64)

  // No plugins can generate executable code.
  sandbox::MitigationFlags flags = config->GetDelayedProcessMitigations();
  flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
  if (sandbox::SBOX_ALL_OK != config->SetDelayedProcessMitigations(flags))
    return false;

  return true;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(USE_ZYGOTE)
ZygoteCommunication* PpapiPluginSandboxedProcessLauncherDelegate::GetZygote() {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType plugin_launcher =
      browser_command_line.GetSwitchValueNative(switches::kPpapiPluginLauncher);
  if (!plugin_launcher.empty())
    return nullptr;
  return GetGenericZygote();
}
#endif  // BUILDFLAG(USE_ZYGOTE)

sandbox::mojom::Sandbox
PpapiPluginSandboxedProcessLauncherDelegate::GetSandboxType() {
  return sandbox::mojom::Sandbox::kPpapi;
}

#if BUILDFLAG(IS_MAC)
bool PpapiPluginSandboxedProcessLauncherDelegate::DisclaimResponsibility() {
  return true;
}
bool PpapiPluginSandboxedProcessLauncherDelegate::
    EnableCpuSecurityMitigations() {
  return true;
}
#endif

}  // namespace content
