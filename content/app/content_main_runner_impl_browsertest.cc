// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/scoped_field_trial_list_resetter.h"
#include "build/buildflag.h"
#include "content/browser/startup_helper.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/gpu/content_gpu_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/utility/content_utility_client.h"
#include "sandbox/policy/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace variations {
class VariationsIdsProvider;
};

namespace content {

class ContentClient;

namespace {

using ::testing::_;
using ::testing::AtMost;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using InvokedIn = ContentMainDelegate::InvokedIn;

#if BUILDFLAG(IS_ANDROID)
// TODO(joenotcharles): Find out why this test crashes on Android, which uses
// custom startup code in BrowserTestBase::SetUp instead of calling ContentMain.
#error This test is not supported on Android.
#endif

// Mocks only the cross-platform methods of ContentMainDelegate.
class MockContentMainDelegate : public ContentMainDelegate {
 public:
  MOCK_METHOD(bool, BasicStartupComplete, (int*), (override));
  MOCK_METHOD(void, PreSandboxStartup, (), (override));
  MOCK_METHOD(void, SandboxInitialized, (const std::string&), (override));
  MOCK_METHOD((absl::variant<int, MainFunctionParams>),
              RunProcess,
              (const std::string&, MainFunctionParams),
              (override));
  MOCK_METHOD(void, ProcessExiting, (const std::string&), (override));
  MOCK_METHOD(int, TerminateForFatalInitializationError, (), (override));
  MOCK_METHOD(bool, ShouldLockSchemeRegistry, (), (override));
  MOCK_METHOD(void, PreBrowserMain, (), (override));
  MOCK_METHOD(bool, ShouldCreateFeatureList, (InvokedIn), (override));
  MOCK_METHOD(variations::VariationsIdsProvider*,
              CreateVariationsIdsProvider,
              (),
              (override));
  MOCK_METHOD(void, PostEarlyInitialization, (InvokedIn), (override));
  MOCK_METHOD(ContentClient*, CreateContentClient, (), (override));
  MOCK_METHOD(ContentBrowserClient*,
              CreateContentBrowserClient,
              (),
              (override));
  MOCK_METHOD(ContentGpuClient*, CreateContentGpuClient, (), (override));
  MOCK_METHOD(ContentRendererClient*,
              CreateContentRendererClient,
              (),
              (override));
  MOCK_METHOD(ContentUtilityClient*,
              CreateContentUtilityClient,
              (),
              (override));
};

// Parameters to control the expectations for MockContentMainDelegate. Each test
// case uses a different set of parameters, since the expectations need to be
// installed before SetUp() is called.
struct MockExpectations {
  bool exit_after_basic_startup = false;
  bool content_main_should_create_feature_list = true;
};

std::ostream& operator<<(std::ostream& os, const MockExpectations& m) {
  return os << "exit_after_basic_startup:" << m.exit_after_basic_startup
            << ",content_main_should_create_feature_list:"
            << m.content_main_should_create_feature_list;
}

// Tests that methods of ContentMainDelegate are called in the expected order.
// The string parameter is the process name (empty for the browser process).
class ContentMainRunnerImplBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, MockExpectations>> {
 protected:
  ContentMainRunnerImplBrowserTest() {
    std::string process_type;
    std::tie(process_type, mock_expectations_) = GetParam();

    // Start without a feature list so the startup sequence can create one.
    // `scoped_field_trial_list_resetter_` will do the same for the field trial
    // list.
    original_feature_list_ = base::FeatureList::ClearInstanceForTesting();

    const bool is_browser_process = process_type.empty();
    if (!is_browser_process) {
      // Fool ContentMain() into thinking this is a different process type.
      base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
      command_line->AppendSwitchASCII(switches::kProcessType, process_type);
      command_line->AppendSwitch(sandbox::policy::switches::kNoSandbox);
    }
    const InvokedIn invoked_in = is_browser_process
                                     ? InvokedIn::kBrowserProcessUnderTest
                                     : InvokedIn::kChildProcess;

    // These methods may or may not be called, depending on configuration.
    EXPECT_CALL(mock_delegate_, ShouldLockSchemeRegistry()).Times(AtMost(1));
    EXPECT_CALL(mock_delegate_, CreateVariationsIdsProvider()).Times(AtMost(1));
    // CreateContentClient() is only called if GetContentClient() returns null.
    EXPECT_CALL(mock_delegate_, CreateContentClient()).Times(AtMost(1));

    // Expect the following entry points to be called, in order.
    //
    // BrowserTestBase::SetUp() calls ContentMain(), which instantiates a
    // ContentMainRunnerImpl, which calls the entry points in
    // ContentMainDelegate. So test expectations must be installed before
    // calling the inherited SetUp().
    ::testing::InSequence s;
    EXPECT_CALL(mock_delegate_, BasicStartupComplete(_))
        .WillOnce(DoAll(
            // Set the exit code.
            SetArgPointee<0>(0),
            // Set the return value.
            Return(mock_expectations_.exit_after_basic_startup)));
    if (mock_expectations_.exit_after_basic_startup) {
      // Expect no more calls.
      return;
    }

    if (is_browser_process) {
      EXPECT_CALL(mock_delegate_, CreateContentBrowserClient())
          .WillOnce(Return(&content_browser_client_));
    } else if (process_type == switches::kGpuProcess) {
      EXPECT_CALL(mock_delegate_, CreateContentGpuClient())
          .WillOnce(Return(&content_gpu_client_));
    } else if (process_type == switches::kRendererProcess) {
      EXPECT_CALL(mock_delegate_, CreateContentRendererClient())
          .WillOnce(Return(&content_renderer_client_));
    } else {
      EXPECT_CALL(mock_delegate_, CreateContentUtilityClient())
          .WillOnce(Return(&content_utility_client_));
    }
    EXPECT_CALL(mock_delegate_, PreSandboxStartup());
    EXPECT_CALL(mock_delegate_, SandboxInitialized(process_type));
    EXPECT_CALL(mock_delegate_, ShouldCreateFeatureList(invoked_in))
        .WillOnce(
            Return(mock_expectations_.content_main_should_create_feature_list));
    // PreBrowserMain is only called in the browser process.
    EXPECT_CALL(mock_delegate_, PreBrowserMain())
        .Times(is_browser_process ? 1 : 0);
    EXPECT_CALL(mock_delegate_, PostEarlyInitialization(invoked_in))
        .WillOnce(Invoke(
            this, &ContentMainRunnerImplBrowserTest::PostEarlyInitialization));
    EXPECT_CALL(mock_delegate_, RunProcess(process_type, _))
        .WillOnce(Return(ByMove(0)));
    EXPECT_CALL(mock_delegate_, ProcessExiting(process_type));
  }

  ~ContentMainRunnerImplBrowserTest() override {
    // Restore the original feature list for other tests. Any temporary
    // FeatureList created during the test (by ContentMainRunnerImpl or
    // PostEarlyInitialization, depending on the value of the
    // `content_main_should_create_feature_list` parameter) must be removed
    // first.
    base::FeatureList::ClearInstanceForTesting();
    base::FeatureList::RestoreInstanceForTesting(
        std::move(original_feature_list_));
  }

  ContentMainDelegate* GetCustomContentMainDelegate() override {
    return &mock_delegate_;
  }

  void PostEarlyInitialization() {
    ASSERT_TRUE(base::ThreadPoolInstance::Get());
    if (mock_expectations_.content_main_should_create_feature_list) {
      ASSERT_TRUE(base::FeatureList::GetInstance());
    } else {
      ASSERT_FALSE(base::FeatureList::GetInstance());

      // ContentMainRunnerImpl will try to use the feature list after
      // PostEarlyInitialization, so we need to create one.
      field_trials_ = SetUpFieldTrialsAndFeatureList();
    }
  }

  ::testing::StrictMock<MockContentMainDelegate> mock_delegate_;
  MockExpectations mock_expectations_;

  // Stubs to return from CreateContent*Client mocks. These will be deleted at
  // the end of the test to satisfy the leak checker.
  ContentBrowserClient content_browser_client_;
  ContentGpuClient content_gpu_client_;
  ContentRendererClient content_renderer_client_;
  ContentUtilityClient content_utility_client_;

  // Ensure that the test can create its own `field_trials_`.
  base::test::ScopedFieldTrialListResetter scoped_field_trial_list_resetter_;
  std::unique_ptr<base::FeatureList> original_feature_list_;
  std::unique_ptr<base::FieldTrialList> field_trials_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ContentMainRunnerImplBrowserTest,
    ::testing::Combine(
        ::testing::Values("",
                          switches::kGpuProcess,
                          switches::kRendererProcess,
                          switches::kUtilityProcess),
        ::testing::Values(MockExpectations{},
                          MockExpectations{
                              .content_main_should_create_feature_list = false},
                          MockExpectations{.exit_after_basic_startup = true})));

IN_PROC_BROWSER_TEST_P(ContentMainRunnerImplBrowserTest, StartupSequence) {
  // All of the work is done in the test suite constructor and SetUp().
}

}  // namespace

}  // namespace content
