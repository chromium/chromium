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
#include "components/devtools/devtools_pipe/devtools_pipe.h"
#include "components/viz/common/switches.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/common/profiling.h"
#include "gpu/config/gpu_switches.h"
#include "headless/lib/browser/command_line_handler.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/lib/browser/headless_content_browser_client.h"
#include "headless/lib/headless_crash_reporter_client.h"
#include "headless/lib/renderer/headless_content_renderer_client.h"
#include "headless/lib/utility/headless_content_utility_client.h"
#include "headless/public/switches.h"
#include "sandbox/policy/switches.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/dark_mode_support.h"
#include "base/win/resource_exhaustion.h"
#endif  // BUILDFLAG(IS_WIN)

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

#if defined(HEADLESS_USE_PREFS)
#include "components/prefs/pref_service.h"
#endif

#if defined(HEADLESS_SUPPORT_FIELD_TRIALS)
#include "content/public/app/initialize_mojo_core.h"
#include "headless/lib/browser/headless_field_trials.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
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

#if BUILDFLAG(IS_WIN)
void OnResourceExhausted() {
  // RegisterClassEx will fail if the session's pool of ATOMs is exhausted. This
  // appears to happen most often when the browser is being driven by automation
  // tools, though the underlying reason for this remains a mystery
  // (https://crbug.com/1470483). There is nothing that Chrome can do to
  // meaningfully run until the user restarts their session by signing out of
  // Windows or restarting their computer.
  LOG(ERROR) << "Your computer has run out of resources. "
                "Sign out of Windows or restart your computer and try again.";
  base::Process::TerminateCurrentProcessImmediately(EXIT_FAILURE);
}
#endif  // BUILDFLAG(IS_WIN)

void InitializeResourceBundle(const base::CommandLine& command_line) {
#if defined(HEADLESS_USE_EMBEDDED_RESOURCES)
  ui::ResourceBundle::InitSharedInstanceWithBuffer(kHeadlessResourcePackStrings,
                                                   ui::kScaleFactorNone);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromBuffer(
      kHeadlessResourcePackData, ui::k100Percent);
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

void AddSwitchesForVirtualTime() {
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
      // Ensure that image animations don't resync their animation timestamps
      // when looping back around.
      blink::switches::kDisableImageAnimationResync,
  };

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  for (const auto* flag : switches) {
    command_line->AppendSwitch(flag);
  }
}

}  // namespace

HeadlessContentMainDelegate::HeadlessContentMainDelegate(
    std::unique_ptr<HeadlessBrowserImpl> browser)
    : browser_(std::move(browser)) {
  DCHECK(!g_current_headless_content_main_delegate);
  g_current_headless_content_main_delegate = this;
}

HeadlessContentMainDelegate::~HeadlessContentMainDelegate() {
  DCHECK(g_current_headless_content_main_delegate == this);
  g_current_headless_content_main_delegate = nullptr;
}

std::optional<int> HeadlessContentMainDelegate::BasicStartupComplete() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  content::Profiling::ProcessStarted();

  // Note that on platforms where zygotes are used, this method is invoked
  // before the zygote fork, so whatever switches are modified here get
  // overridden when zygote forks, potentially causing differences with
  // regard to the platforms that don't use zygotes. Therefore, we don't
  // want this method to ever alter command line in child processes and
  // only rely on flags set in the browsers getting propagated to children
  // via regular flag propagation means. See crbug.com/338414704 for context.
  if (command_line->HasSwitch(::switches::kProcessType)) {
    return std::nullopt;
  }
  // The DevTools remote debugging pipe file descriptors need to be checked
  // before any other files are opened, see https://crbug.com/1423048.
#if BUILDFLAG(IS_WIN)
  const bool pipes_are_specified_explicitly =
      command_line->HasSwitch(::switches::kRemoteDebuggingIoPipes);
#else
  const bool pipes_are_specified_explicitly = false;
#endif
  if (command_line->HasSwitch(::switches::kRemoteDebuggingPipe) &&
      !pipes_are_specified_explicitly &&
      !devtools_pipe::AreFileDescriptorsOpen()) {
    LOG(ERROR) << "Remote debugging pipe file descriptors are not open.";
    return EXIT_FAILURE;
  }

  // Make sure all processes know that we're in headless mode.
  if (!command_line->HasSwitch(::switches::kHeadless)) {
    command_line->AppendSwitchASCII(::switches::kHeadless, "old");
  }

  // Use software rendering by default, but don't mess with gl and angle
  // switches if user is overriding them.
  if (!command_line->HasSwitch(::switches::kUseGL) &&
      !command_line->HasSwitch(::switches::kUseANGLE) &&
      !command_line->HasSwitch(switches::kEnableGPU)) {
    command_line->AppendSwitchASCII(::switches::kUseGL,
                                    gl::kGLImplementationANGLEName);
    command_line->AppendSwitchASCII(
        ::switches::kUseANGLE, gl::kANGLEImplementationSwiftShaderForWebGLName);
  }
#if BUILDFLAG(IS_OZONE)
  // The headless backend is automatically chosen for a headless build, but also
  // adding it here allows us to run in a non-headless build too.
  command_line->AppendSwitchASCII(::switches::kOzonePlatform, "headless");
#endif

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
  return std::nullopt;
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
  base::FilePath user_data_dir =
      command_line.GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty()) {
    log_path = user_data_dir.Append(kDefaultProfileName);
    base::CreateDirectory(log_path);
    log_path = log_path.Append(log_filename);
  }
#endif  // NDEBUG

  // Otherwise we log to where the executable is.
  if (log_path.empty()) {
#if BUILDFLAG(IS_FUCHSIA)
    // TODO(crbug.com/40202595): Use the same solution as used for LOG_DIR.
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
  const std::string process_type =
      command_line.GetSwitchValueASCII(::switches::kProcessType);
  bool enable_crash_reporter =
      process_type.empty() &&
      command_line.HasSwitch(switches::kEnableCrashReporter) &&
      !command_line.HasSwitch(switches::kDisableCrashReporter);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  enable_crash_reporter |=
      command_line.HasSwitch(crash_reporter::switches::kCrashpadHandlerPid);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  if (!enable_crash_reporter) {
    return;
  }

#if BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/40188745): Implement this when crash reporting is available
  // for Fuchsia.
  NOTIMPLEMENTED();
#else
  crash_reporter::SetCrashReporterClient(g_headless_crash_client.Pointer());
  crash_reporter::InitializeCrashKeys();

  if (process_type != ::switches::kZygoteProcess) {
    g_headless_crash_client.Pointer()->set_crash_dumps_dir(
        command_line.GetSwitchValuePath(switches::kCrashDumpsDir));
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

  int result_code = browser_runner->Initialize(std::move(main_function_params));
  DCHECK_LT(result_code, 0)
      << "content::BrowserMainRunner::Initialize failed in "
         "HeadlessContentMainDelegate::RunProcess";

  browser_runner->Run();
  browser_runner->Shutdown();

  return browser_->exit_code();
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

std::optional<int> HeadlessContentMainDelegate::PreBrowserMain() {
  HeadlessBrowser::Options::Builder builder;

  if (!HandleCommandLineSwitches(*base::CommandLine::ForCurrentProcess(),
                                 builder)) {
    return EXIT_FAILURE;
  }
  browser_->SetOptions(builder.Build());

#if BUILDFLAG(IS_WIN)
  // Register callback to handle resource exhaustion.
  base::win::SetOnResourceExhaustedFunction(&OnResourceExhausted);
#endif

#if BUILDFLAG(IS_MAC)
  PlatformPreBrowserMain();
#endif
  return std::nullopt;
}

#if BUILDFLAG(IS_WIN)
bool HeadlessContentMainDelegate::ShouldHandleConsoleControlEvents() {
  // Handle console control events so that orderly shutdown can be performed by
  // HeadlessContentBrowserClient's override of SessionEnding.
  return true;
}
#endif

content::ContentClient* HeadlessContentMainDelegate::CreateContentClient() {
  return &content_client_;
}

content::ContentBrowserClient*
HeadlessContentMainDelegate::CreateContentBrowserClient() {
  DCHECK(browser_);
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
  utility_client_ = std::make_unique<HeadlessContentUtilityClient>();
  return utility_client_.get();
}

std::optional<int> HeadlessContentMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  if (absl::holds_alternative<InvokedInChildProcess>(invoked_in))
    return std::nullopt;

#if defined(HEADLESS_USE_PREFS)
  browser_->CreatePrefService();
#endif

#if defined(HEADLESS_SUPPORT_FIELD_TRIALS)
  // Check if we're telling content to not to create the feature list and do it
  // here if so. Content can create default feature list on its own however here
  // we want the feature list to be created by field trial machinery.
  if (!ShouldCreateFeatureList(invoked_in)) {
    SetUpFieldTrials(browser()->GetPrefs(),
                     browser()->options()->user_data_dir);
    // Schedule a Local State write since the above function may have resulted
    // in some prefs being updated. Headless shell runs are typically short and
    // often end in crashes, so it helps to commit early.
    browser_->GetPrefs()->CommitPendingWrite();
  }

  // Check if we're telling content to not to initialize Mojo and do it here
  // since we want it do be done after the feature list is created.
  if (!ShouldInitializeMojo(invoked_in)) {
    content::InitializeMojoCore();
  }
#endif  // defined(HEADLESS_SUPPORT_FIELD_TRIALS)

  if (base::FeatureList::IsEnabled(features::kVirtualTime)) {
    AddSwitchesForVirtualTime();
  }

#if BUILDFLAG(IS_WIN)
  // Make sure that 'uxtheme.dll' is pinned before blocking on the main thread
  // is disallowed; see https://crbug.com/368388543#comment11.
  base::win::IsDarkModeAvailable();
#endif  // BUILDFLAG(IS_WIN)

  return std::nullopt;
}

#if defined(HEADLESS_SUPPORT_FIELD_TRIALS)
bool HeadlessContentMainDelegate::ShouldCreateFeatureList(
    InvokedIn invoked_in) {
  // The content layer is always responsible for creating the FeatureList in
  // child processes.
  if (absl::holds_alternative<InvokedInChildProcess>(invoked_in)) {
    return true;
  }

  // VariationsFieldTrialCreator::SetUpFieldTrials() instantiates its own
  // feature list so prevent content from instantiating a default one if we're
  // going to set up field trials.
  return !ShouldEnableFieldTrials();
}

bool HeadlessContentMainDelegate::ShouldInitializeMojo(InvokedIn invoked_in) {
  // Mojo cannot be initialized without a feature list instance available so
  // postpone its initialization until after feature list is instantiated by
  // field trials setup if field trials are enabled.
  return ShouldCreateFeatureList(invoked_in);
}
#endif  // defined(HEADLESS_SUPPORT_FIELD_TRIALS)

}  // namespace headless
