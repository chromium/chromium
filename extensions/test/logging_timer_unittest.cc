// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/test/logging_timer.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Shorthand.
const auto& GetMS = base::Milliseconds<int>;

}  // namespace

class LoggingTimerTest : public testing::Test {
 public:
  LoggingTimerTest() {}

  LoggingTimerTest(const LoggingTimerTest&) = delete;
  LoggingTimerTest& operator=(const LoggingTimerTest&) = delete;

  ~LoggingTimerTest() override {}

  void SetUp() override { LoggingTimer::set_clock_for_testing(&tick_clock_); }
  void TearDown() override { LoggingTimer::set_clock_for_testing(nullptr); }

  // Creates a LoggingTimer for the |key|, and advanced the clock by
  // |elapsed|.
  void KillTime(base::TimeDelta elapsed, const char* key) {
    LoggingTimer timer(key);
    tick_clock_.Advance(elapsed);
  }

 private:
  base::SimpleTestTickClock tick_clock_;
};

TEST_F(LoggingTimerTest, TestIncrements) {
  constexpr char key[] = "test_increments";
  EXPECT_TRUE(LoggingTimer::GetTrackedTime(key).is_zero());
  KillTime(GetMS(10), key);
  base::TimeDelta time1 = LoggingTimer::GetTrackedTime(key);
  EXPECT_EQ(GetMS(10), time1);
  KillTime(GetMS(10), key);
  base::TimeDelta time2 = LoggingTimer::GetTrackedTime(key);
  EXPECT_EQ(GetMS(20), time2);
}

TEST_F(LoggingTimerTest, TestMultipleKeys) {
  constexpr char key1[] = "key1";
  constexpr char key2[] = "key2";
  // Timers are grouped by pointer equality, so key3 should be separate
  // (despite having the same name as key1).
  constexpr char key3[] = "key1";

  EXPECT_TRUE(LoggingTimer::GetTrackedTime(key1).is_zero());
  EXPECT_TRUE(LoggingTimer::GetTrackedTime(key2).is_zero());
  EXPECT_TRUE(LoggingTimer::GetTrackedTime(key3).is_zero());

  KillTime(GetMS(10), key1);
  EXPECT_EQ(GetMS(10), LoggingTimer::GetTrackedTime(key1));
  EXPECT_TRUE(LoggingTimer::GetTrackedTime(key2).is_zero());
  EXPECT_TRUE(LoggingTimer::GetTrackedTime(key3).is_zero());

  KillTime(GetMS(10), key1);
  EXPECT_EQ(GetMS(20), LoggingTimer::GetTrackedTime(key1));
  EXPECT_TRUE(LoggingTimer::GetTrackedTime(key2).is_zero());
  EXPECT_TRUE(LoggingTimer::GetTrackedTime(key3).is_zero());

  KillTime(GetMS(10), key2);
  EXPECT_EQ(GetMS(20), LoggingTimer::GetTrackedTime(key1));
  EXPECT_EQ(GetMS(10), LoggingTimer::GetTrackedTime(key2));
  EXPECT_TRUE(LoggingTimer::GetTrackedTime(key3).is_zero());

  KillTime(GetMS(5), key3);
  EXPECT_EQ(GetMS(20), LoggingTimer::GetTrackedTime(key1));
  EXPECT_EQ(GetMS(10), LoggingTimer::GetTrackedTime(key2));
  EXPECT_EQ(GetMS(5), LoggingTimer::GetTrackedTime(key3));
}

}  // namespace extensions
