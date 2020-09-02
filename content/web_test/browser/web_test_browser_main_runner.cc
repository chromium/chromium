// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_browser_main_runner.h"

#include <iostream>
#include <memory>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/viz/common/switches.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/gpu_browsertest_helpers.h"
#include "content/web_test/browser/test_info_extractor.h"
#include "content/web_test/browser/web_test_browser_main_platform_support.h"
#include "content/web_test/browser/web_test_control_host.h"
#include "content/web_test/common/web_test_switches.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/media_switches.h"
#include "net/base/filename_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

namespace content {

namespace {

bool RunOneTest(const content::TestInfo& test_info,
                content::WebTestControlHost* web_test_control_host,
                content::BrowserMainRunner* main_runner) {
  TRACE_EVENT0("shell", "WebTestBrowserMainRunner::RunOneTest");
  DCHECK(web_test_control_host);

  if (!web_test_control_host->PrepareForWebTest(test_info))
    return false;

  main_runner->Run();

  return web_test_control_host->ResetBrowserAfterWebTest();
}

void RunTests(content::BrowserMainRunner* main_runner) {
  TRACE_EVENT0("shell", "WebTestBrowserMainRunner::RunTests");
  content::WebTestControlHost test_controller;
  {
    // We're outside of the message loop here, and this is a test.
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath temp_path;
    base::GetTempDir(&temp_path);
    test_controller.SetTempPath(temp_path);
  }

  {
    // Kick off the launch of the GPU process early, to minimize blocking
    // startup of the first renderer process in PrepareForWebTest. (This avoids
    // GPU process startup time from being counted in the first test's timeout,
    // hopefully making it less likely to time out flakily.)
    // https://crbug.com/953991
    TRACE_EVENT0("shell",
                 "WebTestBrowserMainRunner::RunTests::EstablishGpuChannelSync");
    content::GpuBrowsertestEstablishGpuChannelSyncRunLoop();
  }

  std::cout << "#READY\n";
  std::cout.flush();

  content::TestInfoExtractor test_extractor(
      *base::CommandLine::ForCurrentProcess());
  bool ran_at_least_once = false;
  std::unique_ptr<content::TestInfo> test_info;
  while ((test_info = test_extractor.GetNextTest())) {
    ran_at_least_once = true;
    if (!RunOneTest(*test_info, &test_controller, main_runner))
      break;
  }
  if (!ran_at_least_once) {
    // CloseAllWindows will cause the |main_runner| loop to quit.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&content::Shell::CloseAllWindows));
    main_runner->Run();
  }
}

}  // namespace

void WebTestBrowserMainRunner::Initialize() {
#if defined(OS_WIN)
  bool layout_system_deps_ok = content::WebTestBrowserCheckLayoutSystemDeps();
  CHECK(layout_system_deps_ok);
#endif

  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  CHECK(browser_context_path_for_web_tests_.CreateUniqueTempDir());
  CHECK(!browser_context_path_for_web_tests_.GetPath().MaybeAsASCII().empty());
  command_line.AppendSwitchASCII(
      switches::kContentShellDataPath,
      browser_context_path_for_web_tests_.GetPath().MaybeAsASCII());

  command_line.AppendSwitch(switches::kIgnoreCertificateErrors);

  // Disable occlusion tracking. In a headless shell WebContents would always
  // behave as if they were occluded, i.e. would not render frames and would
  // not receive input events. For non-headless mode we do not want tests
  // running in parallel to trigger occlusion tracking.
  command_line.AppendSwitch(
      switches::kDisableBackgroundingOccludedWindowsForTesting);

  // Always disable the unsandbox GPU process for DX12 Info collection to avoid
  // interference. This GPU process is launched 120 seconds after chrome starts.
  command_line.AppendSwitch(switches::kDisableGpuProcessForDX12InfoCollection);

#if BUILDFLAG(ENABLE_PLUGINS)
  bool ppapi_ok = ppapi::RegisterBlinkTestPlugin(&command_line);
  CHECK(ppapi_ok);
#endif

  command_line.AppendSwitch(cc::switches::kEnableGpuBenchmarking);
  command_line.AppendSwitch(switches::kEnableLogging);
  command_line.AppendSwitch(switches::kAllowFileAccessFromFiles);
  // only default to a software GL if the flag isn't already specified.
  if (!command_line.HasSwitch(switches::kUseGpuInTests) &&
      !command_line.HasSwitch(switches::kUseGL)) {
    command_line.AppendSwitchASCII(
        switches::kUseGL,
        gl::GetGLImplementationName(gl::GetSoftwareGLImplementation()));
  }
  command_line.AppendSwitchASCII(switches::kTouchEventFeatureDetection,
                                 switches::kTouchEventFeatureDetectionEnabled);
  if (!command_line.HasSwitch(switches::kForceDeviceScaleFactor))
    command_line.AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1.0");

  if (!command_line.HasSwitch(switches::kAutoplayPolicy)) {
    command_line.AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  if (!command_line.HasSwitch(switches::kStableReleaseMode)) {
    command_line.AppendSwitch(switches::kEnableExperimentalWebPlatformFeatures);
    command_line.AppendSwitch(switches::kEnableBlinkTestFeatures);
  }

  if (!command_line.HasSwitch(switches::kEnableThreadedCompositing)) {
    command_line.AppendSwitch(switches::kDisableThreadedCompositing);
    command_line.AppendSwitch(cc::switches::kDisableThreadedAnimation);
  }

  // With display compositor pixel dumps, we ensure that we complete all
  // stages of compositing before draw. We also can't have checker imaging,
  // since it's incompatible with single threaded compositor and display
  // compositor pixel dumps.
  //
  // TODO(crbug.com/894613) Add kRunAllCompositorStagesBeforeDraw back here
  // once you figure out why it causes so much web test flakiness.
  // command_line.AppendSwitch(switches::kRunAllCompositorStagesBeforeDraw);
  command_line.AppendSwitch(cc::switches::kDisableCheckerImaging);

  command_line.AppendSwitch(switches::kMuteAudio);

  command_line.AppendSwitch(switches::kEnablePreciseMemoryInfo);

  command_line.AppendSwitchASCII(network::switches::kHostResolverRules,
                                 "MAP nonexistent.*.test ~NOTFOUND,"
                                 "MAP *.test. 127.0.0.1,"
                                 "MAP *.test 127.0.0.1");

  // We want to know determanistically from command line flags if the Gpu
  // process will provide gpu raster in its capabilities or not.
  //
  // If kEnableGpuRasterization is specified, the Gpu process always reports
  // that it can gpu raster, and the renderer will use it. Otherwise, we don't
  // want to choose at runtime, and we ensure that gpu raster is disabled.
  if (!command_line.HasSwitch(switches::kEnableGpuRasterization))
    command_line.AppendSwitch(switches::kDisableGpuRasterization);

  // If the virtual test suite didn't specify a display color space, then
  // force sRGB.
  if (!command_line.HasSwitch(switches::kForceDisplayColorProfile))
    command_line.AppendSwitchASCII(switches::kForceDisplayColorProfile, "srgb");

  // We want stable/baseline results when running web tests.
  command_line.AppendSwitch(switches::kDisableSkiaRuntimeOpts);

  command_line.AppendSwitch(switches::kDisallowNonExactResourceReuse);

  // Always run with fake media devices.
  command_line.AppendSwitch(switches::kUseFakeUIForMediaStream);
  command_line.AppendSwitch(switches::kUseFakeDeviceForMediaStream);

  // Enable the deprecated WebAuthn Mojo Testing API.
  command_line.AppendSwitch(switches::kEnableWebAuthDeprecatedMojoTestingApi);

  // Always disable the unsandbox GPU process for DX12 Info collection to avoid
  // interference. This GPU process is launched 120 seconds after chrome starts.
  command_line.AppendSwitch(switches::kDisableGpuProcessForDX12InfoCollection);

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
  content::WebTestBrowserPlatformInitialize();
#endif

  RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
}

void WebTestBrowserMainRunner::RunBrowserMain(
    const content::MainFunctionParams& parameters) {
  std::unique_ptr<content::BrowserMainRunner> main_runner =
      content::BrowserMainRunner::Create();
  int initialize_exit_code = main_runner->Initialize(parameters);
  DCHECK_LT(initialize_exit_code, 0)
      << "BrowserMainRunner::Initialize failed in WebTestBrowserMainRunner";

  RunTests(main_runner.get());
  base::RunLoop().RunUntilIdle();

  content::Shell::CloseAllWindows();

  main_runner->Shutdown();
}

}  // namespace content
