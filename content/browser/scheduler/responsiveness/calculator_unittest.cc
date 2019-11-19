// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/calculator.h"

#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace responsiveness {

namespace {
// Copied from calculator.cc.
constexpr int kMeasurementIntervalInMs = 30 * 1000;
constexpr int kJankThresholdInMs = 100;

class FakeCalculator : public Calculator {
 public:
  std::vector<size_t>& Emissions() { return janky_slices_; }

  void EmitResponsiveness(size_t janky_slices) override {
    janky_slices_.push_back(janky_slices);
  }

  using Calculator::GetLastCalculationTime;

 private:
  std::vector<size_t> janky_slices_;
};

}  // namespace

class ResponsivenessCalculatorTest : public testing::Test {
 public:
  void SetUp() override {
    calculator_ = std::make_unique<FakeCalculator>();
    last_calculation_time_ = calculator_->GetLastCalculationTime();
  }

  void AddEventUI(int schedule_time_in_ms, int finish_time_in_ms) {
    calculator_->TaskOrEventFinishedOnUIThread(
        last_calculation_time_ +
            base::TimeDelta::FromMilliseconds(schedule_time_in_ms),
        last_calculation_time_ +
            base::TimeDelta::FromMilliseconds(finish_time_in_ms));
  }

  void AddEventIO(int schedule_time_in_ms, int finish_time_in_ms) {
    calculator_->TaskOrEventFinishedOnIOThread(
        last_calculation_time_ +
            base::TimeDelta::FromMilliseconds(schedule_time_in_ms),
        last_calculation_time_ +
            base::TimeDelta::FromMilliseconds(finish_time_in_ms));
  }

  void TriggerCalculation() {
    AddEventUI(kMeasurementIntervalInMs + 1, kMeasurementIntervalInMs + 2);
    last_calculation_time_ = calculator_->GetLastCalculationTime();
  }

 protected:
  // This member sets up BrowserThread::IO and BrowserThread::UI. It must be the
  // first member, as other members may depend on these abstractions.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<FakeCalculator> calculator_;
  base::TimeTicks last_calculation_time_;
};

// A single event of length slightly longer than kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, ShortJank) {
  AddEventUI(40, 40 + kJankThresholdInMs + 5);
  TriggerCalculation();

  ASSERT_EQ(1u, calculator_->Emissions().size());
  EXPECT_EQ(1u, calculator_->Emissions()[0]);
}

// A single event of length slightly longer than 10 * kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, LongJank) {
  AddEventUI(40, 40 + 10 * kJankThresholdInMs + 5);
  TriggerCalculation();

  ASSERT_EQ(1u, calculator_->Emissions().size());
  EXPECT_EQ(10u, calculator_->Emissions()[0]);
}

// Events that last less than 100ms do not jank, regardless of start time.
TEST_F(ResponsivenessCalculatorTest, NoJank) {
  int base_time = 30;
  for (int i = 0; i < kJankThresholdInMs; ++i) {
    AddEventUI(base_time, base_time + i);
  }

  base_time += kJankThresholdInMs;
  for (int i = 0; i < kJankThresholdInMs; ++i) {
    AddEventUI(base_time + i, base_time + 2 * i);
  }

  TriggerCalculation();
  ASSERT_EQ(1u, calculator_->Emissions().size());
  EXPECT_EQ(0u, calculator_->Emissions()[0]);
}

// 10 Jank events, but very closely overlapping. Time slices are discretized and
// fixed, e.g. [0 100] [100 200] [200 300]. In this test, the events all start
// in the [0 100] slice and end in the [100 200] slice. All of them end up
// marking the [100 200] slice as janky.
TEST_F(ResponsivenessCalculatorTest, OverlappingJank) {
  int base_time = 30;
  for (int i = 0; i < 10; ++i) {
    AddEventUI(base_time, base_time + kJankThresholdInMs + 10);
  }

  TriggerCalculation();
  ASSERT_EQ(1u, calculator_->Emissions().size());
  EXPECT_EQ(1u, calculator_->Emissions()[0]);
}

// UI thread has 3 jank events on slices 1, 2, 3
// IO thread has 3 jank events on slices 3, 4, 5,
// There should be a total of 5 jank events.
TEST_F(ResponsivenessCalculatorTest, OverlappingJankMultipleThreads) {
  int base_time = 105;
  for (int i = 0; i < 3; ++i) {
    AddEventUI(base_time + i * kJankThresholdInMs,
               base_time + (i + 1) * kJankThresholdInMs + 10);
  }

  base_time = 305;
  for (int i = 0; i < 3; ++i) {
    AddEventIO(base_time + i * kJankThresholdInMs,
               base_time + (i + 1) * kJankThresholdInMs + 10);
  }

  TriggerCalculation();
  ASSERT_EQ(1u, calculator_->Emissions().size());
  EXPECT_EQ(5u, calculator_->Emissions()[0]);
}

// Three janks, each of length 2, separated by some shorter events.
TEST_F(ResponsivenessCalculatorTest, SeparatedJanks) {
  int base_time = 105;

  for (int i = 0; i < 3; ++i) {
    AddEventUI(base_time, base_time + 1);
    AddEventUI(base_time, base_time + 2 * kJankThresholdInMs + 1);
    base_time += 10 * kJankThresholdInMs;
  }
  TriggerCalculation();

  ASSERT_EQ(1u, calculator_->Emissions().size());
  EXPECT_EQ(6u, calculator_->Emissions()[0]);
}

TEST_F(ResponsivenessCalculatorTest, MultipleTrigger) {
  int base_time = 105;

  // 3 Janks, then trigger, then repeat.
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 3; ++j) {
      AddEventUI(base_time, base_time + 3 * kJankThresholdInMs + 1);
      base_time += 3 * kJankThresholdInMs;
    }
    TriggerCalculation();
  }

  ASSERT_EQ(10u, calculator_->Emissions().size());
  for (int i = 0; i < 10; ++i) {
    EXPECT_EQ(9u, calculator_->Emissions()[i]);
  }
}

// A long delay means that the machine likely went to sleep.
TEST_F(ResponsivenessCalculatorTest, LongDelay) {
  int base_time = 105;
  AddEventUI(base_time, base_time + 3 * kJankThresholdInMs + 1);
  base_time += 10 * kMeasurementIntervalInMs;
  AddEventUI(base_time, base_time + 1);

  ASSERT_EQ(0u, calculator_->Emissions().size());
}

// A long event means that the machine likely went to sleep.
TEST_F(ResponsivenessCalculatorTest, LongEvent) {
  int base_time = 105;
  AddEventUI(base_time, base_time + 10 * kMeasurementIntervalInMs);

  ASSERT_EQ(0u, calculator_->Emissions().size());
}

// An event that crosses a measurement interval boundary should count towards
// both measurement intervals.
TEST_F(ResponsivenessCalculatorTest, EventCrossesBoundary) {
  // Dummy event so that Calculator doesn't think the process is suspended.
  AddEventUI(0.5 * kMeasurementIntervalInMs, 0.5 * kMeasurementIntervalInMs);

  // The event goes from [29801, 30150]. It should count as 1 jank in the first
  // measurement interval and 2 in the second.
  AddEventUI(kMeasurementIntervalInMs - 2 * kJankThresholdInMs + 1,
             kMeasurementIntervalInMs + 1.5 * kJankThresholdInMs);

  // Dummy event so that Calculator doesn't think the process is suspended.
  AddEventUI(1.5 * kMeasurementIntervalInMs, 1.5 * kMeasurementIntervalInMs);

  // Trigger another calculation.
  AddEventUI(2 * kMeasurementIntervalInMs + 1,
             2 * kMeasurementIntervalInMs + 1);
  ASSERT_EQ(2u, calculator_->Emissions().size());
  EXPECT_EQ(1u, calculator_->Emissions()[0]);
  EXPECT_EQ(2u, calculator_->Emissions()[1]);
}

// Events may not be ordered by start or end time.
TEST_F(ResponsivenessCalculatorTest, UnorderedEvents) {
  // We add the following events:
  //   [100, 250]
  //   [150, 300]
  //   [50, 200]
  //   [50, 390]
  // The event [50, 400] subsumes all previous events.
  AddEventUI(kJankThresholdInMs, 2.5 * kJankThresholdInMs);
  AddEventUI(1.5 * kJankThresholdInMs, 3 * kJankThresholdInMs);
  AddEventUI(0.5 * kJankThresholdInMs, 2 * kJankThresholdInMs);
  AddEventUI(0.5 * kJankThresholdInMs, 3.9 * kJankThresholdInMs);

  TriggerCalculation();

  ASSERT_EQ(1u, calculator_->Emissions().size());
  EXPECT_EQ(3u, calculator_->Emissions()[0]);
}

}  // namespace responsiveness
}  // namespace content
