// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/headless_content_main_delegate.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/process/current_process.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/crash/core/common/crash_key.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/viz/common/switches.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/profiling.h"
#include "gpu/config/gpu_switches.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_content_browser_client.h"
#include "headless/lib/headless_crash_reporter_client.h"
#include "headless/lib/renderer/headless_content_renderer_client.h"
#include "headless/lib/utility/headless_content_utility_client.h"
#include "sandbox/policy/switches.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

#if defined(HEADLESS_USE_EMBEDDED_RESOURCES)
#include "headless/embedded_resource_pack_data.h"     // nogncheck
#include "headless/embedded_resource_pack_strings.h"  // nogncheck
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/crash/core/app/crashpad.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "components/crash/core/app/crash_switches.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_POSIX)
#include <signal.h>
#endif

namespace headless {

namespace features {
BASE_FEATURE(kVirtualTime, "VirtualTime", base::FEATURE_DISABLED_BY_DEFAULT);
}

const base::FilePath::CharType kDefaultProfileName[] =
    FILE_PATH_LITERAL("Default");

namespace {

// Keep in sync with content/common/content_constants_internal.h.
// TODO(skyostil): Add a tracing test for this.
const int kTraceEventBrowserProcessSortIndex = -6;

HeadlessContentMainDelegate* g_current_headless_content_main_delegate = nullptr;

#if !BUILDFLAG(IS_FUCHSIA)
base::LazyInstance<HeadlessCrashReporterClient>::Leaky g_headless_crash_client =
    LAZY_INSTANCE_INITIALIZER;
#endif

const char kLogFileName[] = "CHROME_LOG_FILE";
const char kHeadlessCrashKey[] = "headless";

void InitializeResourceBundle(const base::CommandLine& command_line) {
#if defined(HEADLESS_USE_EMBEDDED_RESOURCES)
  ui::ResourceBundle::InitSharedInstanceWithBuffer(
      {kHeadlessResourcePackStrings.contents,
       kHeadlessResourcePackStrings.length},
      ui::kScaleFactorNone);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromBuffer(
      {kHeadlessResourcePackData.contents, kHeadlessResourcePackData.length},
      ui::k100Percent);
#else
  base::FilePath resource_dir;
  bool result = base::PathService::Get(base::DIR_ASSETS, &resource_dir);
  DCHECK(result);

  // Try loading the headless library pak file first. If it doesn't exist (i.e.,
  // when we're running with the --headless switch), fall back to the browser's
  // resource pak.
  base::FilePath string_pack =
      resource_dir.Append(FILE_PATH_LITERAL("headless_lib_strings.pak"));
  if (base::PathExists(string_pack)) {
    ui::ResourceBundle::InitSharedInstanceWithPakPath(string_pack);
    base::FilePath data_pack =
        resource_dir.Append(FILE_PATH_LITERAL("headless_lib_data.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        data_pack, ui::k100Percent);
    return;
  }
  const std::string locale =
      command_line.GetSwitchValueASCII(::switches::kLang);
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      locale, nullptr, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  // Otherwise, load resources.pak, chrome_100 and chrome_200.
  base::FilePath resources_pak =
      resource_dir.Append(FILE_PATH_LITERAL("resources.pak"));
  base::FilePath chrome_100_pak =
      resource_dir.Append(FILE_PATH_LITERAL("chrome_100_percent.pak"));
  base::FilePath chrome_200_pak =
      resource_dir.Append(FILE_PATH_LITERAL("chrome_200_percent.pak"));

#if BUILDFLAG(IS_MAC) && !defined(COMPONENT_BUILD)
  // In non component builds, check if fall back in Resources/ folder is
  // available.
  if (!base::PathExists(resources_pak)) {
    resources_pak =
        resource_dir.Append(FILE_PATH_LITERAL("Resources/resources.pak"));
    chrome_100_pak = resource_dir.Append(
        FILE_PATH_LITERAL("Resources/chrome_100_percent.pak"));
    chrome_200_pak = resource_dir.Append(
        FILE_PATH_LITERAL("Resources/chrome_200_percent.pak"));
  }
#endif

  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      resources_pak, ui::kScaleFactorNone);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(chrome_100_pak,
                                                              ui::k100Percent);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(chrome_200_pak,
                                                              ui::k200Percent);
#endif
}

void InitApplicationLocale(const base::CommandLine& command_line) {
  l10n_util::GetApplicationLocale(
      command_line.GetSwitchValueASCII(::switches::kLang));
}

}  // namespace

HeadlessContentMainDelegate::HeadlessContentMainDelegate(
    std::unique_ptr<HeadlessBrowserImpl> browser)
    : browser_(std::move(browser)) {
  Init();
}

HeadlessContentMainDelegate::HeadlessContentMainDelegate(
    HeadlessBrowser::Options options)
    : options_(std::make_unique<HeadlessBrowser::Options>(std::move(options))) {
  Init();
}

void HeadlessContentMainDelegate::Init() {
  DCHECK(!g_current_headless_content_main_delegate);
  g_current_headless_content_main_delegate = this;
}

HeadlessContentMainDelegate::~HeadlessContentMainDelegate() {
  DCHECK(g_current_headless_content_main_delegate == this);
  g_current_headless_content_main_delegate = nullptr;
}

absl::optional<int> HeadlessContentMainDelegate::BasicStartupComplete() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Make sure all processes know that we're in headless mode.
  if (!command_line->HasSwitch(::switches::kHeadless))
    command_line->AppendSwitch(::switches::kHeadless);

  if (options()->single_process_mode)
    command_line->AppendSwitch(::switches::kSingleProcess);

  if (options()->disable_sandbox)
    command_line->AppendSwitch(sandbox::policy::switches::kNoSandbox);

  if (!options()->enable_resource_scheduler)
    command_line->AppendSwitch(::switches::kDisableResourceScheduler);

#if BUILDFLAG(IS_OZONE)
  // The headless backend is automatically chosen for a headless build, but also
  // adding it here allows us to run in a non-headless build too.
  command_line->AppendSwitchASCII(::switches::kOzonePlatform, "headless");
#endif

  if (!command_line->HasSwitch(::switches::kUseGL) &&
      !options()->gl_implementation.empty()) {
    command_line->AppendSwitchASCII(::switches::kUseGL,
                                    options()->gl_implementation);
    if (!options()->angle_implementation.empty()) {
      command_line->AppendSwitchASCII(::switches::kUseANGLE,
                                      options()->angle_implementation);
    }
  }

  // When running headless there is no need to suppress input until content
  // is ready for display (because it isn't displayed to users). Nor is it
  // necessary to delay compositor commits in any way via PaintHolding,
  // but we disable that feature based on the --headless switch. The code is
  // in content/public/common/content_switch_dependent_feature_overrides.cc
  command_line->AppendSwitch(::blink::switches::kAllowPreCommitInput);

#if BUILDFLAG(IS_WIN)
  command_line->AppendSwitch(
      ::switches::kDisableGpuProcessForDX12InfoCollection);
#endif

  content::Profiling::ProcessStarted();
  return absl::nullopt;
}

void HeadlessContentMainDelegate::InitLogging(
    const base::CommandLine& command_line) {
  const std::string process_type =
      command_line.GetSwitchValueASCII(::switches::kProcessType);
#if !BUILDFLAG(IS_WIN)
  if (!command_line.HasSwitch(::switches::kEnableLogging))
    return;
#else
  // Child processes in Windows are not able to initialize logging.
  if (!process_type.empty())
    return;
#endif  // !BUILDFLAG(IS_WIN)

  logging::LoggingDestination log_mode;
  base::FilePath log_filename(FILE_PATH_LITERAL("chrome_debug.log"));
  if (command_line.GetSwitchValueASCII(::switches::kEnableLogging) ==
      "stderr") {
    log_mode = logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  } else {
    base::FilePath custom_filename(
        command_line.GetSwitchValuePath(::switches::kEnableLogging));
    if (custom_filename.empty()) {
      log_mode = logging::LOG_TO_ALL;
    } else {
      log_mode = logging::LOG_TO_FILE;
      log_filename = custom_filename;
    }
  }

  if (command_line.HasSwitch(::switches::kLoggingLevel) &&
      logging::GetMinLogLevel() >= 0) {
    std::string log_level =
        command_line.GetSwitchValueASCII(::switches::kLoggingLevel);
    int level = 0;
    if (base::StringToInt(log_level, &level) && level >= 0 &&
        level < logging::LOGGING_NUM_SEVERITIES) {
      logging::SetMinLogLevel(level);
    } else {
      DLOG(WARNING) << "Bad log level: " << log_level;
    }
  }

  base::FilePath log_path;
  logging::LoggingSettings settings;

// In release builds we should log into the user profile directory.
#ifdef NDEBUG
  if (!options()->user_data_dir.empty()) {
    log_path = options()->user_data_dir;
    log_path = log_path.Append(kDefaultProfileName);
    base::CreateDirectory(log_path);
    log_path = log_path.Append(log_filename);
  }
#endif  // NDEBUG

  // Otherwise we log to where the executable is.
  if (log_path.empty()) {
#if BUILDFLAG(IS_FUCHSIA)
    // TODO(crbug.com/1262330): Use the same solution as used for LOG_DIR.
    // Use -1 to allow this to compile.
    if (base::PathService::Get(-1, &log_path)) {
#else
    if (base::PathService::Get(base::DIR_MODULE, &log_path)) {
#endif
      log_path = log_path.Append(log_filename);
    } else {
      log_path = log_filename;
    }
  }

  std::string filename;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (env->GetVar(kLogFileName, &filename) && !filename.empty()) {
    log_path = base::FilePath::FromUTF8Unsafe(filename);
  }

  // On Windows, having non canonical forward slashes in log file name causes
  // problems with sandbox filters, see https://crbug.com/859676
  log_path = log_path.NormalizePathSeparators();

  settings.logging_dest = log_mode;
  settings.log_file_path = log_path.value().c_str();
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  settings.delete_old = process_type.empty() ? logging::DELETE_OLD_LOG_FILE
                                             : logging::APPEND_TO_OLD_LOG_FILE;
  bool success = logging::InitLogging(settings);
  DCHECK(success);
}

void HeadlessContentMainDelegate::InitCrashReporter(
    const base::CommandLine& command_line) {
  if (!options()->enable_crash_reporter
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      && !command_line.HasSwitch(crash_reporter::switches::kCrashpadHandlerPid)
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ) {
    return;
  }

#if BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/1226159): Implement this when crash reporting is available
  // for Fuchsia.
  NOTIMPLEMENTED();
#else
  crash_reporter::SetCrashReporterClient(g_headless_crash_client.Pointer());
  crash_reporter::InitializeCrashKeys();

  const std::string process_type =
      command_line.GetSwitchValueASCII(::switches::kProcessType);
  if (process_type != switches::kZygoteProcess) {
    g_headless_crash_client.Pointer()->set_crash_dumps_dir(
        options()->crash_dumps_dir);
#if !BUILDFLAG(IS_WIN)
    crash_reporter::InitializeCrashpad(process_type.empty(), process_type);
#endif  // !BUILDFLAG(IS_WIN)
    crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

  // Mark any bug reports from headless mode as such.
  static crash_reporter::CrashKeyString<32> headless_key(kHeadlessCrashKey);
  headless_key.Set("true");
}

void HeadlessContentMainDelegate::PreSandboxStartup() {
  const base::CommandLine& command_line(
      *base::CommandLine::ForCurrentProcess());
#if BUILDFLAG(IS_WIN)
  // Windows always needs to initialize logging, otherwise you get a renderer
  // crash.
  InitLogging(command_line);
#else
  if (command_line.HasSwitch(::switches::kEnableLogging))
    InitLogging(command_line);
#endif  // BUILDFLAG(IS_WIN)

  InitCrashReporter(command_line);

  InitializeResourceBundle(command_line);

  // Even though InitializeResourceBundle() has indirectly done the locale
  // initialization, do it again explicitly to avoid depending on the resource
  // bundle, which may go away in the future in Headless code.
  InitApplicationLocale(command_line);
}

absl::variant<int, content::MainFunctionParams>
HeadlessContentMainDelegate::RunProcess(
    const std::string& process_type,
    content::MainFunctionParams main_function_params) {
  if (!process_type.empty())
    return std::move(main_function_params);

  base::CurrentProcess::GetInstance().SetProcessType(
      base::CurrentProcessType::PROCESS_BROWSER);
  base::trace_event::TraceLog::GetInstance()->SetProcessSortIndex(
      kTraceEventBrowserProcessSortIndex);

  std::unique_ptr<content::BrowserMainRunner> browser_runner =
      content::BrowserMainRunner::Create();

  int exit_code = browser_runner->Initialize(std::move(main_function_params));
  DCHECK_LT(exit_code, 0) << "content::BrowserMainRunner::Initialize failed in "
                             "HeadlessContentMainDelegate::RunProcess";

  browser_runner->Run();
  CHECK(browser_->did_shutdown());
  browser_runner->Shutdown();
  browser_.reset();

  // Return an int here to disable calling content::BrowserMain.
  return 0;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void SIGTERMProfilingShutdown(int signal) {
  content::Profiling::Stop();
  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));
  sigact.sa_handler = SIG_DFL;
  CHECK_EQ(sigaction(SIGTERM, &sigact, NULL), 0);
  raise(signal);
}

void SetUpProfilingShutdownHandler() {
  struct sigaction sigact;
  sigact.sa_handler = SIGTERMProfilingShutdown;
  sigact.sa_flags = SA_RESETHAND;
  sigemptyset(&sigact.sa_mask);
  CHECK_EQ(sigaction(SIGTERM, &sigact, NULL), 0);
}

void HeadlessContentMainDelegate::ZygoteForked() {
  content::Profiling::ProcessStarted();
  if (content::Profiling::BeingProfiled()) {
    base::debug::RestartProfilingAfterFork();
    SetUpProfilingShutdownHandler();
  }
  const base::CommandLine& command_line(
      *base::CommandLine::ForCurrentProcess());
  if (command_line.HasSwitch(crash_reporter::switches::kCrashpadHandlerPid)) {
    const std::string process_type =
        command_line.GetSwitchValueASCII(::switches::kProcessType);
    crash_reporter::InitializeCrashpad(false, process_type);
    crash_keys::SetSwitchesFromCommandLine(command_line, nullptr);
  }
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// static
HeadlessContentMainDelegate* HeadlessContentMainDelegate::GetInstance() {
  return g_current_headless_content_main_delegate;
}

HeadlessBrowser::Options* HeadlessContentMainDelegate::options() {
  if (browser_)
    return browser_->options();
  return options_.get();
}

content::ContentClient* HeadlessContentMainDelegate::CreateContentClient() {
  return &content_client_;
}

content::ContentBrowserClient*
HeadlessContentMainDelegate::CreateContentBrowserClient() {
  browser_client_ =
      std::make_unique<HeadlessContentBrowserClient>(browser_.get());
  return browser_client_.get();
}

content::ContentRendererClient*
HeadlessContentMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<HeadlessContentRendererClient>();
  return renderer_client_.get();
}

content::ContentUtilityClient*
HeadlessContentMainDelegate::CreateContentUtilityClient() {
  utility_client_ =
      std::make_unique<HeadlessContentUtilityClient>(options()->user_agent);
  return utility_client_.get();
}

absl::optional<int> HeadlessContentMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (absl::holds_alternative<InvokedInChildProcess>(invoked_in))
    return absl::nullopt;

  if (base::FeatureList::IsEnabled(features::kVirtualTime)) {
    // Only pass viz flags into the virtual time mode.
    const char* const switches[] = {
        // TODO(eseckler): Make --run-all-compositor-stages-before-draw a
        // per-BeginFrame mode so that we can activate it for individual
        // requests
        // only. With surface sync becoming the default, we can then make
        // virtual_time_enabled a per-request option, too.
        // We control BeginFrames ourselves and need all compositing stages to
        // run.
        ::switches::kRunAllCompositorStagesBeforeDraw,
        ::switches::kDisableNewContentRenderingTimeout,
        cc::switches::kDisableThreadedAnimation,
        // Animtion-only BeginFrames are only supported when updates from the
        // impl-thread are disabled, see go/headless-rendering.
        cc::switches::kDisableCheckerImaging,
        blink::switches::kDisableThreadedScrolling,
        // Ensure that image animations don't resync their animation timestamps
        // when looping back around.
        blink::switches::kDisableImageAnimationResync,
    };
    for (const auto* flag : switches)
      base::CommandLine::ForCurrentProcess()->AppendSwitch(flag);
  }

  return absl::nullopt;
}

}  // namespace headless
