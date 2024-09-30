// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/shell_main_delegate.h"

#include <iostream>
#include <tuple>
#include <utility>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/current_process.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/memory_system/initializer.h"
#include "components/memory_system/parameters.h"
#include "content/common/content_constants_internal.h"
#include "content/public/app/initialize_mojo_core.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/url_constants.h"
#include "content/shell/app/shell_crash_reporter_client.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_paths.h"
#include "content/shell/common/shell_content_client.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/gpu/shell_content_gpu_client.h"
#include "content/shell/renderer/shell_content_renderer_client.h"
#include "content/shell/utility/shell_content_utility_client.h"
#include "ipc/ipc_buildflags.h"
#include "net/cookies/cookie_monster.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "content/public/common/content_ipc_logging.h"
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
    content::RegisterIPCLogger(msg_id, logger)
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "content/web_test/browser/web_test_browser_main_runner.h"  // nogncheck
#include "content/web_test/browser/web_test_content_browser_client.h"  // nogncheck
#include "content/web_test/renderer/web_test_content_renderer_client.h"  // nogncheck
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/posix/global_descriptors.h"
#include "content/public/browser/android/compositor.h"
#include "content/shell/android/shell_descriptors.h"
#endif

#if !BUILDFLAG(IS_FUCHSIA)
#include "components/crash/core/app/crashpad.h"  // nogncheck
#endif

#if BUILDFLAG(IS_APPLE)
#include "content/shell/app/paths_mac.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/shell/app/shell_main_delegate_mac.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include <initguid.h>
#include <windows.h>

#include "base/logging_win.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "content/shell/common/v8_crashpad_support_win.h"
#endif

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_ANDROID)
#include "v8/include/v8-wasm-trap-handler-posix.h"
#endif

#if BUILDFLAG(IS_IOS)
#include "content/shell/app/ios/shell_application_ios.h"
#endif

namespace {

enum class LoggingDest {
  kFile,
  kStderr,
#if BUILDFLAG(IS_WIN)
  kHandle,
#endif
};

#if !BUILDFLAG(IS_FUCHSIA)
base::LazyInstance<content::ShellCrashReporterClient>::Leaky
    g_shell_crash_client = LAZY_INSTANCE_INITIALIZER;
#endif

#if BUILDFLAG(IS_WIN)
// If "Content Shell" doesn't show up in your list of trace providers in
// Sawbuck, add these registry entries to your machine (NOTE the optional
// Wow6432Node key for x64 machines):
// 1. Find:  HKLM\SOFTWARE\[Wow6432Node\]Google\Sawbuck\Providers
// 2. Add a subkey with the name "{6A3E50A4-7E15-4099-8413-EC94D8C2A4B6}"
// 3. Add these values:
//    "default_flags"=dword:00000001
//    "default_level"=dword:00000004
//    @="Content Shell"

// {6A3E50A4-7E15-4099-8413-EC94D8C2A4B6}
const GUID kContentShellProviderName = {
    0x6a3e50a4, 0x7e15, 0x4099,
        { 0x84, 0x13, 0xec, 0x94, 0xd8, 0xc2, 0xa4, 0xb6 } };
#endif

void InitLogging(const base::CommandLine& command_line) {
  LoggingDest dest = LoggingDest::kFile;

  if (command_line.GetSwitchValueASCII(switches::kEnableLogging) == "stderr") {
    dest = LoggingDest::kStderr;
  }

#if BUILDFLAG(IS_WIN)
  // On Windows child process may be given a handle in the --log-file switch.
  base::win::ScopedHandle log_handle;
  if (command_line.GetSwitchValueASCII(switches::kEnableLogging) == "handle") {
    auto handle_str = command_line.GetSwitchValueNative(switches::kLogFile);
    uint32_t handle_value = 0;
    if (base::StringToUint(handle_str, &handle_value)) {
      // This handle is owned by the logging framework and is closed when the
      // process exits.
      HANDLE duplicate = nullptr;
      if (::DuplicateHandle(GetCurrentProcess(),
                            base::win::Uint32ToHandle(handle_value),
                            GetCurrentProcess(), &duplicate, 0, FALSE,
                            DUPLICATE_SAME_ACCESS)) {
        log_handle.Set(duplicate);
        dest = LoggingDest::kHandle;
      }
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  base::FilePath log_filename;
  if (dest == LoggingDest::kFile) {
    log_filename = command_line.GetSwitchValuePath(switches::kLogFile);
    if (log_filename.empty()) {
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
      base::PathService::Get(base::DIR_TEMP, &log_filename);
#else
      base::PathService::Get(base::DIR_EXE, &log_filename);
#endif
      log_filename = log_filename.AppendASCII("content_shell.log");
    }
  }

  logging::LoggingSettings settings;
#if BUILDFLAG(IS_WIN)
  if (dest == LoggingDest::kHandle) {
    // TODO(crbug.com/328285906) Use a ScopedHandle in logging settings.
    settings.log_file = log_handle.release();
  } else {
    settings.log_file = nullptr;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (dest == LoggingDest::kFile) {
    settings.log_file_path = log_filename.value();
  }

  if (dest == LoggingDest::kStderr) {
    settings.logging_dest =
        logging::LOG_TO_STDERR | logging::LOG_TO_SYSTEM_DEBUG_LOG;
  } else {
    // Includes both handle or provided filename on Windows.
    settings.logging_dest = logging::LOG_TO_ALL;
  }

  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);
}

}  // namespace

namespace content {

ShellMainDelegate::ShellMainDelegate(bool is_content_browsertests)
    : is_content_browsertests_(is_content_browsertests) {}

ShellMainDelegate::~ShellMainDelegate() {
}

std::optional<int> ShellMainDelegate::BasicStartupComplete() {
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch("run-layout-test")) {
    std::cerr << std::string(79, '*') << "\n"
              << "* The flag --run-layout-test is obsolete. Please use --"
              << switches::kRunWebTests << " instead. *\n"
              << std::string(79, '*') << "\n";
    command_line.AppendSwitch(switches::kRunWebTests);
  }

#if BUILDFLAG(IS_ANDROID)
  Compositor::Initialize();
#endif

#if BUILDFLAG(IS_WIN)
  // Enable trace control and transport through event tracing for Windows.
  logging::LogEventProvider::Initialize(kContentShellProviderName);

  v8_crashpad_support::SetUp();
#endif

#if BUILDFLAG(IS_MAC)
  // Needs to happen before InitializeResourceBundle().
  EnsureCorrectResolutionSettings();
#endif  // BUILDFLAG(IS_MAC)

  InitLogging(command_line);

#if !BUILDFLAG(IS_ANDROID)
  if (switches::IsRunWebTestsSwitchPresent()) {
    const bool browser_process =
        command_line.GetSwitchValueASCII(switches::kProcessType).empty();
    if (browser_process) {
      web_test_runner_ = std::make_unique<WebTestBrowserMainRunner>();
      web_test_runner_->Initialize();
    }
  }
#endif

  RegisterShellPathProvider();

  return std::nullopt;
}

bool ShellMainDelegate::ShouldCreateFeatureList(InvokedIn invoked_in) {
  return absl::holds_alternative<InvokedInChildProcess>(invoked_in);
}

bool ShellMainDelegate::ShouldInitializeMojo(InvokedIn invoked_in) {
  return ShouldCreateFeatureList(invoked_in);
}

void ShellMainDelegate::PreSandboxStartup() {
// Disable platform crash handling and initialize the crash reporter, if
// requested.
// TODO(crbug.com/40188745): Implement crash reporter integration for Fuchsia.
#if !BUILDFLAG(IS_FUCHSIA)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    std::string process_type =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType);
    crash_reporter::SetCrashReporterClient(g_shell_crash_client.Pointer());
    // Reporting for sub-processes will be initialized in ZygoteForked.
    if (process_type != switches::kZygoteProcess) {
      crash_reporter::InitializeCrashpad(process_type.empty(), process_type);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      crash_reporter::SetFirstChanceExceptionHandler(
          v8::TryHandleWebAssemblyTrapPosix);
#endif
    }
  }
#endif  // !BUILDFLAG(IS_FUCHSIA)

  crash_reporter::InitializeCrashKeys();

  InitializeResourceBundle();
}

absl::variant<int, MainFunctionParams> ShellMainDelegate::RunProcess(
    const std::string& process_type,
    MainFunctionParams main_function_params) {
  // For non-browser process, return and have the caller run the main loop.
  if (!process_type.empty())
    return std::move(main_function_params);

  base::CurrentProcess::GetInstance().SetProcessType(
      base::CurrentProcessType::PROCESS_BROWSER);
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventBrowserProcessSortIndex);

#if !BUILDFLAG(IS_ANDROID)
  if (switches::IsRunWebTestsSwitchPresent()) {
    // Web tests implement their own BrowserMain() replacement.
    web_test_runner_->RunBrowserMain(std::move(main_function_params));
    web_test_runner_.reset();
    // Returning 0 to indicate that we have replaced BrowserMain() and the
    // caller should not call BrowserMain() itself. Web tests do not ever
    // return an error.
    return 0;
  }
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  // On Android and iOS, we defer to the system message loop when the stack
  // unwinds. So here we only create (and leak) a BrowserMainRunner. The
  // shutdown of BrowserMainRunner doesn't happen in Chrome Android/iOS and
  // doesn't work properly on Android/iOS at all.
  std::unique_ptr<BrowserMainRunner> main_runner = BrowserMainRunner::Create();
  // In browser tests, the |main_function_params| contains a |ui_task| which
  // will execute the testing. The task will be executed synchronously inside
  // Initialize() so we don't depend on the BrowserMainRunner being Run().
  int initialize_exit_code =
      main_runner->Initialize(std::move(main_function_params));
  DCHECK_LT(initialize_exit_code, 0)
      << "BrowserMainRunner::Initialize failed in ShellMainDelegate";
  std::ignore = main_runner.release();
  // Return 0 as BrowserMain() should not be called after this, bounce up to
  // the system message loop for ContentShell, and we're already done thanks
  // to the |ui_task| for browser tests.
  return 0;
#else
  // On non-Android, we can return the |main_function_params| back and have the
  // caller run BrowserMain() normally.
  return std::move(main_function_params);
#endif
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void ShellMainDelegate::ZygoteForked() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    std::string process_type =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType);
    crash_reporter::InitializeCrashpad(false, process_type);
    crash_reporter::SetFirstChanceExceptionHandler(
        v8::TryHandleWebAssemblyTrapPosix);
  }
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

void ShellMainDelegate::InitializeResourceBundle() {
#if BUILDFLAG(IS_ANDROID)
  // On Android, the renderer runs with a different UID and can never access
  // the file system. Use the file descriptor passed in at launch time.
  auto* global_descriptors = base::GlobalDescriptors::GetInstance();
  int pak_fd = global_descriptors->MaybeGet(kShellPakDescriptor);
  base::MemoryMappedFile::Region pak_region;
  if (pak_fd >= 0) {
    pak_region = global_descriptors->GetRegion(kShellPakDescriptor);
  } else {
    pak_fd =
        base::android::OpenApkAsset("assets/content_shell.pak", &pak_region);
    // Loaded from disk for browsertests.
    if (pak_fd < 0) {
      base::FilePath pak_file;
      bool r = base::PathService::Get(base::DIR_ANDROID_APP_DATA, &pak_file);
      DCHECK(r);
      pak_file = pak_file.Append(FILE_PATH_LITERAL("paks"));
      pak_file = pak_file.Append(FILE_PATH_LITERAL("content_shell.pak"));
      int flags = base::File::FLAG_OPEN | base::File::FLAG_READ;
      pak_fd = base::File(pak_file, flags).TakePlatformFile();
      pak_region = base::MemoryMappedFile::Region::kWholeFile;
    }
    global_descriptors->Set(kShellPakDescriptor, pak_fd, pak_region);
  }
  DCHECK_GE(pak_fd, 0);
  // TODO(crbug.com/40346051): A better way to prevent fdsan error from a double
  // close is to refactor GlobalDescriptors.{Get,MaybeGet} to return
  // "const base::File&" rather than fd itself.
  base::File android_pak_file(pak_fd);
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(
      android_pak_file.Duplicate(), pak_region);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
      std::move(android_pak_file), pak_region, ui::k100Percent);
#elif BUILDFLAG(IS_APPLE)
  ui::ResourceBundle::InitSharedInstanceWithPakPath(GetResourcesPakFilePath());
#else
  base::FilePath pak_file;
  bool r = base::PathService::Get(base::DIR_ASSETS, &pak_file);
  DCHECK(r);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("content_shell.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
#endif
}

std::optional<int> ShellMainDelegate::PreBrowserMain() {
  std::optional<int> exit_code = content::ContentMainDelegate::PreBrowserMain();
  if (exit_code.has_value())
    return exit_code;

#if BUILDFLAG(IS_MAC)
  RegisterShellCrApp();
#endif
  return std::nullopt;
}

std::optional<int> ShellMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (!ShouldCreateFeatureList(invoked_in)) {
    // Apply field trial testing configuration since content did not.
    browser_client_->CreateFeatureListAndFieldTrials();
  }
  if (!ShouldInitializeMojo(invoked_in)) {
    InitializeMojoCore();
  }

  const std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  // ShellMainDelegate has GWP-ASan as well as Profiling Client disabled.
  // Consequently, we provide no parameters for these two. The memory_system
  // includes the PoissonAllocationSampler dynamically only if the Profiling
  // Client is enabled. However, we are not sure if this is the only user of
  // PoissonAllocationSampler in the ContentShell. Therefore, enforce inclusion
  // at the moment.
  //
  // TODO(crbug.com/40062835): Clarify which users of
  // PoissonAllocationSampler we have in the ContentShell. Do we really need to
  // enforce it?
  memory_system::Initializer()
      .SetDispatcherParameters(memory_system::DispatcherParameters::
                                   PoissonAllocationSamplerInclusion::kEnforce,
                               memory_system::DispatcherParameters::
                                   AllocationTraceRecorderInclusion::kIgnore,
                               process_type)
      .Initialize(memory_system_);

  return std::nullopt;
}

ContentClient* ShellMainDelegate::CreateContentClient() {
  content_client_ = std::make_unique<ShellContentClient>();
  return content_client_.get();
}

ContentBrowserClient* ShellMainDelegate::CreateContentBrowserClient() {
#if !BUILDFLAG(IS_ANDROID)
  if (switches::IsRunWebTestsSwitchPresent()) {
    browser_client_ = std::make_unique<WebTestContentBrowserClient>();
    return browser_client_.get();
  }
#endif
  browser_client_ = std::make_unique<ShellContentBrowserClient>();
  return browser_client_.get();
}

ContentGpuClient* ShellMainDelegate::CreateContentGpuClient() {
  gpu_client_ = std::make_unique<ShellContentGpuClient>();
  return gpu_client_.get();
}

ContentRendererClient* ShellMainDelegate::CreateContentRendererClient() {
#if !BUILDFLAG(IS_ANDROID)
  if (switches::IsRunWebTestsSwitchPresent()) {
    renderer_client_ = std::make_unique<WebTestContentRendererClient>();
    return renderer_client_.get();
  }
#endif
  renderer_client_ = std::make_unique<ShellContentRendererClient>();
  return renderer_client_.get();
}

ContentUtilityClient* ShellMainDelegate::CreateContentUtilityClient() {
  utility_client_ =
      std::make_unique<ShellContentUtilityClient>(is_content_browsertests_);
  return utility_client_.get();
}

}  // namespace content
