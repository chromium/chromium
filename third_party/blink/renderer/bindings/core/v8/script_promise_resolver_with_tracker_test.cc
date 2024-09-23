// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver_with_tracker.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

class TestHelperFunction : public ScriptFunction::Callable {
 public:
  explicit TestHelperFunction(String* value) : value_(value) {}

  ScriptValue Call(ScriptState* script_state, ScriptValue value) override {
    DCHECK(!value.IsEmpty());
    *value_ = ToCoreString(
        script_state->GetIsolate(),
        value.V8Value()->ToString(script_state->GetContext()).ToLocalChecked());
    return value;
  }

 private:
  String* value_;
};

enum class TestEnum {
  kOk = 0,
  kFailedWithReason = 1,
  kTimedOut = 2,
  kMaxValue = kTimedOut
};

class ScriptPromiseResolverWithTrackerTest : public testing::Test {
 public:
  ScriptPromiseResolverWithTrackerTest()
      : metric_name_prefix_("Histogram.TestEnum"),
        page_holder_(std::make_unique<DummyPageHolder>()) {}

  ~ScriptPromiseResolverWithTrackerTest() override {
    PerformMicrotaskCheckpoint();
  }

  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(&page_holder_->GetFrame());
  }

  void PerformMicrotaskCheckpoint() {
    ScriptState::Scope scope(GetScriptState());
    GetScriptState()->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
        GetScriptState()->GetIsolate());
  }

  ScriptPromiseResolverWithTracker<TestEnum, IDLString>* CreateResultTracker(
      String& on_fulfilled,
      String& on_rejected,
      base::TimeDelta timeout_delay = base::Minutes(1)) {
    ScriptState::Scope scope(GetScriptState());
    auto* result_tracker = MakeGarbageCollected<
        ScriptPromiseResolverWithTracker<TestEnum, IDLString>>(
        GetScriptState(), metric_name_prefix_, timeout_delay);

    ScriptPromiseUntyped promise = result_tracker->Promise();
    promise.Then(MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_fulfilled)),
                 MakeGarbageCollected<ScriptFunction>(
                     GetScriptState(),
                     MakeGarbageCollected<TestHelperFunction>(&on_rejected)));

    PerformMicrotaskCheckpoint();

    CheckResultHistogram(/*expected_count=*/0);
    CheckLatencyHistogram(/*expected_count=*/0);
    return result_tracker;
  }

  void CheckResultHistogram(int expected_count,
                            const std::string& result_string = "Result") {
    histogram_tester_.ExpectTotalCount(
        base::StrCat({metric_name_prefix_, ".", result_string}),
        expected_count);
  }

  void CheckLatencyHistogram(int expected_count) {
    histogram_tester_.ExpectTotalCount(metric_name_prefix_ + ".Latency",
                                       expected_count);
  }

 protected:
  test::TaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::string metric_name_prefix_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_F(ScriptPromiseResolverWithTrackerTest, resolve) {
  String on_fulfilled, on_rejected;
  auto* result_tracker = CreateResultTracker(on_fulfilled, on_rejected);
  result_tracker->Resolve(/*value=*/"hello", /*result=*/TestEnum::kOk);
  PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", on_fulfilled);
  EXPECT_EQ(String(), on_rejected);
  CheckResultHistogram(/*expected_count=*/1);
  CheckLatencyHistogram(/*expected_count=*/1);
}

TEST_F(ScriptPromiseResolverWithTrackerTest, reject) {
  String on_fulfilled, on_rejected;
  auto* result_tracker = CreateResultTracker(on_fulfilled, on_rejected);
  result_tracker->Reject<IDLString>(/*value=*/"hello",
                                    /*result=*/TestEnum::kFailedWithReason);
  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ("hello", on_rejected);
  CheckResultHistogram(/*expected_count=*/1);
  CheckLatencyHistogram(/*expected_count=*/1);
}

TEST_F(ScriptPromiseResolverWithTrackerTest, resolve_reject_again) {
  String on_fulfilled, on_rejected;
  auto* result_tracker = CreateResultTracker(on_fulfilled, on_rejected);
  result_tracker->Reject<IDLString>(/*value=*/"hello",
                                    /*result=*/TestEnum::kFailedWithReason);
  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ("hello", on_rejected);
  CheckResultHistogram(/*expected_count=*/1);
  CheckLatencyHistogram(/*expected_count=*/1);

  // Resolve/Reject on already resolved/rejected promise doesn't log new values
  // in the histogram.
  result_tracker->Resolve(/*value=*/"bye", /*result=*/TestEnum::kOk);
  result_tracker->Reject<IDLString>(/*value=*/"bye",
                                    /*result=*/TestEnum::kFailedWithReason);
  PerformMicrotaskCheckpoint();

  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ("hello", on_rejected);
  CheckResultHistogram(/*expected_count=*/1);
  CheckLatencyHistogram(/*expected_count=*/1);
}

TEST_F(ScriptPromiseResolverWithTrackerTest, timeout) {
  String on_fulfilled, on_rejected;
  base::TimeDelta timeout_delay = base::Milliseconds(200);
  auto* result_tracker =
      CreateResultTracker(on_fulfilled, on_rejected, timeout_delay);

  // Run the tasks scheduled to run within the delay specified.
  test::RunDelayedTasks(timeout_delay);
  PerformMicrotaskCheckpoint();

  // kTimedOut is logged in the Result histogram but nothing is logged in the
  // latency histogram as the promise was never rejected or resolved.
  CheckResultHistogram(/*expected_count=*/1);
  CheckLatencyHistogram(/*expected_count=*/0);

  // Though the timeout has passed, the promise is not yet rejected or resolved.
  EXPECT_EQ(String(), on_fulfilled);
  EXPECT_EQ(String(), on_rejected);

  result_tracker->Reject<IDLString>(/*value=*/"hello",
                                    /*result=*/TestEnum::kFailedWithReason);
  PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", on_rejected);
  EXPECT_EQ(String(), on_fulfilled);

  // Rejected result is not logged again as it was rejected after the timeout
  // had passed. It is still logged in the latency though.
  CheckResultHistogram(/*expected_count=*/1);
  CheckLatencyHistogram(/*expected_count=*/1);
}

TEST_F(ScriptPromiseResolverWithTrackerTest, SetResultSuffix) {
  String on_fulfilled, on_rejected;
  auto* result_tracker = CreateResultTracker(on_fulfilled, on_rejected);
  result_tracker->SetResultSuffix("NewResultSuffix");
  result_tracker->Resolve(/*value=*/"hello", /*result=*/TestEnum::kOk);
  PerformMicrotaskCheckpoint();

  EXPECT_EQ("hello", on_fulfilled);
  EXPECT_EQ(String(), on_rejected);
  CheckResultHistogram(/*expected_count=*/1, "NewResultSuffix");
  CheckLatencyHistogram(/*expected_count=*/1);
}

}  // namespace blink
