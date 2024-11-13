// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/performance/performance_scenario_observer.h"

#include <atomic>
#include <optional>

#include "base/barrier_closure.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/structured_shared_memory.h"
#include "base/scoped_multi_source_observation.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/performance/performance_scenarios.h"

namespace blink::performance_scenarios {

namespace {

using ::testing::_;

class MockPerformanceScenarioObserver : public PerformanceScenarioObserver {
 public:
  MOCK_METHOD(void,
              OnLoadingScenarioChanged,
              (ScenarioScope scope,
               LoadingScenario old_scenario,
               LoadingScenario new_scenario),
              (override));
  MOCK_METHOD(void,
              OnInputScenarioChanged,
              (ScenarioScope scope,
               InputScenario old_scenario,
               InputScenario new_scenario),
              (override));
};
using StrictMockPerformanceScenarioObserver =
    ::testing::StrictMock<MockPerformanceScenarioObserver>;

class PerformanceScenarioObserverTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(process_shared_memory_.has_value());
    ASSERT_TRUE(global_shared_memory_.has_value());
  }

  // Returns a closure that will end TaskEnvironment::RunUntilQuit after
  // `num_expectations` mock expectations are matched.
  base::RepeatingClosure QuitAfterExpectations(size_t num_expectations) {
    return base::BarrierClosure(num_expectations, task_env_.QuitClosure());
  }

  void WaitAndVerifyExpectations(
      MockPerformanceScenarioObserver* mock_observer) {
    task_env_.RunUntilQuit();
    EXPECT_TRUE(::testing::Mock::VerifyAndClearExpectations(mock_observer));
  }

 protected:
  // Writable shared memory regions for the scenario state.
  std::optional<base::StructuredSharedMemory<ScenarioState>>
      process_shared_memory_ =
          base::StructuredSharedMemory<ScenarioState>::Create();
  std::optional<base::StructuredSharedMemory<ScenarioState>>
      global_shared_memory_ =
          base::StructuredSharedMemory<ScenarioState>::Create();

  base::test::TaskEnvironment task_env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(PerformanceScenarioObserverTest, GetForScope) {
  EXPECT_FALSE(PerformanceScenarioObserverList::GetForScope(
      ScenarioScope::kCurrentProcess));
  EXPECT_FALSE(
      PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal));

  {
    ScopedReadOnlyScenarioMemory scoped_process_memory(
        ScenarioScope::kCurrentProcess,
        process_shared_memory_->DuplicateReadOnlyRegion());
    EXPECT_TRUE(PerformanceScenarioObserverList::GetForScope(
        ScenarioScope::kCurrentProcess));
    EXPECT_FALSE(
        PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal));

    {
      ScopedReadOnlyScenarioMemory scoped_global_memory(
          ScenarioScope::kGlobal,
          global_shared_memory_->DuplicateReadOnlyRegion());
      EXPECT_TRUE(PerformanceScenarioObserverList::GetForScope(
          ScenarioScope::kCurrentProcess));
      EXPECT_TRUE(
          PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal));
    }

    EXPECT_TRUE(PerformanceScenarioObserverList::GetForScope(
        ScenarioScope::kCurrentProcess));
    EXPECT_FALSE(
        PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal));
  }

  EXPECT_FALSE(PerformanceScenarioObserverList::GetForScope(
      ScenarioScope::kCurrentProcess));
  EXPECT_FALSE(
      PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal));
}

TEST_F(PerformanceScenarioObserverTest, NotifyOnChange) {
  // Update the process scenario state before creating the ObserverList, to
  // make sure the state tracking doesn't depend on the state starting at
  // kNoPageLoading.
  process_shared_memory_->WritableRef().loading.store(
      LoadingScenario::kFocusedPageLoading, std::memory_order_relaxed);

  ScopedReadOnlyScenarioMemory scoped_process_memory(
      ScenarioScope::kCurrentProcess,
      process_shared_memory_->DuplicateReadOnlyRegion());
  ScopedReadOnlyScenarioMemory scoped_global_memory(
      ScenarioScope::kGlobal, global_shared_memory_->DuplicateReadOnlyRegion());

  StrictMockPerformanceScenarioObserver mock_observer;
  base::ScopedMultiSourceObservation<PerformanceScenarioObserverList,
                                     PerformanceScenarioObserver>
      scoped_observation(&mock_observer);
  scoped_observation.AddObservation(
      PerformanceScenarioObserverList::GetForScope(
          ScenarioScope::kCurrentProcess)
          .get());
  scoped_observation.AddObservation(
      PerformanceScenarioObserverList::GetForScope(ScenarioScope::kGlobal)
          .get());

  // Toggle process loading scenario, then global loading scenario.
  auto quit_closure = QuitAfterExpectations(2);
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kCurrentProcess,
                                       LoadingScenario::kFocusedPageLoading,
                                       LoadingScenario::kBackgroundPageLoading))
      .WillOnce(base::test::RunClosure(quit_closure));
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kGlobal,
                                       LoadingScenario::kNoPageLoading,
                                       LoadingScenario::kVisiblePageLoading))
      .WillOnce(base::test::RunClosure(quit_closure));

  process_shared_memory_->WritableRef().loading.store(
      LoadingScenario::kBackgroundPageLoading, std::memory_order_relaxed);
  global_shared_memory_->WritableRef().loading.store(
      LoadingScenario::kVisiblePageLoading, std::memory_order_relaxed);
  PerformanceScenarioObserverList::NotifyAllScopes();

  WaitAndVerifyExpectations(&mock_observer);

  // Toggle process scenario again without changing global scenario.
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kCurrentProcess,
                                       LoadingScenario::kBackgroundPageLoading,
                                       LoadingScenario::kFocusedPageLoading))
      .WillOnce(base::test::RunClosure(QuitAfterExpectations(1)));

  process_shared_memory_->WritableRef().loading.store(
      LoadingScenario::kFocusedPageLoading, std::memory_order_relaxed);
  PerformanceScenarioObserverList::NotifyAllScopes();

  WaitAndVerifyExpectations(&mock_observer);

  // Stop observing the process scenario, then toggle both scenarios again.
  EXPECT_CALL(mock_observer,
              OnLoadingScenarioChanged(ScenarioScope::kGlobal,
                                       LoadingScenario::kVisiblePageLoading,
                                       LoadingScenario::kNoPageLoading))
      .WillOnce(base::test::RunClosure(QuitAfterExpectations(1)));

  scoped_observation.RemoveObservation(
      PerformanceScenarioObserverList::GetForScope(
          ScenarioScope::kCurrentProcess)
          .get());
  process_shared_memory_->WritableRef().loading.store(
      LoadingScenario::kBackgroundPageLoading, std::memory_order_relaxed);
  global_shared_memory_->WritableRef().loading.store(
      LoadingScenario::kNoPageLoading, std::memory_order_relaxed);
  PerformanceScenarioObserverList::NotifyAllScopes();

  WaitAndVerifyExpectations(&mock_observer);
}

}  // namespace

}  // namespace blink::performance_scenarios
