// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ppapi_plugin_sandboxed_process_launcher_delegate.h"

#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "sandbox/policy/mojom/sandbox.mojom.h"

namespace content {

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
#if BUILDFLAG(IS_WIN)
  return sandbox::mojom::Sandbox::kNoSandbox;
#else
  return sandbox::mojom::Sandbox::kPpapi;
#endif
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
