// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/calculator.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
namespace responsiveness {

using JankType = Calculator::JankType;
using StartupStage = Calculator::StartupStage;
using ::testing::_;

namespace {
// Copied from calculator.cc.
constexpr int kMeasurementIntervalInMs = 30 * 1000;
constexpr int kJankThresholdInMs = 100;

class FakeCalculator : public Calculator {
 public:
  MOCK_METHOD3(EmitResponsivenessMock,
               void(JankType jank_type,
                    size_t janky_slices,
                    StartupStage startup_stage));

  void EmitResponsiveness(JankType jank_type,
                          size_t janky_slices,
                          StartupStage startup_stage) override {
    EmitResponsivenessMock(jank_type, janky_slices, startup_stage);
    // Emit the histograms anyways for verification in some tests.
    Calculator::EmitResponsiveness(jank_type, janky_slices, startup_stage);
  }

  MOCK_METHOD3(EmitJankyIntervalsMeasurementTraceEvent,
               void(base::TimeTicks start_time,
                    base::TimeTicks end_time,
                    size_t amount_of_slices));

  MOCK_METHOD2(EmitJankyIntervalsJankTraceEvent,
               void(base::TimeTicks start_time, base::TimeTicks end_time));

  using Calculator::EmitResponsivenessTraceEvents;
  using Calculator::GetLastCalculationTime;
};

}  // namespace

class ResponsivenessCalculatorTest : public testing::Test {
 public:
  void SetUp() override {
    calculator_ = std::make_unique<testing::StrictMock<FakeCalculator>>();
    last_calculation_time_ = calculator_->GetLastCalculationTime();
#if BUILDFLAG(IS_ANDROID)
    base::android::ApplicationStatusListener::NotifyApplicationStateChange(
        base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
    base::RunLoop().RunUntilIdle();
#endif
  }

  void AddEventUI(int queue_time_in_ms,
                  int execution_start_time_in_ms,
                  int execution_finish_time_in_ms) {
    calculator_->TaskOrEventFinishedOnUIThread(
        last_calculation_time_ + base::Milliseconds(queue_time_in_ms),
        last_calculation_time_ + base::Milliseconds(execution_start_time_in_ms),
        last_calculation_time_ +
            base::Milliseconds(execution_finish_time_in_ms));
  }

  void AddEventIO(int queue_time_in_ms,
                  int execution_start_time_in_ms,
                  int execution_finish_time_in_ms) {
    calculator_->TaskOrEventFinishedOnIOThread(
        last_calculation_time_ + base::Milliseconds(queue_time_in_ms),
        last_calculation_time_ + base::Milliseconds(execution_start_time_in_ms),
        last_calculation_time_ +
            base::Milliseconds(execution_finish_time_in_ms));
  }

  void TriggerCalculation() {
    AddEventUI(kMeasurementIntervalInMs + 1, kMeasurementIntervalInMs + 1,
               kMeasurementIntervalInMs + 1);
    last_calculation_time_ = calculator_->GetLastCalculationTime();
  }

 protected:
  // This member sets up BrowserThread::IO and BrowserThread::UI. It must be the
  // first member, as other members may depend on these abstractions.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<FakeCalculator> calculator_;
  base::TimeTicks last_calculation_time_;
};

#define EXPECT_EXECUTION_JANKY_SLICES(num_slices, phase)                 \
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(JankType::kExecution, \
                                                   num_slices, phase));
#define EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(num_slices, phase)             \
  EXPECT_CALL(*calculator_,                                                    \
              EmitResponsivenessMock(JankType::kQueueAndExecution, num_slices, \
                                     phase));

// A single event executing slightly longer than kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, ShortExecutionJank) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = 40;
  constexpr int kFinishTime = kStartTime + kJankThresholdInMs + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_JANKY_SLICES(1u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// A single event queued slightly longer than kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, ShortQueueJank) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + kJankThresholdInMs + 5;
  constexpr int kFinishTime = kStartTime + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// A single event whose queuing and execution time together take longer than
// kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, ShortCombinedQueueAndExecutionJank) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + (kJankThresholdInMs / 2);
  constexpr int kFinishTime = kStartTime + (kJankThresholdInMs / 2) + 1;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// A single event executing slightly longer than 10 * kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, LongExecutionJank) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = 40;
  constexpr int kFinishTime = kStartTime + 10 * kJankThresholdInMs + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_JANKY_SLICES(10, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(10u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// A single event executing slightly longer than 10 * kJankThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, LongQueueJank) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + 10 * kJankThresholdInMs + 5;
  constexpr int kFinishTime = kStartTime + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(10u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// Events that execute in less than 100ms do not jank, regardless of start time.
TEST_F(ResponsivenessCalculatorTest, NoExecutionJank) {
  int base_time = 30;
  for (int i = 0; i < kJankThresholdInMs; ++i) {
    AddEventUI(base_time, base_time, base_time + i);
  }

  base_time += kJankThresholdInMs;
  for (int i = 0; i < kJankThresholdInMs; ++i) {
    AddEventUI(base_time + i, base_time + i, base_time + 2 * i);
  }

  EXPECT_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// Events that are queued and execute in less than 100ms do not jank, regardless
// of start time.
TEST_F(ResponsivenessCalculatorTest, NoQueueJank) {
  int base_time = 30;
  for (int i = 0; i < kJankThresholdInMs; ++i) {
    AddEventUI(base_time, base_time + i, base_time + i);
  }

  base_time += kJankThresholdInMs;
  for (int i = 0; i < kJankThresholdInMs; ++i) {
    AddEventUI(base_time + i, base_time + 2 * i, base_time + 2 * i);
  }

  EXPECT_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// 10 execution jank events, but very closely overlapping. Time slices are
// discretized and fixed, e.g. [0 100] [100 200] [200 300]. In this test, the
// events all start in the [0 100] slice and end in the [100 200] slice. All of
// them end up marking the [100 200] slice as janky.
TEST_F(ResponsivenessCalculatorTest, OverlappingExecutionJank) {
  int base_time = 30;
  for (int i = 0; i < 10; ++i) {
    const int queue_time = base_time;
    const int start_time = base_time;
    const int finish_time = start_time + kJankThresholdInMs + i;
    AddEventUI(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_JANKY_SLICES(1u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// 10 queue jank events, but very closely overlapping. Time slices are
// discretized and fixed, e.g. [0 100] [100 200] [200 300]. In this test, the
// events are all queued in the [0 100] slice and start executing in the [100
// 200] slice. All of them end up marking the [100 200] slice as janky.
TEST_F(ResponsivenessCalculatorTest, OverlappingQueueJank) {
  int base_time = 30;
  for (int i = 0; i < 10; ++i) {
    const int queue_time = base_time;
    const int start_time = base_time + kJankThresholdInMs + i;
    const int finish_time = start_time + 1;
    AddEventUI(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// UI thread has 3 execution jank events on slices 1, 2, 3
// IO thread has 3 execution jank events on slices 3, 4, 5,
// There should be a total of 5 jank events.
TEST_F(ResponsivenessCalculatorTest, OverlappingExecutionJankMultipleThreads) {
  int base_time = 105;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kJankThresholdInMs;
    const int start_time = queue_time;
    const int finish_time = start_time + kJankThresholdInMs + 10;
    AddEventUI(queue_time, start_time, finish_time);
  }

  base_time = 305;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kJankThresholdInMs;
    const int start_time = queue_time;
    const int finish_time = start_time + kJankThresholdInMs + 10;
    AddEventIO(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_JANKY_SLICES(5u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(5u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// UI thread has 3 queue jank events on slices 1, 2, 3
// IO thread has 3 queue jank events on slices 3, 4, 5,
// There should be a total of 5 jank events.
TEST_F(ResponsivenessCalculatorTest, OverlappingQueueJankMultipleThreads) {
  int base_time = 105;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kJankThresholdInMs;
    const int start_time = queue_time + kJankThresholdInMs + 10;
    const int finish_time = start_time;
    AddEventUI(queue_time, start_time, finish_time);
  }

  base_time = 305;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kJankThresholdInMs;
    const int start_time = queue_time + kJankThresholdInMs + 10;
    const int finish_time = start_time;
    AddEventIO(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(5u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// Three execution janks, each of length 2, separated by some shorter events.
TEST_F(ResponsivenessCalculatorTest, SeparatedExecutionJanks) {
  int base_time = 105;

  for (int i = 0; i < 3; ++i) {
    {
      const int queue_time = base_time;
      const int start_time = base_time;
      const int finish_time = base_time + 1;
      AddEventUI(queue_time, start_time, finish_time);
    }
    {
      const int queue_time = base_time;
      const int start_time = base_time;
      const int finish_time = base_time + 2 * kJankThresholdInMs + 1;
      AddEventUI(queue_time, start_time, finish_time);
    }
    base_time += 10 * kJankThresholdInMs;
  }

  EXPECT_EXECUTION_JANKY_SLICES(6u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(6u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// Three queue janks, each of length 2, separated by some shorter events.
TEST_F(ResponsivenessCalculatorTest, SeparatedQueueJanks) {
  int base_time = 105;

  for (int i = 0; i < 3; ++i) {
    {
      const int queue_time = base_time;
      const int start_time = base_time + 1;
      const int finish_time = start_time;
      AddEventUI(queue_time, start_time, finish_time);
    }
    {
      const int queue_time = base_time;
      const int start_time = base_time + 2 * kJankThresholdInMs + 1;
      const int finish_time = start_time;
      AddEventUI(queue_time, start_time, finish_time);
    }
    base_time += 10 * kJankThresholdInMs;
  }

  EXPECT_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(6u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

TEST_F(ResponsivenessCalculatorTest, MultipleTrigger) {
  int base_time = 105;

  // 3 Janks, then trigger, then repeat.
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 3; ++j) {
      AddEventUI(base_time, base_time, base_time + 3 * kJankThresholdInMs + 1);
      base_time += 3 * kJankThresholdInMs;
    }

    EXPECT_EXECUTION_JANKY_SLICES(
        9u, i == 0 ? StartupStage::kFirstInterval
                   : StartupStage::kFirstIntervalDoneWithoutFirstIdle);
    EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(
        9u, i == 0 ? StartupStage::kFirstInterval
                   : StartupStage::kFirstIntervalDoneWithoutFirstIdle);
    TriggerCalculation();
    testing::Mock::VerifyAndClear(calculator_.get());
  }
}

// A long delay means that the machine likely went to sleep.
TEST_F(ResponsivenessCalculatorTest, LongDelay) {
  int base_time = 105;
  AddEventUI(base_time, base_time, base_time + 3 * kJankThresholdInMs + 1);
  base_time += 10 * kMeasurementIntervalInMs;
  AddEventUI(base_time, base_time, base_time + 1);

  EXPECT_CALL(*calculator_, EmitResponsivenessMock(_, _, _)).Times(0);
}

// A long event means that the machine likely went to sleep.
TEST_F(ResponsivenessCalculatorTest, LongEvent) {
  int base_time = 105;
  AddEventUI(base_time, base_time, base_time + 10 * kMeasurementIntervalInMs);

  EXPECT_CALL(*calculator_, EmitResponsivenessMock(_, _, _)).Times(0);
}

#if BUILDFLAG(IS_ANDROID)
// Metric should not be recorded when application is in background.
TEST_F(ResponsivenessCalculatorTest, ApplicationInBackground) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = 40;
  constexpr int kFinishTime = kStartTime + kJankThresholdInMs + 5;
  AddEventUI(kQueueTime, kStartTime, kFinishTime);

  base::android::ApplicationStatusListener::NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  base::RunLoop().RunUntilIdle();

  AddEventUI(kQueueTime, kStartTime + 1, kFinishTime + 1);
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(_, _, _)).Times(0);
  TriggerCalculation();
}
#endif

TEST_F(ResponsivenessCalculatorTest, StartupStages) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + 10 * kJankThresholdInMs + 5;
  constexpr int kFinishTime = kStartTime + 5;

  absl::optional<base::HistogramTester> histograms;

  // Queue jank event during the first kMeasurementInterval.
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(JankType::kExecution, 0,
                                     StartupStage::kFirstInterval));
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(JankType::kQueueAndExecution, 10u,
                                     StartupStage::kFirstInterval));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds", 0, 1);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Initial", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Periodic", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Periodic", 0);

  // Queue jank event during a few kMeasurementInterval (without having seen
  // OnFirstIdle()). Neither .Initial nor .Periodic
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(JankType::kExecution, 0,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(JankType::kQueueAndExecution, 10u,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Periodic", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Periodic", 0);
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(JankType::kExecution, 0,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(JankType::kQueueAndExecution, 10u,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Periodic", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Periodic", 0);

  // OnFirstIdle() eventually during a kMeasurementInterval. Same as above, last
  // one of these.
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  calculator_->OnFirstIdle();
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(JankType::kExecution, 0,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(JankType::kQueueAndExecution, 10u,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Periodic", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Periodic", 0);

  // Events in intervals after OnFirstIdle(). Janky3.Initial still no .Periodic.
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(
                                JankType::kExecution, 0,
                                StartupStage::kFirstIntervalAfterFirstIdle));
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(
                                JankType::kQueueAndExecution, 10u,
                                StartupStage::kFirstIntervalAfterFirstIdle));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Periodic", 0);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3", 10, 1);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Initial", 10, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Periodic", 0);

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(JankType::kExecution, 0,
                                                   StartupStage::kPeriodic));
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(JankType::kQueueAndExecution, 10u,
                                     StartupStage::kPeriodic));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Initial", 0);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Periodic", 0, 1);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3", 10, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Initial", 0);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Periodic", 10, 1);
}

TEST_F(ResponsivenessCalculatorTest, FastStartupStages) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + 10 * kJankThresholdInMs + 5;
  constexpr int kFinishTime = kStartTime + 5;

  absl::optional<base::HistogramTester> histograms;

  // OnFirstIdle() right away during the first kMeasurementInterval. Still
  // considered as kFirstInterval, but second interval will go straight
  // to kFirstIntervalAfterFirstIdle.
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  calculator_->OnFirstIdle();
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(JankType::kExecution, 0,
                                     StartupStage::kFirstInterval));
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(JankType::kQueueAndExecution, 10u,
                                     StartupStage::kFirstInterval));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds", 0, 1);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Initial", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Periodic", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Periodic", 0);

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(
                                JankType::kExecution, 0,
                                StartupStage::kFirstIntervalAfterFirstIdle));
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(
                                JankType::kQueueAndExecution, 10u,
                                StartupStage::kFirstIntervalAfterFirstIdle));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Periodic", 0);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3", 10, 1);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Initial", 10, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Periodic", 0);

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(JankType::kExecution, 0,
                                                   StartupStage::kPeriodic));
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(JankType::kQueueAndExecution, 10u,
                                     StartupStage::kPeriodic));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Initial", 0);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds.Periodic", 0, 1);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3", 10, 1);
  histograms->ExpectTotalCount(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Initial", 0);
  histograms->ExpectUniqueSample(
      "Browser.Responsiveness.JankyIntervalsPerThirtySeconds3.Periodic", 10, 1);
}

// An event execution that crosses a measurement interval boundary should count
// towards both measurement intervals.
TEST_F(ResponsivenessCalculatorTest, ExecutionCrossesBoundary) {
  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 0.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // The event goes from [29801, 30150]. It should count as 1 jank in the first
  // measurement interval and 2 in the second.
  {
    EXPECT_EXECUTION_JANKY_SLICES(1u, StartupStage::kFirstInterval);
    EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u, StartupStage::kFirstInterval);
    const int queue_time =
        kMeasurementIntervalInMs - 2 * kJankThresholdInMs + 1;
    const int start_time = queue_time;
    const int finish_time = kMeasurementIntervalInMs + 1.5 * kJankThresholdInMs;
    AddEventUI(queue_time, start_time, finish_time);
  }

  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 1.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // Trigger another calculation.
  EXPECT_EXECUTION_JANKY_SLICES(
      2u, StartupStage::kFirstIntervalDoneWithoutFirstIdle);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(
      2u, StartupStage::kFirstIntervalDoneWithoutFirstIdle);

  const int kTime = 2 * kMeasurementIntervalInMs + 1;
  AddEventUI(kTime, kTime, kTime);
}

// An event queuing that crosses a measurement interval boundary should count
// towards both measurement intervals.
TEST_F(ResponsivenessCalculatorTest, QueuingCrossesBoundary) {
  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 0.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // The event goes from [29801, 30150]. It should count as 1 jank in the first
  // measurement interval and 2 in the second.
  {
    EXPECT_EXECUTION_JANKY_SLICES(0u, StartupStage::kFirstInterval);
    EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(1u, StartupStage::kFirstInterval);
    const int queue_time =
        kMeasurementIntervalInMs - 2 * kJankThresholdInMs + 1;
    const int start_time = kMeasurementIntervalInMs + 1.5 * kJankThresholdInMs;
    const int finish_time = start_time;
    AddEventUI(queue_time, start_time, finish_time);
  }

  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 1.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // Trigger another calculation.
  EXPECT_EXECUTION_JANKY_SLICES(
      0u, StartupStage::kFirstIntervalDoneWithoutFirstIdle);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(
      2u, StartupStage::kFirstIntervalDoneWithoutFirstIdle);

  const int kTime = 2 * kMeasurementIntervalInMs + 1;
  AddEventUI(kTime, kTime, kTime);
}

// Events may not be ordered by start or end time.
TEST_F(ResponsivenessCalculatorTest, UnorderedEvents) {
  // We add the following tasks:
  //   [100, 100, 250]
  //   [150, 150, 300]
  //   [50, 50, 200]
  //   [50, 50, 390] <- A
  //
  //   [1100, 1250, 1251]
  //   [1150, 1300, 1301]
  //   [1050, 1200, 1201]
  //   [1050, 1390, 1391] <- B
  //
  // The execution jank in A subsumes all other execution janks. The queue jank
  // in B subsumes all other queue janks.
  AddEventUI(100, 100, 250);
  AddEventUI(150, 150, 300);
  AddEventUI(50, 50, 200);
  AddEventUI(50, 50, 390);

  AddEventUI(1100, 1250, 1251);
  AddEventUI(1150, 1300, 1301);
  AddEventUI(1050, 1200, 1201);
  AddEventUI(1050, 1390, 1391);

  EXPECT_EXECUTION_JANKY_SLICES(3u, StartupStage::kFirstInterval);
  EXPECT_QUEUE_AND_EXECUTION_JANKY_SLICES(6u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

TEST_F(ResponsivenessCalculatorTest, EmitResponsivenessTraceEventsEmpty) {
  constexpr base::TimeTicks kStartTime = base::TimeTicks();
  constexpr base::TimeTicks kFinishTime =
      kStartTime + base::Milliseconds(kMeasurementIntervalInMs);
  const std::set<int> janky_slices;

  EXPECT_CALL(*calculator_, EmitJankyIntervalsMeasurementTraceEvent(_, _, _))
      .Times(0);

  calculator_->EmitResponsivenessTraceEvents(
      JankType::kQueueAndExecution, kStartTime, kFinishTime, janky_slices);
}

TEST_F(ResponsivenessCalculatorTest, EmitResponsivenessTraceEventsWrongMetric) {
  constexpr base::TimeTicks kStartTime = base::TimeTicks();
  constexpr base::TimeTicks kFinishTime =
      kStartTime + base::Milliseconds(kMeasurementIntervalInMs);
  const std::set<int> janky_slices = {1};

  EXPECT_CALL(*calculator_, EmitJankyIntervalsMeasurementTraceEvent(_, _, _))
      .Times(0);

  calculator_->EmitResponsivenessTraceEvents(JankType::kExecution, kStartTime,
                                             kFinishTime, janky_slices);
}

TEST_F(ResponsivenessCalculatorTest, EmitResponsivenessTraceEvents) {
  constexpr base::TimeDelta kSliceInterval =
      base::Milliseconds(kJankThresholdInMs);
  constexpr base::TimeTicks kStartTime = base::TimeTicks();
  constexpr base::TimeTicks kFinishTime =
      kStartTime + base::Milliseconds(kMeasurementIntervalInMs);

  const std::set<int> janky_slices = {3, 4, 5, 12, 15};

  EXPECT_CALL(*calculator_, EmitJankyIntervalsMeasurementTraceEvent(
                                kStartTime, kFinishTime, janky_slices.size()));

  EXPECT_CALL(*calculator_, EmitJankyIntervalsJankTraceEvent(
                                kStartTime + 3 * kSliceInterval,
                                kStartTime + 6 * kSliceInterval));
  EXPECT_CALL(*calculator_, EmitJankyIntervalsJankTraceEvent(
                                kStartTime + 12 * kSliceInterval,
                                kStartTime + 13 * kSliceInterval));
  EXPECT_CALL(*calculator_, EmitJankyIntervalsJankTraceEvent(
                                kStartTime + 15 * kSliceInterval,
                                kStartTime + 16 * kSliceInterval));

  calculator_->EmitResponsivenessTraceEvents(
      JankType::kQueueAndExecution, kStartTime, kFinishTime, janky_slices);
}

}  // namespace responsiveness
}  // namespace content
