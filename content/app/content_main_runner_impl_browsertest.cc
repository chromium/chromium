// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/overloaded.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "build/build_config.h"
#include "content/public/app/content_main_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/common/main_function_params.h"
#include "content/public/gpu/content_gpu_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_shell_main_delegate.h"
#include "content/public/utility/content_utility_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace variations {
class VariationsIdsProvider;
}

namespace content {

namespace {

using ::testing::_;
using ::testing::AtMost;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using InvokedIn = ContentMainDelegate::InvokedIn;
using VariationsIdsProvider = variations::VariationsIdsProvider;

// Mocks only the cross-platform methods of ContentMainDelegate. Also calls the
// parent implementation of each method, since the test setup may depend on it.
class MockContentMainDelegate : public ContentBrowserTestShellMainDelegate {
 public:
  using Super = ContentBrowserTestShellMainDelegate;

  MOCK_METHOD(std::optional<int>, MockBasicStartupComplete, ());
  std::optional<int> BasicStartupComplete() override {
    std::optional<int> result = MockBasicStartupComplete();
    // Check for early exit code.
    if (result.has_value())
      return result;
    return Super::BasicStartupComplete();
  }

  MOCK_METHOD(void, MockPreSandboxStartup, ());
  void PreSandboxStartup() override {
    MockPreSandboxStartup();
    Super::PreSandboxStartup();
  }

  MOCK_METHOD(void, MockSandboxInitialized, (const std::string&));
  void SandboxInitialized(const std::string& process_type) override {
    MockSandboxInitialized(process_type);
    Super::SandboxInitialized(process_type);
  }

  // The return value of RunProcess is platform-dependent and the startup
  // sequence depends heavily on it, so don't allow it to be mocked.
  MOCK_METHOD(void, MockRunProcess, (const std::string&, MainFunctionParams));
  absl::variant<int, MainFunctionParams> RunProcess(
      const std::string& process_type,
      MainFunctionParams main_function_params) override {
    // MainFunctionParams is move-only so pass a dummy to the mock.
    MainFunctionParams dummy_main_function_params(
        base::CommandLine::ForCurrentProcess());
    MockRunProcess(process_type, std::move(dummy_main_function_params));
    return Super::RunProcess(process_type, std::move(main_function_params));
  }

  MOCK_METHOD(void, MockProcessExiting, (const std::string&));
  void ProcessExiting(const std::string& process_type) override {
    MockProcessExiting(process_type);
    Super::ProcessExiting(process_type);
  }

  // The return value of ShouldLockSchemeRegistry is dangerous to override so
  // don't allow it to be mocked.
  MOCK_METHOD(void, MockShouldLockSchemeRegistry, ());
  bool ShouldLockSchemeRegistry() override {
    MockShouldLockSchemeRegistry();
    return Super::ShouldLockSchemeRegistry();
  }

  MOCK_METHOD(std::optional<int>, MockPreBrowserMain, ());
  std::optional<int> PreBrowserMain() override {
    std::optional<int> result = MockPreBrowserMain();
    // Check for early exit code.
    if (result.has_value())
      return result;
    return Super::PreBrowserMain();
  }

  // No need to call the parent delegate for these methods since they have no
  // side effects.
  MOCK_METHOD(bool, ShouldCreateFeatureList, (InvokedIn), (override));
  MOCK_METHOD(bool, ShouldInitializeMojo, (InvokedIn), (override));

  MOCK_METHOD(VariationsIdsProvider*, MockCreateVariationsIdsProvider, ());
  VariationsIdsProvider* CreateVariationsIdsProvider() override {
    VariationsIdsProvider* result = MockCreateVariationsIdsProvider();
    if (result)
      return result;
    return Super::CreateVariationsIdsProvider();
  }

  MOCK_METHOD(std::optional<int>, MockPostEarlyInitialization, (InvokedIn));
  std::optional<int> PostEarlyInitialization(InvokedIn invoked_in) override {
    std::optional<int> result = MockPostEarlyInitialization(invoked_in);
    // Check for early exit code.
    if (result.has_value())
      return result;
    return Super::PostEarlyInitialization(invoked_in);
  }

  MOCK_METHOD(ContentClient*, MockCreateContentClient, ());
  ContentClient* CreateContentClient() override {
    ContentClient* result = MockCreateContentClient();
    if (result)
      return result;
    return Super::CreateContentClient();
  }

  MOCK_METHOD(ContentBrowserClient*, MockCreateContentBrowserClient, ());
  ContentBrowserClient* CreateContentBrowserClient() override {
    ContentBrowserClient* result = MockCreateContentBrowserClient();
    if (result)
      return result;
    return Super::CreateContentBrowserClient();
  }

  MOCK_METHOD(ContentGpuClient*, MockCreateContentGpuClient, ());
  ContentGpuClient* CreateContentGpuClient() override {
    ContentGpuClient* result = MockCreateContentGpuClient();
    if (result)
      return result;
    return Super::CreateContentGpuClient();
  }

  MOCK_METHOD(ContentRendererClient*, MockCreateContentRendererClient, ());
  ContentRendererClient* CreateContentRendererClient() override {
    ContentRendererClient* result = MockCreateContentRendererClient();
    if (result)
      return result;
    return Super::CreateContentRendererClient();
  }

  MOCK_METHOD(ContentUtilityClient*, MockCreateContentUtilityClient, ());
  ContentUtilityClient* CreateContentUtilityClient() override {
    ContentUtilityClient* result = MockCreateContentUtilityClient();
    if (result)
      return result;
    return Super::CreateContentUtilityClient();
  }
};

MATCHER_P(InvokedInMatcher, process_type, "") {
  // `arg` is an absl::variant. Return true if the type held by the variant is
  // correct for `process_type` (empty means the browser process).
  return absl::visit(base::Overloaded{
                         [&](ContentMainDelegate::InvokedInBrowserProcess) {
                           return process_type.empty();
                         },
                         [&](ContentMainDelegate::InvokedInChildProcess) {
                           return !process_type.empty();
                         },
                     },
                     arg);
}

// Tests that methods of ContentMainDelegate are called in the expected order.
class ContentMainRunnerImplBrowserTest : public ContentBrowserTest {
 protected:
  using Self = ContentMainRunnerImplBrowserTest;
  using Super = ContentBrowserTest;

  void SetUp() override {
    // Empty process name means the browser process.
    const std::string kBrowserProcessType = "";

    // These methods may or may not be called, depending on configuration.
    EXPECT_CALL(mock_delegate_, MockShouldLockSchemeRegistry())
        .Times(AtMost(1));
    EXPECT_CALL(mock_delegate_, MockCreateVariationsIdsProvider())
        .Times(AtMost(1));
    // CreateContentClient() is only called if GetContentClient() returns null.
    EXPECT_CALL(mock_delegate_, MockCreateContentClient()).Times(AtMost(1));

    // ContentBrowserTestShellMainDelegate calls these internally, so allow
    // extra calls to them out of sequence.
    EXPECT_CALL(mock_delegate_, ShouldCreateFeatureList(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(mock_delegate_, ShouldInitializeMojo(_))
        .WillRepeatedly(Return(true));

    // Expect the following entry points to be called, in order.
    //
    // BrowserTestBase::SetUp() calls ContentMain(), which instantiates a
    // ContentMainRunnerImpl, which calls the entry points in
    // ContentMainDelegate. So test expectations must be installed before
    // calling the inherited SetUp().
    ::testing::InSequence s;
    EXPECT_CALL(mock_delegate_, MockBasicStartupComplete())
        .WillOnce(DoAll(
            // Test the starting state of ContentMainRunnerImpl.
            Invoke(this, &Self::TestBasicStartupComplete),
            Return(std::nullopt)));
    EXPECT_CALL(mock_delegate_, MockCreateContentBrowserClient());
    EXPECT_CALL(mock_delegate_, MockPreSandboxStartup());
    EXPECT_CALL(mock_delegate_, MockSandboxInitialized(kBrowserProcessType));
    EXPECT_CALL(mock_delegate_,
                ShouldCreateFeatureList(InvokedInMatcher(kBrowserProcessType)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_delegate_,
                ShouldInitializeMojo(InvokedInMatcher(kBrowserProcessType)))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_delegate_, MockPreBrowserMain())
        .WillOnce(Return(std::nullopt));
    EXPECT_CALL(mock_delegate_, MockPostEarlyInitialization(
                                    InvokedInMatcher(kBrowserProcessType)))
        .WillOnce(DoAll(Invoke(this, &Self::TestPostEarlyInitialization),
                        Return(std::nullopt)));
    EXPECT_CALL(mock_delegate_, MockRunProcess(kBrowserProcessType, _));
#if !BUILDFLAG(IS_ANDROID)
    // Android never calls ProcessExiting, since it leaks its ContentMainRunner
    // and ProcessExiting is called from the destructor.
    EXPECT_CALL(mock_delegate_, MockProcessExiting(kBrowserProcessType));
#endif

    // This will call ContentMain(), which should satisfy the expectations
    // above.
    Super::SetUp();
  }

  ContentMainDelegate* GetOptionalContentMainDelegateOverride() override {
    return &mock_delegate_;
  }

  void TestBasicStartupComplete() {
    // The PostEarlyInitialization test checks that ContentMainRunnerImpl set up
    // the FeatureList. This test is invalid if it already exists
    // before starting.
    EXPECT_FALSE(base::FeatureList::GetInstance());
  }

  void TestPostEarlyInitialization() {
    // ContentMainRunnerImpl should have set up the ThreadPoolInstance and
    // FeatureList by this point.
    EXPECT_TRUE(base::ThreadPoolInstance::Get());
    EXPECT_TRUE(base::FeatureList::GetInstance());
  }

  ::testing::StrictMock<MockContentMainDelegate> mock_delegate_;
};

IN_PROC_BROWSER_TEST_F(ContentMainRunnerImplBrowserTest, StartupSequence) {
  // All of the work is done in SetUp().
}

}  // namespace

}  // namespace content
