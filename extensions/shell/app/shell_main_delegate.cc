// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/app/shell_main_delegate.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/nacl/common/buildflags.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/shell/common/shell_switches.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/default_shell_browser_main_delegate.h"
#include "extensions/shell/browser/shell_content_browser_client.h"
#include "extensions/shell/common/shell_content_client.h"
#include "extensions/shell/renderer/shell_content_renderer_client.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/constants/dbus_paths.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_paths.h"
#endif

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_switches.h"  // nogncheck
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/nacl/common/nacl_paths.h"  // nogncheck
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
#include "components/nacl/zygote/nacl_fork_delegate_linux.h"
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(IS_WIN)
#include "base/base_paths_win.h"
#include "base/process/process_info.h"
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/nix/xdg_util.h"
#elif BUILDFLAG(IS_MAC)
#include "base/base_paths_mac.h"
#endif

namespace {

// Returns the same directory that the browser context will later be
// initialized with.
base::FilePath GetDataPath() {
  // TODO(michaelpg): Use base::PathService to initialize the data path
  // earlier, instead of reading the switch both here and in
  // ShellBrowserContext::InitWhileIOAllowed().
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kContentShellUserDataDir)) {
    return cmd_line->GetSwitchValuePath(switches::kContentShellUserDataDir);
  }

  base::FilePath data_dir;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  data_dir = base::nix::GetXDGDirectory(
      env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir);
#elif BUILDFLAG(IS_WIN)
  CHECK(base::PathService::Get(base::DIR_LOCAL_APP_DATA, &data_dir));
#elif BUILDFLAG(IS_MAC)
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &data_dir));
#else
  NOTIMPLEMENTED();
#endif

  // TODO(michaelpg): Use a different directory for app_shell.
  // See crbug.com/724725.
  return data_dir.Append(FILE_PATH_LITERAL("content_shell"));
}

void InitLogging() {
  uint32_t logging_dest = logging::LOG_TO_ALL;
  base::FilePath log_path;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kLogFile)) {
    log_path = base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        switches::kLogFile);
#if BUILDFLAG(IS_WIN)
  } else if (base::IsCurrentProcessInAppContainer()) {
    // Sandboxed appcontainer processes are unable to resolve the default log
    // file path without asserting.
    logging_dest = (logging_dest & ~logging::LOG_TO_FILE);
#endif
  } else {
    log_path = GetDataPath().Append(FILE_PATH_LITERAL("app_shell.log"));
  }

  // Set up log initialization settings.
  logging::LoggingSettings settings;
  settings.logging_dest = logging_dest;
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
  base::PathService::Get(base::DIR_ASSETS, &extensions_shell_and_test_pak_path);
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

std::optional<int> ShellMainDelegate::BasicStartupComplete() {
  InitLogging();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::RegisterPathProvider();
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_ASH)
  chromeos::dbus_paths::RegisterPathProvider();
#endif
#if BUILDFLAG(ENABLE_NACL) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS))
  nacl::RegisterPathProvider();
#endif
  extensions::RegisterPathProvider();
  return std::nullopt;
}

void ShellMainDelegate::PreSandboxStartup() {
  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);
  if (ProcessNeedsResourceBundle(process_type))
    ui::ResourceBundle::InitSharedInstanceWithPakPath(
        GetResourcesPakFilePath());
}

content::ContentClient* ShellMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<ShellContentClient>();
  return content_client_.get();
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

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
void ShellMainDelegate::ZygoteStarting(
    std::vector<std::unique_ptr<content::ZygoteForkDelegate>>* delegates) {
#if BUILDFLAG(ENABLE_NACL)
  nacl::AddNaClZygoteForkDelegates(delegates);
#endif  // BUILDFLAG(ENABLE_NACL)
}
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)

// static
bool ShellMainDelegate::ProcessNeedsResourceBundle(
    const std::string& process_type) {
  // The browser process has no process type flag, but needs resources.
  // On Linux the zygote process opens the resources for the renderers.
  return process_type.empty() || process_type == switches::kZygoteProcess ||
         process_type == switches::kRendererProcess ||
#if BUILDFLAG(ENABLE_NACL)
         process_type == switches::kNaClLoaderProcess ||
#endif
#if BUILDFLAG(IS_MAC)
         process_type == switches::kGpuProcess ||
#endif
         process_type == switches::kUtilityProcess;
}

}  // namespace extensions
