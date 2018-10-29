// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/app/shell_main_delegate.h"

#include <iostream>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/crash/core/common/crash_key.h"
#include "components/viz/common/switches.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/layouttest_support.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/shell/app/blink_test_platform_support.h"
#include "content/shell/app/shell_crash_reporter_client.h"
#include "content/shell/browser/layout_test/layout_test_browser_main.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/shell_browser_main.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/common/layout_test/layout_test_content_client.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/common/shell_content_client.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/gpu/shell_content_gpu_client.h"
#include "content/shell/renderer/layout_test/layout_test_content_renderer_client.h"
#include "content/shell/renderer/shell_content_renderer_client.h"
#include "content/shell/utility/shell_content_utility_client.h"
#include "gpu/config/gpu_switches.h"
#include "ipc/ipc_buildflags.h"
#include "media/base/media_switches.h"
#include "net/cookies/cookie_monster.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/service_manager/embedder/switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "content/public/common/content_ipc_logging.h"
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
    content::RegisterIPCLogger(msg_id, logger)
#include "content/shell/common/shell_messages.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/apk_assets.h"
#include "base/posix/global_descriptors.h"
#include "content/public/test/nested_message_pump_android.h"
#include "content/shell/android/shell_descriptors.h"
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "components/crash/content/app/crashpad.h"  // nogncheck
#endif

#if defined(OS_MACOSX)
#include "content/shell/app/paths_mac.h"
#include "content/shell/app/shell_main_delegate_mac.h"
#endif  // OS_MACOSX

#if defined(OS_WIN)
#include <windows.h>

#include <initguid.h>
#include "base/logging_win.h"
#include "content/shell/common/v8_crashpad_support_win.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "components/crash/content/app/breakpad_linux.h"
#endif

#if defined(OS_FUCHSIA)
#include "base/base_paths_fuchsia.h"
#endif  // OS_FUCHSIA

namespace {

#if defined(OS_ANDROID)
std::unique_ptr<base::MessagePump> CreateMessagePumpForUI() {
  return std::make_unique<content::NestedMessagePumpAndroid>();
}
#endif

#if !defined(OS_FUCHSIA)
base::LazyInstance<content::ShellCrashReporterClient>::Leaky
    g_shell_crash_client = LAZY_INSTANCE_INITIALIZER;
#endif

#if defined(OS_WIN)
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
  base::FilePath log_filename =
      command_line.GetSwitchValuePath(switches::kLogFile);
  if (log_filename.empty()) {
#if defined(OS_FUCHSIA)
    base::PathService::Get(base::DIR_TEMP, &log_filename);
#else
    base::PathService::Get(base::DIR_EXE, &log_filename);
#endif
    log_filename = log_filename.AppendASCII("content_shell.log");
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = log_filename.value().c_str();
  settings.delete_old = logging::DELETE_OLD_LOG_FILE;
  logging::InitLogging(settings);
  logging::SetLogItems(true /* Process ID */, true /* Thread ID */,
                       true /* Timestamp */, false /* Tick count */);
}

}  // namespace

namespace content {

ShellMainDelegate::ShellMainDelegate(bool is_browsertest)
    : is_browsertest_(is_browsertest) {}

ShellMainDelegate::~ShellMainDelegate() {
}

bool ShellMainDelegate::BasicStartupComplete(int* exit_code) {
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  int dummy;
  if (!exit_code)
    exit_code = &dummy;

#if defined(OS_WIN)
  // Enable trace control and transport through event tracing for Windows.
  logging::LogEventProvider::Initialize(kContentShellProviderName);

  v8_crashpad_support::SetUp();
#endif
#if defined(OS_LINUX)
  breakpad::SetFirstChanceExceptionHandler(v8::V8::TryHandleSignal);
#endif
#if defined(OS_MACOSX)
  // Needs to happen before InitializeResourceBundle() and before
  // BlinkTestPlatformInitialize() are called.
  OverrideFrameworkBundlePath();
  OverrideOuterBundlePath();
  OverrideChildProcessPath();
  OverrideSourceRootPath();
  EnsureCorrectResolutionSettings();
  OverrideBundleID();
#endif  // OS_MACOSX

  InitLogging(command_line);

  if (command_line.HasSwitch("run-layout-test")) {
    std::cerr << std::string(79, '*') << "\n"
              << "* The flag --run-layout-test is obsolete. Please use --"
              << switches::kRunWebTests << " instead. *\n"
              << std::string(79, '*') << "\n";
    command_line.AppendSwitch(switches::kRunWebTests);
  }

  if (command_line.HasSwitch(switches::kRunWebTests)) {
    // Only run CheckLayoutSystemDeps on the browser process.
    if (!command_line.HasSwitch(switches::kProcessType)) {
      // If CheckLayoutSystemDeps fails, we early exit as there's no point in
      // running the tests.
      if (!CheckLayoutSystemDeps()) {
        *exit_code = 1;
        return true;
      }
    }

    EnableBrowserLayoutTestMode();

#if BUILDFLAG(ENABLE_PLUGINS)
    if (!ppapi::RegisterBlinkTestPlugin(&command_line)) {
      *exit_code = 1;
      return true;
    }
#endif
    command_line.AppendSwitch(cc::switches::kEnableGpuBenchmarking);
    command_line.AppendSwitch(switches::kEnableLogging);
    command_line.AppendSwitch(switches::kAllowFileAccessFromFiles);
#if !defined(OS_ANDROID)
    // TODO(crbug/567947) Enable display compositor pixel dumps for Android
    // once testing becomes possible on post-kitkat OSes, and once we've
    // had a chance to debug the layout test failures that occur when this
    // flag is present.
    command_line.AppendSwitch(switches::kEnableDisplayCompositorPixelDump);
#endif
    // only default to a software GL if the flag isn't already specified.
    if (!command_line.HasSwitch(switches::kUseGpuInTests) &&
        !command_line.HasSwitch(switches::kUseGL)) {
      command_line.AppendSwitchASCII(
          switches::kUseGL,
          gl::GetGLImplementationName(gl::GetSoftwareGLImplementation()));
    }
    command_line.AppendSwitchASCII(
        switches::kTouchEventFeatureDetection,
        switches::kTouchEventFeatureDetectionEnabled);
    if (!command_line.HasSwitch(switches::kForceDeviceScaleFactor))
      command_line.AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");

    if (!command_line.HasSwitch(switches::kAutoplayPolicy)) {
      command_line.AppendSwitchASCII(
          switches::kAutoplayPolicy,
          switches::autoplay::kNoUserGestureRequiredPolicy);
    }

    if (!command_line.HasSwitch(switches::kStableReleaseMode)) {
      command_line.AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    }

    if (!command_line.HasSwitch(switches::kEnableThreadedCompositing)) {
      command_line.AppendSwitch(switches::kDisableThreadedCompositing);
      command_line.AppendSwitch(cc::switches::kDisableThreadedAnimation);
    }

    // If we're doing a display compositor pixel dump we ensure that
    // we complete all stages of compositing before draw. We also can't have
    // checker imaging, since it's imcompatible with single threaded compositor
    // and display compositor pixel dumps.
    if (command_line.HasSwitch(switches::kEnableDisplayCompositorPixelDump)) {
      // TODO(crbug.com/894613) Add kRunAllCompositorStagesBeforeDraw back here
      // once you figure out why it causes so much layout test flakiness.
      // command_line.AppendSwitch(switches::kRunAllCompositorStagesBeforeDraw);
      command_line.AppendSwitch(cc::switches::kDisableCheckerImaging);
    }

    command_line.AppendSwitch(switches::kMuteAudio);

    command_line.AppendSwitch(switches::kEnablePreciseMemoryInfo);

    command_line.AppendSwitchASCII(network::switches::kHostResolverRules,
                                   "MAP nonexistent.*.test ~NOTFOUND,"
                                   "MAP *.test 127.0.0.1");

    command_line.AppendSwitch(switches::kEnablePartialRaster);
    command_line.AppendSwitch(switches::kEnableWebAuthTestingAPI);

    if (!command_line.HasSwitch(switches::kForceGpuRasterization) &&
        !command_line.HasSwitch(switches::kEnableGpuRasterization)) {
      command_line.AppendSwitch(switches::kDisableGpuRasterization);
    }

    // If the virtual test suite didn't specify a display color space, then
    // force sRGB.
    if (!command_line.HasSwitch(switches::kForceDisplayColorProfile)) {
      command_line.AppendSwitchASCII(switches::kForceDisplayColorProfile,
                                     "srgb");
    }

    // We want stable/baseline results when running layout tests.
    command_line.AppendSwitch(switches::kDisableSkiaRuntimeOpts);

    command_line.AppendSwitch(switches::kDisallowNonExactResourceReuse);

    // Always run with fake media devices.
    command_line.AppendSwitch(switches::kUseFakeUIForMediaStream);
    command_line.AppendSwitch(switches::kUseFakeDeviceForMediaStream);

    // Always disable the unsandbox GPU process for DX12 and Vulkan Info
    // collection to avoid interference. This GPU process is launched 15
    // seconds after chrome starts.
    command_line.AppendSwitch(
        switches::kDisableGpuProcessForDX12VulkanInfoCollection);

    if (!BlinkTestPlatformInitialize()) {
      *exit_code = 1;
      return true;
    }
  }

  content_client_.reset(switches::IsRunWebTestsSwitchPresent()
                            ? new LayoutTestContentClient
                            : new ShellContentClient);
  SetContentClient(content_client_.get());

  return false;
}

void ShellMainDelegate::PreSandboxStartup() {
#if defined(ARCH_CPU_ARM_FAMILY) && (defined(OS_ANDROID) || defined(OS_LINUX))
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info.
  base::CPU cpu_info;
#endif

// Disable platform crash handling and initialize the crash reporter, if
// requested.
// TODO(crbug.com/753619): Implement crash reporter integration for Fuchsia.
#if !defined(OS_FUCHSIA)
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    std::string process_type =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType);
    crash_reporter::SetCrashReporterClient(g_shell_crash_client.Pointer());
#if defined(OS_MACOSX) || defined(OS_WIN)
    crash_reporter::InitializeCrashpad(process_type.empty(), process_type);
#elif defined(OS_LINUX)
    // Reporting for sub-processes will be initialized in ZygoteForked.
    if (process_type != service_manager::switches::kZygoteProcess)
      breakpad::InitCrashReporter(process_type);
#elif defined(OS_ANDROID)
    if (process_type.empty())
      breakpad::InitCrashReporter(process_type);
    else
      breakpad::InitNonBrowserCrashReporterForAndroid(process_type);
#endif  // defined(OS_ANDROID)
  }
#endif  // !defined(OS_FUCHSIA)

  crash_reporter::InitializeCrashKeys();

  InitializeResourceBundle();
}

int ShellMainDelegate::RunProcess(
    const std::string& process_type,
    const MainFunctionParams& main_function_params) {
  if (!process_type.empty())
    return -1;

#if !defined(OS_ANDROID)
  // Android stores the BrowserMainRunner instance as a scoped member pointer
  // on the ShellMainDelegate class because of different object lifetime.
  std::unique_ptr<BrowserMainRunner> browser_runner_;
#endif

  base::trace_event::TraceLog::GetInstance()->set_process_name("Browser");
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventBrowserProcessSortIndex);

  browser_runner_.reset(BrowserMainRunner::Create());
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kRunWebTests)
             ? LayoutTestBrowserMain(main_function_params, browser_runner_)
             : ShellBrowserMain(main_function_params, browser_runner_);
}

#if defined(OS_LINUX)
void ShellMainDelegate::ZygoteForked() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableCrashReporter)) {
    std::string process_type =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kProcessType);
    breakpad::InitCrashReporter(process_type);
  }
}
#endif  // defined(OS_LINUX)

void ShellMainDelegate::InitializeResourceBundle() {
#if defined(OS_ANDROID)
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
  // This is clearly wrong. See crbug.com/330930
  ui::ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd),
                                                          pak_region);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
      base::File(pak_fd), pak_region, ui::SCALE_FACTOR_100P);
#elif defined(OS_MACOSX)
  ui::ResourceBundle::InitSharedInstanceWithPakPath(GetResourcesPakFilePath());
#else
  base::FilePath pak_file;
  bool r = base::PathService::Get(base::DIR_ASSETS, &pak_file);
  DCHECK(r);
  pak_file = pak_file.Append(FILE_PATH_LITERAL("content_shell.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
#endif
}

void ShellMainDelegate::PreCreateMainMessageLoop() {
#if defined(OS_ANDROID)
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRunWebTests)) {
    bool success =
        base::MessageLoop::InitMessagePumpForUIFactory(&CreateMessagePumpForUI);
    CHECK(success) << "Unable to initialize the message pump for Android";
  }
#elif defined(OS_MACOSX)
  RegisterShellCrApp();
#endif
}

ContentBrowserClient* ShellMainDelegate::CreateContentBrowserClient() {
  browser_client_.reset(switches::IsRunWebTestsSwitchPresent()
                            ? new LayoutTestContentBrowserClient
                            : new ShellContentBrowserClient);

  return browser_client_.get();
}

ContentGpuClient* ShellMainDelegate::CreateContentGpuClient() {
  gpu_client_.reset(new ShellContentGpuClient);
  return gpu_client_.get();
}

ContentRendererClient* ShellMainDelegate::CreateContentRendererClient() {
  renderer_client_.reset(switches::IsRunWebTestsSwitchPresent()
                             ? new LayoutTestContentRendererClient
                             : new ShellContentRendererClient);

  return renderer_client_.get();
}

ContentUtilityClient* ShellMainDelegate::CreateContentUtilityClient() {
  utility_client_.reset(new ShellContentUtilityClient(is_browsertest_));
  return utility_client_.get();
}

}  // namespace content
