// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "content/shell/app/shell_main_delegate.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/dark_mode_support.h"
#include "base/win/win_util.h"
#include "content/public/app/sandbox_helper_win.h"
#include "sandbox/win/src/sandbox_types.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "base/at_exit.h"                                 // nogncheck
#include "base/command_line.h"                            // nogncheck
#include "build/ios_buildflags.h"                         // nogncheck
#include "content/public/common/content_switches.h"       // nogncheck
#include "content/shell/app/ios/shell_application_ios.h"
#include "content/shell/app/ios/web_tests_support_ios.h"
#include "content/shell/common/shell_switches.h"
#endif

#if BUILDFLAG(IS_WIN)

#if !defined(WIN_CONSOLE_APP)
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
#else
int main() {
  HINSTANCE instance = GetModuleHandle(NULL);
#endif
  // Load and pin user32.dll and uxtheme.dll to avoid having to load them once
  // tests start while on the main thread loop where blocking calls are
  // disallowed. This will also ensure the Windows dark mode support is enabled
  // for the app if available.
  base::win::PinUser32();
  base::win::AllowDarkModeForApp(true);
  sandbox::SandboxInterfaceInfo sandbox_info = {nullptr};
  content::InitializeSandboxInfo(&sandbox_info);
  content::ShellMainDelegate delegate;

  content::ContentMainParams params(&delegate);
  params.instance = instance;
  params.sandbox_info = &sandbox_info;
  return content::ContentMain(std::move(params));
}

#elif BUILDFLAG(IS_IOS)

#define IOS_INIT_EXPORT __attribute__((visibility("default")))

extern "C" IOS_INIT_EXPORT int ChildProcessMain(int argc, const char** argv) {
  // Create this here since it's needed to start the crash handler.
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
  content::ShellMainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;
  return content::ContentMain(std::move(params));
}

extern "C" IOS_INIT_EXPORT int ContentAppMain(int argc, const char** argv) {
  // Create this here since it's needed to start the crash handler.
  base::AtExitManager at_exit;

  // Check if this is the browser process or a subprocess. Only the browser
  // browser should run UIApplicationMain.
  base::CommandLine::Init(argc, argv);
  auto type = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kProcessType);

  // The browser process has no --process-type argument.
  if (type.empty()) {
    if (switches::IsRunWebTestsSwitchPresent()) {
      // We create a simple UIApplication to run the web tests.
      return RunWebTestsFromIOSApp(argc, argv);
    } else {
      // We will create the ContentMainRunner once the UIApplication is ready.
      return RunShellApplication(argc, argv);
    }
  } else {
    content::ShellMainDelegate delegate;
    content::ContentMainParams params(&delegate);
    params.argc = argc;
    params.argv = argv;
    return content::ContentMain(std::move(params));
  }
}

#else

int main(int argc, const char** argv) {
  content::ShellMainDelegate delegate;
  content::ContentMainParams params(&delegate);
  params.argc = argc;
  params.argv = argv;
  return content::ContentMain(std::move(params));
}

#endif  // BUILDFLAG(IS_WIN)
