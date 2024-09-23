// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_browser_main_runner.h"

#include <iostream>
#include <memory>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/viz/common/switches.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
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
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/url_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/base/ui_base_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/public/test/ppapi_test_utils.h"
#endif

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
#include <sys/socket.h>
#include <unistd.h>
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "ui/ozone/public/ozone_switches.h"
#endif

namespace content {

namespace {

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
// Fuchsia doesn't support stdin stream for packaged apps, and stdout from
// run-test-suite not only has extra emissions from the Fuchsia test
// infrastructure, it also merges stderr and stdout together. Combined, these
// mean that when running content_shell on Fuchsia it's not possible to use
// stdin to pass list of tests or to reliably use stdout to emit results. To
// workaround this issue for web tests we redirect stdin and stdout to a TCP
// socket connected to the web test runner. The runner uses --stdio-redirect to
// specify address and port for stdin and stdout redirection.
//
// iOS is in a similar situation where the simulator does not support the use of
// the stdin stream for applications. Therefore, iOS also redirects stdin and
// stdout to a TCP socket that is connected to the web test runner.
constexpr char kStdioRedirectSwitch[] = "stdio-redirect";

void ConnectStdioSocket(const std::string& host_and_port) {
  std::string host;
  int port;
  net::IPAddress address;
  if (!net::ParseHostAndPort(host_and_port, &host, &port) ||
      !address.AssignFromIPLiteral(host)) {
    LOG(FATAL) << "Invalid stdio address: " << host_and_port;
  }

  sockaddr_storage sockaddr_storage;
  sockaddr* addr = reinterpret_cast<sockaddr*>(&sockaddr_storage);
  socklen_t addr_len = sizeof(sockaddr_storage);
  net::IPEndPoint endpoint(address, port);
  bool converted = endpoint.ToSockAddr(addr, &addr_len);
  CHECK(converted);

  int fd = socket(addr->sa_family, SOCK_STREAM, 0);
  PCHECK(fd >= 0);
  int result = connect(fd, addr, addr_len);
  PCHECK(result == 0) << "Failed to connect to " << host_and_port;

  result = dup2(fd, STDIN_FILENO);
  PCHECK(result == STDIN_FILENO) << "Failed to dup socket to stdin";

  result = dup2(fd, STDOUT_FILENO);
  PCHECK(result == STDOUT_FILENO) << "Failed to dup socket to stdout";

  PCHECK(close(fd) == 0);
}

#endif  // BUILDFLAG(IS_FUCHSIA)

void RunOneTest(const content::TestInfo& test_info,
                content::WebTestControlHost* web_test_control_host,
                content::BrowserMainRunner* main_runner) {
  TRACE_EVENT0("shell", "WebTestBrowserMainRunner::RunOneTest");
  DCHECK(web_test_control_host);

  web_test_control_host->PrepareForWebTest(test_info);

  main_runner->Run();

  web_test_control_host->ResetBrowserAfterWebTest();
}

void RunTests(content::BrowserMainRunner* main_runner) {
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
  if (auto& cmd_line = *base::CommandLine::ForCurrentProcess();
      cmd_line.HasSwitch(kStdioRedirectSwitch)) {
    ConnectStdioSocket(cmd_line.GetSwitchValueASCII(kStdioRedirectSwitch));
  }
#endif  // BUILDFLAG(IS_FUCHSIA)
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
  std::unique_ptr<content::TestInfo> test_info;
  while ((test_info = test_extractor.GetNextTest())) {
    RunOneTest(*test_info, &test_controller, main_runner);
  }
}

}  // namespace

void WebTestBrowserMainRunner::Initialize() {
#if BUILDFLAG(IS_WIN)
  bool layout_system_deps_ok = content::WebTestBrowserCheckLayoutSystemDeps();
  CHECK(layout_system_deps_ok);
#endif

  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  CHECK(browser_context_path_for_web_tests_.CreateUniqueTempDir());
  CHECK(!browser_context_path_for_web_tests_.GetPath().MaybeAsASCII().empty());
  command_line.AppendSwitchASCII(
      switches::kContentShellUserDataDir,
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

#if BUILDFLAG(ENABLE_PPAPI)
  CHECK(ppapi::RegisterBlinkTestPlugin(&command_line));
#endif

  command_line.AppendSwitch(cc::switches::kEnableGpuBenchmarking);
  command_line.AppendSwitch(switches::kEnableLogging);
  command_line.AppendSwitch(switches::kAllowFileAccessFromFiles);

  // On IOS, we always use hardware GL for the web test as content_browsertests.
  // See also https://crrev.com/c/4885954.
  if constexpr (!BUILDFLAG(IS_IOS)) {
    // only default to a software GL if the flag isn't already specified.
    if (!command_line.HasSwitch(switches::kUseGpuInTests) &&
        !command_line.HasSwitch(switches::kUseGL)) {
      gl::SetSoftwareGLCommandLineSwitches(&command_line);
    }
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

  // With display compositor pixel dumps, we ensure that we complete all
  // stages of compositing before draw. We also can't have checker imaging,
  // since it's incompatible with single threaded compositor and display
  // compositor pixel dumps.
  //
  // TODO(crbug.com/41420287) Add kRunAllCompositorStagesBeforeDraw back here
  // once you figure out why it causes so much web test flakiness.
  // command_line.AppendSwitch(switches::kRunAllCompositorStagesBeforeDraw);
  command_line.AppendSwitch(cc::switches::kDisableCheckerImaging);

  command_line.AppendSwitch(switches::kMuteAudio);

  command_line.AppendSwitch(switches::kEnablePreciseMemoryInfo);

  command_line.AppendSwitchASCII(network::switches::kHostResolverRules,
                                 "MAP nonexistent.*.test ^NOTFOUND,"
                                 "MAP web-platform.test:443 127.0.0.1:8444,"
                                 "MAP not-web-platform.test:443 127.0.0.1:8444,"
                                 "MAP devtools.test:443 127.0.0.1:8443,"
                                 "MAP *.test. 127.0.0.1,"
                                 "MAP *.test 127.0.0.1");

  // These must be kept in sync with
  // //third_party/blink/web_tests/external/wpt/config.json.
  command_line.AppendSwitchASCII(network::switches::kIpAddressSpaceOverrides,
                                 "127.0.0.1:8082=private,"
                                 "127.0.0.1:8093=public,"
                                 "127.0.0.1:8446=private,"
                                 "127.0.0.1:8447=public");

  // We want to know determanistically from command line flags if the Gpu
  // process will provide gpu raster in its capabilities or not.
  //
  // If kEnableGpuRasterization is specified, the Gpu process always reports
  // that it can gpu raster, and the renderer will use it. Otherwise, we don't
  // want to choose at runtime, and we ensure that gpu raster is disabled.
  if (!command_line.HasSwitch(switches::kEnableGpuRasterization))
    command_line.AppendSwitch(switches::kDisableGpuRasterization);

  // If Graphite is not explicitly enabled, disable it. This is to keep using
  // Ganesh as renderer for web tests for now until we finish rebaselining all
  // images for Graphite renderer.
  if (!command_line.HasSwitch(switches::kEnableSkiaGraphite)) {
    command_line.AppendSwitch(switches::kDisableSkiaGraphite);
  }

  // If the virtual test suite didn't specify a display color space, then
  // force sRGB.
  if (!command_line.HasSwitch(switches::kForceDisplayColorProfile))
    command_line.AppendSwitchASCII(switches::kForceDisplayColorProfile, "srgb");

  // We want stable/baseline results when running web tests.
  command_line.AppendSwitch(switches::kDisableSkiaRuntimeOpts);

  command_line.AppendSwitch(switches::kDisallowNonExactResourceReuse);

  // Always run with fake media devices.
  command_line.AppendSwitch(switches::kUseFakeUIForMediaStream);

  // The check here ensures that a test's custom value for the switch is not
  // overwritten by the default one.
  if (!command_line.HasSwitch(switches::kUseFakeDeviceForMediaStream))
    command_line.AppendSwitch(switches::kUseFakeDeviceForMediaStream);

  // Always run with fake FedCM UI.
  command_line.AppendSwitch(switches::kUseFakeUIForFedCM);

  // Always run with fake digital identity credential UI.
  command_line.AppendSwitch(switches::kUseFakeUIForDigitalIdentity);

  // Enable the deprecated WebAuthn Mojo Testing API.
  command_line.AppendSwitch(switches::kEnableWebAuthDeprecatedMojoTestingApi);

  // Always disable the unsandbox GPU process for DX12 Info collection to avoid
  // interference. This GPU process is launched 120 seconds after chrome starts.
  command_line.AppendSwitch(switches::kDisableGpuProcessForDX12InfoCollection);

  // Disable the backgrounding of renderers to make running tests faster.
  command_line.AppendSwitch(switches::kDisableRendererBackgrounding);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  content::WebTestBrowserPlatformInitialize();
#endif

  RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
}

void WebTestBrowserMainRunner::RunBrowserMain(
    content::MainFunctionParams parameters) {
  std::unique_ptr<content::BrowserMainRunner> main_runner =
      content::BrowserMainRunner::Create();
  int initialize_exit_code = main_runner->Initialize(std::move(parameters));
  DCHECK_LT(initialize_exit_code, 0)
      << "BrowserMainRunner::Initialize failed in WebTestBrowserMainRunner";

  RunTests(main_runner.get());

  // Shell::Shutdown() will cause the |main_runner| loop to quit.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Shell::Shutdown));
  main_runner->Run();

  main_runner->Shutdown();
}

}  // namespace content
