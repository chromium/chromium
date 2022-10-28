// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/test/headless_policy_browsertest.h"

#include <fcntl.h>

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/policy/headless_mode_policy.h"
#include "headless/public/headless_browser.h"
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

INSTANTIATE_TEST_SUITE_P(
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

namespace {

class CaptureStdErr {
 public:
  CaptureStdErr() {
#if BUILDFLAG(IS_WIN)
    CHECK_EQ(_pipe(pipes_, 4096, O_BINARY), 0);
#else
    CHECK_EQ(pipe(pipes_), 0);
#endif
    stderr_ = dup(fileno(stderr));
    CHECK_NE(stderr_, -1);
  }

  ~CaptureStdErr() {
    StopCapture();
    close(pipes_[kReadPipe]);
    close(pipes_[kWritePipe]);
    close(stderr_);
  }

  void StartCapture() {
    if (capturing_)
      return;

    fflush(stderr);
    CHECK_NE(dup2(pipes_[kWritePipe], fileno(stderr)), -1);

    capturing_ = true;
  }

  void StopCapture() {
    if (!capturing_)
      return;

    char eop = kPipeEnd;
    CHECK_NE(write(pipes_[kWritePipe], &eop, sizeof(eop)), -1);

    fflush(stderr);
    CHECK_NE(dup2(stderr_, fileno(stderr)), -1);

    capturing_ = false;
  }

  std::string ReadCapturedData() {
    CHECK(!capturing_);

    std::string captured_data;
    for (;;) {
      constexpr size_t kChunkSize = 256;
      char buffer[kChunkSize];
      int bytes_read = read(pipes_[kReadPipe], buffer, kChunkSize);
      CHECK_NE(bytes_read, -1);
      captured_data.append(buffer, bytes_read);
      if (captured_data.rfind(kPipeEnd) != std::string::npos)
        break;
    }
    return captured_data;
  }

  std::vector<std::string> ReadCapturedLines() {
    return base::SplitString(ReadCapturedData(), "\n", base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  }

 private:
  enum { kReadPipe, kWritePipe };

  static constexpr char kPipeEnd = '\xff';

  base::ScopedAllowBlockingForTesting allow_blocking_calls_;

  bool capturing_ = false;
  int pipes_[2] = {-1, -1};
  int stderr_ = -1;
};

}  // namespace

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
    capture_stderr_.StartCapture();
  }

  bool expect_remote_debugging_available() { return GetParam(); }

  CaptureStdErr capture_stderr_;
};

INSTANTIATE_TEST_SUITE_P(HeadlessBrowserTestWithRemoteDebuggingAllowedPolicy,
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

  enum { kUnknown, kDisallowed, kListening } remote_debugging_state = kUnknown;
  for (const std::string& line : capture_stderr_.ReadCapturedLines()) {
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
