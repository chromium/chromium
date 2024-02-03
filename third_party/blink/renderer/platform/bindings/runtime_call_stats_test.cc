// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"

#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

RuntimeCallStats::CounterId test_counter_1_id =
    RuntimeCallStats::CounterId::kTestCounter1;
RuntimeCallStats::CounterId test_counter_2_id =
    RuntimeCallStats::CounterId::kTestCounter2;

}  // namespace

class RuntimeCallStatsTest : public testing::Test {
 public:
  void SetUp() override {
    // Add one millisecond because RuntimeCallTimer uses |start_ticks_| =
    // base::TimeTicks() to represent that the timer is not running.
    clock_.SetNowTicks(base::TimeTicks() + base::Milliseconds(1));
  }

  void TearDown() override {
    features_backup_.Restore();
  }

  void AdvanceClock(int milliseconds) {
    clock_.Advance(base::Milliseconds(milliseconds));
  }

  const base::TickClock* clock() { return &clock_; }

 private:
  RuntimeEnabledFeatures::Backup features_backup_;
  base::SimpleTestTickClock clock_;
};

TEST_F(RuntimeCallStatsTest, InitialCountShouldBeZero) {
  RuntimeCallCounter counter("counter");
  EXPECT_EQ(0ul, counter.GetCount());
}

TEST_F(RuntimeCallStatsTest, StatsCounterNameIsCorrect) {
  RuntimeCallStats stats(clock());
  EXPECT_STREQ("Blink_TestCounter1",
               stats.GetCounter(test_counter_1_id)->GetName());
}

TEST_F(RuntimeCallStatsTest, TestBindingsCountersForMethods) {
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* method_counter =
      stats.GetCounter(RuntimeCallStats::CounterId::kBindingsMethodTestCounter);
  EXPECT_STREQ("Blink_BindingsMethodTestCounter", method_counter->GetName());
}

TEST_F(RuntimeCallStatsTest, TestBindingsCountersForReadOnlyAttributes) {
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* getter_counter =
      stats.GetCounter(RuntimeCallStats::CounterId::
                           kBindingsReadOnlyAttributeTestCounter_Getter);
  EXPECT_STREQ("Blink_BindingsReadOnlyAttributeTestCounter_Getter",
               getter_counter->GetName());
}

TEST_F(RuntimeCallStatsTest, TestBindingsCountersForAttributes) {
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* getter_counter = stats.GetCounter(
      RuntimeCallStats::CounterId::kBindingsAttributeTestCounter_Getter);
  RuntimeCallCounter* setter_counter = stats.GetCounter(
      RuntimeCallStats::CounterId::kBindingsAttributeTestCounter_Setter);
  EXPECT_STREQ("Blink_BindingsAttributeTestCounter_Getter",
               getter_counter->GetName());
  EXPECT_STREQ("Blink_BindingsAttributeTestCounter_Setter",
               setter_counter->GetName());
}

TEST_F(RuntimeCallStatsTest, CountIsUpdatedAfterLeave) {
  RuntimeCallTimer timer(clock());
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  stats.Enter(&timer, test_counter_1_id);
  EXPECT_EQ(0ul, counter->GetCount());
  stats.Leave(&timer);
  EXPECT_EQ(1ul, counter->GetCount());
}

TEST_F(RuntimeCallStatsTest, TimeIsUpdatedAfterLeave) {
  RuntimeCallStats stats(clock());
  RuntimeCallTimer timer(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  stats.Enter(&timer, test_counter_1_id);
  AdvanceClock(50);
  stats.Leave(&timer);
  EXPECT_EQ(50, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, CountAndTimeAreUpdatedAfterMultipleExecutions) {
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  const unsigned func_duration = 20;
  const unsigned loops = 5;

  RuntimeCallStatsTest* test = this;
  auto func = [&stats, test]() {
    RuntimeCallTimer timer(test->clock());
    stats.Enter(&timer, test_counter_1_id);
    test->AdvanceClock(func_duration);
    stats.Leave(&timer);
  };

  for (unsigned i = 0; i < loops; i++)
    func();

  EXPECT_EQ((uint64_t)loops, counter->GetCount());
  EXPECT_EQ(loops * func_duration, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, NestedTimersTest) {
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* outer_counter = stats.GetCounter(test_counter_1_id);
  RuntimeCallCounter* inner_counter = stats.GetCounter(test_counter_2_id);

  const unsigned inner_func_duration = 50;
  const unsigned outer_func_duration = 20;

  RuntimeCallStatsTest* test = this;
  auto inner_func = [&stats, test]() {
    RuntimeCallTimer timer(test->clock());
    stats.Enter(&timer, test_counter_2_id);
    test->AdvanceClock(inner_func_duration);
    stats.Leave(&timer);
  };

  auto outer_func = [&stats, &inner_func, test]() {
    RuntimeCallTimer timer(test->clock());
    stats.Enter(&timer, test_counter_1_id);
    inner_func();
    test->AdvanceClock(outer_func_duration);
    stats.Leave(&timer);
  };

  outer_func();

  EXPECT_EQ(1ul, outer_counter->GetCount());
  EXPECT_EQ(1ul, inner_counter->GetCount());
  EXPECT_EQ(outer_func_duration, outer_counter->GetTime().InMilliseconds());
  EXPECT_EQ(inner_func_duration, inner_counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, RuntimeCallTimerScopeTest) {
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  RuntimeCallStatsTest* test = this;
  auto func = [&stats, test]() {
    RuntimeCallTimerScope scope(&stats, test_counter_1_id);
    test->AdvanceClock(50);
  };

  func();

  EXPECT_EQ(1ul, counter->GetCount());
  EXPECT_EQ(50, counter->GetTime().InMilliseconds());

  func();

  EXPECT_EQ(2ul, counter->GetCount());
  EXPECT_EQ(100, counter->GetTime().InMilliseconds());
}

static void RecursiveRCSTestHelper(int remaining_count,
                                   RuntimeCallStatsTest& fixture,
                                   RuntimeCallStats& stats) {
  RuntimeCallTimerScope scope(&stats, test_counter_1_id);
  if (remaining_count <= 0)
    return;
  fixture.AdvanceClock(50);
  RecursiveRCSTestHelper(remaining_count - 1, fixture, stats);
}

TEST_F(RuntimeCallStatsTest, RecursiveFunctionWithScopeTest) {
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  RecursiveRCSTestHelper(5, *this, stats);

  EXPECT_EQ(6ul, counter->GetCount());
  EXPECT_EQ(250, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, ReuseTimer) {
  RuntimeCallStats stats(clock());
  RuntimeCallTimer timer(clock());
  RuntimeCallCounter* counter1 = stats.GetCounter(test_counter_1_id);
  RuntimeCallCounter* counter2 = stats.GetCounter(test_counter_2_id);

  stats.Enter(&timer, test_counter_1_id);
  AdvanceClock(50);
  stats.Leave(&timer);

  timer.Reset();

  stats.Enter(&timer, test_counter_2_id);
  AdvanceClock(25);
  stats.Leave(&timer);

  EXPECT_EQ(1ul, counter1->GetCount());
  EXPECT_EQ(1ul, counter2->GetCount());
  EXPECT_EQ(50, counter1->GetTime().InMilliseconds());
  EXPECT_EQ(25, counter2->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, ResetCallStats) {
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter1 = stats.GetCounter(test_counter_1_id);
  RuntimeCallCounter* counter2 = stats.GetCounter(test_counter_2_id);

  {
    RuntimeCallTimerScope scope1(&stats, test_counter_1_id);
    RuntimeCallTimerScope scope2(&stats, test_counter_2_id);
  }

  EXPECT_EQ(1ul, counter1->GetCount());
  EXPECT_EQ(1ul, counter2->GetCount());

  stats.Reset();

  EXPECT_EQ(0ul, counter1->GetCount());
  EXPECT_EQ(0ul, counter2->GetCount());
}

TEST_F(RuntimeCallStatsTest, TestEnterAndLeaveMacrosWithCallStatsDisabled) {
  ScopedBlinkRuntimeCallStatsForTest blink_runtime_call_stats(false);
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);
  RuntimeCallTimer timer(clock());

  RUNTIME_CALL_STATS_ENTER_WITH_RCS(&stats, &timer, test_counter_1_id);
  AdvanceClock(25);
  RUNTIME_CALL_STATS_LEAVE_WITH_RCS(&stats, &timer);

  EXPECT_EQ(0ul, counter->GetCount());
  EXPECT_EQ(0, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, TestEnterAndLeaveMacrosWithCallStatsEnabled) {
  ScopedBlinkRuntimeCallStatsForTest blink_runtime_call_stats(true);
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);
  RuntimeCallTimer timer(clock());

  RUNTIME_CALL_STATS_ENTER_WITH_RCS(&stats, &timer, test_counter_1_id);
  AdvanceClock(25);
  RUNTIME_CALL_STATS_LEAVE_WITH_RCS(&stats, &timer);

  EXPECT_EQ(1ul, counter->GetCount());
  EXPECT_EQ(25, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, TestScopeMacroWithCallStatsDisabled) {
  ScopedBlinkRuntimeCallStatsForTest blink_runtime_call_stats(false);
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  {
    RUNTIME_CALL_TIMER_SCOPE_WITH_RCS(&stats, test_counter_1_id);
    AdvanceClock(25);
  }

  EXPECT_EQ(0ul, counter->GetCount());
  EXPECT_EQ(0, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, TestScopeMacroWithCallStatsEnabled) {
  ScopedBlinkRuntimeCallStatsForTest blink_runtime_call_stats(true);
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  {
    RUNTIME_CALL_TIMER_SCOPE_WITH_RCS(&stats, test_counter_1_id);
    AdvanceClock(25);
  }

  EXPECT_EQ(1ul, counter->GetCount());
  EXPECT_EQ(25, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, TestScopeWithOptionalMacroWithCallStatsDisabled) {
  ScopedBlinkRuntimeCallStatsForTest blink_runtime_call_stats(false);
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  {
    std::optional<RuntimeCallTimerScope> scope;
    RUNTIME_CALL_TIMER_SCOPE_WITH_OPTIONAL_RCS(scope, &stats,
                                               test_counter_1_id);
    AdvanceClock(25);
  }

  EXPECT_EQ(0ul, counter->GetCount());
  EXPECT_EQ(0, counter->GetTime().InMilliseconds());
}

TEST_F(RuntimeCallStatsTest, TestScopeWithOptionalMacroWithCallStatsEnabled) {
  ScopedBlinkRuntimeCallStatsForTest blink_runtime_call_stats(true);
  RuntimeCallStats stats(clock());
  RuntimeCallCounter* counter = stats.GetCounter(test_counter_1_id);

  {
    std::optional<RuntimeCallTimerScope> scope;
    RUNTIME_CALL_TIMER_SCOPE_WITH_OPTIONAL_RCS(scope, &stats,
                                               test_counter_1_id);
    AdvanceClock(25);
  }

  EXPECT_EQ(1ul, counter->GetCount());
  EXPECT_EQ(25, counter->GetTime().InMilliseconds());
}

}  // namespace blink
