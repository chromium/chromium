// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/process_selection_deferring_condition_runner.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/process_selection_deferring_condition.h"
#include "content/public/browser/process_selection_user_data.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/mock_process_selection_deferring_condition_tester.h"
#include "content/test/test_content_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

namespace {

// A simple ProcessSelectionUserData::Data implementation for testing.
class ProcessSelectionTestData
    : public ProcessSelectionUserData::Data<ProcessSelectionTestData> {
 public:
  explicit ProcessSelectionTestData(int value) : value_(value) {}
  int value() const { return value_; }

 private:
  friend ProcessSelectionUserData::Data<ProcessSelectionTestData>;
  PROCESS_SELECTION_USER_DATA_KEY_DECL();
  int value_;
};

PROCESS_SELECTION_USER_DATA_KEY_IMPL(ProcessSelectionTestData);

class ProcessSelectionDeferringBrowserClient : public TestContentBrowserClient {
 public:
  ProcessSelectionDeferringBrowserClient() = default;
  ~ProcessSelectionDeferringBrowserClient() override = default;

  std::vector<std::unique_ptr<ProcessSelectionDeferringCondition>>
  CreateProcessSelectionDeferringConditionsForNavigation(
      NavigationHandle& navigation_handle) override {
    return std::move(conditions_);
  }

  void AddCondition(
      std::unique_ptr<ProcessSelectionDeferringCondition> condition) {
    conditions_.push_back(std::move(condition));
  }

 private:
  std::vector<std::unique_ptr<ProcessSelectionDeferringCondition>> conditions_;
};

}  // namespace

class ProcessSelectionDeferringConditionRunnerTest
    : public RenderViewHostTestHarness {
 public:
  ProcessSelectionDeferringConditionRunnerTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    browser_client_ =
        std::make_unique<ProcessSelectionDeferringBrowserClient>();
    old_browser_client_ = SetBrowserClientForTesting(browser_client_.get());

    navigation_ = NavigationSimulator::CreateRendererInitiated(
        GURL("https://example.com"), main_rfh());
    navigation_->Start();
  }
  void TearDown() override {
    SetBrowserClientForTesting(old_browser_client_);
    // Release the runner so that the NavigationHandle is released before
    // tearing down the test harness.
    runner_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  ProcessSelectionDeferringBrowserClient* browser_client() {
    return browser_client_.get();
  }

  ProcessSelectionDeferringConditionRunner* CreateRunner() {
    CHECK(navigation_);
    runner_ = ProcessSelectionDeferringConditionRunner::Create(
        *NavigationRequest::From(navigation_->GetNavigationHandle()));
    return runner_.get();
  }

  NavigationHandle* navigation_handle() {
    return navigation_->GetNavigationHandle();
  }

 private:
  std::unique_ptr<ProcessSelectionDeferringBrowserClient> browser_client_;
  raw_ptr<ContentBrowserClient> old_browser_client_ = nullptr;
  std::unique_ptr<NavigationSimulator> navigation_;
  std::unique_ptr<ProcessSelectionDeferringConditionRunner> runner_;
};

TEST_F(ProcessSelectionDeferringConditionRunnerTest,
       NoConditionsRunsTheProvidedCallback) {
  auto* runner = CreateRunner();
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  runner->WillSelectFinalProcess(callback.Get());
}

TEST_F(ProcessSelectionDeferringConditionRunnerTest,
       DoesNotCallCallbackUntilDeferringConditionResumes) {
  MockProcessSelectionDeferringConditionTester condition(
      *navigation_handle(), ProcessSelectionDeferringCondition::Result::kDefer);
  browser_client()->AddCondition(condition.Release());
  auto* runner = CreateRunner();

  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run()).Times(0);
  runner->WillSelectFinalProcess(callback.Get());

  // The callback should not be called because the condition should be
  // deferring.
  EXPECT_TRUE(condition.WasOnWillSelectFinalProcessCalled());
  testing::Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, Run());
  condition.CallResumeClosure();
}

TEST_F(ProcessSelectionDeferringConditionRunnerTest,
       ProceedConditionCallbackIsCalledSynchronously) {
  MockProcessSelectionDeferringConditionTester condition(
      *navigation_handle(),
      ProcessSelectionDeferringCondition::Result::kProceed);

  browser_client()->AddCondition(condition.Release());
  auto* runner = CreateRunner();
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  runner->WillSelectFinalProcess(callback.Get());

  EXPECT_TRUE(condition.WasOnWillSelectFinalProcessCalled());
}

TEST_F(ProcessSelectionDeferringConditionRunnerTest,
       ProvidedCallbackIsRunOnlyAfterAllConditionsAreDone) {
  MockProcessSelectionDeferringConditionTester condition1(
      *navigation_handle(), ProcessSelectionDeferringCondition::Result::kDefer);
  MockProcessSelectionDeferringConditionTester condition2(
      *navigation_handle(), ProcessSelectionDeferringCondition::Result::kDefer);

  browser_client()->AddCondition(condition1.Release());
  browser_client()->AddCondition(condition2.Release());
  auto* runner = CreateRunner();

  base::MockCallback<base::OnceClosure> callback;
  runner->WillSelectFinalProcess(callback.Get());
  EXPECT_CALL(callback, Run()).Times(0);
  EXPECT_TRUE(condition1.WasOnWillSelectFinalProcessCalled());
  EXPECT_FALSE(condition2.WasOnWillSelectFinalProcessCalled());

  condition1.CallResumeClosure();
  EXPECT_TRUE(condition2.WasOnWillSelectFinalProcessCalled());

  // The callback should not have been called yet.
  testing::Mock::VerifyAndClearExpectations(&callback);

  // The callback should run after condition2 resumes.
  EXPECT_CALL(callback, Run());
  condition2.CallResumeClosure();
}

TEST_F(ProcessSelectionDeferringConditionRunnerTest,
       ProceedConditionAfterDeferConditionCallsCallback) {
  MockProcessSelectionDeferringConditionTester condition1(
      *navigation_handle(), ProcessSelectionDeferringCondition::Result::kDefer);
  MockProcessSelectionDeferringConditionTester condition2(
      *navigation_handle(),
      ProcessSelectionDeferringCondition::Result::kProceed);

  browser_client()->AddCondition(condition1.Release());
  browser_client()->AddCondition(condition2.Release());
  auto* runner = CreateRunner();

  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run()).Times(0);
  runner->WillSelectFinalProcess(callback.Get());
  EXPECT_TRUE(condition1.WasOnWillSelectFinalProcessCalled());
  EXPECT_FALSE(condition2.WasOnWillSelectFinalProcessCalled());

  // The callback should not have been called yet.
  testing::Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, Run());
  condition1.CallResumeClosure();
  EXPECT_TRUE(condition2.WasOnWillSelectFinalProcessCalled());
}

TEST_F(ProcessSelectionDeferringConditionRunnerTest,
       OnRequestRedirectedCallsOnRequestRedirectedHandlers) {
  MockProcessSelectionDeferringConditionTester condition1(
      *navigation_handle(),
      ProcessSelectionDeferringCondition::Result::kProceed);
  MockProcessSelectionDeferringConditionTester condition2(
      *navigation_handle(),
      ProcessSelectionDeferringCondition::Result::kProceed);

  browser_client()->AddCondition(condition1.Release());
  browser_client()->AddCondition(condition2.Release());
  auto* runner = CreateRunner();
  runner->OnRequestRedirected();
  // All conditions should have their request redirected call count incremented.
  EXPECT_EQ(condition1.GetOnRequestRedirectedCallCount(), 1);
  EXPECT_EQ(condition2.GetOnRequestRedirectedCallCount(), 1);

  // Redirecting again should call the handlers again.
  runner->OnRequestRedirected();
  EXPECT_EQ(condition1.GetOnRequestRedirectedCallCount(), 2);
  EXPECT_EQ(condition2.GetOnRequestRedirectedCallCount(), 2);
}

TEST_F(ProcessSelectionDeferringConditionRunnerTest,
       ConditionCanSetProcessSelectionUserData) {
  // Define a simple `ProcessSelectionDeferringCondition` that sets a value on
  // its `ProcessSelectionUserData`.
  class ProcessSelectionDataSettingCondition
      : public ProcessSelectionDeferringCondition {
   public:
    explicit ProcessSelectionDataSettingCondition(
        NavigationHandle& navigation_handle)
        : ProcessSelectionDeferringCondition(navigation_handle) {}

    Result OnWillSelectFinalProcess(base::OnceClosure resume) override {
      ProcessSelectionUserData& user_data = GetProcessSelectionUserData();
      user_data.SetUserData(ProcessSelectionTestData::UserDataKey(),
                            std::make_unique<ProcessSelectionTestData>(42));
      return Result::kProceed;
    }
  };

  browser_client()->AddCondition(
      std::make_unique<ProcessSelectionDataSettingCondition>(
          *navigation_handle()));
  auto* runner = CreateRunner();
  base::MockCallback<base::OnceClosure> callback;
  EXPECT_CALL(callback, Run());
  runner->WillSelectFinalProcess(callback.Get());

  const ProcessSelectionTestData* retrieved_data =
      ProcessSelectionTestData::FromProcessSelectionUserData(
          navigation_handle()->GetProcessSelectionUserData());
  EXPECT_EQ(42, retrieved_data->value());
}

}  // namespace content
