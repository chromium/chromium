// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/app/shell_main_delegate.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/default_shell_browser_main_delegate.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/common/shell_content_client.h"
#include "extensions/shell/renderer/shell_content_renderer_client.h"
#include "services/service_manager/embedder/switches.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_paths.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_switches.h"  // nogncheck
#if defined(OS_LINUX)
#include "components/nacl/common/nacl_paths.h"  // nogncheck
#endif  // OS_LINUX
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
#include "components/nacl/zygote/nacl_fork_delegate_linux.h"
#endif  // OS_POSIX && !OS_MACOSX && !OS_ANDROID
#endif  // BUILDFLAG(ENABLE_NACL)

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#elif defined(OS_MACOSX)
#include "base/base_paths_mac.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "components/crash/content/app/breakpad_linux.h"         // nogncheck
#include "components/crash/content/app/crash_reporter_client.h"  // nogncheck
#include "extensions/shell/app/shell_crash_reporter_client.h"
#endif

namespace {

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
extensions::ShellCrashReporterClient* GetCrashReporterClient() {
  static base::NoDestructor<extensions::ShellCrashReporterClient> instance;
  return instance.get();
}
#endif

// Returns the same directory that the browser context will later be
// initialized with.
base::FilePath GetDataPath() {
  // TODO(michaelpg): Use base::PathService to initialize the data path
  // earlier, instead of reading the switch both here and in
  // ShellBrowserContext::InitWhileIOAllowed().
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kContentShellDataPath))
    return cmd_line->GetSwitchValuePath(switches::kContentShellDataPath);

  base::FilePath data_dir;
#if defined(OS_LINUX)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  data_dir = base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir);
#elif defined(OS_WIN)
  CHECK(base::PathService::Get(base::DIR_LOCAL_APP_DATA, &data_dir));
#elif defined(OS_MACOSX)
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &data_dir));
#else
  NOTIMPLEMENTED();
#endif

  // TODO(michaelpg): Use a different directory for app_shell.
  // See crbug.com/724725.
  return data_dir.Append(FILE_PATH_LITERAL("content_shell"));
}

void InitLogging() {
  base::FilePath log_path;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kLogFile)) {
    log_path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        switches::kLogFile);
  } else {
    log_path = GetDataPath().Append(FILE_PATH_LITERAL("app_shell.log"));
  }

  // Set up log initialization settings.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file_path = log_path.value().c_str();

  // Replace the old log file if this is the first process.
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  settings.delete_old = process_type.empty() ? logging::DELETE_OLD_LOG_FILE
                                             : logging::APPEND_TO_OLD_LOG_FILE;

  logging::InitLogging(settings);
  logging::SetLogItems(true, true, true, true);
}

// Returns the path to the extensions_shell_and_test.pak file.
base::FilePath GetResourcesPakFilePath() {
  base::FilePath extensions_shell_and_test_pak_path;
  base::PathService::Get(base::DIR_MODULE, &extensions_shell_and_test_pak_path);
  extensions_shell_and_test_pak_path =
      extensions_shell_and_test_pak_path.AppendASCII(
          "extensions_shell_and_test.pak");
  return extensions_shell_and_test_pak_path;
}

}  // namespace

namespace extensions {

ShellMainDelegate::ShellMainDelegate() {
}

ShellMainDelegate::~ShellMainDelegate() {
}

bool ShellMainDelegate::BasicStartupComplete(int* exit_code) {
  InitLogging();
  content_client_.reset(new ShellContentClient);
  SetContentClient(content_client_.get());

#if defined(OS_CHROMEOS)
  chromeos::RegisterPathProvider();
#endif
#if BUILDFLAG(ENABLE_NACL) && defined(OS_LINUX)
  nacl::RegisterPathProvider();
#endif
  extensions::RegisterPathProvider();
  return false;
}

void ShellMainDelegate::PreSandboxStartup() {
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  crash_reporter::SetCrashReporterClient(GetCrashReporterClient());
  // Reporting for sub-processes will be initialized in ZygoteForked.
  if (process_type != service_manager::switches::kZygoteProcess)
    breakpad::InitCrashReporter(process_type);
#endif

  if (ProcessNeedsResourceBundle(process_type))
    ui::ResourceBundle::InitSharedInstanceWithPakPath(
        GetResourcesPakFilePath());
}

content::ContentBrowserClient* ShellMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<ShellContentBrowserClient>(
      new DefaultShellBrowserMainDelegate);
  return browser_client_.get();
}

content::ContentRendererClient*
ShellMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<ShellContentRendererClient>();
  return renderer_client_.get();
}

void ShellMainDelegate::ProcessExiting(const std::string& process_type) {
  logging::CloseLogFile();
}

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
void ShellMainDelegate::ZygoteStarting(
    std::vector<std::unique_ptr<service_manager::ZygoteForkDelegate>>*
        delegates) {
#if BUILDFLAG(ENABLE_NACL)
  nacl::AddNaClZygoteForkDelegates(delegates);
#endif  // BUILDFLAG(ENABLE_NACL)
}
#endif  // OS_POSIX && !OS_MACOSX && !OS_ANDROID

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void ShellMainDelegate::ZygoteForked() {
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  breakpad::InitCrashReporter(process_type);
}
#endif

// static
bool ShellMainDelegate::ProcessNeedsResourceBundle(
    const std::string& process_type) {
  // The browser process has no process type flag, but needs resources.
  // On Linux the zygote process opens the resources for the renderers.
  return process_type.empty() ||
         process_type == service_manager::switches::kZygoteProcess ||
         process_type == switches::kRendererProcess ||
#if BUILDFLAG(ENABLE_NACL)
         process_type == switches::kNaClLoaderProcess ||
#endif
#if defined(OS_MACOSX)
         process_type == switches::kGpuProcess ||
#endif
         process_type == switches::kUtilityProcess;
}

}  // namespace extensions
