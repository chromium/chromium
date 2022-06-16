// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_protocol_browsertest.h"

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "headless/app/headless_shell_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"

namespace headless {

namespace {
static const char kResetResults[] = "reset-results";
static const char kDumpDevToolsProtocol[] = "dump-devtools-protocol";
}  // namespace

HeadlessProtocolBrowserTest::HeadlessProtocolBrowserTest() {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "third_party/blink/web_tests/http/tests/inspector-protocol");
  EXPECT_TRUE(embedded_test_server()->Start());
}

void HeadlessProtocolBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(::network::switches::kHostResolverRules,
                                  "MAP *.test 127.0.0.1");
  HeadlessAsyncDevTooledBrowserTest::SetUpCommandLine(command_line);

  // Make sure the navigations spawn new processes. We run test harness
  // in one process (harness.test) and tests in another.
  command_line->AppendSwitch(::switches::kSitePerProcess);

  // Make sure proxy related tests are not affected by a platform specific
  // system proxy configuration service.
  command_line->AppendSwitch(switches::kNoSystemProxyConfigService);
}

std::vector<std::string> HeadlessProtocolBrowserTest::GetPageUrlExtraParams() {
  return {};
}

void HeadlessProtocolBrowserTest::RunDevTooledTest() {
  browser_devtools_client_->SetRawProtocolListener(this);
  devtools_client_->GetRuntime()->GetExperimental()->AddObserver(this);
  devtools_client_->GetRuntime()->Enable();
  devtools_client_->GetRuntime()->GetExperimental()->AddBinding(
      headless::runtime::AddBindingParams::Builder()
          .SetName("sendProtocolMessage")
          .Build(),
      base::BindOnce(&HeadlessProtocolBrowserTest::BindingCreated,
                     base::Unretained(this)));
}

void HeadlessProtocolBrowserTest::BindingCreated(
    std::unique_ptr<headless::runtime::AddBindingResult>) {
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
  GURL target_url =
      embedded_test_server()->GetURL("127.0.0.1", "/protocol/" + script_name_);

  std::string extra_params;
  for (const auto& param : GetPageUrlExtraParams()) {
    extra_params += "&" + param;
  }

  GURL page_url = embedded_test_server()->GetURL(
      "harness.test",
      "/protocol/inspector-protocol-test.html?test=" + test_url.spec() +
          "&target=" + target_url.spec() + extra_params);
  devtools_client_->GetPage()->Navigate(page_url.spec());
}

void HeadlessProtocolBrowserTest::OnBindingCalled(
    const runtime::BindingCalledParams& params) {
  std::string json_message = params.GetPayload();
  std::unique_ptr<base::Value> message =
      base::JSONReader::ReadDeprecated(json_message);
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

bool HeadlessProtocolBrowserTest::OnProtocolMessage(
    base::span<const uint8_t> json_message,
    const base::DictionaryValue& parsed_message) {
  SendMessageToJS(base::StringPiece(
      reinterpret_cast<const char*>(json_message.data()), json_message.size()));
  return true;
}

void HeadlessProtocolBrowserTest::SendMessageToJS(base::StringPiece message) {
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

void HeadlessProtocolBrowserTest::FinishTest() {
  test_finished_ = true;
  FinishAsynchronousTest();
}

// TODO(crbug.com/1086872): The whole test suite is flaky on Mac ASAN.
#if (BUILDFLAG(IS_MAC) && defined(ADDRESS_SANITIZER))
#define HEADLESS_PROTOCOL_TEST(TEST_NAME, SCRIPT_NAME)                        \
  IN_PROC_BROWSER_TEST_F(HeadlessProtocolBrowserTest, DISABLED_##TEST_NAME) { \
    test_folder_ = "/protocol/";                                              \
    script_name_ = SCRIPT_NAME;                                               \
    RunTest();                                                                \
  }
#else
#define HEADLESS_PROTOCOL_TEST(TEST_NAME, SCRIPT_NAME)             \
  IN_PROC_BROWSER_TEST_F(HeadlessProtocolBrowserTest, TEST_NAME) { \
    test_folder_ = "/protocol/";                                   \
    script_name_ = SCRIPT_NAME;                                    \
    RunTest();                                                     \
  }
#endif

// Headless-specific tests
HEADLESS_PROTOCOL_TEST(VirtualTimeBasics, "emulation/virtual-time-basics.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeInterrupt,
                       "emulation/virtual-time-interrupt.js")

// Flaky on Linux, Mac & Win. TODO(crbug.com/930717): Re-enable.
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
// Flaky on all platforms. https://crbug.com/1295644
HEADLESS_PROTOCOL_TEST(DISABLED_VirtualTimeErrorLoop,
                       "emulation/virtual-time-error-loop.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeFetchStream,
                       "emulation/virtual-time-fetch-stream.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeDialogWhileLoading,
                       "emulation/virtual-time-dialog-while-loading.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeHistoryNavigation,
                       "emulation/virtual-time-history-navigation.js")
HEADLESS_PROTOCOL_TEST(VirtualTimeHistoryNavigationSameDoc,
                       "emulation/virtual-time-history-navigation-same-doc.js")

// Flaky on Mac. TODO(crbug.com/1164173): Re-enable.
#if BUILDFLAG(IS_MAC)
#define MAYBE_VirtualTimeFetchKeepalive DISABLED_VirtualTimeFetchKeepalive
#else
#define MAYBE_VirtualTimeFetchKeepalive VirtualTimeFetchKeepalive
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_VirtualTimeFetchKeepalive,
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

// https://crbug.com/1204620
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_InputClipboardOps DISABLED_InputClipboardOps
#else
#define MAYBE_InputClipboardOps InputClipboardOps
#endif
HEADLESS_PROTOCOL_TEST(MAYBE_InputClipboardOps, "input/input-clipboard-ops.js")

HEADLESS_PROTOCOL_TEST(HeadlessSessionBasicsTest,
                       "sessions/headless-session-basics.js")

HEADLESS_PROTOCOL_TEST(HeadlessSessionCreateContextDisposeOnDetach,
                       "sessions/headless-createContext-disposeOnDetach.js")

HEADLESS_PROTOCOL_TEST(BrowserSetInitialProxyConfig,
                       "sanity/browser-set-initial-proxy-config.js")

HEADLESS_PROTOCOL_TEST(BrowserUniversalNetworkAccess,
                       "sanity/universal-network-access.js")

HEADLESS_PROTOCOL_TEST(ShowDirectoryPickerNoCrash,
                       "sanity/show-directory-picker-no-crash.js")

HEADLESS_PROTOCOL_TEST(ShowFilePickerInterception,
                       "sanity/show-file-picker-interception.js")

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
  std::vector<std::string> GetPageUrlExtraParams() override {
    std::string proxy = proxy_server()->host_port_pair().ToString();
    return {"&proxy=" + proxy};
  }

 private:
  net::EmbeddedTestServer proxy_server_;
};

#define HEADLESS_PROTOCOL_TEST_WITH_PROXY(TEST_NAME, SCRIPT_NAME)           \
  IN_PROC_BROWSER_TEST_F(HeadlessProtocolBrowserTestWithProxy, TEST_NAME) { \
    test_folder_ = "/protocol/";                                            \
    script_name_ = SCRIPT_NAME;                                             \
    RunTest();                                                              \
  }

HEADLESS_PROTOCOL_TEST_WITH_PROXY(BrowserSetProxyConfig,
                                  "sanity/browser-set-proxy-config.js")

}  // namespace headless
