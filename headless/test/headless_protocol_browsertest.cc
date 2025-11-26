// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_protocol_browsertest.h"

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/config/linux/dbus/buildflags.h"
#include "components/headless/test/shared_test_util.h"
#include "content/public/common/content_switches.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/switches.h"
#include "headless/test/headless_browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"

namespace headless {

namespace switches {
static const char kDumpDevToolsProtocol[] = "dump-devtools-protocol";
}  // namespace switches

namespace {
static const base::FilePath kTestDataDir(
    FILE_PATH_LITERAL("headless/test/data"));
static const base::FilePath kSharedTestDataDir(
    FILE_PATH_LITERAL("components/headless/test/data"));

constexpr char kProtocolTestDir[] = "protocol";
}  // namespace

HeadlessProtocolBrowserTest::HeadlessProtocolBrowserTest() = default;
HeadlessProtocolBrowserTest::~HeadlessProtocolBrowserTest() = default;

base::FilePath HeadlessProtocolBrowserTest::GetTestDataDir() {
  return IsSharedTestScript() ? kSharedTestDataDir : kTestDataDir;
}

base::FilePath HeadlessProtocolBrowserTest::GetScriptPath() {
  base::FilePath src_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_dir));
  return src_dir.Append(GetTestDataDir())
      .AppendASCII(kProtocolTestDir)
      .AppendASCII(GetScriptName());
}

base::FilePath HeadlessProtocolBrowserTest::GetTestExpectationFilePath() {
  return headless::GetTestExpectationFilePath(GetScriptPath(), test_meta_info_,
                                              HeadlessType::kHeadlessShell);
}

bool HeadlessProtocolBrowserTest::IsSharedTestScript() {
  return headless::IsSharedTestScript(GetScriptName());
}

void HeadlessProtocolBrowserTest::SetUp() {
  LoadTestMetaInfo();
  HeadlessDevTooledBrowserTest::SetUp();
}

void HeadlessProtocolBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(::network::switches::kHostResolverRules,
                                  "MAP *.test 127.0.0.1");
  HeadlessDevTooledBrowserTest::SetUpCommandLine(command_line);

  test_meta_info_.AppendToCommandLine(*command_line);
}

base::Value::Dict HeadlessProtocolBrowserTest::GetPageUrlExtraParams() {
  return base::Value::Dict();
}

void HeadlessProtocolBrowserTest::LoadTestMetaInfo() {
  base::FilePath script_path = GetScriptPath();
  std::string script_body;
  CHECK(base::ReadFileToString(script_path, &script_body))
      << "script_path=" << script_path;

  auto test_meta_info = TestMetaInfo::FromString(script_body);
  CHECK(test_meta_info.has_value()) << test_meta_info.error();

  test_meta_info_ = test_meta_info.value();
}

void HeadlessProtocolBrowserTest::StartEmbeddedTestServer() {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "third_party/blink/web_tests/http/tests/inspector-protocol");

  if (IsSharedTestScript()) {
    embedded_test_server()->ServeFilesFromSourceDirectory(GetTestDataDir());
  }

  CHECK(embedded_test_server()->Start());
}

void HeadlessProtocolBrowserTest::RunDevTooledTest() {
  StartEmbeddedTestServer();

  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(
          HeadlessWebContentsImpl::From(web_contents_)->web_contents());

  // Set up Page domain.
  devtools_client_.AddEventHandler(
      "Page.loadEventFired",
      base::BindRepeating(&HeadlessProtocolBrowserTest::OnLoadEventFired,
                          base::Unretained(this)));
  devtools_client_.SendCommand("Page.enable");

  // Expose DevTools protocol to the target.
  browser_devtools_client_.SendCommand(
      "Target.exposeDevToolsProtocol", Param("targetId", agent_host->GetId()),
      base::BindOnce(&HeadlessProtocolBrowserTest::OnceSetUp,
                     base::Unretained(this)));
}

void HeadlessProtocolBrowserTest::OnceSetUp(base::Value::Dict) {
  // Navigate to test harness page
  GURL page_url = embedded_test_server()->GetURL(
      "harness.test", "/protocol/inspector-protocol-test.html");
  devtools_client_.SendCommand("Page.navigate", Param("url", page_url.spec()));
}

void HeadlessProtocolBrowserTest::OnLoadEventFired(
    const base::Value::Dict& params) {
  ASSERT_THAT(params, DictHasValue("method", "Page.loadEventFired"));

  std::string script_name = GetScriptName();
  GURL test_url = embedded_test_server()->GetURL("harness.test",
                                                 "/protocol/" + script_name);
  GURL target_url =
      embedded_test_server()->GetURL("127.0.0.1", "/protocol/" + script_name);

  base::Value::Dict test_params;
  test_params.Set("test", test_url.spec());
  test_params.Set("target", target_url.spec());
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpDevToolsProtocol)) {
    test_params.Set("dumpDevToolsProtocol", true);
  }
  test_params.Merge(GetPageUrlExtraParams());

  std::string json_test_params = base::WriteJson(test_params).value_or("");
  std::string evaluate_script = "runTest(" + json_test_params + ")";

  base::Value::Dict evaluate_params;
  evaluate_params.Set("expression", evaluate_script);
  evaluate_params.Set("awaitPromise", true);
  evaluate_params.Set("returnByValue", true);
  devtools_client_.SendCommand(
      "Runtime.evaluate", std::move(evaluate_params),
      base::BindOnce(&HeadlessProtocolBrowserTest::OnEvaluateResult,
                     base::Unretained(this)));
}

void HeadlessProtocolBrowserTest::OnEvaluateResult(base::Value::Dict params) {
  ProcessTestResult(DictString(params, "result.result.value"));

  FinishTest();
}

void HeadlessProtocolBrowserTest::ProcessTestResult(
    const std::string& test_result) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath expectation_path = GetTestExpectationFilePath();

  if (ShouldUpdateExpectations()) {
    LOG(INFO) << "Updating expectations at " << expectation_path;
    bool succcess = base::WriteFile(expectation_path, test_result);
    CHECK(succcess);
  }

  std::string expectation;
  if (!base::ReadFileToString(expectation_path, &expectation)) {
    ADD_FAILURE() << "Unable to read expectations at " << expectation_path;
  }

  EXPECT_EQ(expectation, test_result);
}

void HeadlessProtocolBrowserTest::FinishTest() {
  test_finished_ = true;
  FinishAsynchronousTest();
}

// Headless-specific tests
HEADLESS_PROTOCOL_TEST(VirtualTimeBasics, "emulation/virtual-time-basics.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeInterrupt,
                       "emulation/virtual-time-interrupt.js")

// Flaky on Linux, Mac & Win. TODO(crbug.com/41440558): Re-enable.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
#define MAYBE_VirtualTimeCrossProcessNavigation \
  DISABLED_VirtualTimeCrossProcessNavigation
#else
#define MAYBE_VirtualTimeCrossProcessNavigation \
  VirtualTimeCrossProcessNavigation
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_VirtualTimeCrossProcessNavigation,
                       "emulation/virtual-time-cross-process-navigation.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeDetachFrame,
                       "emulation/virtual-time-detach-frame.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeNoBlock404, "emulation/virtual-time-404.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeLocalStorage,
                       "emulation/virtual-time-local-storage.js")
HEADLESS_PROTOCOL_TEST(VirtualTimePendingScript,
                       "emulation/virtual-time-pending-script.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeRedirect,
                       "emulation/virtual-time-redirect.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeSessionStorage,
                       "emulation/virtual-time-session-storage.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeStarvation,
                       "emulation/virtual-time-starvation.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeVideo, "emulation/virtual-time-video.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeErrorLoop,
                       "emulation/virtual-time-error-loop.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeFetchStream,
                       "emulation/virtual-time-fetch-stream.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeFetchReadBody,
                       "emulation/virtual-time-fetch-read-body.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeFetchBlobReadBodyBlob,
                       "emulation/virtual-time-fetch-read-body-blob.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeDialogWhileLoading,
                       "emulation/virtual-time-dialog-while-loading.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeHistoryNavigation,
                       "emulation/virtual-time-history-navigation.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeHistoryNavigationSameDoc,
                       "emulation/virtual-time-history-navigation-same-doc.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeSVG, "emulation/virtual-time-svg.js")

// Flaky on Mac. TODO(crbug.com/352304682): Re-enable.
#if BUILDFLAG(IS_MAC)
#define MAYBE_VirtualTimeWorkerBasic DISABLED_VirtualTimeWorkerBasic
#else
#define MAYBE_VirtualTimeWorkerBasic VirtualTimeWorkerBasic
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_VirtualTimeWorkerBasic,
                       "emulation/virtual-time-worker-basic.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeWorkerLockstep,
                       "emulation/virtual-time-worker-lockstep.js")

// Flaky on Mac. TODO(crbug.com/352304682): Re-enable.
#if BUILDFLAG(IS_MAC)
#define MAYBE_VirtualTimeWorkerFetch DISABLED_VirtualTimeWorkerFetch
#else
#define MAYBE_VirtualTimeWorkerFetch VirtualTimeWorkerFetch
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_VirtualTimeWorkerFetch,
                       "emulation/virtual-time-worker-fetch.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeWorkerTerminate,
                       "emulation/virtual-time-worker-terminate.js")

HEADLESS_PROTOCOL_TEST(VirtualTimeFetchKeepalive,
                       "emulation/virtual-time-fetch-keepalive.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeDisposeWhileRunning,
                       "emulation/virtual-time-dispose-while-running.js")
HEADLESS_PROTOCOL_TEST(VirtualTimePausesDocumentLoading,
                       "emulation/virtual-time-pauses-document-loading.js")

HEADLESS_PROTOCOL_TEST(PageBeforeUnload, "page/page-before-unload.js")

// http://crbug.com/633321
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_VirtualTimeTimerOrder DISABLED_VirtualTimeTimerOrder
#define MAYBE_VirtualTimeTimerSuspend DISABLED_VirtualTimeTimerSuspend
#else
#define MAYBE_VirtualTimeTimerOrder VirtualTimeTimerOrder
#define MAYBE_VirtualTimeTimerSuspend VirtualTimeTimerSuspend
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_VirtualTimeTimerOrder,
                       "emulation/virtual-time-timer-order.js")
HEADLESS_PROTOCOL_TEST(MAYBE_VirtualTimeTimerSuspend,
                       "emulation/virtual-time-timer-suspended.js")
#undef MAYBE_VirtualTimeTimerOrder
#undef MAYBE_VirtualTimeTimerSuspend

HEADLESS_PROTOCOL_TEST(Geolocation, "emulation/geolocation-crash.js")

HEADLESS_PROTOCOL_TEST(DragStarted, "input/dragIntercepted.js")

// https://crbug.com/1414190
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_InputClipboardOps DISABLED_InputClipboardOps
#else
#define MAYBE_InputClipboardOps InputClipboardOps
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_InputClipboardOps, "input/input-clipboard-ops.js")

HEADLESS_PROTOCOL_TEST(ClipboardApiCopyPaste,
                       "input/clipboard-api-copy-paste.js")

HEADLESS_PROTOCOL_TEST(FocusBlurNotifications,
                       "input/focus-blur-notifications.js")

HEADLESS_PROTOCOL_TEST(HeadlessSessionBasicsTest,
                       "sessions/headless-session-basics.js")

HEADLESS_PROTOCOL_TEST(HeadlessSessionCreateContextDisposeOnDetach,
                       "sessions/headless-createContext-disposeOnDetach.js")

HEADLESS_PROTOCOL_TEST(BrowserSetInitialProxyConfig,
                       "sanity/browser-set-initial-proxy-config.js")

HEADLESS_PROTOCOL_TEST(BrowserUniversalNetworkAccess,
                       "sanity/universal-network-access.js")

// TODO(445548057): the test actually passes on regular Linux
// configurations that include D-Bus, however they take 25s
// due to an unrelated problem. Once this is fixed, the tests
// can be re-enabled on linux.
#if BUILDFLAG(IS_LINUX) && BUILDFLAG(USE_DBUS)
#define MAYBE_GetClientCapabilities DISABLED_GetClientCapabilities
#else
#define MAYBE_GetClientCapabilities GetClientCapabilities
#endif

HEADLESS_PROTOCOL_TEST(MAYBE_GetClientCapabilities,
                       "sanity/get-client-capabilities.js")

HEADLESS_PROTOCOL_TEST(ShowDirectoryPickerNoCrash,
                       "sanity/show-directory-picker-no-crash.js")

HEADLESS_PROTOCOL_TEST(ShowFilePickerInterception,
                       "sanity/show-file-picker-interception.js")

// The `change-window-*.js` tests cover DevTools methods, while `window-*.js`
// cover `window.*` JS APIs.
HEADLESS_PROTOCOL_TEST(ChangeWindowSize, "shared/change-window-size.js")
HEADLESS_PROTOCOL_TEST(ChangeWindowState, "shared/change-window-state.js")

HEADLESS_PROTOCOL_TEST(WindowOuterSize, "shared/window-outer-size.js")
HEADLESS_PROTOCOL_TEST(WindowInnerSize, "shared/window-inner-size.js")
HEADLESS_PROTOCOL_TEST(WindowInnerSizeScaled,
                       "shared/window-inner-size-scaled.js")
HEADLESS_PROTOCOL_TEST(WindowInnerSizeLargerThanScreen,
                       "shared/window-inner-size-larger-than-screen.js")

// This is not shared because Chrome Headless Mode window.resizeTo() only works
// under certain conditions which are note currently satisfied by the test.
HEADLESS_PROTOCOL_TEST(WindowResizeTo, "sanity/window-resize-to.js")

HEADLESS_PROTOCOL_TEST(HiddenTargetCreate, "shared/hidden-target-create.js")
HEADLESS_PROTOCOL_TEST(HiddenTargetClose, "shared/hidden-target-close.js")
HEADLESS_PROTOCOL_TEST(HiddenTargetCreateInvalidParams,
                       "shared/hidden-target-create-invalid-params.js")
HEADLESS_PROTOCOL_TEST(HiddenTargetPageEnable,
                       "shared/hidden-target-page-enable.js")

// https://crbug.com/378531862
#if BUILDFLAG(IS_MAC)
#define MAYBE_CreateTargetPosition DISABLED_CreateTargetPosition
#else
#define MAYBE_CreateTargetPosition CreateTargetPosition
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_CreateTargetPosition,
                       "shared/create-target-position.js")

HEADLESS_PROTOCOL_TEST(WindowSizeOnStart, "sanity/window-size-on-start.js")

HEADLESS_PROTOCOL_TEST(LargeBrowserWindowSize,
                       "shared/large-browser-window-size.js")

HEADLESS_PROTOCOL_TEST(ScreencastBasics, "shared/screencast-basics.js")
HEADLESS_PROTOCOL_TEST(ScreencastViewport, "shared/screencast-viewport.js")

HEADLESS_PROTOCOL_TEST(GrantPermissions, "sanity/grant_permissions.js")

#if !defined(HEADLESS_USE_EMBEDDED_RESOURCES)
HEADLESS_PROTOCOL_TEST(AutoHyphenation, "sanity/auto-hyphenation.js")
#endif

// Web Bluetooth is still experimental on Linux.
#if !BUILDFLAG(IS_LINUX)
HEADLESS_PROTOCOL_TEST(Bluetooth, "emulation/bluetooth.js")
#endif

class HeadlessProtocolBrowserTestWithKnownPermission
    : public HeadlessProtocolBrowserTest {
 public:
  HeadlessProtocolBrowserTestWithKnownPermission() = default;

 protected:
  base::Value::Dict GetPageUrlExtraParams() override {
    base::Value::List permissions;
    const std::vector<blink::PermissionType>& types =
        blink::GetAllPermissionTypes();
    for (blink::PermissionType type : types) {
      std::string permission = blink::GetPermissionString(type);
      NormalizePermissionName(permission);
      permissions.Append(permission);
    }

    base::Value::Dict dict;
    dict.Set("permissions", std::move(permissions));
    return dict;
  }

  static void NormalizePermissionName(std::string& permission) {
    if (IsAllAsciiUpper(permission)) {
      permission = base::ToLowerASCII(permission);
    } else {
      permission[0] = base::ToLowerASCII(permission[0]);
    }

    // Handle known exceptions.
    if (permission == "midiSysEx") {
      permission = "midiSysex";
    }
  }

  static bool IsAllAsciiUpper(const std::string& permission) {
    for (char ch : permission) {
      if (!base::IsAsciiUpper(ch)) {
        return false;
      }
    }
    return true;
  }
};

HEADLESS_PROTOCOL_TEST_F(HeadlessProtocolBrowserTestWithKnownPermission,
                         KnownPermissionTypes,
                         "sanity/known-permission-types.js")

class HeadlessProtocolBrowserTestWithProxy
    : public HeadlessProtocolBrowserTest {
 public:
  HeadlessProtocolBrowserTestWithProxy()
      : proxy_server_(net::EmbeddedTestServer::TYPE_HTTP) {
    proxy_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("headless/test/data")));
  }

  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    HeadlessProtocolBrowserTest::SetUp();
  }

  void TearDown() override {
    EXPECT_TRUE(proxy_server_.ShutdownAndWaitUntilComplete());
    HeadlessProtocolBrowserTest::TearDown();
  }

  net::EmbeddedTestServer* proxy_server() { return &proxy_server_; }

 protected:
  base::Value::Dict GetPageUrlExtraParams() override {
    std::string proxy = proxy_server()->host_port_pair().ToString();
    base::Value::Dict dict;
    dict.Set("proxy", proxy);
    return dict;
  }

 private:
  net::EmbeddedTestServer proxy_server_;
};

HEADLESS_PROTOCOL_TEST_F(HeadlessProtocolBrowserTestWithProxy,
                         BrowserSetProxyConfig,
                         "sanity/browser-set-proxy-config.js")

class PopupWindowOpenTest : public HeadlessProtocolBrowserTest,
                            public testing::WithParamInterface<bool> {
 protected:
  PopupWindowOpenTest() = default;

  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    builder.SetBlockNewWebContents(ShouldBlockNewWebContents());
  }

  base::Value::Dict GetPageUrlExtraParams() override {
    base::Value::Dict params;
    params.Set("blockingNewWebContents", ShouldBlockNewWebContents());
    return params;
  }

  bool ShouldBlockNewWebContents() const { return GetParam(); }
};

HEADLESS_PROTOCOL_TEST_P(PopupWindowOpenTest,
                         Open,
                         "sanity/popup-window-open.js")

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         PopupWindowOpenTest_Open,
                         ::testing::Bool());

class HeadlessProtocolBrowserTestWithoutSiteIsolation
    : public HeadlessProtocolBrowserTest {
 public:
  HeadlessProtocolBrowserTestWithoutSiteIsolation() = default;

 protected:
  bool ShouldEnableSitePerProcess() override { return false; }
};

HEADLESS_PROTOCOL_TEST_F(
    HeadlessProtocolBrowserTestWithoutSiteIsolation,
    VirtualTimeLocalStorageDetachedFrame,
    "emulation/virtual-time-local-storage-detached-frame.js")

class HeadlessProtocolBrowserTestWithFileInputDirectoryUpload
    : public HeadlessProtocolBrowserTest {
 protected:
  static constexpr char kFileInputDirectoryUpload[] =
      "resources/file-input-directory-upload";

  base::Value::Dict GetPageUrlExtraParams() override {
    base::FilePath data_path =
        GetScriptPath().DirName().AppendASCII(kFileInputDirectoryUpload);

    base::Value::Dict dict;
    dict.Set("data_path", data_path.AsUTF8Unsafe());
    return dict;
  }
};

HEADLESS_PROTOCOL_TEST_F(
    HeadlessProtocolBrowserTestWithFileInputDirectoryUpload,
    Upload,
    "sanity/file-input-directory-upload.js")

HEADLESS_PROTOCOL_TEST(GetDOMCountersForLeakDetection,
                       "sanity/get-dom-counters-for-leak-detection.js")

class HeadlessProtocolBrowserTestSitePerProcess
    : public HeadlessProtocolBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  bool ShouldEnableSitePerProcess() override { return GetParam(); }

  base::Value::Dict GetPageUrlExtraParams() override {
    base::Value::Dict params;
    params.Set("sitePerProcessEnabled", ShouldEnableSitePerProcess());
    return params;
  }
};

HEADLESS_PROTOCOL_TEST_P(HeadlessProtocolBrowserTestSitePerProcess,
                         SitePerProcess,
                         "sanity/site-per-process.js")

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HeadlessProtocolBrowserTestSitePerProcess_SitePerProcess,
    ::testing::Bool());

HEADLESS_PROTOCOL_TEST(DataURIIframe, "sanity/data-uri-iframe.js")

// The test brlow requires beginFrameControl which is currently not supported
// on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_IOCommandAfterInput DISABLED_IOCommandAfterInput
#else
#define MAYBE_IOCommandAfterInput IOCommandAfterInput
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_IOCommandAfterInput,
                       "input/io-command-after-input.js")

HEADLESS_PROTOCOL_TEST(PrintToPdfTinyPage, "shared/print-to-pdf-tiny-page.js")

HEADLESS_PROTOCOL_TEST(ScreenDetailsRotationAngle,
                       "shared/screen-details-rotation-angle.js")

HEADLESS_PROTOCOL_TEST(ScreenOrientationLockNaturalLandscape,
                       "sanity/screen-orientation-lock-natural-landscape.js")

HEADLESS_PROTOCOL_TEST(ScreenOrientationLockNaturalPortrait,
                       "sanity/screen-orientation-lock-natural-portrait.js")

HEADLESS_PROTOCOL_TEST(ScreenDetailsMultipleScreens,
                       "shared/screen-details-multiple-screens.js")

HEADLESS_PROTOCOL_TEST(ScreenDetailsMultipleScreensScaled,
                       "shared/screen-details-multiple-screens-scaled.js")

HEADLESS_PROTOCOL_TEST(ScreenDetailsPixelRatio,
                       "shared/screen-details-pixel-ratio.js")

HEADLESS_PROTOCOL_TEST(ScreenDetailsColorDepth,
                       "shared/screen-details-color-depth.js")

HEADLESS_PROTOCOL_TEST(ScreenDetailsWorkArea,
                       "shared/screen-details-work-area.js")

HEADLESS_PROTOCOL_TEST(ScreenDetailsWorkAreaScaled,
                       "shared/screen-details-work-area-scaled.js")

HEADLESS_PROTOCOL_TEST(RequestFullscreen, "shared/request-fullscreen.js")

HEADLESS_PROTOCOL_TEST(RequestFullscreenOnSecondaryScreen,
                       "shared/request-fullscreen-on-secondary-screen.js")

HEADLESS_PROTOCOL_TEST(MinimizeRestoreWindow,
                       "shared/minimize-restore-window.js")

HEADLESS_PROTOCOL_TEST(MaximizeRestoreWindow,
                       "shared/maximize-restore-window.js")

HEADLESS_PROTOCOL_TEST(FullscreenRestoreWindow,
                       "shared/fullscreen-restore-window.js")

HEADLESS_PROTOCOL_TEST(MaximizedWindowSize, "shared/maximized-window-size.js")

HEADLESS_PROTOCOL_TEST(FullscreenWindowSize, "shared/fullscreen-window-size.js")

HEADLESS_PROTOCOL_TEST(FullscreenWindowSizeScaled,
                       "shared/fullscreen-window-size-scaled.js")

HEADLESS_PROTOCOL_TEST(WindowOpenOnSecondaryScreen,
                       "shared/window-open-on-secondary-screen.js")

HEADLESS_PROTOCOL_TEST(ScreenRotationSecondaryScreen,
                       "sanity/screen-rotation-secondary-screen.js")

HEADLESS_PROTOCOL_TEST(MoveWindowBetweenScreens,
                       "shared/move-window-between-screens.js")

// This fails on Mac with RenderDocument enabled, http://crbug.com/446689489.
#if BUILDFLAG(IS_MAC)
#define MAYBE_CreateTargetSecondaryScreen DISABLED_CreateTargetSecondaryScreen
#else
#define MAYBE_CreateTargetSecondaryScreen CreateTargetSecondaryScreen
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_CreateTargetSecondaryScreen,
                       "shared/create-target-secondary-screen.js")

HEADLESS_PROTOCOL_TEST(CreateTargetWindowState,
                       "shared/create-target-window-state.js")

HEADLESS_PROTOCOL_TEST(DocumentVisibilityState,
                       "shared/document-visibility-state.js")

HEADLESS_PROTOCOL_TEST(DocumentVisibilityStatePopup,
                       "shared/document-visibility-state-popup.js")

// This currently results in an unexpected screen orientation type,
// see http://crbug.com/398150465.
HEADLESS_PROTOCOL_TEST(MultipleScreenDetails,
                       "shared/multiple-screen-details.js")

HEADLESS_PROTOCOL_TEST(WindowOpenPopupPlacement,
                       "shared/window-open-popup-placement.js")

HEADLESS_PROTOCOL_TEST(WindowSizeSwitchHandling,
                       "shared/window-size-switch-handling.js")

HEADLESS_PROTOCOL_TEST(WindowSizeSwitchLargerThanScreen,
                       "shared/window-size-switch-larger-than-screen.js")

HEADLESS_PROTOCOL_TEST(WindowScreenAvail, "shared/window-screen-avail.js")

HEADLESS_PROTOCOL_TEST(WindowStateTransitions,
                       "shared/window-state-transitions.js")

HEADLESS_PROTOCOL_TEST(WindowZoomOnSecondaryScreen,
                       "shared/window-zoom-on-secondary-screen.js")

HEADLESS_PROTOCOL_TEST(WindowZoomSizeMatchesWorkArea,
                       "shared/window-zoom-size-matches-work-area.js")

HEADLESS_PROTOCOL_TEST(WindowScreenScaleFactor,
                       "shared/window-screen-scale-factor.js")

HEADLESS_PROTOCOL_TEST(WindowScreenSizeOrientation,
                       "shared/window-screen-size-orientation.js")

HEADLESS_PROTOCOL_TEST(GetScreenInfos, "shared/get-screen-infos.js")

HEADLESS_PROTOCOL_TEST(AddScreen, "shared/add-screen.js")

HEADLESS_PROTOCOL_TEST(AddScreenScaleFactor,
                       "shared/add-screen-scale-factor.js")

HEADLESS_PROTOCOL_TEST(AddScreenWorkArea, "shared/add-screen-work-area.js")

HEADLESS_PROTOCOL_TEST(AddScreenGetScreenDetails,
                       "shared/add-screen-get-screen-details.js")

HEADLESS_PROTOCOL_TEST(RemoveScreen, "shared/remove-screen.js")

HEADLESS_PROTOCOL_TEST(RemoveScreenGetScreenDetails,
                       "shared/remove-screen-get-screen-details.js")

HEADLESS_PROTOCOL_TEST(AddRemoveScreen, "shared/add-remove-screen.js")

HEADLESS_PROTOCOL_TEST(DispatchMouseEventScreenCoordinates,
                       "shared/dispatch-mouse-event-screen-coordinates.js")

HEADLESS_PROTOCOL_TEST(DispatchTouchEventScreenCoordinates,
                       "shared/dispatch-touch-event-screen-coordinates.js")

HEADLESS_PROTOCOL_TEST(
    EmulateTouchFromMouseEventScreenCoordinates,
    "shared/emulate-touch-from-mouse-event-screen-coordinates.js")

HEADLESS_PROTOCOL_TEST(WindowWithNewContext,
                       "shared/window-with-new-context.js")

HEADLESS_PROTOCOL_TEST(SetZoomedWindowBounds,
                       "shared/set-zoomed-window-bounds.js")

}  // namespace headless
