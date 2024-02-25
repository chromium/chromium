// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/calculator.h"

#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/responsiveness_calculator_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace responsiveness {

using CongestionType = Calculator::CongestionType;
using StartupStage = Calculator::StartupStage;
using ::testing::_;

namespace {
// Copied from calculator.cc.
constexpr int kMeasurementIntervalInMs = 30 * 1000;
constexpr int kCongestionThresholdInMs = 100;

class FakeCalculator : public Calculator {
 public:
  using Calculator::Calculator;

  MOCK_METHOD3(EmitResponsivenessMock,
               void(CongestionType congestion_type,
                    size_t congested_slices,
                    StartupStage startup_stage));

  void EmitResponsiveness(CongestionType congestion_type,
                          size_t congested_slices,
                          StartupStage startup_stage) override {
    EmitResponsivenessMock(congestion_type, congested_slices, startup_stage);
    // Emit the histograms anyways for verification in some tests.
    Calculator::EmitResponsiveness(congestion_type, congested_slices,
                                   startup_stage);
  }

  MOCK_METHOD3(EmitCongestedIntervalsMeasurementTraceEvent,
               void(base::TimeTicks start_time,
                    base::TimeTicks end_time,
                    size_t amount_of_slices));

  MOCK_METHOD2(EmitCongestedIntervalTraceEvent,
               void(base::TimeTicks start_time, base::TimeTicks end_time));

  using Calculator::EmitResponsivenessTraceEvents;
  using Calculator::GetLastCalculationTime;
};

}  // namespace

class MockDelegate : public ResponsivenessCalculatorDelegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  // ResponsivenessCalculatorDelegate:
  MOCK_METHOD(void, OnMeasurementIntervalEnded, (), (override));
  MOCK_METHOD(void,
              OnResponsivenessEmitted,
              (int sample,
               int min,
               int exclusive_max,
               size_t buckets),
              (override));
};

class ResponsivenessCalculatorTest : public testing::Test {
 public:
  void SetUp() override {
    auto delegate = std::make_unique<
        testing::NiceMock<MockDelegate>>();  // NiceMock because the delegate is
                                             // only verified in one test.
    delegate_ = delegate.get();
    calculator_ = std::make_unique<testing::StrictMock<FakeCalculator>>(
        std::move(delegate));
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
  raw_ptr<MockDelegate> delegate_;
  base::TimeTicks last_calculation_time_;
};

#define EXPECT_EXECUTION_CONGESTED_SLICES(num_slices, phase)         \
  EXPECT_CALL(*calculator_,                                          \
              EmitResponsivenessMock(CongestionType::kExecutionOnly, \
                                     num_slices, phase));
#define EXPECT_CONGESTED_SLICES(num_slices, phase)                       \
  EXPECT_CALL(*calculator_,                                              \
              EmitResponsivenessMock(CongestionType::kQueueAndExecution, \
                                     num_slices, phase));

// A single event executing slightly longer than kCongestionThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, ShortExecutionCongestion) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = 40;
  constexpr int kFinishTime = kStartTime + kCongestionThresholdInMs + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// A single event queued slightly longer than kCongestionThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, ShortQueueCongestion) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + kCongestionThresholdInMs + 5;
  constexpr int kFinishTime = kStartTime + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// A single event whose queuing and execution time together take longer than
// kCongestionThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, ShortCombinedQueueAndExecutionCongestion) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + (kCongestionThresholdInMs / 2);
  constexpr int kFinishTime = kStartTime + (kCongestionThresholdInMs / 2) + 1;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// A single event executing slightly longer than 10 * kCongestionThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, LongExecutionCongestion) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = 40;
  constexpr int kFinishTime = kStartTime + 10 * kCongestionThresholdInMs + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_CONGESTED_SLICES(10, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(10u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// A single event executing slightly longer than 10 * kCongestionThresholdInMs.
TEST_F(ResponsivenessCalculatorTest, LongQueueCongestion) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + 10 * kCongestionThresholdInMs + 5;
  constexpr int kFinishTime = kStartTime + 5;

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_EXECUTION_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(10u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// Events that execute in less than 100ms are not congested, regardless of start
// time.
TEST_F(ResponsivenessCalculatorTest, NoExecutionCongestion) {
  int base_time = 30;
  for (int i = 0; i < kCongestionThresholdInMs; ++i) {
    AddEventUI(base_time, base_time, base_time + i);
  }

  base_time += kCongestionThresholdInMs;
  for (int i = 0; i < kCongestionThresholdInMs; ++i) {
    AddEventUI(base_time + i, base_time + i, base_time + 2 * i);
  }

  EXPECT_EXECUTION_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// Events that are queued and execute in less than 100ms are not congested,
// regardless of start time.
TEST_F(ResponsivenessCalculatorTest, NoQueueCongestion) {
  int base_time = 30;
  for (int i = 0; i < kCongestionThresholdInMs; ++i) {
    AddEventUI(base_time, base_time + i, base_time + i);
  }

  base_time += kCongestionThresholdInMs;
  for (int i = 0; i < kCongestionThresholdInMs; ++i) {
    AddEventUI(base_time + i, base_time + 2 * i, base_time + 2 * i);
  }

  EXPECT_EXECUTION_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// 10 execution congestion events, but very closely overlapping. Time slices are
// discretized and fixed, e.g. [0 100] [100 200] [200 300]. In this test, the
// events all start in the [0 100] slice and end in the [100 200] slice. All of
// them end up marking the [100 200] slice as congested.
TEST_F(ResponsivenessCalculatorTest, OverlappingExecutionCongestion) {
  int base_time = 30;
  for (int i = 0; i < 10; ++i) {
    const int queue_time = base_time;
    const int start_time = base_time;
    const int finish_time = start_time + kCongestionThresholdInMs + i;
    AddEventUI(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// 10 queue congestion events, but very closely overlapping. Time slices are
// discretized and fixed, e.g. [0 100] [100 200] [200 300]. In this test, the
// events are all queued in the [0 100] slice and start executing in the [100
// 200] slice. All of them end up marking the [100 200] slice as congested.
TEST_F(ResponsivenessCalculatorTest, OverlappingQueueCongestion) {
  int base_time = 30;
  for (int i = 0; i < 10; ++i) {
    const int queue_time = base_time;
    const int start_time = base_time + kCongestionThresholdInMs + i;
    const int finish_time = start_time + 1;
    AddEventUI(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// UI thread has 3 execution congestion events on slices 1, 2, 3
// IO thread has 3 execution congestion events on slices 3, 4, 5,
// There should be a total of 5 congestion events.
TEST_F(ResponsivenessCalculatorTest,
       OverlappingExecutionCongestionMultipleThreads) {
  int base_time = 105;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kCongestionThresholdInMs;
    const int start_time = queue_time;
    const int finish_time = start_time + kCongestionThresholdInMs + 10;
    AddEventUI(queue_time, start_time, finish_time);
  }

  base_time = 305;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kCongestionThresholdInMs;
    const int start_time = queue_time;
    const int finish_time = start_time + kCongestionThresholdInMs + 10;
    AddEventIO(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_CONGESTED_SLICES(5u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(5u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// UI thread has 3 queue congestion events on slices 1, 2, 3
// IO thread has 3 queue congestion events on slices 3, 4, 5,
// There should be a total of 5 congestion events.
TEST_F(ResponsivenessCalculatorTest,
       OverlappingQueueCongestionMultipleThreads) {
  int base_time = 105;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kCongestionThresholdInMs;
    const int start_time = queue_time + kCongestionThresholdInMs + 10;
    const int finish_time = start_time;
    AddEventUI(queue_time, start_time, finish_time);
  }

  base_time = 305;
  for (int i = 0; i < 3; ++i) {
    const int queue_time = base_time + i * kCongestionThresholdInMs;
    const int start_time = queue_time + kCongestionThresholdInMs + 10;
    const int finish_time = start_time;
    AddEventIO(queue_time, start_time, finish_time);
  }

  EXPECT_EXECUTION_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(5u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// Three execution congestions, each of length 2, separated by some shorter
// events.
TEST_F(ResponsivenessCalculatorTest, SeparatedExecutionCongestions) {
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
      const int finish_time = base_time + 2 * kCongestionThresholdInMs + 1;
      AddEventUI(queue_time, start_time, finish_time);
    }
    base_time += 10 * kCongestionThresholdInMs;
  }

  EXPECT_EXECUTION_CONGESTED_SLICES(6u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(6u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

// Three queue congestions, each of length 2, separated by some shorter events.
TEST_F(ResponsivenessCalculatorTest, SeparatedQueueCongestions) {
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
      const int start_time = base_time + 2 * kCongestionThresholdInMs + 1;
      const int finish_time = start_time;
      AddEventUI(queue_time, start_time, finish_time);
    }
    base_time += 10 * kCongestionThresholdInMs;
  }

  EXPECT_EXECUTION_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(6u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

TEST_F(ResponsivenessCalculatorTest, MultipleTrigger) {
  int base_time = 105;

  // 3 Congestions, then trigger, then repeat.
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 3; ++j) {
      AddEventUI(base_time, base_time,
                 base_time + 3 * kCongestionThresholdInMs + 1);
      base_time += 3 * kCongestionThresholdInMs;
    }

    EXPECT_EXECUTION_CONGESTED_SLICES(
        9u, i == 0 ? StartupStage::kFirstInterval
                   : StartupStage::kFirstIntervalDoneWithoutFirstIdle);
    EXPECT_CONGESTED_SLICES(
        9u, i == 0 ? StartupStage::kFirstInterval
                   : StartupStage::kFirstIntervalDoneWithoutFirstIdle);
    TriggerCalculation();
    testing::Mock::VerifyAndClear(calculator_.get());
  }
}

// A long delay means that the machine likely went to sleep.
TEST_F(ResponsivenessCalculatorTest, LongDelay) {
  int base_time = 105;
  AddEventUI(base_time, base_time,
             base_time + 3 * kCongestionThresholdInMs + 1);
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
  constexpr int kFinishTime = kStartTime + kCongestionThresholdInMs + 5;
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
  constexpr int kStartTime = kQueueTime + 10 * kCongestionThresholdInMs + 5;
  constexpr int kFinishTime = kStartTime + 5;

  std::optional<base::HistogramTester> histograms;

  // Queue congestion event during the first kMeasurementInterval.
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(CongestionType::kExecutionOnly, 0,
                                     StartupStage::kFirstInterval));
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(CongestionType::kQueueAndExecution, 10u,
                                     StartupStage::kFirstInterval));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.RunningOnly", 0,
                                 1);
  histograms->ExpectUniqueSample(
      "Browser.MainThreadsCongestion.RunningOnly.Initial", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Periodic", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Initial", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Periodic", 0);

  // Queue congestion event during a few kMeasurementInterval (without having
  // seen OnFirstIdle()). Neither .Initial nor .Periodic
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(CongestionType::kExecutionOnly, 0,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(CongestionType::kQueueAndExecution, 10u,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.RunningOnly", 0,
                                 1);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Periodic", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Initial", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Periodic", 0);
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(CongestionType::kExecutionOnly, 0,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(CongestionType::kQueueAndExecution, 10u,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.RunningOnly", 0,
                                 1);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Periodic", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Initial", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Periodic", 0);

  // OnFirstIdle() eventually during a kMeasurementInterval. Same as above, last
  // one of these.
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  calculator_->OnFirstIdle();
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(CongestionType::kExecutionOnly, 0,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  EXPECT_CALL(
      *calculator_,
      EmitResponsivenessMock(CongestionType::kQueueAndExecution, 10u,
                             StartupStage::kFirstIntervalDoneWithoutFirstIdle));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.RunningOnly", 0,
                                 1);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Periodic", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Initial", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Periodic", 0);

  // Events in intervals after OnFirstIdle(). congested3.Initial still no
  // .Periodic.
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(
                                CongestionType::kExecutionOnly, 0,
                                StartupStage::kFirstIntervalAfterFirstIdle));
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(
                                CongestionType::kQueueAndExecution, 10u,
                                StartupStage::kFirstIntervalAfterFirstIdle));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.RunningOnly", 0,
                                 1);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Periodic", 0);
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion", 10, 1);
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.Initial", 10,
                                 1);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Periodic", 0);

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(CongestionType::kExecutionOnly, 0,
                                     StartupStage::kPeriodic));
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(CongestionType::kQueueAndExecution, 10u,
                                     StartupStage::kPeriodic));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.RunningOnly", 0,
                                 1);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Initial", 0);
  histograms->ExpectUniqueSample(
      "Browser.MainThreadsCongestion.RunningOnly.Periodic", 0, 1);
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion", 10, 1);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Initial", 0);
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.Periodic", 10,
                                 1);
}

TEST_F(ResponsivenessCalculatorTest, FastStartupStages) {
  constexpr int kQueueTime = 35;
  constexpr int kStartTime = kQueueTime + 10 * kCongestionThresholdInMs + 5;
  constexpr int kFinishTime = kStartTime + 5;

  std::optional<base::HistogramTester> histograms;

  // OnFirstIdle() right away during the first kMeasurementInterval. Still
  // considered as kFirstInterval, but second interval will go straight
  // to kFirstIntervalAfterFirstIdle.
  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  calculator_->OnFirstIdle();
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(CongestionType::kExecutionOnly, 0,
                                     StartupStage::kFirstInterval));
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(CongestionType::kQueueAndExecution, 10u,
                                     StartupStage::kFirstInterval));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.RunningOnly", 0,
                                 1);
  histograms->ExpectUniqueSample(
      "Browser.MainThreadsCongestion.RunningOnly.Initial", 0, 1);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Periodic", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Initial", 0);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Periodic", 0);

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(
                                CongestionType::kExecutionOnly, 0,
                                StartupStage::kFirstIntervalAfterFirstIdle));
  EXPECT_CALL(*calculator_, EmitResponsivenessMock(
                                CongestionType::kQueueAndExecution, 10u,
                                StartupStage::kFirstIntervalAfterFirstIdle));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.RunningOnly", 0,
                                 1);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Initial", 0);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Periodic", 0);
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion", 10, 1);
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.Initial", 10,
                                 1);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Periodic", 0);

  AddEventUI(kQueueTime, kStartTime, kFinishTime);
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(CongestionType::kExecutionOnly, 0,
                                     StartupStage::kPeriodic));
  EXPECT_CALL(*calculator_,
              EmitResponsivenessMock(CongestionType::kQueueAndExecution, 10u,
                                     StartupStage::kPeriodic));
  histograms.emplace();
  TriggerCalculation();
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.RunningOnly", 0,
                                 1);
  histograms->ExpectTotalCount(
      "Browser.MainThreadsCongestion.RunningOnly.Initial", 0);
  histograms->ExpectUniqueSample(
      "Browser.MainThreadsCongestion.RunningOnly.Periodic", 0, 1);
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion", 10, 1);
  histograms->ExpectTotalCount("Browser.MainThreadsCongestion.Initial", 0);
  histograms->ExpectUniqueSample("Browser.MainThreadsCongestion.Periodic", 10,
                                 1);
}

// An event execution that crosses a measurement interval boundary should count
// towards both measurement intervals.
TEST_F(ResponsivenessCalculatorTest, ExecutionCrossesBoundary) {
  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 0.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // The event goes from [29801, 30150]. It should count as 1 congestion in the
  // first measurement interval and 2 in the second.
  {
    EXPECT_EXECUTION_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
    EXPECT_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
    const int queue_time =
        kMeasurementIntervalInMs - 2 * kCongestionThresholdInMs + 1;
    const int start_time = queue_time;
    const int finish_time =
        kMeasurementIntervalInMs + 1.5 * kCongestionThresholdInMs;
    AddEventUI(queue_time, start_time, finish_time);
  }

  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 1.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // Trigger another calculation.
  EXPECT_EXECUTION_CONGESTED_SLICES(
      2u, StartupStage::kFirstIntervalDoneWithoutFirstIdle);
  EXPECT_CONGESTED_SLICES(2u, StartupStage::kFirstIntervalDoneWithoutFirstIdle);

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

  // The event goes from [29801, 30150]. It should count as 1 congestion in the
  // first measurement interval and 2 in the second.
  {
    EXPECT_EXECUTION_CONGESTED_SLICES(0u, StartupStage::kFirstInterval);
    EXPECT_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
    const int queue_time =
        kMeasurementIntervalInMs - 2 * kCongestionThresholdInMs + 1;
    const int start_time =
        kMeasurementIntervalInMs + 1.5 * kCongestionThresholdInMs;
    const int finish_time = start_time;
    AddEventUI(queue_time, start_time, finish_time);
  }

  // Dummy event so that Calculator doesn't think the process is suspended.
  {
    const int kTime = 1.5 * kMeasurementIntervalInMs;
    AddEventUI(kTime, kTime, kTime);
  }

  // Trigger another calculation.
  EXPECT_EXECUTION_CONGESTED_SLICES(
      0u, StartupStage::kFirstIntervalDoneWithoutFirstIdle);
  EXPECT_CONGESTED_SLICES(2u, StartupStage::kFirstIntervalDoneWithoutFirstIdle);

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
  // The execution congestion in A subsumes all other execution congestions. The
  // queue congestion in B subsumes all other queue congestions.
  AddEventUI(100, 100, 250);
  AddEventUI(150, 150, 300);
  AddEventUI(50, 50, 200);
  AddEventUI(50, 50, 390);

  AddEventUI(1100, 1250, 1251);
  AddEventUI(1150, 1300, 1301);
  AddEventUI(1050, 1200, 1201);
  AddEventUI(1050, 1390, 1391);

  EXPECT_EXECUTION_CONGESTED_SLICES(3u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(6u, StartupStage::kFirstInterval);
  TriggerCalculation();
}

TEST_F(ResponsivenessCalculatorTest, EmitResponsivenessTraceEventsEmpty) {
  constexpr base::TimeTicks kStartTime = base::TimeTicks();
  constexpr base::TimeTicks kFinishTime =
      kStartTime + base::Milliseconds(kMeasurementIntervalInMs);
  const std::set<int> congested_slices;

  EXPECT_CALL(*calculator_,
              EmitCongestedIntervalsMeasurementTraceEvent(_, _, _))
      .Times(0);

  calculator_->EmitResponsivenessTraceEvents(CongestionType::kQueueAndExecution,
                                             kStartTime, kFinishTime,
                                             congested_slices);
}

TEST_F(ResponsivenessCalculatorTest, EmitResponsivenessTraceEventsWrongMetric) {
  constexpr base::TimeTicks kStartTime = base::TimeTicks();
  constexpr base::TimeTicks kFinishTime =
      kStartTime + base::Milliseconds(kMeasurementIntervalInMs);
  const std::set<int> congested_slices = {1};

  EXPECT_CALL(*calculator_,
              EmitCongestedIntervalsMeasurementTraceEvent(_, _, _))
      .Times(0);

  calculator_->EmitResponsivenessTraceEvents(CongestionType::kExecutionOnly,
                                             kStartTime, kFinishTime,
                                             congested_slices);
}

TEST_F(ResponsivenessCalculatorTest, EmitResponsivenessTraceEvents) {
  constexpr base::TimeDelta kSliceInterval =
      base::Milliseconds(kCongestionThresholdInMs);
  constexpr base::TimeTicks kStartTime = base::TimeTicks();
  constexpr base::TimeTicks kFinishTime =
      kStartTime + base::Milliseconds(kMeasurementIntervalInMs);

  const std::set<int> congested_slices = {3, 4, 5, 12, 15};

  EXPECT_CALL(*calculator_,
              EmitCongestedIntervalsMeasurementTraceEvent(
                  kStartTime, kFinishTime, congested_slices.size()));

  EXPECT_CALL(*calculator_,
              EmitCongestedIntervalTraceEvent(kStartTime + 3 * kSliceInterval,
                                              kStartTime + 6 * kSliceInterval));
  EXPECT_CALL(*calculator_, EmitCongestedIntervalTraceEvent(
                                kStartTime + 12 * kSliceInterval,
                                kStartTime + 13 * kSliceInterval));
  EXPECT_CALL(*calculator_, EmitCongestedIntervalTraceEvent(
                                kStartTime + 15 * kSliceInterval,
                                kStartTime + 16 * kSliceInterval));

  calculator_->EmitResponsivenessTraceEvents(CongestionType::kQueueAndExecution,
                                             kStartTime, kFinishTime,
                                             congested_slices);
}

TEST_F(ResponsivenessCalculatorTest, Delegate) {
  calculator_->OnFirstIdle();

  // To have a valid interval, there needs to be at least 30 seconds that
  // passed, during which there was at least one event.
  int interval_start = 0;
  int interval_mid = 15000;
  int interval_end = kMeasurementIntervalInMs + 1000;

  EXPECT_EXECUTION_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);
  EXPECT_CONGESTED_SLICES(1u, StartupStage::kFirstInterval);

  // OnResponsivenessEmitted() is not invoked for the first interval.
  EXPECT_CALL(*delegate_, OnMeasurementIntervalEnded());
  EXPECT_CALL(*delegate_, OnResponsivenessEmitted(_, _, _, _)).Times(0);

  AddEventUI(interval_start, interval_start, interval_start);
  AddEventUI(interval_mid, interval_mid,
             interval_mid + kCongestionThresholdInMs + 20);
  AddEventUI(interval_end, interval_end, interval_end);

  // Repeat the same interval immediately following the first one.
  interval_start += interval_end;
  interval_mid += interval_end;
  interval_end += interval_end;

  EXPECT_EXECUTION_CONGESTED_SLICES(1u,
                                    StartupStage::kFirstIntervalAfterFirstIdle);
  EXPECT_CONGESTED_SLICES(1u, StartupStage::kFirstIntervalAfterFirstIdle);

  // OnResponsivenessEmitted() is invoked for subsequent intervals.
  {
    testing::InSequence in_sequence;
    EXPECT_CALL(*delegate_, OnMeasurementIntervalEnded());
    EXPECT_CALL(*delegate_, OnResponsivenessEmitted(_, _, _, _));
  }

  AddEventUI(interval_start, interval_start, interval_start);
  AddEventUI(interval_mid, interval_mid,
             interval_mid + kCongestionThresholdInMs + 20);
  AddEventUI(interval_end, interval_end, interval_end);
}

}  // namespace responsiveness
}  // namespace content
