// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/process/current_process.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/common/content_constants_internal.h"
#include "content/common/content_switches_internal.h"
#include "content/ppapi_plugin/ppapi_thread.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "ipc/ipc_sender.h"
#include "ppapi/proxy/plugin_globals.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_init_impl_win.h"
#include "sandbox/win/src/sandbox.h"
#include "third_party/blink/public/web/win/web_font_rendering.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/win/direct_write.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "base/files/file_util.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "sandbox/policy/linux/sandbox_linux.h"
#include "sandbox/policy/sandbox_type.h"
#endif

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#include "gin/v8_initializer.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
#include <stdlib.h>
#endif

#if BUILDFLAG(IS_MAC)
#include "base/system/sys_info.h"
#endif

#if BUILDFLAG(IS_WIN)
sandbox::TargetServices* g_target_services = NULL;
#else
void* g_target_services = nullptr;
#endif

namespace content {

// Main function for starting the PPAPI plugin process.
int PpapiPluginMain(MainFunctionParams parameters) {
  const base::CommandLine& command_line = *parameters.command_line;

#if BUILDFLAG(IS_MAC)
  // Declare that this process has CPU security mitigations enabled (see
  // PpapiPluginSandboxedProcessLauncherDelegate::EnableCpuSecurityMitigations).
  // This must be done before the first call to
  // base::SysInfo::NumberOfProcessors().
  base::SysInfo::SetCpuSecurityMitigationsEnabled();
#endif

#if BUILDFLAG(IS_WIN)
  // https://crbug.com/1139752 Premature unload of shell32 caused process to
  // crash during process shutdown. Fixed in Windows 11.
  if (base::win::GetVersion() < base::win::Version::WIN11) {
    HMODULE shell32_pin = ::LoadLibrary(L"shell32.dll");
    UNREFERENCED_PARAMETER(shell32_pin);
  }

  g_target_services = parameters.sandbox_info->target_services;
#endif

  // If |g_target_services| is not null this process is sandboxed. One side
  // effect is that we can't pop dialogs like WaitForDebugger() does.
  if (command_line.HasSwitch(switches::kPpapiStartupDialog)) {
    if (g_target_services)
      base::debug::WaitForDebugger(2*60, false);
    else
      WaitForDebugger("Ppapi");
  }

  // Set the default locale to be the current UI language. WebKit uses ICU's
  // default locale for some font settings (especially switching between
  // Japanese and Chinese fonts for the same characters).
  if (command_line.HasSwitch(switches::kLang)) {
    std::string locale = command_line.GetSwitchValueASCII(switches::kLang);
    base::i18n::SetICUDefaultLocale(locale);

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_ANDROID)
    // TODO(shess): Flash appears to have a POSIX locale dependency
    // outside of the existing PPAPI ICU support.  Certain games hang
    // while loading, and it seems related to datetime formatting.
    // http://crbug.com/155396
    // http://crbug.com/155671
    //
    // ICU can accept "en-US" or "en_US", but POSIX wants "en_US".
    std::replace(locale.begin(), locale.end(), '-', '_');
    locale.append(".UTF-8");
    setlocale(LC_ALL, locale.c_str());
    setenv("LANG", locale.c_str(), 0);
#endif
  }

#if BUILDFLAG(IS_CHROMEOS)
  // Specifies $HOME explicitly because some plugins rely on $HOME but
  // no other part of Chrome OS uses that.  See crbug.com/335290.
  base::FilePath homedir;
  base::PathService::Get(base::DIR_HOME, &homedir);
  setenv("HOME", homedir.value().c_str(), 1);
#endif

  base::SingleThreadTaskExecutor main_thread_task_executor;
  base::PlatformThread::SetName("CrPPAPIMain");
  base::CurrentProcess::GetInstance().SetProcessType(
      base::CurrentProcessType::PROCESS_PPAPI_PLUGIN);
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventPpapiProcessSortIndex);

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
  gin::V8Initializer::LoadV8Snapshot();
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  sandbox::policy::SandboxLinux::GetInstance()->InitializeSandbox(
      sandbox::policy::SandboxTypeFromCommandLine(command_line),
      sandbox::policy::SandboxLinux::PreSandboxHook(),
      sandbox::policy::SandboxLinux::Options());
#endif

  ChildProcess ppapi_process;
  base::RunLoop run_loop;
  ppapi_process.set_main_thread(
      new PpapiThread(run_loop.QuitClosure(), command_line));

  // Mojo IPC support is brought up by PpapiThread, so startup tracing is
  // enabled here if it needs to start after mojo init (normally so the mojo
  // broker can bypass the sandbox to allocate startup tracing's SMB).
  if (parameters.needs_startup_tracing_after_mojo_init) {
    tracing::EnableStartupTracingIfNeeded();
  }

#if BUILDFLAG(IS_WIN)
  if (!base::win::IsUser32AndGdi32Available())
    gfx::win::InitializeDirectWrite();
  InitializeDWriteFontProxy();

  int antialiasing_enabled = 1;
  base::StringToInt(
      command_line.GetSwitchValueASCII(switches::kPpapiAntialiasedTextEnabled),
      &antialiasing_enabled);
  blink::WebFontRendering::SetAntialiasedTextEnabled(
      antialiasing_enabled ? true : false);

  int subpixel_rendering = 0;
  base::StringToInt(command_line.GetSwitchValueASCII(
                        switches::kPpapiSubpixelRenderingSetting),
                    &subpixel_rendering);
  blink::WebFontRendering::SetLCDTextEnabled(
      subpixel_rendering != gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE);
#endif

  run_loop.Run();

#if BUILDFLAG(IS_WIN)
  UninitializeDWriteFontProxy();
#endif
  return 0;
}

}  // namespace content
