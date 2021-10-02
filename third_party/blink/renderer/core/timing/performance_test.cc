// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/window_performance.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_observer_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/timing/performance_long_task_timing.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {

class TestPerformance : public Performance {
 public:
  explicit TestPerformance(ScriptState* script_state)
      : Performance(base::TimeTicks(),
                    ExecutionContext::From(script_state)
                        ->CrossOriginIsolatedCapability(),
                    ExecutionContext::From(script_state)
                        ->GetTaskRunner(TaskType::kPerformanceTimeline)),
        execution_context_(ExecutionContext::From(script_state)) {}
  ~TestPerformance() override = default;

  ExecutionContext* GetExecutionContext() const override {
    return execution_context_.Get();
  }

  int NumActiveObservers() { return active_observers_.size(); }

  int NumObservers() { return observers_.size(); }

  bool HasPerformanceObserverFor(PerformanceEntry::EntryType entry_type) {
    return HasObserverFor(entry_type);
  }

  void Trace(Visitor* visitor) const override {
    Performance::Trace(visitor);
    visitor->Trace(execution_context_);
  }

 private:
  Member<ExecutionContext> execution_context_;
};

class PerformanceTest : public PageTestBase {
 protected:
  ~PerformanceTest() override { execution_context_->NotifyContextDestroyed(); }

  void Initialize(ScriptState* script_state) {
    v8::Local<v8::Function> callback =
        v8::Function::New(script_state->GetContext(), nullptr).ToLocalChecked();
    base_ = MakeGarbageCollected<TestPerformance>(script_state);
    cb_ = V8PerformanceObserverCallback::Create(callback);
    observer_ = MakeGarbageCollected<PerformanceObserver>(
        ExecutionContext::From(script_state), base_, cb_);
  }

  void SetUp() override {
    PageTestBase::SetUp();
    execution_context_ = MakeGarbageCollected<NullExecutionContext>();
  }

  ExecutionContext* GetExecutionContext() { return execution_context_.Get(); }

  int NumPerformanceEntriesInObserver() {
    return observer_->performance_entries_.size();
  }

  static bool AllowsTimingRedirect(
      const Vector<ResourceResponse>& redirect_chain,
      const ResourceResponse& final_response,
      const SecurityOrigin& initiator_security_origin,
      ExecutionContext* context) {
    return Performance::AllowsTimingRedirect(
        redirect_chain, final_response, initiator_security_origin, context);
  }

  Persistent<TestPerformance> base_;
  Persistent<ExecutionContext> execution_context_;
  Persistent<PerformanceObserver> observer_;
  Persistent<V8PerformanceObserverCallback> cb_;
};

TEST_F(PerformanceTest, Register) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  EXPECT_EQ(0, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());

  base_->RegisterPerformanceObserver(*observer_.Get());
  EXPECT_EQ(1, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());

  base_->UnregisterPerformanceObserver(*observer_.Get());
  EXPECT_EQ(0, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());
}

TEST_F(PerformanceTest, Activate) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  EXPECT_EQ(0, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());

  base_->RegisterPerformanceObserver(*observer_.Get());
  EXPECT_EQ(1, base_->NumObservers());
  EXPECT_EQ(0, base_->NumActiveObservers());

  base_->ActivateObserver(*observer_.Get());
  EXPECT_EQ(1, base_->NumObservers());
  EXPECT_EQ(1, base_->NumActiveObservers());

  base_->UnregisterPerformanceObserver(*observer_.Get());
  EXPECT_EQ(0, base_->NumObservers());
  EXPECT_EQ(1, base_->NumActiveObservers());
}

TEST_F(PerformanceTest, AddLongTaskTiming) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  // Add a long task entry, but no observer registered.
  base_->AddLongTaskTiming(base::TimeTicks() + base::Seconds(1234),
                           base::TimeTicks() + base::Seconds(5678), "window",
                           "same-origin", "www.foo.com/bar", "", "");
  EXPECT_FALSE(base_->HasPerformanceObserverFor(PerformanceEntry::kLongTask));
  EXPECT_EQ(0, NumPerformanceEntriesInObserver());  // has no effect

  // Make an observer for longtask
  NonThrowableExceptionState exception_state;
  PerformanceObserverInit* options = PerformanceObserverInit::Create();
  Vector<String> entry_type_vec;
  entry_type_vec.push_back("longtask");
  options->setEntryTypes(entry_type_vec);
  observer_->observe(options, exception_state);

  EXPECT_TRUE(base_->HasPerformanceObserverFor(PerformanceEntry::kLongTask));
  // Add a long task entry
  base_->AddLongTaskTiming(base::TimeTicks() + base::Seconds(1234),
                           base::TimeTicks() + base::Seconds(5678), "window",
                           "same-origin", "www.foo.com/bar", "", "");
  EXPECT_EQ(1, NumPerformanceEntriesInObserver());  // added an entry
}

TEST_F(PerformanceTest, AllowsTimingRedirect) {
  // When there are no cross-origin redirects.
  AtomicString origin_domain = "http://127.0.0.1:8000";
  Vector<ResourceResponse> redirect_chain;
  KURL url(origin_domain + "/foo.html");
  ResourceResponse redirect_response1(url);
  ResourceResponse redirect_response2(url);
  redirect_chain.push_back(redirect_response1);
  redirect_chain.push_back(redirect_response2);
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::Create(url);
  // When finalResponse is an empty object.
  ResourceResponse empty_final_response;
  EXPECT_FALSE(AllowsTimingRedirect(redirect_chain, empty_final_response,
                                    *security_origin.get(),
                                    GetExecutionContext()));
  // Final response is same origin as requestor.
  ResourceResponse final_response(url);
  EXPECT_TRUE(AllowsTimingRedirect(redirect_chain, final_response,
                                   *security_origin.get(),
                                   GetExecutionContext()));
  // When there exist cross-origin redirects.
  AtomicString cross_origin_domain = "http://126.0.0.1:8000";
  KURL redirect_url(cross_origin_domain + "/bar.html");
  ResourceResponse redirect_response3(redirect_url);
  redirect_chain.push_back(redirect_response3);
  EXPECT_FALSE(AllowsTimingRedirect(redirect_chain, final_response,
                                    *security_origin.get(),
                                    GetExecutionContext()));

  // When cross-origin redirect opts in, but the final response doesn't.
  redirect_chain.back().SetHttpHeaderField(http_names::kTimingAllowOrigin,
                                           origin_domain);
  EXPECT_FALSE(AllowsTimingRedirect(redirect_chain, final_response,
                                    *security_origin.get(),
                                    GetExecutionContext()));
  // When cross-origin redirect opts in and the final response has as well, but
  // the tainted origin flag is set.
  final_response.SetHttpHeaderField(http_names::kTimingAllowOrigin,
                                    origin_domain);
  EXPECT_FALSE(AllowsTimingRedirect(redirect_chain, final_response,
                                    *security_origin.get(),
                                    GetExecutionContext()));
  // Change the opt ins to be '*' and then the check should pass.
  redirect_chain.back().SetHttpHeaderField(http_names::kTimingAllowOrigin, "*");
  final_response.SetHttpHeaderField(http_names::kTimingAllowOrigin, "*");
  EXPECT_TRUE(AllowsTimingRedirect(redirect_chain, final_response,
                                   *security_origin.get(),
                                   GetExecutionContext()));
}

}  // namespace blink
