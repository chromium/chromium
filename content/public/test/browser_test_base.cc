// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_base.h"

#include <fcntl.h>
#include <stddef.h>

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/callback_list.h"
#include "base/command_line.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/variations/variations_ids_provider.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/network_service_instance_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/scheduler/browser_task_executor.h"
#include "content/browser/startup_data_impl.h"
#include "content/browser/startup_helper.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/tracing/background_tracing_manager_impl.h"
#include "content/browser/tracing/memory_instrumentation_util.h"
#include "content/browser/tracing/startup_tracing_controller.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/app/content_main.h"
#include "content/public/app/initialize_mojo_core.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/main_function_params.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "content/test/content_browser_consistency_checker.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "net/dns/mock_host_resolver.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "net/dns/public/secure_dns_mode.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/tracing/public/cpp/trace_startup.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/platform_window/common/platform_window_defaults.h"  // nogncheck
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/task_scheduler/post_task_android.h"
#include "components/discardable_memory/service/discardable_shared_memory_manager.h"  // nogncheck
#include "content/app/content_main_runner_impl.h"
#include "content/app/mojo/mojo_init.h"
#include "content/app/mojo_ipc_support.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/common/content_paths.h"
#include "testing/android/native_test/native_browser_test_support.h"
#include "ui/base/ui_base_paths.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "content/browser/sandbox_parameters_mac.h"
#include "net/test/test_data_directory.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/event_generator_delegate_mac.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include "base/process/process_handle.h"
#endif

#if defined(USE_AURA)
#include "content/browser/compositor/image_transport_factory.h"
#include "ui/aura/test/event_generator_delegate_aura.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/files/scoped_file.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"  // nogncheck
#include "chromeos/startup/browser_params_proxy.h"
#include "chromeos/startup/startup_switches.h"  // nogncheck
#include "mojo/public/cpp/platform/socket_utils_posix.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/system_info.h"
#include "ui/platform_window/fuchsia/initialize_presenter_api_view.h"
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
#include <shlobj.h>

#include "base/files/file_util.h"
#include "base/test/test_reg_util_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace content {

namespace {

// Whether an instance of BrowserTestBase has already been created in this
// process. Browser tests should each be run in a new process.
bool g_instance_already_created = false;

#if BUILDFLAG(IS_POSIX)
// On SIGSEGV or SIGTERM (sent by the runner on timeouts), dump a stack trace
// (to make debugging easier) and also exit with a known error code (so that
// the test framework considers this a failure -- http://crbug.com/57578).
// Note: We only want to do this in the browser process, and not forked
// processes. That might lead to hangs because of locks inside the OS.
// See http://crbug.com/141302.
int g_browser_process_pid;

// A shutdown function set on signal callback registration.
base::OnceCallback<void(int)> ShutdownHandler;

void SignalHandler(int signal) {
  std::move(ShutdownHandler).Run(signal);

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableInProcessStackTraces) &&
      g_browser_process_pid == base::GetCurrentProcId()) {
    std::string message("BrowserTestBase received signal: ");
    message += strsignal(signal);
    message += ". Backtrace:\n";
    logging::RawLog(logging::LOGGING_ERROR, message.c_str());
    auto stack_trace = base::debug::StackTrace();
    stack_trace.OutputToStream(&std::cerr);
#if BUILDFLAG(IS_ANDROID)
    // Also output the trace to logcat on Android.
    stack_trace.Print();
#endif
  }
  _exit(128 + signal);
}
#endif  // BUILDFLAG(IS_POSIX)

void RunTaskOnRendererThread(base::OnceClosure task,
                             base::OnceClosure quit_task) {
  std::move(task).Run();
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(quit_task));
}

enum class TraceBasenameType {
  kWithoutTestStatus,
  kWithTestStatus,
};

std::string GetDefaultTraceBasename(TraceBasenameType type) {
  const testing::TestInfo* test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();

  // A default is required in case we are in a fuzz test or something else
  // without gtest.
  std::string test_suite_name = "<unknown>";
  std::string test_name = "<unknown>";
  if (test_info) {
    test_suite_name = test_info->test_suite_name();
    test_name = test_info->name();
  }
  // Parameterised tests might have slashes in their full name — replace them
  // before using it as a file name to avoid trying to write to an incorrect
  // location.
  base::ReplaceChars(test_suite_name, "/", "_", &test_suite_name);
  base::ReplaceChars(test_name, "/", "_", &test_name);
  // Add random number to the trace file to distinguish traces from different
  // test runs. We don't use timestamp here to avoid collisions with parallel
  // runs of the same test. Browser test runner runs one test per browser
  // process instantiation, so saving the seed here is appopriate.
  // GetDefaultTraceBasename() is going to be called twice:
  // - for the first time, before the test starts to get the name of the file to
  // stream the results (to avoid losing them if test crashes).
  // - the second time, if test execution finishes normally, to calculate the
  // resulting name of the file, including test result.
  static std::string random_seed =
      base::NumberToString(base::RandInt(1e7, 1e8 - 1));
  std::string status;
  if (type == TraceBasenameType::kWithTestStatus) {
    if (test_info) {
      status = test_info->result()->Passed() ? "OK" : "FAIL";
    } else {
      status = "UNKNOWN";  // for fuzz tests only, not functional tests
    }
  } else {
    // In order to be able to stream the test to the file,
    status = "NOT_FINISHED";
  }
  return "trace_test_" + test_suite_name + "_" + test_name + "_" + random_seed +
         "_" + status;
}

// See SetInitialWebContents comment for more information.
class InitialNavigationObserver : public WebContentsObserver {
 public:
  InitialNavigationObserver(WebContents* web_contents,
                            base::OnceClosure callback)
      : WebContentsObserver(web_contents), callback_(std::move(callback)) {}

  InitialNavigationObserver(const InitialNavigationObserver&) = delete;
  InitialNavigationObserver& operator=(const InitialNavigationObserver&) =
      delete;

  // WebContentsObserver implementation:
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    if (callback_)
      std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

}  // namespace

BrowserTestBase::BrowserTestBase() {
  CHECK(!g_instance_already_created)
      << "Each browser test should be run in a new process. If you are adding "
         "a new browser test suite that runs on Android, please add it to "
         "//build/android/pylib/gtest/gtest_test_instance.py.";
  g_instance_already_created = true;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  ui::test::EnableTestConfigForPlatformWindows();
#endif

#if BUILDFLAG(IS_POSIX)
  handle_sigterm_ = true;
#endif

#if BUILDFLAG(IS_WIN)
  // Disallow overriding HKLM during browser test startup. This is because it
  // will interfere with process launches, which rely on there being a valid
  // HKLM. This functionality is restored just before the test fixture itself
  // starts in ProxyRunTestOnMainThreadLoop, after browser startup has been
  // completed.
  registry_util::RegistryOverrideManager::
      SetAllowHKLMRegistryOverrideForIntegrationTests(/*allow=*/false);
#endif

  embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>();

#if BUILDFLAG(IS_WIN)
  // Even if running as admin, browser tests should not write temp files to
  // secure temp, otherwise any left-over files cannot be cleaned up by the test
  // runner.
  if (::IsUserAnAdmin()) {
    system_temp_override_.emplace(base::DIR_SYSTEM_TEMP,
                                  base::PathService::CheckedGet(base::DIR_TEMP),
                                  /*is_absolute=*/true, /*create=*/false);
  }
#endif  // BUILDFLAG(IS_WIN)

#if defined(USE_AURA)
  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      base::BindRepeating(&aura::test::EventGeneratorDelegateAura::Create));
#elif BUILDFLAG(IS_MAC)
  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      base::BindRepeating(&views::test::CreateEventGeneratorDelegateMac));
  EnableNativeWindowActivation();
#endif
}

BrowserTestBase::~BrowserTestBase() {
  CHECK(set_up_called_ || IsSkipped() || HasFatalFailure())
      << "SetUp was not called. This probably means that the "
         "developer has overridden the method and not called "
         "the superclass version. In this case, the test "
         "does not run and reports a false positive result.";
}

void BrowserTestBase::SetUp() {
  set_up_called_ = true;

  if (!UseProductionQuotaSettings()) {
    // By default use hardcoded quota settings to have a consistent testing
    // environment.
    const int kQuota = 5 * 1024 * 1024;
    quota_settings_ =
        std::make_unique<storage::QuotaSettings>(kQuota * 5, kQuota, 0, 0);
    StoragePartitionImpl::SetDefaultQuotaSettingsForTesting(
        quota_settings_.get());
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  if (!command_line->HasSwitch(switches::kUseFakeDeviceForMediaStream))
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);

  // Features that depend on external factors (e.g. memory pressure monitor) can
  // disable themselves based on the switch below (to ensure that browser tests
  // behave deterministically / do not flakily change behavior based on external
  // factors).
  command_line->AppendSwitch(switches::kBrowserTest);

  // Override the child process connection timeout since tests can exceed that
  // when sharded.
  command_line->AppendSwitchASCII(
      switches::kIPCConnectionTimeout,
      base::NumberToString(TestTimeouts::action_max_timeout().InSeconds()));

  // Useful when debugging test failures.
  command_line->AppendSwitch(switches::kLogMissingUnloadACK);

  command_line->AppendSwitch(switches::kDomAutomationController);

  // It is sometimes useful when looking at browser test failures to know which
  // GPU blocklist decisions were made.
  command_line->AppendSwitch(switches::kLogGpuControlListDecisions);

  // Make sure software compositing tests don't attempt to force hardware
  // compositing.
  if (use_software_compositing_) {
    command_line->AppendSwitch(switches::kDisableGpu);
    command_line->RemoveSwitch(switches::kDisableSoftwareCompositingFallback);
  }

  // The layout of windows on screen is unpredictable during tests, so disable
  // occlusion when running browser tests.
  command_line->AppendSwitch(
      switches::kDisableBackgroundingOccludedWindowsForTesting);

  if (enable_pixel_output_) {
    DCHECK(!command_line->HasSwitch(switches::kForceDeviceScaleFactor))
        << "--force-device-scale-factor flag already present. Tests using "
        << "EnablePixelOutput should specify a forced device scale factor by "
        << "passing it as an argument to EnblePixelOutput.";
    DCHECK(force_device_scale_factor_);

    // We do this before setting enable_pixel_output_ from the switch below so
    // that the device scale factor is forced only when enabled from test code.
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", force_device_scale_factor_));
  }

#if defined(USE_AURA)
  // Most tests do not need pixel output, so we don't produce any. The command
  // line can override this behaviour to allow for visual debugging.
  if (command_line->HasSwitch(switches::kEnablePixelOutputInTests))
    enable_pixel_output_ = true;

  if (command_line->HasSwitch(switches::kDisableGLDrawingForTests)) {
    NOTREACHED_IN_MIGRATION()
        << "kDisableGLDrawingForTests should not be used as it "
           "is chosen by tests. Use kEnablePixelOutputInTests "
           "to enable pixel output.";
  }

  // Don't enable pixel output for browser tests unless they override and force
  // us to, or it's requested on the command line.
  if (!enable_pixel_output_ && !use_software_compositing_)
    command_line->AppendSwitch(switches::kDisableGLDrawingForTests);
#endif

  // Disable animations when verifying pixel output, as they make tests flaky.
  if (command_line->HasSwitch(switches::kVerifyPixels)) {
    disable_layer_animations_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
    disable_rich_animations_ =
        gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
  }

  bool use_software_gl = true;

  // We usually use software GL as this works on all bots. The command
  // line can override this behaviour to use hardware GL.
  if (command_line->HasSwitch(switches::kUseGpuInTests))
    use_software_gl = false;

  // Some bots pass this flag when they want to use hardware GL.
  if (command_line->HasSwitch("enable-gpu"))
    use_software_gl = false;

#if BUILDFLAG(IS_APPLE)
  // On Apple we always use hardware GL.
  use_software_gl = false;
#endif

#if BUILDFLAG(IS_MAC)
  // Expand the network service sandbox to allow reading the test TLS
  // certificates.
  SetNetworkTestCertsDirectoryForTesting(net::GetTestCertsDirectory());
#endif

#if BUILDFLAG(IS_ANDROID)
  // On Android we always use hardware GL.
  use_software_gl = false;
#endif

#if BUILDFLAG(IS_FUCHSIA)
  // GPU support is not available to tests.
  // TODO(crbug.com/40797662): Enable GPU support.
  command_line->AppendSwitch(switches::kDisableGpu);

  ui::fuchsia::IgnorePresentCallsForTest();

  // Clear the per-process cached system info, which was initialized by
  // TestSuite::Initialize(), to prevent a DCHECK for multiple calls during
  // in-process browser tests. There is not a single TestSuite for all browser
  // tests and some use the cached values, so skipping the earlier
  // initialization is not an option.
  base::ClearCachedSystemInfoForTesting();
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If the test is running on the lacros environment, a file descriptor needs
  // to be obtained and used to launch lacros-chrome so that a mojo connection
  // between lacros-chrome and ash-chrome can be established.
  // For more details, please see:
  // //chrome/browser/ash/crosapi/test_mojo_connection_manager.h.
  {
    if (!chromeos::BrowserParamsProxy::Get()->IsCrosapiDisabledForTesting()) {
      // TODO(crbug.com/40719121): Switch to use |kLacrosMojoSocketForTesting|
      // in
      // //ash/constants/ash_switches.h.
      // Please refer to the CL comments for why it can't be done now:
      // http://crrev.com/c/2402580/2/content/public/test/browser_test_base.cc
      CHECK(command_line->HasSwitch("lacros-mojo-socket-for-testing"));
      std::string socket_path =
          command_line->GetSwitchValueASCII("lacros-mojo-socket-for-testing");
      auto channel = mojo::NamedPlatformChannel::ConnectToServer(socket_path);
      base::ScopedFD socket_fd = channel.TakePlatformHandle().TakeFD();

      // Mark the channel as blocking.
      int flags = fcntl(socket_fd.get(), F_GETFL);
      std::string helper_msg =
          "On bot, open CAS outputs on test result page(Milo),"
          "there is a ash_chrome.log file which contains ash log."
          "For local debugging, pass in --ash-logging-path to test runner.";
      PCHECK(flags != -1) << "Ash is probably not running. Perhaps it crashed?"
                          << helper_msg;
      fcntl(socket_fd.get(), F_SETFL, flags & ~O_NONBLOCK);

      uint8_t buf[32];
      std::vector<base::ScopedFD> descriptors;
      auto size = mojo::SocketRecvmsg(socket_fd.get(), buf, sizeof(buf),
                                      &descriptors, true /*block*/);
      if (size < 0)
        PLOG(ERROR) << "Error receiving message from the socket" << helper_msg;

      ASSERT_EQ(1, size) << "It must receive a version number with 1 byte.";
      ASSERT_EQ(buf[0], 1u)
          << "Mojo connection protocol version must be 1. Version 0 is "
          << "deprecated.";
      ASSERT_EQ(descriptors.size(), 2u)
          << "ash-chrome must sends 2 FDs, the first one contains startup data "
          << "and the second one is for a crosapi Mojo connection.";

      // Ok to release the FD here, too.
      command_line->AppendSwitchASCII(
          chromeos::switches::kCrosStartupDataFD,
          base::NumberToString(descriptors[0].release()));
      command_line->AppendSwitchASCII(
          crosapi::kCrosapiMojoPlatformChannelHandle,
          base::NumberToString(descriptors[1].release()));
    }
  }
#endif

  if (use_software_gl && !use_software_compositing_)
    command_line->AppendSwitch(switches::kOverrideUseSoftwareGLForTests);

  // Use an sRGB color profile to ensure that the machine's color profile does
  // not affect the results.
  command_line->AppendSwitchASCII(switches::kForceDisplayColorProfile, "srgb");

  if (!allow_network_access_to_host_resolutions_)
    test_host_resolver_ = std::make_unique<TestHostResolver>();

  ContentBrowserConsistencyChecker scoped_enable_consistency_checks;

  SetUpInProcessBrowserTestFixture();

  // Should not use CommandLine to modify features. Please use ScopedFeatureList
  // instead.
  DCHECK(!command_line->HasSwitch(switches::kEnableFeatures));
  DCHECK(!command_line->HasSwitch(switches::kDisableFeatures));

  // At this point, copy features to the command line, since BrowserMain will
  // wipe out the current feature list.
  std::string enabled_features;
  std::string disabled_features;
  if (base::FeatureList::GetInstance()) {
    base::FeatureList::GetInstance()->GetFeatureOverrides(&enabled_features,
                                                          &disabled_features);
  }

  if (!enabled_features.empty()) {
    command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                    enabled_features);
  }
  if (!disabled_features.empty()) {
    command_line->AppendSwitchASCII(switches::kDisableFeatures,
                                    disabled_features);
  }

  // Always disable the unsandbox GPU process for DX12 Info collection to avoid
  // interference. This GPU process is launched 120 seconds after chrome starts.
  command_line->AppendSwitch(switches::kDisableGpuProcessForDX12InfoCollection);

  // The current global field trial list contains any trials that were activated
  // prior to main browser startup. That global field trial list is about to be
  // destroyed below, and will be recreated during the browser_tests browser
  // process startup code. Pass the currently active trials to the subsequent
  // list via the command line.
  std::string field_trial_states;
  base::FieldTrialList::AllStatesToString(&field_trial_states);
  if (!field_trial_states.empty()) {
    // Please use ScopedFeatureList to modify feature and field trials at the
    // same time.
    DCHECK(!command_line->HasSwitch(switches::kForceFieldTrials));
    command_line->AppendSwitchASCII(switches::kForceFieldTrials,
                                    field_trial_states);
  }

  // Need to wipe feature list clean, since BrowserMain calls
  // FeatureList::SetInstance, which expects no instance to exist.
  base::FeatureList::ClearInstanceForTesting();

  auto created_main_parts_closure = base::BindOnce(
      &BrowserTestBase::CreatedBrowserMainPartsImpl, base::Unretained(this));

  // If tracing is enabled, customise the output filename based on the name of
  // the test.
  StartupTracingController::GetInstance().SetDefaultBasename(
      GetDefaultTraceBasename(TraceBasenameType::kWithoutTestStatus),
      StartupTracingController::ExtensionType::kAppendAppropriate);
  // Write to the provided file directly to recover at least some data when the
  // test crashes or times out.
  StartupTracingController::GetInstance().SetUsingTemporaryFile(
      StartupTracingController::TempFilePolicy::kWriteDirectly);
  // Set a logging handler to flush a trace before crashing the test when
  // hitting a DCHECK / LOG(FATAL).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableTracing)) {
    DCHECK(!logging::GetLogMessageHandler());
    logging::SetLogMessageHandler([](int severity, const char* file, int line,
                                     size_t message_start,
                                     const std::string& str) {
      // TODO(crbug.com/40161080): Print the message to the console before
      // calling this to ensure that the message is still printed if something
      // goes wrong.
      if (severity == logging::LOGGING_FATAL)
        StartupTracingController::EmergencyStop();
      return false;
    });
  }

  auto content_main_params = CopyContentMainParams();
  content_main_params.created_main_parts_closure =
      std::move(created_main_parts_closure);
  content_main_params.ui_task = base::BindOnce(
      &BrowserTestBase::ProxyRunTestOnMainThreadLoop, base::Unretained(this));

  ContentMainDelegate* overridden_delegate =
      GetOptionalContentMainDelegateOverride();
  if (overridden_delegate)
    content_main_params.delegate = overridden_delegate;

#if !BUILDFLAG(IS_ANDROID)
  // ContentMain which goes through the normal browser initialization paths
  // and will invoke `content_main_params.ui_task`, which runs the test.
  EXPECT_EQ(expected_exit_code_, ContentMain(std::move(content_main_params)));
#else
  // Android's equivalent of ContentMain is in Java so browser tests must set
  // things up manually. A meager re-implementation of ContentMainRunnerImpl
  // follows.

  // Unlike other platforms, android_browsertests can reuse the same process for
  // multiple tests. Need to reset startup metrics to allow recording them
  // again.
  startup_metric_utils::GetBrowser().ResetSessionForTesting();

  // The ContentMainDelegate and ContentClient should have been set by
  // JNI_OnLoad for the test target.
  ContentMainDelegate* delegate = content_main_params.delegate;
  ASSERT_TRUE(delegate);
  ASSERT_TRUE(GetContentClientForTesting());

  delegate->CreateThreadPool("Browser");

  std::optional<int> startup_error = delegate->BasicStartupComplete();
  ASSERT_FALSE(startup_error.has_value());

  // We can only setup startup tracing after mojo is initialized above.
  tracing::EnableStartupTracingIfNeeded();

  {
    ContentClient::SetBrowserClientAlwaysAllowForTesting(
        delegate->CreateContentBrowserClient());
    if (command_line->HasSwitch(switches::kSingleProcess))
      SetRendererClientForTesting(delegate->CreateContentRendererClient());

    content::RegisterPathProvider();
    ui::RegisterPathProvider();

    delegate->PreSandboxStartup();
    delegate->SandboxInitialized("");

    const ContentMainDelegate::InvokedInBrowserProcess invoked_in_browser{
        .is_running_test = true};
    DCHECK(!field_trial_list_);
    if (delegate->ShouldCreateFeatureList(invoked_in_browser))
      field_trial_list_ = SetUpFieldTrialsAndFeatureList();
    if (delegate->ShouldInitializeMojo(invoked_in_browser))
      InitializeMojoCore();

    std::optional<int> pre_browser_main_exit_code = delegate->PreBrowserMain();
    ASSERT_FALSE(pre_browser_main_exit_code.has_value());

    BrowserTaskExecutor::Create();

    auto* provider = delegate->CreateVariationsIdsProvider();
    if (!provider) {
      variations::VariationsIdsProvider::Create(
          variations::VariationsIdsProvider::Mode::kUseSignedInState);
    }

    std::optional<int> post_early_initialization_exit_code =
        delegate->PostEarlyInitialization(invoked_in_browser);
    ASSERT_FALSE(post_early_initialization_exit_code.has_value());

    StartBrowserThreadPool();

    BrowserTaskExecutor::PostFeatureListSetup();
    tracing::InitTracingPostThreadPoolStartAndFeatureList(
        /* enable_consumer */ true);
    InitializeBrowserMemoryInstrumentationClient();
  }

  blink::TrialTokenValidator::SetOriginTrialPolicyGetter(
      base::BindRepeating([]() -> blink::OriginTrialPolicy* {
        ContentClient* client = GetContentClientForTesting();
        return client ? client->GetOriginTrialPolicy() : nullptr;
      }));

  // All FeatureList overrides should have been registered prior to browser test
  // SetUp().
  base::FeatureList::ScopedDisallowOverrides disallow_feature_overrides(
      "FeatureList overrides must happen in the test constructor, before "
      "BrowserTestBase::SetUp() has run.");

  auto discardable_shared_memory_manager =
      std::make_unique<discardable_memory::DiscardableSharedMemoryManager>();
  auto ipc_support =
      std::make_unique<MojoIpcSupport>(BrowserTaskExecutor::CreateIOThread());
  std::unique_ptr<StartupDataImpl> startup_data =
      ipc_support->CreateBrowserStartupData();

  // ContentMain would normally call RunProcess() on the delegate and fallback
  // to BrowserMain() if it did not run it (or equivalent) itself. On Android,
  // RunProcess() will return 0 so we don't have to fallback to BrowserMain().
  {
    // This loop will wait until Java completes async initializion and the test
    // is ready to run. We must allow nestable tasks so that tasks posted to the
    // UI thread run as well. The loop is created before RunProcess() so that
    // the StartupTaskRunner tasks will be nested inside this loop and able to
    // run.
    base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};

    // The MainFunctionParams must out-live all the startup tasks running.
    MainFunctionParams params(command_line);
    params.created_main_parts_closure =
        std::move(content_main_params.created_main_parts_closure);
    params.startup_data = std::move(startup_data);
    params.ui_task = base::BindOnce(&BrowserTestBase::WaitUntilJavaIsReady,
                                    base::Unretained(this), loop.QuitClosure(),
                                    /*wait_retry_left=*/
                                    TestTimeouts::action_max_timeout());
    // Passing "" as the process type to indicate the browser process.
    auto exit_code = delegate->RunProcess("", std::move(params));
    DCHECK(absl::holds_alternative<int>(exit_code));
    DCHECK_EQ(absl::get<int>(exit_code), 0);

    // Waits for Java to finish initialization, then we can run the test.
    loop.Run();

    // The BrowserMainLoop startup tasks will call DisallowUnresponsiveTasks().
    // So when we run the ProxyRunTestOnMainThreadLoop() we no longer can block,
    // but tests should be allowed to. So we undo that blocking inside here.
    base::ScopedAllowUnresponsiveTasksForTesting allow_unresponsive;
    // Runs the test now that the Java setup is complete. The closure must be
    // invoked directly from the same call stack as RUN_ALL_TESTS(), it may not
    // be inside a posted task, or it would prevent NonNestable tasks from
    // running inside tests.
    std::move(content_main_params.ui_task).Run();
  }

  {
    base::ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
    // Shutting these down will block the thread.
    ShutDownNetworkService();
    ipc_support.reset();
    discardable_shared_memory_manager.reset();
  }

  // Like in BrowserMainLoop::ShutdownThreadsAndCleanUp(), allow IO during main
  // thread tear down.
  base::PermanentThreadAllowance::AllowBlocking();

  BrowserTaskExecutor::Shutdown();
#endif  // BUILDFLAG(IS_ANDROID)

  TearDownInProcessBrowserTestFixture();
}

void BrowserTestBase::TearDown() {
  if (embedded_test_server()->Started())
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      ui::test::EventGeneratorDelegate::FactoryFunction());
#endif

  StoragePartitionImpl::SetDefaultQuotaSettingsForTesting(nullptr);
}

bool BrowserTestBase::UseProductionQuotaSettings() {
  return false;
}

void BrowserTestBase::SimulateNetworkServiceCrash() {
  CHECK(!IsInProcessNetworkService())
      << "Can't crash the network service if it's running in-process!";

  // Check if any unexpected crashes have occurred *before* the expected crash
  // that we will trigger/simulate below.
  AssertThatNetworkServiceDidNotCrash();

  // `network_service_test_` field might not be ready yet - some tests call
  // SimulateNetworkServiceCrash from SetUpOnMainThread, before
  // InitializeNetworkProcess has been called.
  mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
  content::GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  network_service_test.set_disconnect_handler(run_loop.QuitClosure());

  network_service_test->SimulateCrash();
  run_loop.Run();

  // Make sure the cached mojo::Remote<NetworkService> receives error
  // notification.
  FlushNetworkServiceInstanceForTesting();

  // Need to re-initialize the network process.
  ForceInitializeNetworkProcess();
}

void BrowserTestBase::IgnoreNetworkServiceCrashes() {
  network_service_test_.reset();
}

#if BUILDFLAG(IS_ANDROID)
void BrowserTestBase::WaitUntilJavaIsReady(
    base::OnceClosure quit_closure,
    const base::TimeDelta& wait_retry_left) {
  CHECK_GE(wait_retry_left.InMilliseconds(), 0)
      << "WaitUntilJavaIsReady() timed out.";

  if (testing::android::JavaAsyncStartupTasksCompleteForBrowserTests()) {
    std::move(quit_closure).Run();
    return;
  }

  base::TimeDelta retry_interval = base::Milliseconds(100);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BrowserTestBase::WaitUntilJavaIsReady,
                     base::Unretained(this), std::move(quit_closure),
                     wait_retry_left - retry_interval),
      retry_interval);
  return;
}
#endif

void BrowserTestBase::ProxyRunTestOnMainThreadLoop() {
  // Chrome bans unresponsive tasks just before starting the main message loop.
  // Re-allow such tasks while for init / tear down
  // (ScopedDisallowBlocking objects below ensure the test body is tested under
  // the same blocking-ban as the regular main message loop).
  // TODO(crbug.com/40793886): Remove this wide allowance in favor of localized
  // allowances for init/teardown phases.
  base::ScopedAllowUnresponsiveTasksForTesting allow_for_init;

#if !BUILDFLAG(IS_ANDROID)
  // All FeatureList overrides should have been registered prior to browser test
  // SetUp(). Note that on Android, this scoper lives in SetUp() above.
  base::FeatureList::ScopedDisallowOverrides disallow_feature_overrides(
      "FeatureList overrides must happen in the test constructor, before "
      "BrowserTestBase::SetUp() has run.");
#endif

  // Install a RunLoop timeout if none is present but do not override tests that
  // set a ScopedRunLoopTimeout from their fixture's constructor (which
  // happens as part of setting up the test factory in gtest while
  // ProxyRunTestOnMainThreadLoop() happens later as part of SetUp()).
  std::optional<base::test::ScopedRunLoopTimeout> scoped_run_timeout;
  if (!base::test::ScopedRunLoopTimeout::ExistsForCurrentThread()) {
    // TODO(crbug.com/40608077): determine whether the timeout can be
    // reduced from action_max_timeout() to action_timeout().
    scoped_run_timeout.emplace(FROM_HERE, TestTimeouts::action_max_timeout());
  }

#if BUILDFLAG(IS_POSIX)
  g_browser_process_pid = base::GetCurrentProcId();
  signal(SIGSEGV, SignalHandler);

  if (handle_sigterm_)
    signal(SIGTERM, SignalHandler);

  ShutdownHandler = base::BindOnce(&BrowserTestBase::SignalRunTestOnMainThread,
                                   base::Unretained(this));
#endif  // BUILDFLAG(IS_POSIX)

  {
    // This shouldn't be invoked from a posted task.
    DCHECK(!base::RunLoop::IsRunningOnCurrentThread());

#if !BUILDFLAG(IS_ANDROID)
    // Fail the test if a renderer crashes while the test is running.
    //
    // This cannot be enabled on Android, because of renderer kills triggered
    // aggressively by the OS itself.
    no_renderer_crashes_assertion_ =
        std::make_unique<NoRendererCrashesAssertion>();
#endif

    PreRunTestOnMainThread();

    // Flush startup tasks to reach the OnFirstIdle() phase before
    // SetUpOnMainThread() (which must be right before RunTestOnMainThread()).
    {
      TRACE_EVENT0("test", "FlushStartupTasks");

      base::ScopedDisallowBlocking disallow_blocking;

      // Flush remaining startup tasks to make sure the
      // BrowserMainParts::OnFirstIdle phase has occurred before entering the
      // test body.
      base::RunLoop flush_startup_tasks;
      flush_startup_tasks.RunUntilIdle();
      // Make sure there isn't an odd caller which reached |flush_startup_tasks|
      DCHECK(!flush_startup_tasks.AnyQuitCalled());
    }

    std::unique_ptr<InitialNavigationObserver> initial_navigation_observer;
    if (initial_web_contents_) {
      // Some tests may add host_resolver() rules in their SetUpOnMainThread
      // method and navigate inside of it. This is a best effort to catch that
      // and sync the host_resolver() rules to the network process in that case,
      // to avoid navigations silently failing. This won't catch all cases, i.e.
      // if the test creates a new window or tab and navigates that.
      initial_navigation_observer = std::make_unique<InitialNavigationObserver>(
          initial_web_contents_.get(),
          base::BindOnce(&BrowserTestBase::InitializeNetworkProcess,
                         base::Unretained(this)));
    }
    initial_web_contents_.reset();

    base::CallbackListSubscription on_network_service_restarted_subscription =
        RegisterNetworkServiceProcessGoneHandler(base::BindRepeating(
            [](BrowserTestBase* browser_test_base, bool crashed) {
              if (!crashed) {
                browser_test_base->ForceInitializeNetworkProcess();
              }
            },
            base::Unretained(this)));

    SetUpOnMainThread();

#if BUILDFLAG(IS_WIN)
    // Now that most of process startup is complete, including launching the
    // network service process, HKLM override can be safely permitted again.
    registry_util::RegistryOverrideManager::
        SetAllowHKLMRegistryOverrideForIntegrationTests(/*allow=*/true);
#endif  // BUILDFLAG(IS_WIN)

    if (!IsSkipped()) {
      initial_navigation_observer.reset();

      // Tests would have added their host_resolver() rules by now, so copy them
      // to the network process if it's in use.
      InitializeNetworkProcess();

      {
        auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
        // This might be nullptr in a fuzz test or something else without gtest.
        if (test) {
          TRACE_EVENT("test", "RunTestOnMainThread", "test_name",
                      test->test_suite_name() + std::string(".") + test->name(),
                      "file", test->file(), "line", test->line());
        }
        base::ScopedDisallowBlocking disallow_blocking;
        RunTestOnMainThread();
      }
    }

    TearDownOnMainThread();
    AssertThatNetworkServiceDidNotCrash();

    // The subscription should be reset after asserting that the network service
    // did not crash, otherwise a network service restart task might be
    // processed in AssertThatNetworkServiceDidNotCrash() and the network
    // service will not be correctly initialized, which causes
    // AssertThatNetworkServiceDidNotCrash() to incorrectly report crashes.
    on_network_service_restarted_subscription = {};
  }

  PostRunTestOnMainThread();

  // Sometimes tests initialize a storage partition and the initialization
  // schedules some tasks which need to be executed before finishing tests.
  // Run these tasks.
  content::RunAllPendingInMessageLoop();

  // Update the trace output filename to include the test result.
  StartupTracingController::GetInstance().SetDefaultBasename(
      GetDefaultTraceBasename(TraceBasenameType::kWithTestStatus),
      StartupTracingController::ExtensionType::kAppendAppropriate);

#if BUILDFLAG(IS_ANDROID)
  // On Android, browser main runner is not shut down, so stop trace recording
  // here.
  StartupTracingController::GetInstance().WaitUntilStopped();
#endif
}

void BrowserTestBase::SetAllowNetworkAccessToHostResolutions() {
  // Must be called before Setup() to take effect. This mode can only be
  // used in manual tests to prevent flakiness in tryjobs due to the
  // dependency on network access.
  CHECK(!set_up_called_);

#if BUILDFLAG(IS_CHROMEOS_DEVICE)
  // External network access is only allowed for ChromeOS integration tests
  // running on real devices or VMs.
  CHECK(base::SysInfo::IsRunningOnChromeOS())
      << "External network access is only allowed for on device ChromeOS "
         "integration tests";
#else
  const char kManualTestPrefix[] = "MANUAL_";
  CHECK(base::StartsWith(
      testing::UnitTest::GetInstance()->current_test_info()->name(),
      kManualTestPrefix, base::CompareCase::SENSITIVE));
#endif  // BUILDFLAG(IS_CHROMEOS_DEVICE)

  LOG(WARNING) << "External network access is allowed. "
               << "This could lead to DoS on web sites and is normally only "
               << "allowed for manual tests and ChromeOS integration tests on "
               << "devices.";
  allow_network_access_to_host_resolutions_ = true;
}

void BrowserTestBase::SetReplaceSystemDnsConfig() {
  replace_system_dns_config_ = true;
}

void BrowserTestBase::SetTestDohConfig(net::SecureDnsMode secure_dns_mode,
                                       net::DnsOverHttpsConfig config) {
  DCHECK(!test_doh_config_.has_value());
  test_doh_config_ = std::make_pair(secure_dns_mode, std::move(config));
}

void BrowserTestBase::CreateTestServer(const base::FilePath& test_server_base) {
  embedded_test_server()->AddDefaultHandlers(test_server_base);
}

void BrowserTestBase::PostTaskToInProcessRendererAndWait(
    base::OnceClosure task) {
  CHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess));

  scoped_refptr<base::SingleThreadTaskRunner> renderer_task_runner =
      RenderProcessHostImpl::GetInProcessRendererThreadTaskRunnerForTesting();
  CHECK(renderer_task_runner);

  base::RunLoop run_loop;
  renderer_task_runner->PostTask(
      FROM_HERE, base::BindOnce(&RunTaskOnRendererThread, std::move(task),
                                run_loop.QuitClosure()));
  run_loop.Run();
}

void BrowserTestBase::EnablePixelOutput(float force_device_scale_factor) {
  enable_pixel_output_ = true;
  force_device_scale_factor_ = force_device_scale_factor;
}

void BrowserTestBase::UseSoftwareCompositing() {
  use_software_compositing_ = true;
}

void BrowserTestBase::SetInitialWebContents(WebContents* web_contents) {
  DCHECK(!initial_web_contents_);
  initial_web_contents_ = web_contents->GetWeakPtr();
}

void BrowserTestBase::AssertThatNetworkServiceDidNotCrash() {
  if (!IsOutOfProcessNetworkService()) {
    return;
  }

  // TODO(https://crbug.com/1169431#c2): Enable NetworkService crash detection
  // on Fuchsia.
#if !BUILDFLAG(IS_FUCHSIA)
  if (initialized_network_process_ && network_service_test_.is_bound()) {
    // If there was a crash, then |network_service_test_| will receive an error
    // notification, but it's not guaranteed to have arrived at this point.
    // Flush the remote to make sure the notification has been received.
    network_service_test_.FlushForTesting();

    EXPECT_TRUE(network_service_test_.is_connected())
        << "Expecting no NetworkService crashes";
  }
#endif
}

void BrowserTestBase::ForceInitializeNetworkProcess() {
  initialized_network_process_ = false;
  InitializeNetworkProcess();
}

void BrowserTestBase::InitializeNetworkProcess() {
  if (initialized_network_process_)
    return;

  initialized_network_process_ = true;

  // Test host resolver may not be initialized if host resolutions are allowed
  // to reach the network.
  if (host_resolver()) {
    host_resolver()->DisableModifications();
  }

  // Send the host resolver rules and other DNS settings to the network service.
  // If the network service is in the browser process, it will automatically
  // pick up the host resolver rules, but it will not automatically see
  // `replace_system_dns_config_` and `test_doh_config_`.
  //
  // TODO(crbug.com/40821298): Can the Mojo interface also be used in
  // this case?
  if (IsInProcessNetworkService()) {
    if (replace_system_dns_config_ || test_doh_config_) {
      base::RunLoop run_loop;
      content::GetNetworkTaskRunner()->PostTaskAndReply(
          FROM_HERE, base::BindLambdaForTesting([&] {
            network::NetworkService* network_service =
                network::NetworkService::GetNetworkServiceForTesting();
            ASSERT_TRUE(network_service);
            if (replace_system_dns_config_) {
              // The test must not run before the system DNS config has been
              // successfully replaced, see https://crrev.com/c/4247942.
              base::RunLoop run_loop_dns_config_service(
                  base::RunLoop::Type::kNestableTasksAllowed);
              network_service->ReplaceSystemDnsConfigForTesting(
                  run_loop_dns_config_service.QuitClosure());
              run_loop_dns_config_service.Run();
            }
            if (test_doh_config_) {
              network_service->SetTestDohConfigForTesting(
                  test_doh_config_->first, test_doh_config_->second);
            }
          }),
          run_loop.QuitClosure());
      run_loop.Run();
    }
    return;
  }

  network_service_test_.reset();
  content::GetNetworkService()->BindTestInterfaceForTesting(
      network_service_test_.BindNewPipeAndPassReceiver());

  // Do not set up host resolver rules if we allow the test to access
  // the network.
  if (allow_network_access_to_host_resolutions_) {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test_->SetAllowNetworkAccessToHostResolutions();
    return;
  }

  if (replace_system_dns_config_) {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test_->ReplaceSystemDnsConfig();
  }

  if (test_doh_config_.has_value()) {
    mojo::ScopedAllowSyncCallForTesting allow_sync_call;
    network_service_test_->SetTestDohConfig(test_doh_config_->first,
                                            test_doh_config_->second);
  }

  std::vector<network::mojom::RulePtr> mojo_rules;

  if (host_resolver()) {
    net::RuleBasedHostResolverProc::RuleList rules =
        host_resolver()->GetRules();
    for (const auto& rule : rules) {
      // For now, this covers all the rules used in content's tests.
      // TODO(jam: expand this when we try to make browser_tests and
      // components_browsertests work.
      if (rule.resolver_type ==
              net::RuleBasedHostResolverProc::Rule::kResolverTypeFail ||
          rule.resolver_type ==
              net::RuleBasedHostResolverProc::Rule::kResolverTypeFailTimeout) {
        // The host "wpad" is added automatically in TestHostResolver, so we
        // don't need to send it to NetworkServiceTest.
        if (rule.host_pattern != "wpad") {
          network::mojom::RulePtr mojo_rule = network::mojom::Rule::New();
          mojo_rule->resolver_type =
              (rule.resolver_type ==
               net::RuleBasedHostResolverProc::Rule::kResolverTypeFail)
                  ? network::mojom::ResolverType::kResolverTypeFail
                  : network::mojom::ResolverType::kResolverTypeFailTimeout;
          mojo_rule->host_pattern = rule.host_pattern;
          mojo_rules.push_back(std::move(mojo_rule));
        }
        continue;
      }

      if ((rule.resolver_type !=
               net::RuleBasedHostResolverProc::Rule::kResolverTypeSystem &&
           rule.resolver_type !=
               net::RuleBasedHostResolverProc::Rule::kResolverTypeIPLiteral) ||
          rule.address_family !=
              net::AddressFamily::ADDRESS_FAMILY_UNSPECIFIED ||
          !!rule.latency_ms) {
        continue;
      }
      network::mojom::RulePtr mojo_rule = network::mojom::Rule::New();
      if (rule.resolver_type ==
          net::RuleBasedHostResolverProc::Rule::kResolverTypeSystem) {
        mojo_rule->resolver_type =
            rule.replacement.empty()
                ? network::mojom::ResolverType::kResolverTypeDirectLookup
                : network::mojom::ResolverType::kResolverTypeSystem;
      } else {
        mojo_rule->resolver_type =
            network::mojom::ResolverType::kResolverTypeIPLiteral;
      }
      mojo_rule->host_pattern = rule.host_pattern;
      mojo_rule->replacement = rule.replacement;
      mojo_rule->host_resolver_flags = rule.host_resolver_flags;
      mojo_rule->dns_aliases = rule.dns_aliases;
      mojo_rules.push_back(std::move(mojo_rule));
    }
  }

  if (mojo_rules.empty()) {
    return;
  }

  // Send the DNS rules to network service process. Android needs the RunLoop
  // to dispatch a Java callback that makes network process to enter native
  // code.
  base::RunLoop loop{base::RunLoop::Type::kNestableTasksAllowed};
  network_service_test_->AddRules(std::move(mojo_rules), loop.QuitClosure());
  loop.Run();
}

void BrowserTestBase::CreatedBrowserMainPartsImpl(
    BrowserMainParts* browser_main_parts) {
  browser_main_parts_ = browser_main_parts;
  GetCurrentTestLauncherDelegate()->CreatedBrowserMainParts(browser_main_parts);
  CreatedBrowserMainParts(browser_main_parts);
}

ContentMainDelegate* BrowserTestBase::GetOptionalContentMainDelegateOverride() {
  return nullptr;
}

}  // namespace content
