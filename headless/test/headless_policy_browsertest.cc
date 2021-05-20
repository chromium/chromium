// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>
#include <string>
#include <tuple>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/policy/headless_mode_policy.h"
#include "headless/public/headless_browser.h"
#include "headless/test/headless_browser_test.h"
#include "headless/test/headless_policy_browsertest.h"
#include "net/base/host_port_pair.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

// The following enum values must match HeadlessMode policy template in
// components/policy/resources/policy_templates.json
enum {
  kHeadlessModePolicyEnabled = 1,
  kHeadlessModePolicyDisabled = 2,
  kHeadlessModePolicyUnset = -1,  // not in the template
};

class HeadlessBrowserTestWithHeadlessModePolicy
    : public HeadlessBrowserTestWithPolicy<HeadlessBrowserTest>,
      public testing::WithParamInterface<std::tuple<int, bool>> {
 protected:
  void SetPolicy() override {
    int headless_mode_policy = std::get<0>(GetParam());
    if (headless_mode_policy != kHeadlessModePolicyUnset) {
      SetHeadlessModePolicy(
          static_cast<policy::HeadlessModePolicy::HeadlessMode>(
              headless_mode_policy));
    }
  }

  void SetHeadlessModePolicy(
      policy::HeadlessModePolicy::HeadlessMode headless_mode) {
    policy::PolicyMap policy;
    policy.Set("HeadlessMode", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(headless_mode)),
               /*external_data_fetcher=*/nullptr);
    mock_provider_->UpdateChromePolicy(policy);
  }

  bool expected_enabled() { return std::get<1>(GetParam()); }
  bool actual_enabled() {
    return !policy::HeadlessModePolicy::IsHeadlessDisabled(GetPrefs());
  }
};

INSTANTIATE_TEST_CASE_P(
    HeadlessBrowserTestWithHeadlessModePolicy,
    HeadlessBrowserTestWithHeadlessModePolicy,
    testing::Values(std::make_tuple(kHeadlessModePolicyEnabled, true),
                    std::make_tuple(kHeadlessModePolicyDisabled, false),
                    std::make_tuple(kHeadlessModePolicyUnset, true)));

IN_PROC_BROWSER_TEST_P(HeadlessBrowserTestWithHeadlessModePolicy,
                       HeadlessModePolicySettings) {
  EXPECT_EQ(actual_enabled(), expected_enabled());
}

class HeadlessBrowserTestWithUrlBlockPolicy
    : public HeadlessBrowserTestWithPolicy<HeadlessBrowserTest> {
 protected:
  void SetPolicy() override {
    base::Value value(base::Value::Type::LIST);
    value.Append("*/blocked.html");

    policy::PolicyMap policy;
    policy.Set("URLBlocklist", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               std::move(value), /*external_data_fetcher=*/nullptr);
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
    : public HeadlessBrowserTestWithPolicy<HeadlessBrowserTest>,
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

  void SetUpInProcessBrowserTestFixture() override {
    HeadlessBrowserTestWithPolicy<
        HeadlessBrowserTest>::SetUpInProcessBrowserTestFixture();
    options()->devtools_endpoint = net::HostPortPair("localhost", 0);
    DeleteActiveRemoteDebuggingPortFile();
  }

  base::FilePath active_remote_debugging_port_file_path() {
    base::FilePath file_path =
        user_data_dir().Append(FILE_PATH_LITERAL("DevToolsActivePort"));
    return file_path;
  }

  void DeleteActiveRemoteDebuggingPortFile() {
    base::ScopedAllowBlockingForTesting allow_blocking_calls;
    base::DeleteFile(active_remote_debugging_port_file_path());
  }

  int GetActiveRemoteDebuggingPort() {
    std::string contents;
    {
      base::ScopedAllowBlockingForTesting allow_blocking_calls;
      if (!ReadFileToString(active_remote_debugging_port_file_path(),
                            &contents)) {
        return -1;
      }
    }

    std::vector<std::string> port_info = base::SplitString(
        contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (port_info.empty())
      return -1;

    int port = 0;
    if (!base::StringToInt(port_info[0], &port))
      return -1;

    return port;
  }

  bool expect_remote_debugging_available() { return GetParam(); }
};

INSTANTIATE_TEST_CASE_P(HeadlessBrowserTestWithRemoteDebuggingAllowedPolicy,
                        HeadlessBrowserTestWithRemoteDebuggingAllowedPolicy,
                        testing::Values(true, false));

// Remote debugging with ephemeral port is not working on Fuchsia, see
// crbug.com/1209251.
#if defined(OS_FUCHSIA)
#define MAYBE_RemoteDebuggingDisallowed DISABLED_RemoteDebuggingDisallowed
#else
#define MAYBE_RemoteDebuggingDisallowed RemoteDebuggingDisallowed
#endif
IN_PROC_BROWSER_TEST_P(HeadlessBrowserTestWithRemoteDebuggingAllowedPolicy,
                       MAYBE_RemoteDebuggingDisallowed) {
  bool has_remote_debugging_port = GetActiveRemoteDebuggingPort() > 0;
  EXPECT_EQ(has_remote_debugging_port, expect_remote_debugging_available());
}

}  // namespace headless
