// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/navigation_simulator.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

// This class defers a navigation via a no-op async task on the provided task
// runner.
class TaskRunnerDeferringThrottle : public NavigationThrottle {
 public:
  TaskRunnerDeferringThrottle(scoped_refptr<base::TaskRunner> task_runner,
                              NavigationHandle* handle)
      : NavigationThrottle(handle), task_runner_(std::move(task_runner)) {}
  ~TaskRunnerDeferringThrottle() override {}

  static std::unique_ptr<NavigationThrottle> Create(
      scoped_refptr<base::TaskRunner> task_runner,
      NavigationHandle* handle) {
    return base::WrapUnique(
        new TaskRunnerDeferringThrottle(std::move(task_runner), handle));
  }

  // NavigationThrottle:
  ThrottleCheckResult WillStartRequest() override { return DeferToPostTask(); }
  ThrottleCheckResult WillRedirectRequest() override {
    return DeferToPostTask();
  }
  ThrottleCheckResult WillProcessResponse() override {
    return DeferToPostTask();
  }
  const char* GetNameForLogging() override {
    return "TaskRunnerDeferringThrottle";
  }

 private:
  ThrottleCheckResult DeferToPostTask() {
    task_runner_->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindOnce(&TaskRunnerDeferringThrottle::Resume,
                       weak_factory_.GetWeakPtr()));

    return NavigationThrottle::DEFER;
  }
  scoped_refptr<base::TaskRunner> task_runner_;
  base::WeakPtrFactory<TaskRunnerDeferringThrottle> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(TaskRunnerDeferringThrottle);
};

class NavigationSimulatorTest : public RenderViewHostImplTestHarness {};

class CancellingNavigationSimulatorTest
    : public RenderViewHostImplTestHarness,
      public WebContentsObserver,
      public testing::WithParamInterface<
          std::tuple<base::Optional<TestNavigationThrottle::ThrottleMethod>,
                     TestNavigationThrottle::ResultSynchrony>> {
 public:
  CancellingNavigationSimulatorTest() {}
  ~CancellingNavigationSimulatorTest() override {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    Observe(RenderViewHostImplTestHarness::web_contents());
    std::tie(cancel_time_, sync_) = GetParam();
    simulator_ = NavigationSimulator::CreateRendererInitiated(
        GURL("https://example.test"), main_rfh());
  }

  void TearDown() override {
    EXPECT_TRUE(did_finish_navigation_);
    RenderViewHostImplTestHarness::TearDown();
  }

  void DidStartNavigation(content::NavigationHandle* handle) override {
    auto throttle = std::make_unique<TestNavigationThrottle>(handle);
    throttle->SetCallback(
        TestNavigationThrottle::WILL_FAIL_REQUEST,
        base::BindRepeating(
            &CancellingNavigationSimulatorTest::OnWillFailRequestCalled,
            base::Unretained(this)));
    if (cancel_time_.has_value()) {
      throttle->SetResponse(cancel_time_.value(), sync_,
                            NavigationThrottle::CANCEL);
    }
    handle->RegisterThrottleForTesting(
        std::unique_ptr<TestNavigationThrottle>(std::move(throttle)));
  }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    did_finish_navigation_ = true;
    if (handle->GetResponseHeaders()) {
      response_headers_ = handle->GetResponseHeaders()->raw_headers();
    }
  }

  void OnWillFailRequestCalled() { will_fail_request_called_ = true; }

  base::Optional<TestNavigationThrottle::ThrottleMethod> cancel_time_;
  TestNavigationThrottle::ResultSynchrony sync_;
  std::unique_ptr<NavigationSimulator> simulator_;
  bool did_finish_navigation_ = false;
  bool will_fail_request_called_ = false;
  std::string response_headers_;
  base::WeakPtrFactory<CancellingNavigationSimulatorTest> weak_ptr_factory_{
      this};

 private:
  DISALLOW_COPY_AND_ASSIGN(CancellingNavigationSimulatorTest);
};

class MethodCheckingNavigationSimulatorTest : public NavigationSimulatorTest,
                                              public WebContentsObserver {
 public:
  MethodCheckingNavigationSimulatorTest() = default;
  ~MethodCheckingNavigationSimulatorTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    Observe(RenderViewHostImplTestHarness::web_contents());
  }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    did_finish_navigation_ = true;
    is_post_ = handle->IsPost();
  }

  bool did_finish_navigation() { return did_finish_navigation_; }
  bool is_post() { return is_post_; }

 private:
  // set upon DidFinishNavigation.
  bool did_finish_navigation_ = false;

  // Not valid until |did_finish_navigation_| is true;
  bool is_post_ = false;

  DISALLOW_COPY_AND_ASSIGN(MethodCheckingNavigationSimulatorTest);
};

TEST_F(NavigationSimulatorTest, AutoAdvanceOff) {
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->SetAutoAdvance(false);

  auto task_runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();
  auto* raw_runner = task_runner.get();
  TestNavigationThrottleInserter throttle_inserter(
      web_contents(), base::BindRepeating(&TaskRunnerDeferringThrottle::Create,
                                          std::move(task_runner)));

  simulator->Start();
  EXPECT_EQ(1u, raw_runner->NumPendingTasks());
  EXPECT_TRUE(simulator->IsDeferred());
  raw_runner->RunPendingTasks();
  simulator->Wait();

  simulator->Redirect(GURL("https://example.test/redirect"));
  EXPECT_EQ(1u, raw_runner->NumPendingTasks());
  EXPECT_TRUE(simulator->IsDeferred());
  raw_runner->RunPendingTasks();
  simulator->Wait();

  simulator->ReadyToCommit();
  EXPECT_EQ(1u, raw_runner->NumPendingTasks());
  EXPECT_TRUE(simulator->IsDeferred());
  raw_runner->RunPendingTasks();
  simulator->Wait();
  simulator->Commit();
}

TEST_F(MethodCheckingNavigationSimulatorTest, SetMethodPostWithRedirect) {
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->SetMethod("POST");
  simulator->Start();
  simulator->Redirect(GURL("https://example.test/2.html"));
  simulator->Commit();

  ASSERT_TRUE(did_finish_navigation());
  EXPECT_FALSE(is_post())
      << "If a POST request redirects, it should convert to a GET";
}

TEST_F(MethodCheckingNavigationSimulatorTest, SetMethodPost) {
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateRendererInitiated(
          GURL("https://example.test/"), main_rfh());
  simulator->SetMethod("POST");
  simulator->Start();
  simulator->Commit();

  ASSERT_TRUE(did_finish_navigation());
  EXPECT_TRUE(is_post());
}

// Stress test the navigation simulator by having a navigation throttle cancel
// the navigation at various points in the flow, both synchronously and
// asynchronously.
TEST_P(CancellingNavigationSimulatorTest, Cancel) {
  SCOPED_TRACE(::testing::Message()
               << "CancelTime: "
               << (cancel_time_.has_value() ? cancel_time_.value() : -1)
               << " ResultSynchrony: " << sync_);
  simulator_->Start();
  if (cancel_time_.has_value() &&
      cancel_time_.value() == TestNavigationThrottle::WILL_START_REQUEST) {
    EXPECT_EQ(NavigationThrottle::CANCEL,
              simulator_->GetLastThrottleCheckResult());
    // NavigationRequest::OnStartChecksComplete will post a task to finish the
    // navigation, so pump the run loop here to ensure checks in TearDown()
    // succeed.
    base::RunLoop().RunUntilIdle();
    return;
  }
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator_->GetLastThrottleCheckResult());
  simulator_->Redirect(GURL("https://example.redirect"));
  if (cancel_time_.has_value() &&
      cancel_time_.value() == TestNavigationThrottle::WILL_REDIRECT_REQUEST) {
    EXPECT_EQ(NavigationThrottle::CANCEL,
              simulator_->GetLastThrottleCheckResult());
    return;
  }
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator_->GetLastThrottleCheckResult());
  simulator_->Commit();
  if (cancel_time_.has_value() &&
      cancel_time_.value() == TestNavigationThrottle::WILL_PROCESS_RESPONSE) {
    EXPECT_EQ(NavigationThrottle::CANCEL,
              simulator_->GetLastThrottleCheckResult());
    return;
  }
  EXPECT_EQ(NavigationThrottle::PROCEED,
            simulator_->GetLastThrottleCheckResult());
}

INSTANTIATE_TEST_SUITE_P(
    CancelMethod,
    CancellingNavigationSimulatorTest,
    ::testing::Combine(
        ::testing::Values(TestNavigationThrottle::WILL_START_REQUEST,
                          TestNavigationThrottle::WILL_REDIRECT_REQUEST,
                          TestNavigationThrottle::WILL_PROCESS_RESPONSE,
                          base::nullopt),
        ::testing::Values(TestNavigationThrottle::SYNCHRONOUS,
                          TestNavigationThrottle::ASYNCHRONOUS)));

// Create a version of the test class for parameterized testing.
using NavigationSimulatorTestCancelFail = CancellingNavigationSimulatorTest;

// Test canceling the simulated navigation.
TEST_P(NavigationSimulatorTestCancelFail, Fail) {
  simulator_->Start();
  simulator_->Fail(net::ERR_CERT_DATE_INVALID);
  EXPECT_TRUE(will_fail_request_called_);
  EXPECT_EQ(NavigationThrottle::CANCEL,
            simulator_->GetLastThrottleCheckResult());
}

// Test canceling the simulated navigation with response headers.
TEST_P(NavigationSimulatorTestCancelFail, FailWithResponseHeaders) {
  simulator_->Start();

  using std::string_literals::operator""s;
  std::string header =
      "HTTP/1.1 404 Not Found\0"
      "content-encoding: gzip\0\0"s;

  simulator_->FailWithResponseHeaders(
      net::ERR_CERT_DATE_INVALID,
      base::MakeRefCounted<net::HttpResponseHeaders>(header));
  EXPECT_EQ(response_headers_, header);
}

INSTANTIATE_TEST_SUITE_P(
    Fail,
    NavigationSimulatorTestCancelFail,
    ::testing::Combine(
        ::testing::Values(TestNavigationThrottle::WILL_FAIL_REQUEST),
        ::testing::Values(TestNavigationThrottle::SYNCHRONOUS,
                          TestNavigationThrottle::ASYNCHRONOUS)));

// Create a version of the test class for parameterized testing.
using NavigationSimulatorTestCancelFailErrAborted =
    CancellingNavigationSimulatorTest;

// Test canceling the simulated navigation with net::ERR_ABORTED, which should
// not call WillFailRequest on the throttle.
TEST_P(NavigationSimulatorTestCancelFailErrAborted, Fail) {
  simulator_->Start();
  simulator_->Fail(net::ERR_ABORTED);
  EXPECT_FALSE(will_fail_request_called_);
}

INSTANTIATE_TEST_SUITE_P(
    Fail,
    NavigationSimulatorTestCancelFailErrAborted,
    ::testing::Combine(
        ::testing::Values(TestNavigationThrottle::WILL_FAIL_REQUEST),
        ::testing::Values(TestNavigationThrottle::SYNCHRONOUS,
                          TestNavigationThrottle::ASYNCHRONOUS)));

}  // namespace content
