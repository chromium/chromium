// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "headless/public/headless_shell.h"

#if BUILDFLAG(IS_WIN)
#include "base/command_line.h"
#include "components/crash/core/app/crash_switches.h"
#include "content/public/app/sandbox_helper_win.h"
#include "content/public/common/content_switches.h"
#include "sandbox/win/src/sandbox_types.h"  // nogncheck
#elif BUILDFLAG(IS_MAC)
#include "base/check.h"
#include "sandbox/mac/seatbelt_exec.h"
#endif

int main(int argc, const char** argv) {
  content::ContentMainParams params(nullptr);
#if BUILDFLAG(IS_WIN)
  base::CommandLine::Init(0, nullptr);
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  // Crashpad handlers do not use content sandbox services.
  if (command_line.GetSwitchValueASCII(switches::kProcessType) !=
      crash_reporter::switches::kCrashpadHandler) {
    content::InitializeSandboxInfo(&sandbox_info);
  }
  // Sandbox info has to be set and initialized.
  params.sandbox_info = &sandbox_info;
#elif !BUILDFLAG(IS_ANDROID)
  params.argc = argc;
  params.argv = argv;
#if BUILDFLAG(IS_MAC)
  sandbox::SeatbeltExecServer::CreateFromArgumentsResult seatbelt =
      sandbox::SeatbeltExecServer::CreateFromArguments(
          argv[0], argc, const_cast<char**>(argv));
  if (seatbelt.sandbox_required) {
    CHECK(seatbelt.server->InitializeSandbox());
  }
#endif  // BUILDFLAG(IS_MAC)
#endif  // BUILDFLAG(IS_WIN)

  return headless::HeadlessShellMain(std::move(params));
}
