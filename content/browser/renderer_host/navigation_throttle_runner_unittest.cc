// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_throttle_runner.h"

#include <optional>
#include <set>

#include "base/functional/bind.h"
#include "base/metrics/metrics_hashes.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/renderer_host/navigation_throttle_registry_impl.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_navigation_throttle.h"
#include "content/public/test/test_renderer_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

// Test version of a NavigationThrottle that will execute a callback when
// called.
class DeletingNavigationThrottle : public NavigationThrottle {
 public:
  DeletingNavigationThrottle(NavigationThrottleRegistry& registry,
                             const base::RepeatingClosure& deletion_callback)
      : NavigationThrottle(registry), deletion_callback_(deletion_callback) {}
  ~DeletingNavigationThrottle() override {}

  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillFailRequest() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  NavigationThrottle::ThrottleCheckResult WillCommitWithoutUrlLoader()
      override {
    deletion_callback_.Run();
    return NavigationThrottle::PROCEED;
  }

  const char* GetNameForLogging() override {
    return "DeletingNavigationThrottle";
  }

 private:
  base::RepeatingClosure deletion_callback_;
};

class NavigationThrottleRunnerTest : public RenderViewHostTestHarness,
                                     public NavigationThrottleRegistryBase {
 public:
  NavigationThrottleRunnerTest()
      : delegate_result_(NavigationThrottle::DEFER) {}
  ~NavigationThrottleRunnerTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    runner_ = std::make_unique<NavigationThrottleRunner>(this, 1, true);
  }

  void Resume() {
    ASSERT_EQ(1u, deferring_throttles_.size());
    runner_->ResumeProcessingNavigationEvent(*deferring_throttles_.begin());
    deferring_throttles_.clear();
  }

  void SimulateEvent(NavigationThrottleEvent event) {
    was_delegate_notified_ = false;
    delegate_result_ = NavigationThrottle::DEFER;
    observer_last_event_ = NavigationThrottleEvent::kNoEvent;
    runner_->ProcessNavigationEvent(event);
  }

  // Whether the callback was called.
  bool was_delegate_notified() const { return was_delegate_notified_; }

  // Returns the delegate_result.
  NavigationThrottle::ThrottleCheckResult delegate_result() const {
    return delegate_result_;
  }

  NavigationThrottleEvent observer_last_event() const {
    return observer_last_event_;
  }

  bool is_deferring() { return !deferring_throttles_.empty(); }

  NavigationThrottleRunner* runner() { return runner_.get(); }

  void CheckNotNotified(TestNavigationThrottle* throttle) {
    CHECK_EQ(
        0, throttle->GetCallCount(TestNavigationThrottle::WILL_START_REQUEST));
    CHECK_EQ(0, throttle->GetCallCount(
                    TestNavigationThrottle::WILL_REDIRECT_REQUEST));
    CHECK_EQ(0,
             throttle->GetCallCount(TestNavigationThrottle::WILL_FAIL_REQUEST));
    CHECK_EQ(0, throttle->GetCallCount(
                    TestNavigationThrottle::WILL_PROCESS_RESPONSE));
  }

  void CheckNotifiedOfEvent(TestNavigationThrottle* throttle,
                            NavigationThrottleEvent event) {
    if (event == NavigationThrottleEvent::kWillStartRequest) {
      CHECK_EQ(1, throttle->GetCallCount(
                      TestNavigationThrottle::WILL_START_REQUEST));
    } else {
      CHECK_EQ(0, throttle->GetCallCount(
                      TestNavigationThrottle::WILL_START_REQUEST));
    }
    if (event == NavigationThrottleEvent::kWillRedirectRequest) {
      CHECK_EQ(1, throttle->GetCallCount(
                      TestNavigationThrottle::WILL_REDIRECT_REQUEST));
    } else {
      CHECK_EQ(0, throttle->GetCallCount(
                      TestNavigationThrottle::WILL_REDIRECT_REQUEST));
    }
    if (event == NavigationThrottleEvent::kWillFailRequest) {
      CHECK_EQ(
          1, throttle->GetCallCount(TestNavigationThrottle::WILL_FAIL_REQUEST));
    } else {
      CHECK_EQ(
          0, throttle->GetCallCount(TestNavigationThrottle::WILL_FAIL_REQUEST));
    }
    if (event == NavigationThrottleEvent::kWillProcessResponse) {
      CHECK_EQ(1, throttle->GetCallCount(
                      TestNavigationThrottle::WILL_PROCESS_RESPONSE));
    } else {
      CHECK_EQ(0, throttle->GetCallCount(
                      TestNavigationThrottle::WILL_PROCESS_RESPONSE));
    }
    if (event == NavigationThrottleEvent::kWillCommitWithoutUrlLoader) {
      CHECK_EQ(1, throttle->GetCallCount(
                      TestNavigationThrottle::WILL_COMMIT_WITHOUT_URL_LOADER));
    } else {
      CHECK_EQ(0, throttle->GetCallCount(
                      TestNavigationThrottle::WILL_COMMIT_WITHOUT_URL_LOADER));
    }
  }

  // Creates, register and returns a TestNavigationThrottle that will
  // synchronously return |result| on checks by default.
  TestNavigationThrottle* CreateTestNavigationThrottle(
      NavigationThrottle::ThrottleCheckResult result) {
    TestNavigationThrottle* test_throttle = new TestNavigationThrottle(*this);
    test_throttle->SetResponseForAllMethods(TestNavigationThrottle::SYNCHRONOUS,
                                            result);
    AddThrottle(std::unique_ptr<TestNavigationThrottle>(test_throttle));
    return test_throttle;
  }

  // Creates, register and returns a TestNavigationThrottle that will
  // synchronously return |result| on check for the given |method|, and
  // NavigationThrottle::PROCEED otherwise.
  TestNavigationThrottle* CreateTestNavigationThrottle(
      TestNavigationThrottle::ThrottleMethod method,
      NavigationThrottle::ThrottleCheckResult result) {
    TestNavigationThrottle* test_throttle =
        CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
    test_throttle->SetResponse(method, TestNavigationThrottle::SYNCHRONOUS,
                               result);
    return test_throttle;
  }

  // Creates and register a NavigationThrottle that will delete the
  // NavigationHandle in checks.
  void AddDeletingNavigationThrottle() {
    AddThrottle(std::make_unique<DeletingNavigationThrottle>(
        *this, base::BindRepeating(
                   &NavigationThrottleRunnerTest::ResetNavigationThrottleRunner,
                   base::Unretained(this))));
  }

  ukm::TestUkmRecorder& test_ukm_recorder() { return test_ukm_recorder_; }

 private:
  // NavigationThrottleRegistry:
  NavigationHandle& GetNavigationHandle() override { return handle_; }
  void AddThrottle(
      std::unique_ptr<NavigationThrottle> navigation_throttle) override {
    throttles_.push_back(std::move(navigation_throttle));
  }
  bool IsHTTPOrHTTPS() override {
    return handle_.GetURL().SchemeIsHTTPOrHTTPS();
  }
  MOCK_METHOD(bool, HasThrottle, (const std::string& name), (override));
  MOCK_METHOD(bool, EraseThrottleForTesting, (const std::string& name),
              (override));

  // NavigationThrottleRegistryBase:
  void OnEventProcessed(
      NavigationThrottleEvent event,
      NavigationThrottle::ThrottleCheckResult result) override {
    DCHECK(!was_delegate_notified_);
    delegate_result_ = result;
    was_delegate_notified_ = true;
    observer_last_event_ = event;
  }
  void OnDeferProcessingNavigationEvent(
      NavigationThrottle* deferring_throttle) override {
    deferring_throttles_.insert(deferring_throttle);
  }
  std::vector<std::unique_ptr<NavigationThrottle>>& GetThrottles() override {
    return throttles_;
  }
  NavigationThrottle& GetThrottleAtIndex(size_t index) override {
    EXPECT_LT(index, throttles_.size());
    return *throttles_[index];
  }
  const std::set<NavigationThrottle*>& GetDeferringThrottles() const override {
    return deferring_throttles_;
  }

  void ResetNavigationThrottleRunner() { runner_.reset(); }

  MockNavigationHandle handle_;
  std::vector<std::unique_ptr<NavigationThrottle>> throttles_;
  std::set<NavigationThrottle*> deferring_throttles_;
  std::unique_ptr<NavigationThrottleRunner> runner_;
  NavigationThrottleEvent observer_last_event_ =
      NavigationThrottleEvent::kNoEvent;
  bool was_delegate_notified_ = false;
  NavigationThrottle::ThrottleCheckResult delegate_result_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

class NavigationThrottleRunnerTestWithEvent
    : public NavigationThrottleRunnerTest,
      public testing::WithParamInterface<NavigationThrottleEvent> {
 public:
  NavigationThrottleRunnerTestWithEvent() = default;
  ~NavigationThrottleRunnerTestWithEvent() override = default;
  void SetUp() override {
    NavigationThrottleRunnerTest::SetUp();
    event_ = GetParam();
  }

  NavigationThrottleEvent event() const { return event_; }

  void CheckNotified(TestNavigationThrottle* throttle) {
    CheckNotifiedOfEvent(throttle, event());
  }

 private:
  NavigationThrottleEvent event_;
};

// Checks that a navigation deferred by a NavigationThrottle can be properly
// resumed.
TEST_P(NavigationThrottleRunnerTestWithEvent, ResumeDeferred) {
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  CheckNotNotified(test_throttle);

  // Simulate the event. The request should be deferred. The observer
  // should not have been notified.
  SimulateEvent(event());
  CheckNotified(test_throttle);
  EXPECT_TRUE(is_deferring());
  EXPECT_FALSE(was_delegate_notified());

  // Resume the request. It should no longer be deferred and the observer
  // should have been notified.
  Resume();
  CheckNotified(test_throttle);
  EXPECT_FALSE(is_deferring());
  EXPECT_TRUE(was_delegate_notified());
  EXPECT_EQ(NavigationThrottle::PROCEED, delegate_result());
  EXPECT_EQ(event(), observer_last_event());
}

// Checks that a NavigationThrottleRunner can be safely deleted by the execution
// of one of its NavigationThrottle.
TEST_P(NavigationThrottleRunnerTestWithEvent, DeletionByNavigationThrottle) {
  AddDeletingNavigationThrottle();
  SimulateEvent(event());
  EXPECT_EQ(nullptr, runner());
  EXPECT_FALSE(was_delegate_notified());
}

// Checks that a NavigationThrottleRunner can be safely deleted by the execution
// of one of its NavigationThrottle following a call to
// ResumeProcessingNavigationEvent.
TEST_P(NavigationThrottleRunnerTestWithEvent,
       DeletionByNavigationThrottleAfterResume) {
  CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  AddDeletingNavigationThrottle();
  SimulateEvent(event());
  EXPECT_NE(nullptr, runner());
  Resume();
  EXPECT_EQ(nullptr, runner());
  EXPECT_FALSE(was_delegate_notified());
}

INSTANTIATE_TEST_SUITE_P(
    AllEvents,
    NavigationThrottleRunnerTestWithEvent,
    ::testing::Values(NavigationThrottleEvent::kWillStartRequest,
                      NavigationThrottleEvent::kWillRedirectRequest,
                      NavigationThrottleEvent::kWillFailRequest,
                      NavigationThrottleEvent::kWillProcessResponse,
                      NavigationThrottleEvent::kWillCommitWithoutUrlLoader));

class NavigationThrottleRunnerTestWithEventAndAction
    : public NavigationThrottleRunnerTest,
      public testing::WithParamInterface<
          std::tuple<NavigationThrottleEvent,
                     NavigationThrottle::ThrottleAction>> {
 public:
  NavigationThrottleRunnerTestWithEventAndAction() = default;
  ~NavigationThrottleRunnerTestWithEventAndAction() override = default;
  void SetUp() override {
    NavigationThrottleRunnerTest::SetUp();
    std::tie(event_, action_) = GetParam();
  }
  NavigationThrottleEvent event() const { return event_; }
  NavigationThrottle::ThrottleAction action() const { return action_; }

  void CheckNotified(TestNavigationThrottle* throttle) {
    CheckNotifiedOfEvent(throttle, event());
  }

 private:
  NavigationThrottleEvent event_;
  NavigationThrottle::ThrottleAction action_;
};

// Checks that a NavigationThrottle asking during to defer
// followed by another NavigationThrottle behave correctly.
TEST_P(NavigationThrottleRunnerTestWithEventAndAction, DeferThenAction) {
  TestNavigationThrottle* defer_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(action());
  CheckNotNotified(defer_throttle);
  CheckNotNotified(test_throttle);

  // Simulate the event. The request should be deferred. The observer
  // should not have been notified. The second throttle should not have been
  // notified.
  SimulateEvent(event());
  CheckNotified(defer_throttle);
  CheckNotNotified(test_throttle);
  EXPECT_TRUE(is_deferring());
  EXPECT_FALSE(was_delegate_notified());

  // Resume the request. It should no longer be deferred and the observer
  // should have been notified. The second throttle should have been notified.
  Resume();
  CheckNotified(defer_throttle);
  CheckNotified(test_throttle);
  EXPECT_FALSE(is_deferring());
  EXPECT_TRUE(was_delegate_notified());
  EXPECT_EQ(action(), delegate_result().action());
  EXPECT_EQ(event(), observer_last_event());
}

// Checks that a NavigationThrottle asking to cancel followed by a
// NavigationThrottle asking to proceed behave correctly. The navigation will
// be stopped directly, and the second throttle will not be called.
TEST_P(NavigationThrottleRunnerTestWithEventAndAction, CancelThenProceed) {
  if (action() == NavigationThrottle::PROCEED) {
    return;
  }
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(action());
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  CheckNotNotified(test_throttle);
  CheckNotNotified(proceed_throttle);

  // Simulate the event. The request should be not be deferred. The observer
  // should have been notified. The second throttle should not have been
  // notified.
  SimulateEvent(event());
  CheckNotified(test_throttle);
  CheckNotNotified(proceed_throttle);
  EXPECT_FALSE(is_deferring());
  EXPECT_TRUE(was_delegate_notified());
  EXPECT_EQ(action(), delegate_result().action());
  EXPECT_EQ(event(), observer_last_event());
}

// Checks that a NavigationThrottle asking to proceed followed by a
// NavigationThrottle asking to cancel behave correctly.
// Both throttles will be called, and the request will be cancelled.
TEST_P(NavigationThrottleRunnerTestWithEventAndAction, ProceedThenCancel) {
  if (action() == NavigationThrottle::PROCEED) {
    return;
  }
  TestNavigationThrottle* proceed_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::PROCEED);
  TestNavigationThrottle* test_throttle =
      CreateTestNavigationThrottle(action());
  CheckNotNotified(test_throttle);
  CheckNotNotified(proceed_throttle);

  // Simulate the event. The request should be not be deferred. The observer
  // should have been notified.
  SimulateEvent(event());
  CheckNotified(proceed_throttle);
  CheckNotified(test_throttle);
  EXPECT_FALSE(is_deferring());
  EXPECT_TRUE(was_delegate_notified());
  EXPECT_EQ(action(), delegate_result().action());
  EXPECT_EQ(event(), observer_last_event());
}

// Checks that a NavigationThrottle being deferred and resumed records UKM about
// the deferral.
TEST_P(NavigationThrottleRunnerTestWithEventAndAction, DeferRecordsUKM) {
  TestNavigationThrottle* defer_throttle =
      CreateTestNavigationThrottle(NavigationThrottle::DEFER);
  CheckNotNotified(defer_throttle);

  // Simulate the event. The request should be deferred.
  SimulateEvent(event());
  CheckNotified(defer_throttle);
  EXPECT_TRUE(is_deferring());

  // Resume the request. This should record UKM.
  Resume();

  // There should be one entry with name hash matching the logging name and
  // event that is being run. Ignore the time for testing as it is variable, and
  // even possibly 0.
  const auto& entries = test_ukm_recorder().GetEntriesByName(
      ukm::builders::NavigationThrottleDeferredTime::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    EXPECT_EQ(*ukm::TestUkmRecorder::GetEntryMetric(
                  entry, ukm::builders::NavigationThrottleDeferredTime::
                             kNavigationThrottleEventTypeName),
              static_cast<int64_t>(event()));
    EXPECT_EQ(*ukm::TestUkmRecorder::GetEntryMetric(
                  entry, ukm::builders::NavigationThrottleDeferredTime::
                             kNavigationThrottleNameHashName),
              static_cast<int64_t>(
                  base::HashMetricName(defer_throttle->GetNameForLogging())));
  }
}

INSTANTIATE_TEST_SUITE_P(
    AllEvents,
    NavigationThrottleRunnerTestWithEventAndAction,
    ::testing::Combine(
        ::testing::Values(NavigationThrottleEvent::kWillStartRequest,
                          NavigationThrottleEvent::kWillRedirectRequest,
                          NavigationThrottleEvent::kWillFailRequest,
                          NavigationThrottleEvent::kWillProcessResponse,
                          NavigationThrottleEvent::kWillCommitWithoutUrlLoader),
        ::testing::Values(NavigationThrottle::PROCEED,
                          NavigationThrottle::CANCEL,
                          NavigationThrottle::CANCEL_AND_IGNORE,
                          NavigationThrottle::BLOCK_REQUEST,
                          NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE,
                          NavigationThrottle::BLOCK_RESPONSE)));

class NavigationThrottleRunnerTestWithEventAndError
    : public NavigationThrottleRunnerTest,
      public testing::WithParamInterface<
          std::tuple<NavigationThrottleEvent,
                     net::Error,
                     std::optional<std::string>>> {
 public:
  NavigationThrottleRunnerTestWithEventAndError() = default;
  ~NavigationThrottleRunnerTestWithEventAndError() override = default;
  void SetUp() override {
    NavigationThrottleRunnerTest::SetUp();
    std::tie(event_, error_, custom_error_page_) = GetParam();
  }
  NavigationThrottleEvent event() const { return event_; }
  net::Error error() const { return error_; }
  const std::optional<std::string>& custom_error_page() const {
    return custom_error_page_;
  }

  void CheckNotified(TestNavigationThrottle* throttle) {
    CheckNotifiedOfEvent(throttle, event());
  }

 private:
  NavigationThrottleEvent event_;
  net::Error error_;
  std::optional<std::string> custom_error_page_ = std::nullopt;
};

// Checks that the NavigationThrottleRunner correctly propagates a
// ThrottleCheckResult with a custom error page and/or error code to its
// delegate.
TEST_P(NavigationThrottleRunnerTestWithEventAndError, CustomNetError) {
  SCOPED_TRACE(::testing::Message()
               << "Event: " << static_cast<int>(event())
               << " Error: " << error() << " Custom error page: "
               << (custom_error_page().has_value() ? custom_error_page().value()
                                                   : ""));
  NavigationThrottle::ThrottleCheckResult result(NavigationThrottle::CANCEL,
                                                 error());
  if (custom_error_page().has_value()) {
    result = NavigationThrottle::ThrottleCheckResult(
        NavigationThrottle::CANCEL, error(), custom_error_page().value());
  }
  TestNavigationThrottle* test_throttle = CreateTestNavigationThrottle(result);
  CheckNotNotified(test_throttle);

  // Simulate the event. The request should not be deferred. The
  // callback should have been called.
  SimulateEvent(event());
  EXPECT_FALSE(is_deferring());
  EXPECT_TRUE(was_delegate_notified());
  EXPECT_EQ(NavigationThrottle::CANCEL, delegate_result().action());
  EXPECT_EQ(error(), delegate_result().net_error_code());
  EXPECT_EQ(custom_error_page().has_value(),
            delegate_result().error_page_content().has_value());
  if (custom_error_page().has_value()) {
    EXPECT_EQ(custom_error_page().value(),
              delegate_result().error_page_content().value());
  }
  CheckNotified(test_throttle);
}

INSTANTIATE_TEST_SUITE_P(
    AllEvents,
    NavigationThrottleRunnerTestWithEventAndError,
    ::testing::Combine(
        ::testing::Values(NavigationThrottleEvent::kWillStartRequest,
                          NavigationThrottleEvent::kWillRedirectRequest,
                          NavigationThrottleEvent::kWillFailRequest,
                          NavigationThrottleEvent::kWillProcessResponse,
                          NavigationThrottleEvent::kWillCommitWithoutUrlLoader),
        ::testing::Values(net::ERR_BLOCKED_BY_ADMINISTRATOR, net::ERR_ABORTED),
        ::testing::Values(std::nullopt, "<html><body>test</body></html>")));

}  // namespace content
