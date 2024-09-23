// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/renderer_sandboxed_process_launcher_delegate.h"

#include <string_view>

#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/win/nt_status.h"
#include "sandbox/policy/features.h"
#include "sandbox/policy/win/sandbox_win.h"
#include "sandbox/win/src/sandbox_policy_base.h"
#include "sandbox/win/src/security_level.h"
#include "third_party/blink/public/common/switches.h"
#endif

#if BUILDFLAG(USE_ZYGOTE)
#include "content/public/common/content_switches.h"
#include "content/public/common/zygote/zygote_handle.h"  // nogncheck
#endif

namespace content {

#if BUILDFLAG(USE_ZYGOTE)
ZygoteCommunication* RendererSandboxedProcessLauncherDelegate::GetZygote() {
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringType renderer_prefix =
      browser_command_line.GetSwitchValueNative(switches::kRendererCmdPrefix);
  if (!renderer_prefix.empty())
    return nullptr;
  return GetGenericZygote();
}
#endif  // BUILDFLAG(USE_ZYGOTE)

#if BUILDFLAG(IS_MAC)
bool RendererSandboxedProcessLauncherDelegate::EnableCpuSecurityMitigations() {
  return true;
}
#endif  // BUILDFLAG(IS_MAC)

sandbox::mojom::Sandbox
RendererSandboxedProcessLauncherDelegate::GetSandboxType() {
  return sandbox::mojom::Sandbox::kRenderer;
}

#if BUILDFLAG(IS_WIN)
RendererSandboxedProcessLauncherDelegateWin::
    RendererSandboxedProcessLauncherDelegateWin(
        const base::CommandLine& cmd_line,
        bool is_pdf_renderer,
        bool is_jit_disabled)
    : renderer_code_integrity_enabled_(
          GetContentClient()->browser()->IsRendererCodeIntegrityEnabled()),
      renderer_app_container_disabled_(
          GetContentClient()->browser()->IsAppContainerDisabled(
              sandbox::mojom::Sandbox::kRenderer)),
      is_pdf_renderer_(is_pdf_renderer) {
  // PDF renderers must be jitless.
  CHECK(!is_pdf_renderer || is_jit_disabled);
  if (is_jit_disabled) {
    dynamic_code_can_be_disabled_ = true;
    return;
  }
  if (cmd_line.HasSwitch(blink::switches::kJavaScriptFlags)) {
    std::string js_flags =
        cmd_line.GetSwitchValueASCII(blink::switches::kJavaScriptFlags);
    std::vector<std::string_view> js_flag_list = base::SplitStringPiece(
        js_flags, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& js_flag : js_flag_list) {
      if (js_flag == "--jitless") {
        // If v8 is running jitless then there is no need for the ability to
        // mark writable pages as executable to be available to the process.
        dynamic_code_can_be_disabled_ = true;
        break;
      }
    }
  }
}

std::string RendererSandboxedProcessLauncherDelegateWin::GetSandboxTag() {
  if (is_pdf_renderer_) {
    // All pdf renderers are jitless so only need one tag for these.
    return sandbox::policy::SandboxWin::GetSandboxTagForDelegate(
        "renderer-pdfium", GetSandboxType());
  } else {
    // Some renderers can be jitless so need different tags.
    return sandbox::policy::SandboxWin::GetSandboxTagForDelegate(
        dynamic_code_can_be_disabled_ ? "renderer-jitless" : "renderer",
        GetSandboxType());
  }
}

bool RendererSandboxedProcessLauncherDelegateWin::InitializeConfig(
    sandbox::TargetConfig* config) {
  DCHECK(!config->IsConfigured());

  sandbox::policy::SandboxWin::AddBaseHandleClosePolicy(config);

  ContentBrowserClient::AppContainerFlags ac_flags(
      ContentBrowserClient::AppContainerFlags::kAppContainerFlagNone);
  if (renderer_app_container_disabled_) {
    ac_flags = ContentBrowserClient::AppContainerFlags::
        kAppContainerFlagDisableAppContainer;
  }
  const std::wstring& sid =
      GetContentClient()->browser()->GetAppContainerSidForSandboxType(
          GetSandboxType(), ac_flags);
  if (!sid.empty()) {
    sandbox::policy::SandboxWin::AddAppContainerPolicy(config, sid.c_str());
  }

  // If the renderer process is protected by code integrity, more
  // mitigations become available.
  if (renderer_code_integrity_enabled_ && dynamic_code_can_be_disabled_) {
    sandbox::MitigationFlags mitigation_flags =
        config->GetDelayedProcessMitigations();
    mitigation_flags |= sandbox::MITIGATION_DYNAMIC_CODE_DISABLE;
    if (sandbox::SBOX_ALL_OK !=
        config->SetDelayedProcessMitigations(mitigation_flags)) {
      return false;
    }
  }

  config->SetFilterEnvironment(/*filter=*/true);

  ContentBrowserClient::ChildSpawnFlags flags(
      ContentBrowserClient::ChildSpawnFlags::kChildSpawnFlagNone);
  if (renderer_code_integrity_enabled_) {
    flags = ContentBrowserClient::ChildSpawnFlags::
        kChildSpawnFlagRendererCodeIntegrity;
  }
  return GetContentClient()->browser()->PreSpawnChild(
      config, sandbox::mojom::Sandbox::kRenderer, flags);
}

void RendererSandboxedProcessLauncherDelegateWin::PostSpawnTarget(
    base::ProcessHandle process) {
  FILETIME creation_time, exit_time, kernel_time, user_time;
  // Should never fail. If it does, then something really bad has happened, such
  // as something external unsuspending the renderer process.
  if (!::GetProcessTimes(process, &creation_time, &exit_time, &kernel_time,
                         &user_time)) {
    return;
  }

  // These should always be zero but if they are not, then something on the
  // client has triggered execution in the child process putting it into a
  // undefined state. Try and detect this here to diagnose this happening in the
  // wild.
  base::UmaHistogramBoolean(
      "BrowserRenderProcessHost.SuspendedChild.UserExecutionRecorded",
      base::TimeDelta::FromFileTime(user_time).InMicroseconds() > 0);
  base::UmaHistogramBoolean(
      "BrowserRenderProcessHost.SuspendedChild.KernelExecutionRecorded",
      base::TimeDelta::FromFileTime(kernel_time).InMicroseconds() > 0);
}

bool RendererSandboxedProcessLauncherDelegateWin::CetCompatible() {
  // Disable CET for renderer because v8 deoptimization swaps stacks in a
  // non-compliant way. CET can be enabled where the renderer is known to
  // be jitless.
  return dynamic_code_can_be_disabled_;
}

bool RendererSandboxedProcessLauncherDelegateWin::
    ShouldUseUntrustedMojoInvitation() {
  return true;
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
