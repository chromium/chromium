// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "components/headless/policy/headless_mode_policy.h"
#include "components/headless/test/capture_std_stream.h"
#include "components/policy/core/browser/browser_policy_connector_base.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_browser_impl.h"
#include "headless/public/headless_browser.h"
#include "headless/public/switches.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_browser_test_utils.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_WIN)
#include <unistd.h>
#endif

namespace headless {

// The following enum values must match HeadlessMode policy template in
// components/policy/resources/templates/policy_definitions/Miscellaneous/HeadlessMode.yaml
enum {
  kHeadlessModePolicyEnabled = 1,
  kHeadlessModePolicyDisabled = 2,
  kHeadlessModePolicyUnset = -1,  // not in the template
};

class HeadlessBrowserTestWithPolicy : public HeadlessBrowserTest {
 protected:
  // Implement to set policies before headless browser is instantiated.
  virtual void SetPolicy() {}

  void SetUp() override {
    mock_provider_ = std::make_unique<
        testing::NiceMock<policy::MockConfigurationPolicyProvider>>();
    mock_provider_->SetDefaultReturns(
        /*is_initialization_complete_return=*/false,
        /*is_first_policy_load_complete_return=*/false);
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(
        mock_provider_.get());
    SetPolicy();
    HeadlessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    command_line->AppendSwitchPath(switches::kUserDataDir,
                                   user_data_dir_.GetPath());
  }

  void TearDown() override {
    HeadlessBrowserTest::TearDown();
    mock_provider_->Shutdown();
    policy::BrowserPolicyConnectorBase::SetPolicyProviderForTesting(nullptr);
  }

  PrefService* GetPrefs() {
    return static_cast<HeadlessBrowserImpl*>(browser())->GetPrefs();
  }

  base::ScopedTempDir user_data_dir_;
  std::unique_ptr<policy::MockConfigurationPolicyProvider> mock_provider_;
};

class HeadlessBrowserTestWithHeadlessModePolicy
    : public HeadlessBrowserTestWithPolicy,
      public testing::WithParamInterface<std::tuple<int, bool>> {
 protected:
  void SetPolicy() override {
    int headless_mode_policy = std::get<0>(GetParam());
    if (headless_mode_policy != kHeadlessModePolicyUnset) {
      SetHeadlessModePolicy(
          static_cast<HeadlessModePolicy::HeadlessMode>(headless_mode_policy));
    }
  }

  void SetHeadlessModePolicy(HeadlessModePolicy::HeadlessMode headless_mode) {
    policy::PolicyMap policy;
    policy.Set("HeadlessMode", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(headless_mode)),
               /*external_data_fetcher=*/nullptr);
    mock_provider_->UpdateChromePolicy(policy);
  }

  bool expected_enabled() { return std::get<1>(GetParam()); }
  bool actual_enabled() {
    return !HeadlessModePolicy::IsHeadlessModeDisabled(GetPrefs());
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    HeadlessBrowserTestWithHeadlessModePolicy,
    testing::Values(std::make_tuple(kHeadlessModePolicyEnabled, true),
                    std::make_tuple(kHeadlessModePolicyDisabled, false),
                    std::make_tuple(kHeadlessModePolicyUnset, true)));

IN_PROC_BROWSER_TEST_P(HeadlessBrowserTestWithHeadlessModePolicy,
                       HeadlessModePolicySettings) {
  EXPECT_EQ(actual_enabled(), expected_enabled());
}

class HeadlessBrowserTestWithUrlBlockPolicy
    : public HeadlessBrowserTestWithPolicy {
 protected:
  void SetPolicy() override {
    base::Value::List value;
    value.Append("*/blocked.html");

    policy::PolicyMap policy;
    policy.Set("URLBlocklist", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(std::move(value)),
               /*external_data_fetcher=*/nullptr);
    mock_provider_->UpdateChromePolicy(policy);
  }
};

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTestWithUrlBlockPolicy, BlockUrl) {
  EXPECT_TRUE(embedded_test_server()->Start());

  HeadlessBrowserContext* browser_context =
      browser()->CreateBrowserContextBuilder().Build();

  GURL url = embedded_test_server()->GetURL("/blocked.html");
  HeadlessWebContents* web_contents =
      browser_context->CreateWebContentsBuilder().SetInitialURL(url).Build();

  net::Error error = net::OK;
  EXPECT_FALSE(WaitForLoad(web_contents, &error));
  EXPECT_EQ(error, net::ERR_BLOCKED_BY_ADMINISTRATOR);
}

class HeadlessBrowserTestWithRemoteDebuggingAllowedPolicy
    : public HeadlessBrowserTestWithPolicy,
      public testing::WithParamInterface<bool> {
 protected:
  void SetPolicy() override {
    policy::PolicyMap policy;
    policy.Set("RemoteDebuggingAllowed", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(expect_remote_debugging_available()),
               /*external_data_fetcher=*/nullptr);
    mock_provider_->UpdateChromePolicy(policy);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    HeadlessBrowserTestWithPolicy::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kRemoteDebuggingPort, "0");
  }

  void SetUpInProcessBrowserTestFixture() override {
    HeadlessBrowserTestWithPolicy::SetUpInProcessBrowserTestFixture();
    capture_stderr_.StartCapture();
  }

  bool expect_remote_debugging_available() { return GetParam(); }

  CaptureStdErr capture_stderr_;
};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         HeadlessBrowserTestWithRemoteDebuggingAllowedPolicy,
                         testing::Values(true, false));

// Remote debugging with ephemeral port is not working on Fuchsia, see
// crbug.com/1209251.
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_RemoteDebuggingDisallowed DISABLED_RemoteDebuggingDisallowed
#else
#define MAYBE_RemoteDebuggingDisallowed RemoteDebuggingDisallowed
#endif
IN_PROC_BROWSER_TEST_P(HeadlessBrowserTestWithRemoteDebuggingAllowedPolicy,
                       MAYBE_RemoteDebuggingDisallowed) {
  // DevTools starts its remote debugging port listener asynchronously and
  // there is no reliable way to know when it is started, so resort to an
  // ugly wait then check captured stderr.
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());
  capture_stderr_.StopCapture();

  std::vector<std::string> captured_lines =
      base::SplitString(capture_stderr_.TakeCapturedData(), "\n",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  enum { kUnknown, kDisallowed, kListening } remote_debugging_state = kUnknown;
  for (const std::string& line : captured_lines) {
    LOG(INFO) << "stderr: " << line;
    if (base::MatchPattern(line, "DevTools remote debugging is disallowed *")) {
      EXPECT_EQ(remote_debugging_state, kUnknown);
      remote_debugging_state = kDisallowed;
    } else if (base::MatchPattern(line, "DevTools listening on *")) {
      EXPECT_EQ(remote_debugging_state, kUnknown);
      remote_debugging_state = kListening;
    }
  }

  EXPECT_NE(remote_debugging_state, kUnknown);

  if (expect_remote_debugging_available())
    EXPECT_EQ(remote_debugging_state, kListening);
  else
    EXPECT_EQ(remote_debugging_state, kDisallowed);
}

}  // namespace headless
