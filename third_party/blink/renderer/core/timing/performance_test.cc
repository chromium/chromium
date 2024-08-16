// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance.h"

#include <algorithm>

#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_observer_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_performance_observer_init.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/performance_entry_names.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/timing/back_forward_cache_restoration.h"
#include "third_party/blink/renderer/core/timing/performance_long_task_timing.h"
#include "third_party/blink/renderer/core/timing/performance_observer.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"

namespace blink {
namespace {
constexpr int kTimeOrigin = 1;
constexpr int kEvent1Time = 123;
constexpr int kEvent1PageshowStart = 456;
constexpr int kEvent1PageshowEnd = 789;
constexpr int kEvent2Time = 321;
constexpr int kEvent2PageshowStart = 654;
constexpr int kEvent2PageshowEnd = 987;
}  // namespace

class LocalDOMWindow;

class TestPerformance : public Performance {
 public:
  explicit TestPerformance(ScriptState* script_state)
      : Performance(base::TimeTicks() + base::Milliseconds(kTimeOrigin),
                    ExecutionContext::From(script_state)
                        ->CrossOriginIsolatedCapability(),
                    ExecutionContext::From(script_state)
                        ->GetTaskRunner(TaskType::kPerformanceTimeline)),
        execution_context_(ExecutionContext::From(script_state)) {}
  ~TestPerformance() override = default;

  ExecutionContext* GetExecutionContext() const override {
    return execution_context_.Get();
  }
  uint64_t interactionCount() const override { return 0; }

  int NumActiveObservers() { return active_observers_.size(); }

  int NumObservers() { return observers_.size(); }

  bool HasPerformanceObserverFor(PerformanceEntry::EntryType entry_type) {
    return HasObserverFor(entry_type);
  }

  base::TimeTicks MsAfterTimeOrigin(uint32_t ms) {
    LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
    DocumentLoader* loader = window->GetFrame()->Loader().GetDocumentLoader();
    return loader->GetTiming().ReferenceMonotonicTime() +
           base::Milliseconds(ms);
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

  PerformanceEntryVector PerformanceEntriesInObserver() {
    return observer_->performance_entries_;
  }

  void CheckBackForwardCacheRestoration(PerformanceEntryVector entries) {
    // Expect there are 2 back forward cache restoration entries.
    EXPECT_EQ(2, base::ranges::count(entries, "back-forward-cache-restoration",
                                     &PerformanceEntry::entryType));

    // Retain only back forward cache restoration entries.
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [](const PerformanceEntry* e) -> bool {
                                   return e->entryType() !=
                                          "back-forward-cache-restoration";
                                 }),
                  entries.end());

    BackForwardCacheRestoration* b1 =
        static_cast<BackForwardCacheRestoration*>(entries[0].Get());
    EXPECT_EQ(kEvent1Time - kTimeOrigin, b1->startTime());
    EXPECT_EQ(kEvent1PageshowStart - kTimeOrigin, b1->pageshowEventStart());
    EXPECT_EQ(kEvent1PageshowEnd - kTimeOrigin, b1->pageshowEventEnd());

    BackForwardCacheRestoration* b2 =
        static_cast<BackForwardCacheRestoration*>(entries[1].Get());
    EXPECT_EQ(kEvent2Time - kTimeOrigin, b2->startTime());
    EXPECT_EQ(kEvent2PageshowStart - kTimeOrigin, b2->pageshowEventStart());
    EXPECT_EQ(kEvent2PageshowEnd - kTimeOrigin, b2->pageshowEventEnd());
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
                           base::TimeTicks() + base::Seconds(5678),
                           AtomicString("window"), AtomicString("same-origin"),
                           AtomicString("www.foo.com/bar"), g_empty_atom,
                           g_empty_atom);
  EXPECT_FALSE(base_->HasPerformanceObserverFor(PerformanceEntry::kLongTask));
  EXPECT_EQ(0, NumPerformanceEntriesInObserver());  // has no effect

  // Make an observer for longtask
  NonThrowableExceptionState exception_state;
  PerformanceObserverInit* options = PerformanceObserverInit::Create();
  Vector<String> entry_type_vec;
  entry_type_vec.push_back("longtask");
  options->setEntryTypes(entry_type_vec);
  observer_->observe(scope.GetScriptState(), options, exception_state);

  EXPECT_TRUE(base_->HasPerformanceObserverFor(PerformanceEntry::kLongTask));
  // Add a long task entry
  base_->AddLongTaskTiming(base::TimeTicks() + base::Seconds(1234),
                           base::TimeTicks() + base::Seconds(5678),
                           AtomicString("window"), AtomicString("same-origin"),
                           AtomicString("www.foo.com/bar"), g_empty_atom,
                           g_empty_atom);
  EXPECT_EQ(1, NumPerformanceEntriesInObserver());  // added an entry
}

TEST_F(PerformanceTest, BackForwardCacheRestoration) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  NonThrowableExceptionState exception_state;
  PerformanceObserverInit* options = PerformanceObserverInit::Create();

  Vector<String> entry_type_vec;
  entry_type_vec.push_back("back-forward-cache-restoration");
  options->setEntryTypes(entry_type_vec);
  observer_->observe(scope.GetScriptState(), options, exception_state);

  EXPECT_TRUE(base_->HasPerformanceObserverFor(
      PerformanceEntry::kBackForwardCacheRestoration));

  base_->AddBackForwardCacheRestoration(
      base::TimeTicks() + base::Milliseconds(kEvent1Time),
      base::TimeTicks() + base::Milliseconds(kEvent1PageshowStart),
      base::TimeTicks() + base::Milliseconds(kEvent1PageshowEnd));

  base_->AddBackForwardCacheRestoration(
      base::TimeTicks() + base::Milliseconds(kEvent2Time),
      base::TimeTicks() + base::Milliseconds(kEvent2PageshowStart),
      base::TimeTicks() + base::Milliseconds(kEvent2PageshowEnd));

  auto entries = PerformanceEntriesInObserver();
  CheckBackForwardCacheRestoration(entries);

  entries = base_->getEntries();
  CheckBackForwardCacheRestoration(entries);

  entries = base_->getEntriesByType(
      performance_entry_names::kBackForwardCacheRestoration);
  CheckBackForwardCacheRestoration(entries);
}

// Validate ordering after insertion into an empty vector.
TEST_F(PerformanceTest, InsertEntryOnEmptyBuffer) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  PerformanceEntryVector test_buffer_;

  PerformanceEventTiming::EventTimingReportingInfo info{
      .creation_time = base_->MsAfterTimeOrigin(0),
      .processing_start_time = base_->MsAfterTimeOrigin(0),
      .processing_end_time = base_->MsAfterTimeOrigin(0)};

  PerformanceEventTiming* test_entry = PerformanceEventTiming::Create(
      AtomicString("event"), info, false, nullptr,
      LocalDOMWindow::From(scope.GetScriptState()));

  base_->InsertEntryIntoSortedBuffer(test_buffer_, *test_entry,
                                     Performance::kDoNotRecordSwaps);

  PerformanceEntryVector sorted_buffer_;
  sorted_buffer_.push_back(*test_entry);

  EXPECT_EQ(test_buffer_, sorted_buffer_);
}

// Validate ordering after insertion into a non-empty vector.
TEST_F(PerformanceTest, InsertEntryOnExistingBuffer) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  PerformanceEntryVector test_buffer_;

  // Insert 3 entries into the vector.
  for (int i = 0; i < 3; i++) {
    double tmp = 1.0;
    PerformanceEventTiming::EventTimingReportingInfo info{
        .creation_time = base_->MsAfterTimeOrigin(tmp * i),
        .processing_start_time = base_->MsAfterTimeOrigin(0),
        .processing_end_time = base_->MsAfterTimeOrigin(0)};
    PerformanceEventTiming* entry = PerformanceEventTiming::Create(
        AtomicString("event"), info, false, nullptr,
        LocalDOMWindow::From(scope.GetScriptState()));
    test_buffer_.push_back(*entry);
  }

  PerformanceEventTiming::EventTimingReportingInfo info{
      .creation_time = base_->MsAfterTimeOrigin(1),
      .processing_start_time = base_->MsAfterTimeOrigin(0),
      .processing_end_time = base_->MsAfterTimeOrigin(0)};
  PerformanceEventTiming* test_entry = PerformanceEventTiming::Create(
      AtomicString("event"), info, false, nullptr,
      LocalDOMWindow::From(scope.GetScriptState()));

  // Create copy of the test_buffer_.
  PerformanceEntryVector sorted_buffer_ = test_buffer_;

  base_->InsertEntryIntoSortedBuffer(test_buffer_, *test_entry,
                                     Performance::kDoNotRecordSwaps);

  sorted_buffer_.push_back(*test_entry);
  std::sort(sorted_buffer_.begin(), sorted_buffer_.end(),
            PerformanceEntry::StartTimeCompareLessThan);

  EXPECT_EQ(test_buffer_, sorted_buffer_);
}

// Validate ordering when inserting to the front of a buffer.
TEST_F(PerformanceTest, InsertEntryToFrontOfBuffer) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  PerformanceEntryVector test_buffer_;

  // Insert 3 entries into the vector.
  for (int i = 0; i < 3; i++) {
    double tmp = 1.0;

    PerformanceEventTiming::EventTimingReportingInfo info{
        .creation_time = base_->MsAfterTimeOrigin(tmp * i),
        .processing_start_time = base_->MsAfterTimeOrigin(0),
        .processing_end_time = base_->MsAfterTimeOrigin(0)};

    PerformanceEventTiming* entry = PerformanceEventTiming::Create(
        AtomicString("event"), info, false, nullptr,
        LocalDOMWindow::From(scope.GetScriptState()));
    test_buffer_.push_back(*entry);
  }

  PerformanceEventTiming::EventTimingReportingInfo info{
      .creation_time = base_->MsAfterTimeOrigin(0),
      .processing_start_time = base_->MsAfterTimeOrigin(0),
      .processing_end_time = base_->MsAfterTimeOrigin(0)};

  PerformanceEventTiming* test_entry = PerformanceEventTiming::Create(
      AtomicString("event"), info, false, nullptr,
      LocalDOMWindow::From(scope.GetScriptState()));

  // Create copy of the test_buffer_.
  PerformanceEntryVector sorted_buffer_ = test_buffer_;

  base_->InsertEntryIntoSortedBuffer(test_buffer_, *test_entry,
                                     Performance::kDoNotRecordSwaps);

  sorted_buffer_.push_back(*test_entry);
  std::sort(sorted_buffer_.begin(), sorted_buffer_.end(),
            PerformanceEntry::StartTimeCompareLessThan);

  EXPECT_EQ(test_buffer_, sorted_buffer_);
}

TEST_F(PerformanceTest, MergePerformanceEntryVectorsTest) {
  V8TestingScope scope;
  Initialize(scope.GetScriptState());

  PerformanceEntryVector first_vector;
  PerformanceEntryVector second_vector;

  PerformanceEntryVector test_vector;

  for (int i = 0; i < 6; i += 2) {
    double tmp = 1.0;

    PerformanceEventTiming::EventTimingReportingInfo info{
        .creation_time = base_->MsAfterTimeOrigin(tmp * i),
        .processing_start_time = base_->MsAfterTimeOrigin(0),
        .processing_end_time = base_->MsAfterTimeOrigin(0)};

    PerformanceEventTiming* entry = PerformanceEventTiming::Create(
        AtomicString("event"), info, false, nullptr,
        LocalDOMWindow::From(scope.GetScriptState()));
    first_vector.push_back(*entry);
    test_vector.push_back(*entry);
  }

  for (int i = 1; i < 6; i += 2) {
    double tmp = 1.0;

    PerformanceEventTiming::EventTimingReportingInfo info{
        .creation_time = base_->MsAfterTimeOrigin(tmp * i),
        .processing_start_time = base_->MsAfterTimeOrigin(0),
        .processing_end_time = base_->MsAfterTimeOrigin(0)};

    PerformanceEventTiming* entry = PerformanceEventTiming::Create(
        AtomicString("event"), info, false, nullptr,
        LocalDOMWindow::From(scope.GetScriptState()));
    second_vector.push_back(*entry);
    test_vector.push_back(*entry);
  }

  PerformanceEntryVector all_entries;
  all_entries =
      MergePerformanceEntryVectors(all_entries, first_vector, g_null_atom);
  all_entries =
      MergePerformanceEntryVectors(all_entries, second_vector, g_null_atom);

  std::sort(test_vector.begin(), test_vector.end(),
            PerformanceEntry::StartTimeCompareLessThan);

  EXPECT_EQ(all_entries, test_vector);
}

}  // namespace blink
