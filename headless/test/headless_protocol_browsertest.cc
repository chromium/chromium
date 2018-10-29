// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/runtime.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace headless {

namespace {
static const char kResetResults[] = "reset-results";
static const char kDumpDevToolsProtocol[] = "dump-devtools-protocol";
}  // namespace

class HeadlessProtocolBrowserTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public HeadlessDevToolsClient::RawProtocolListener,
      public runtime::ExperimentalObserver {
 public:
  HeadlessProtocolBrowserTest() {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        "third_party/WebKit/LayoutTests/http/tests/inspector-protocol");
    EXPECT_TRUE(embedded_test_server()->Start());
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(::network::switches::kHostResolverRules,
                                    "MAP *.test 127.0.0.1");
    HeadlessAsyncDevTooledBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  // HeadlessWebContentsObserver implementation.
  void DevToolsTargetReady() override {
    HeadlessAsyncDevTooledBrowserTest::DevToolsTargetReady();
    devtools_client_->GetRuntime()->GetExperimental()->AddObserver(this);
    devtools_client_->GetRuntime()->Enable();
    devtools_client_->GetRuntime()->GetExperimental()->AddBinding(
        headless::runtime::AddBindingParams::Builder()
            .SetName("sendProtocolMessage")
            .Build());
    browser_devtools_client_->SetRawProtocolListener(this);
  }

  void RunDevTooledTest() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath src_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    static const base::FilePath kTestsDirectory(
        FILE_PATH_LITERAL("headless/test/data/protocol"));
    base::FilePath test_path =
        src_dir.Append(kTestsDirectory).AppendASCII(script_name_);
    std::string script;
    if (!base::ReadFileToString(test_path, &script)) {
      ADD_FAILURE() << "Unable to read test at " << test_path;
      FinishTest();
      return;
    }
    GURL test_url = embedded_test_server()->GetURL("harness.test",
                                                   "/protocol/" + script_name_);
    GURL target_url = embedded_test_server()->GetURL(
        "127.0.0.1", "/protocol/" + script_name_);
    GURL page_url = embedded_test_server()->GetURL(
        "harness.test", "/protocol/inspector-protocol-test.html?test=" +
                            test_url.spec() + "&target=" + target_url.spec());
    devtools_client_->GetPage()->Navigate(page_url.spec());
  }

  // runtime::Observer implementation.
  void OnBindingCalled(const runtime::BindingCalledParams& params) override {
    std::string json_message = params.GetPayload();
    std::unique_ptr<base::Value> message = base::JSONReader::Read(json_message);
    const base::DictionaryValue* message_dict;
    const base::DictionaryValue* params_dict;
    std::string method;
    int id;
    if (!message || !message->GetAsDictionary(&message_dict) ||
        !message_dict->GetString("method", &method) ||
        !message_dict->GetDictionary("params", &params_dict) ||
        !message_dict->GetInteger("id", &id)) {
      LOG(ERROR) << "Poorly formed message " << json_message;
      FinishTest();
      return;
    }

    if (method != "DONE") {
      if (base::CommandLine::ForCurrentProcess()->HasSwitch(
              kDumpDevToolsProtocol)) {
        LOG(INFO) << "FromJS: " << json_message;
      }
      // Pass unhandled commands onto the inspector.
      browser_devtools_client_->SendRawDevToolsMessage(json_message);
      return;
    }

    std::string test_result;
    message_dict->GetString("result", &test_result);
    static const base::FilePath kTestsDirectory(
        FILE_PATH_LITERAL("headless/test/data/protocol"));

    base::ScopedAllowBlockingForTesting allow_blocking;

    base::FilePath src_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir));
    base::FilePath expectation_path =
        src_dir.Append(kTestsDirectory)
            .AppendASCII(script_name_.substr(0, script_name_.length() - 3) +
                         "-expected.txt");

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(kResetResults)) {
      LOG(INFO) << "Updating expectations at " << expectation_path;
      int result = base::WriteFile(expectation_path, test_result.data(),
                                   static_cast<int>(test_result.size()));
      CHECK(test_result.size() == static_cast<size_t>(result));
    }

    std::string expectation;
    if (!base::ReadFileToString(expectation_path, &expectation)) {
      ADD_FAILURE() << "Unable to read expectations at " << expectation_path;
    }
    EXPECT_EQ(test_result, expectation);
    FinishTest();
  }

  // HeadlessDevToolsClient::RawProtocolListener
  bool OnProtocolMessage(const std::string& json_message,
                         const base::DictionaryValue& parsed_message) override {
    SendMessageToJS(json_message);
    return true;
  }

  void SendMessageToJS(const std::string& message) {
    if (test_finished_)
      return;

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            kDumpDevToolsProtocol)) {
      LOG(INFO) << "ToJS: " << message;
    }

    std::string encoded;
    base::Base64Encode(message, &encoded);
    devtools_client_->GetRuntime()->Evaluate("onmessage(atob(\"" + encoded +
                                             "\"))");
  }

  void FinishTest() {
    test_finished_ = true;
    FinishAsynchronousTest();
  }

  // HeadlessBrowserTest overrides.
  void CustomizeHeadlessBrowserContext(
      HeadlessBrowserContext::Builder& builder) override {
    // Make sure the navigations spawn new processes. We run test harness
    // in one process (harness.test) and tests in another.
    builder.SetSitePerProcess(true);
  }

 protected:
  bool test_finished_ = false;
  std::string test_folder_;
  std::string script_name_;
};

#define HEADLESS_PROTOCOL_TEST(TEST_NAME, SCRIPT_NAME)             \
  IN_PROC_BROWSER_TEST_F(HeadlessProtocolBrowserTest, TEST_NAME) { \
    test_folder_ = "/protocol/";                                   \
    script_name_ = SCRIPT_NAME;                                    \
    RunTest();                                                     \
  }

#define LAYOUT_PROTOCOL_TEST(TEST_NAME, SCRIPT_NAME)               \
  IN_PROC_BROWSER_TEST_F(HeadlessProtocolBrowserTest, TEST_NAME) { \
    test_folder_ = "/";                                            \
    script_name_ = SCRIPT_NAME;                                    \
    RunTest();                                                     \
  }

// Headless-specific tests
HEADLESS_PROTOCOL_TEST(VirtualTimeAdvance, "emulation/virtual-time-advance.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeBasics, "emulation/virtual-time-basics.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeInterrupt,
                       "emulation/virtual-time-interrupt.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeCrossProcessNavigation,
                       "emulation/virtual-time-cross-process-navigation.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeDetachFrame,
                       "emulation/virtual-time-detach-frame.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeNoBlock404, "emulation/virtual-time-404.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeLocalStorage,
                       "emulation/virtual-time-local-storage.js");
HEADLESS_PROTOCOL_TEST(VirtualTimePendingScript,
                       "emulation/virtual-time-pending-script.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeHtmlImport,
                       "emulation/virtual-time-html-import.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeRedirect,
                       "emulation/virtual-time-redirect.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeSessionStorage,
                       "emulation/virtual-time-session-storage.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeStarvation,
                       "emulation/virtual-time-starvation.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeVideo, "emulation/virtual-time-video.js");
HEADLESS_PROTOCOL_TEST(VirtualTimeErrorLoop,
                       "emulation/virtual-time-error-loop.js");

// Flaky Test crbug.com/859382
HEADLESS_PROTOCOL_TEST(DISABLED_VirtualTimeHistoryNavigation,
                       "emulation/virtual-time-history-navigation.js");

// http://crbug.com/633321
#if defined(OS_ANDROID)
#define MAYBE_VirtualTimeTimerOrder DISABLED_VirtualTimeTimerOrder
#define MAYBE_VirtualTimeTimerSuspend DISABLED_VirtualTimeTimerSuspend
#else
#define MAYBE_VirtualTimeTimerOrder VirtualTimeTimerOrder
#define MAYBE_VirtualTimeTimerSuspend VirtualTimeTimerSuspend
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_VirtualTimeTimerOrder,
                       "emulation/virtual-time-timer-order.js");
HEADLESS_PROTOCOL_TEST(MAYBE_VirtualTimeTimerSuspend,
                       "emulation/virtual-time-timer-suspended.js");
#undef MAYBE_VirtualTimeTimerOrder
#undef MAYBE_VirtualTimeTimerSuspend

class HeadlessProtocolCompositorBrowserTest
    : public HeadlessProtocolBrowserTest {
 public:
  HeadlessProtocolCompositorBrowserTest() = default;

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessProtocolBrowserTest::SetUpCommandLine(command_line);
    // The following switches are recommended for BeginFrameControl required by
    // compositor tests, see https://goo.gl/3zHXhB for details
    static const char* const compositor_switches[] = {
        // We control BeginFrames ourselves and need all compositing stages to
        // run.
        switches::kRunAllCompositorStagesBeforeDraw,
        switches::kDisableNewContentRenderingTimeout,

        // Animtion-only BeginFrames are only supported when updates from the
        // impl-thread are disabled, see go/headless-rendering.
        cc::switches::kDisableThreadedAnimation,
        cc::switches::kDisableCheckerImaging,
        switches::kDisableThreadedScrolling,

        // Ensure that image animations don't resync their animation timestamps
        // when looping back around.
        switches::kDisableImageAnimationResync,
    };

    for (auto* compositor_switch : compositor_switches) {
      command_line->AppendSwitch(compositor_switch);
    }

    // In surface synchronization, child surface IDs are allocated by
    // parents and new CompositorFrames only activate once all their child
    // surfaces exist. In --run-all-compositor-stages-before-draw mode, this
    // means that child surface initialization and resize fully propagates
    // within a single BeginFrame.
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableSurfaceSynchronization);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// BeginFrameControl is not supported on MacOS yet, see: https://cs.chromium.org
// chromium/src/headless/lib/browser/protocol/target_handler.cc?
// rcl=5811aa08e60ba5ac7622f029163213cfbdb682f7&l=32
#if defined(OS_MACOSX)
#define HEADLESS_PROTOCOL_COMPOSITOR_TEST(TEST_NAME, SCRIPT_NAME) \
  IN_PROC_BROWSER_TEST_F(HeadlessProtocolCompositorBrowserTest,   \
                         DISABLED_##TEST_NAME) {                  \
    test_folder_ = "/protocol/";                                  \
    script_name_ = SCRIPT_NAME;                                   \
    RunTest();                                                    \
  }
#else
#define HEADLESS_PROTOCOL_COMPOSITOR_TEST(TEST_NAME, SCRIPT_NAME)            \
  IN_PROC_BROWSER_TEST_F(HeadlessProtocolCompositorBrowserTest, TEST_NAME) { \
    test_folder_ = "/protocol/";                                             \
    script_name_ = SCRIPT_NAME;                                              \
    RunTest();                                                               \
  }
#endif

HEADLESS_PROTOCOL_COMPOSITOR_TEST(CompositorBasicRaf,
                                  "emulation/compositor-basic-raf.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    CompositorImageAnimation,
    "emulation/compositor-image-animation-test.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(CompositorCssAnimation,
                                  "emulation/compositor-css-animation-test.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(VirtualTimeControllerTest,
                                  "helpers/virtual-time-controller-test.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererHelloWorld,
                                  "sanity/renderer-hello-world.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererOverrideTitleJsEnabled,
    "sanity/renderer-override-title-js-enabled.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererOverrideTitleJsDisabled,
    "sanity/renderer-override-title-js-disabled.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererJavaScriptConsoleErrors,
    "sanity/renderer-javascript-console-errors.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererDelayedCompletion,
                                  "sanity/renderer-delayed-completion.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererClientRedirectChain,
                                  "sanity/renderer-client-redirect-chain.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererClientRedirectChainNoJs,
    "sanity/renderer-client-redirect-chain-no-js.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererServerRedirectChain,
                                  "sanity/renderer-server-redirect-chain.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererServerRedirectToFailure,
    "sanity/renderer-server-redirect-to-failure.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererServerRedirectRelativeChain,
    "sanity/renderer-server-redirect-relative-chain.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererMixedRedirectChain,
                                  "sanity/renderer-mixed-redirect-chain.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererFramesRedirectChain,
                                  "sanity/renderer-frames-redirect-chain.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererDoubleRedirect,
                                  "sanity/renderer-double-redirect.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererRedirectAfterCompletion,
    "sanity/renderer-redirect-after-completion.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererRedirect307PostMethod,
    "sanity/renderer-redirect-307-post-method.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererRedirectPostChain,
                                  "sanity/renderer-redirect-post-chain.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererRedirect307PutMethod,
                                  "sanity/renderer-redirect-307-put-method.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererRedirect303PutGet,
                                  "sanity/renderer-redirect-303-put-get.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererRedirectBaseUrl,
                                  "sanity/renderer-redirect-base-url.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererRedirectNonAsciiUrl,
                                  "sanity/renderer-redirect-non-ascii-url.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererRedirectEmptyUrl,
                                  "sanity/renderer-redirect-empty-url.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererRedirectInvalidUrl,
                                  "sanity/renderer-redirect-invalid-url.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererRedirectKeepsFragment,
                                  "sanity/renderer-redirect-keeps-fragment.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererRedirectReplacesFragment,
    "sanity/renderer-redirect-replaces-fragment.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererRedirectNewFragment,
                                  "sanity/renderer-redirect-new-fragment.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererWindowLocationFragments,
    "sanity/renderer-window-location-fragments.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererCookieSetFromJs,
                                  "sanity/renderer-cookie-set-from-js.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(
    RendererCookieSetFromJsNoCookies,
    "sanity/renderer-cookie-set-from-js-no-cookies.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererCookieUpdatedFromJs,
                                  "sanity/renderer-cookie-updated-from-js.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererInCrossOriginObject,
                                  "sanity/renderer-in-cross-origin-object.js");

HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererContentSecurityPolicy,
                                  "sanity/renderer-content-security-policy.js");

HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererFrameLoadEvents,
                                  "sanity/renderer-frame-load-events.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererCssUrlFilter,
                                  "sanity/renderer-css-url-filter.js");
HEADLESS_PROTOCOL_COMPOSITOR_TEST(RendererCanvas, "sanity/renderer-canvas.js");

}  // namespace headless
