// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"
#include "base/test/simple_test_clock.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/crash_report_private/crash_report_private_api.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/common/switches.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

using browsertest_util::ExecuteScriptInBackgroundPage;

namespace {

constexpr const char* kTestExtensionId = "jjeoclcdfjddkdjokiejckgcildcflpp";

std::string GetOsVersion() {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
  return base::StringPrintf("%d.%d.%d", os_major_version, os_minor_version,
                            os_bugfix_version);
}

}  // namespace

class CrashReportPrivateApiTest : public ShellApiTest {
 public:
  CrashReportPrivateApiTest() = default;
  ~CrashReportPrivateApiTest() override = default;

  void SetUpOnMainThread() override {
    ShellApiTest::SetUpOnMainThread();

    constexpr char kKey[] =
        "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC+uU63MD6T82Ldq5wjrDFn5mGmPnnnj"
        "WZBWxYXfpG4kVf0s+p24VkXwTXsxeI12bRm8/ft9sOq0XiLfgQEh5JrVUZqvFlaZYoS+g"
        "iZfUqzKFGMLa4uiSMDnvv+byxrqAepKz5G8XX/q5Wm5cvpdjwgiu9z9iM768xJy+Ca/G5"
        "qQwIDAQAB";
    constexpr char kManifestTemplate[] =
        R"({
      "key": "%s",
      "name": "chrome.crashReportPrivate basic extension tests",
      "version": "1.0",
      "manifest_version": 2,
      "background": { "scripts": ["test.js"] },
      "permissions": ["crashReportPrivate"]
    })";

    TestExtensionDir test_dir;
    test_dir.WriteManifest(base::StringPrintf(kManifestTemplate, kKey));
    test_dir.WriteFile(FILE_PATH_LITERAL("test.js"),
                       R"(chrome.test.sendMessage('ready');)");

    ExtensionTestMessageListener listener("ready", false);
    extension_ = LoadExtension(test_dir.UnpackedPath());
    EXPECT_TRUE(listener.WaitUntilSatisfied());

    crash_endpoint_ =
        std::make_unique<MockCrashEndpoint>(embedded_test_server());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kTestExtensionId);
    ShellApiTest::SetUpCommandLine(command_line);
  }

 protected:
  const base::Optional<MockCrashEndpoint::Report>& last_report() {
    return crash_endpoint_->last_report();
  }
  const Extension* extension_;
  std::unique_ptr<MockCrashEndpoint> crash_endpoint_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashReportPrivateApiTest);
};

IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, Basic) {
  constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com",
      },
      () => window.domAutomationController.send(""));
  )";
  ExecuteScriptInBackgroundPage(browser_context(), extension_->id(),
                                kTestScript);

  const base::Optional<MockCrashEndpoint::Report>& report = last_report();
  ASSERT_TRUE(report);
  EXPECT_EQ(report->query,
            "browser=Chrome&browser_version=1.2.3.4&channel=Stable&"
            "error_message=hi&full_url=http%3A%2F%2Fwww.test.com%2F&"
            "os=ChromeOS&os_version=" +
                GetOsVersion() +
                "&prod=Chrome_ChromeOS&src=http%3A%2F%2Fwww.test."
                "com%2F&type=JavascriptError&url=%2F&ver=1.2.3.4");
  EXPECT_EQ(report->content, "");
}

IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, ExtraParamsAndStackTrace) {
  constexpr char kTestScript[] = R"-(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com/foo",
        product: "Chrome (Chrome OS)",
        version: "1.0.0.0",
        lineNumber: 123,
        columnNumber: 456,
        stackTrace: "   at <anonymous>:1:1",
      },
      () => window.domAutomationController.send(""));
  )-";
  ExecuteScriptInBackgroundPage(browser_context(), extension_->id(),
                                kTestScript);

  const base::Optional<MockCrashEndpoint::Report>& report = last_report();
  ASSERT_TRUE(report);
  // The product name is escaped twice. The first time, it becomes
  // "Chrome%20(Chrome%20OS)" and then the second escapes the '%' into '%25'.
  EXPECT_EQ(report->query,
            "browser=Chrome&browser_version=1.2.3.4&channel=Stable&column=456&"
            "error_message=hi&full_url=http%3A%2F%2Fwww.test.com%2Ffoo"
            "&line=123&os=ChromeOS&os_version=" +
                GetOsVersion() +
                "&prod=Chrome%2520(Chrome%2520OS)&"
                "src=http%3A%2F%2Fwww.test.com%2Ffoo&"
                "type=JavascriptError&url=%2Ffoo&ver=1.0.0.0");
  EXPECT_EQ(report->content, "   at <anonymous>:1:1");
}

IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, StackTraceWithErrorMessage) {
  constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com/foo",
        product: 'TestApp',
        version: '1.0.0.0',
        lineNumber: 123,
        columnNumber: 456,
        stackTrace: 'hi'
      },
      () => window.domAutomationController.send(""));
  )";
  ExecuteScriptInBackgroundPage(browser_context(), extension_->id(),
                                kTestScript);

  const base::Optional<MockCrashEndpoint::Report>& report = last_report();
  ASSERT_TRUE(report);
  EXPECT_EQ(report->query,
            "browser=Chrome&browser_version=1.2.3.4&channel=Stable&column=456&"
            "error_message=hi&full_url=http%3A%2F%2Fwww.test.com%2Ffoo&"
            "line=123&os=ChromeOS&os_version=" +
                GetOsVersion() +
                "&prod=TestApp&src=http%3A%2F%2Fwww.test.com%2Ffoo&type="
                "JavascriptError&url=%2Ffoo&ver=1.0.0.0");
  EXPECT_EQ(report->content, "");
}

IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, RedactMessage) {
  // We use the feedback APIs redaction tool, which scrubs many different types
  // of PII. As a sanity check, test if MAC addresses are redacted.
  constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "06-00-00-00-00-00",
        url: "http://www.test.com/foo",
        product: 'TestApp',
        version: '1.0.0.0',
        lineNumber: 123,
        columnNumber: 456,
      },
      () => window.domAutomationController.send(""));
  )";
  ExecuteScriptInBackgroundPage(browser_context(), extension_->id(),
                                kTestScript);

  const base::Optional<MockCrashEndpoint::Report>& report = last_report();
  ASSERT_TRUE(report);
  EXPECT_EQ(report->query,
            "browser=Chrome&browser_version=1.2.3.4&channel=Stable&column=456&"
            "error_message=%5BMAC%20OUI%3D06%3A00%3A00%20IFACE%3D1%5D&"
            "full_url=http%3A%2F%2Fwww.test.com%2Ffoo&line=123&os=ChromeOS&"
            "os_version=" +
                GetOsVersion() +
                "&prod=TestApp&src=http%3A%2F%2Fwww.test.com%2Ffoo&type="
                "JavascriptError&url=%2Ffoo&ver=1.0.0.0");
  EXPECT_EQ(report->content, "");
}

IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, Throttling) {
  constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com",
      },
      () => {
        window.domAutomationController.send(chrome.runtime.lastError ?
            chrome.runtime.lastError.message : "")
      });
  )";

  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  api::SetClockForTesting(&test_clock);

  // Use an exact time for the first API call.
  EXPECT_EQ("", ExecuteScriptInBackgroundPage(browser_context(),
                                              extension_->id(), kTestScript));

  // API is limited to one call per hr. So pretend the second call is just
  // before 1 hr.
  test_clock.Advance(base::TimeDelta::FromMinutes(59));
  EXPECT_EQ("Too many calls to this API",
            ExecuteScriptInBackgroundPage(browser_context(), extension_->id(),
                                          kTestScript));

  // Call again after 1 hr.
  test_clock.Advance(base::TimeDelta::FromMinutes(2));
  EXPECT_EQ("", ExecuteScriptInBackgroundPage(browser_context(),
                                              extension_->id(), kTestScript));
}

// Ensures that reportError checks user consent for data collection on the
// correct thread and correctly handles the case where consent is not given.
IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, NoConsent) {
  constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com",
      },
      () => {
        window.domAutomationController.send(chrome.runtime.lastError ?
            chrome.runtime.lastError.message : "")
      });
  )";

  crash_endpoint_->set_consented(false);
  EXPECT_EQ("", ExecuteScriptInBackgroundPage(browser_context(),
                                              extension_->id(), kTestScript));
  // The server should not receive any reports.
  const base::Optional<MockCrashEndpoint::Report>& report = last_report();
  EXPECT_FALSE(report);
}

}  // namespace extensions
