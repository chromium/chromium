// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe_manager.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/system_cpu/pressure_test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using system_cpu::CpuSample;
using system_cpu::FakeCpuProbe;
using system_cpu::StreamingCpuProbe;

// Different amounts of delay to insert to test interactions between responses
// to subsequent RequestSample() calls.
enum class ResponseDelay {
  // Responses will be posted immediately. With MOCK_TIME, they'll arrive on the
  // same time tick as the RequestSample() call.
  kNone,
  // Responses will be delayed but arrive before the next RequestSample() call.
  kLessThanSampleInterval,
  // Responses will arrive after the next RequestSample() call.
  kGreaterThanSampleInterval,
  // Responses will arrive on the same time tick as the next RequestSample()
  // call.
  kEqualToSampleInterval,
};

base::TimeDelta GetResponseDelayDelta(ResponseDelay delay) {
  switch (delay) {
    case ResponseDelay::kNone:
      return base::TimeDelta();
    case ResponseDelay::kLessThanSampleInterval:
      return TestTimeouts::tiny_timeout() / 2;
    case ResponseDelay::kGreaterThanSampleInterval:
      return TestTimeouts::tiny_timeout() * 2;
    case ResponseDelay::kEqualToSampleInterval:
      return TestTimeouts::tiny_timeout();
  }
}

}  // namespace

class CpuProbeManagerTest : public ::testing::TestWithParam<ResponseDelay> {
 public:
  CpuProbeManagerTest()
      : cpu_probe_manager_(CpuProbeManager::CreateForTesting(
            std::make_unique<FakeCpuProbe>(GetResponseDelayDelta(GetParam())),
            TestTimeouts::tiny_timeout(),
            base::BindRepeating(&CpuProbeManagerTest::CollectorCallback,
                                base::Unretained(this)))) {}

  void WaitForUpdate() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    SetNextUpdateCallback(task_environment_.QuitClosure());
    task_environment_.RunUntilQuit();
  }

  void CollectorCallback(mojom::PressureState sample) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    samples_.push_back(sample);
    if (update_callback_) {
      std::move(update_callback_).Run();
      update_callback_.Reset();
    }
  }

  void SetProbeSample(std::optional<CpuSample> cpu_sample) {
    static_cast<FakeCpuProbe*>(cpu_probe_manager_->cpu_probe())
        ->SetLastSample(cpu_sample);
  }

  void ClearUpdateCallback() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    update_callback_.Reset();
  }

 protected:
  SEQUENCE_CHECKER(sequence_checker_);

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // This member is a std::unique_ptr instead of a plain CpuProbeManager
  // so it can be replaced inside tests.
  std::unique_ptr<CpuProbeManager> cpu_probe_manager_;

  // The samples reported by the callback.
  std::vector<mojom::PressureState> samples_
      GUARDED_BY_CONTEXT(sequence_checker_);

 private:
  void SetNextUpdateCallback(base::OnceClosure callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!update_callback_)
        << __func__ << " already called before update received";
    update_callback_ = std::move(callback);
  }

  // Used to implement WaitForUpdate().
  base::OnceClosure update_callback_ GUARDED_BY_CONTEXT(sequence_checker_);
};

using CpuProbeManagerDeathTest = CpuProbeManagerTest;
using CpuProbeManagerDelayedResponseTest = CpuProbeManagerTest;

// Most tests won't include a response delay.
INSTANTIATE_TEST_SUITE_P(NoResponseDelay,
                         CpuProbeManagerTest,
                         ::testing::Values(ResponseDelay::kNone));

INSTANTIATE_TEST_SUITE_P(NoResponseDelay,
                         CpuProbeManagerDeathTest,
                         ::testing::Values(ResponseDelay::kNone));

INSTANTIATE_TEST_SUITE_P(
    AllResponseDelays,
    CpuProbeManagerDelayedResponseTest,
    ::testing::Values(ResponseDelay::kNone,
                      ResponseDelay::kLessThanSampleInterval,
                      ResponseDelay::kGreaterThanSampleInterval,
                      ResponseDelay::kEqualToSampleInterval));

TEST_P(CpuProbeManagerTest, CreateCpuProbeExists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<CpuProbeManager> cpu_probe_manager =
      CpuProbeManager::Create(TestTimeouts::tiny_timeout(), base::DoNothing());
  if (cpu_probe_manager) {
    EXPECT_TRUE(!!cpu_probe_manager->cpu_probe());
  }
}

TEST_P(CpuProbeManagerTest, EnsureStarted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetProbeSample(CpuSample{0.9});
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();

  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_P(CpuProbeManagerTest, InvalidSampleIsIgnored) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We need to verify that CollectorCallback() was _not_ called, we
  // basically let things run and check that samples_ is empty.
  {
    base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                             TestTimeouts::action_timeout());
    SetProbeSample(std::nullopt);
    cpu_probe_manager_->EnsureStarted();
    EXPECT_NONFATAL_FAILURE({ WaitForUpdate(); }, "timed out");
    EXPECT_TRUE(samples_.empty());
    ClearUpdateCallback();
  }

  SetProbeSample(CpuSample{0.9});
  WaitForUpdate();
  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_P(CpuProbeManagerTest, EnsureStartedSkipsFirstSample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<CpuSample> samples = {
      // Value right after construction.
      CpuSample{0.6},
      // Value after first Update(), should be discarded.
      CpuSample{0.9},
      // Value after second Update(), should be reported.
      CpuSample{0.65},
  };

  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), task_environment_.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  task_environment_.RunUntilQuit();

  EXPECT_THAT(samples_, ::testing::ElementsAre(
                            mojom::PressureState{mojom::PressureState::kFair}));
}

TEST_P(CpuProbeManagerDeathTest, CalculateStateValueTooLarge) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EXPECT_DCHECK_DEATH_WITH(cpu_probe_manager_->CalculateState(CpuSample{1.1}),
                           "unexpected value: 1.1");
}

TEST_P(CpuProbeManagerTest, EnsureStartedCheckBreakCalibrationMitigation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetProbeSample(CpuSample{0.86});
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_.back(),
              mojom::PressureState(mojom::PressureState::kSerious));

  cpu_probe_manager_->Stop();
  samples_.clear();

  SetProbeSample(CpuSample{0.86});
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  // First toggling.
  task_environment_.FastForwardBy(
      cpu_probe_manager_->GetRandomizationTimeForTesting());
  EXPECT_THAT(samples_.back(),
              mojom::PressureState(mojom::PressureState::kCritical));
  // Second toggling.
  task_environment_.FastForwardBy(
      cpu_probe_manager_->GetRandomizationTimeForTesting());
  EXPECT_THAT(samples_.back(),
              mojom::PressureState(mojom::PressureState::kSerious));
}

TEST_P(CpuProbeManagerTest, EnsureStartedCheckCalculateStateHysteresisUp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<CpuSample> samples = {
      // Value right after construction.
      CpuSample{0.6},
      // Value after first Update(), should be discarded.
      CpuSample{0.9},
      // kNominal value after should be reported.
      CpuSample{0.3},
      // kFair value should be reported.
      CpuSample{0.7},
      // kSerious value should be reported.
      CpuSample{0.8},
      // kCritical value should be reported.
      CpuSample{1.0},
  };

  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), task_environment_.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  task_environment_.RunUntilQuit();

  EXPECT_THAT(samples_,
              ::testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kNominal},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kSerious},
                  mojom::PressureState{mojom::PressureState::kCritical}));
}

TEST_P(CpuProbeManagerTest, EnsureStartedCheckCalculateStateHysteresisDown) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<CpuSample> samples = {
      // Value right after construction.
      CpuSample{1.0},
      // Value after first Update(), should be discarded.
      CpuSample{0.85},
      // kCritical value after should be reported.
      CpuSample{1.0},
      // kSerious value should be reported.
      CpuSample{0.85},
      // kFair value should be reported.
      CpuSample{0.70},
      // kNominal value should be reported.
      CpuSample{0.55},
  };

  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), task_environment_.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  task_environment_.RunUntilQuit();

  EXPECT_THAT(samples_,
              ::testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kSerious},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kNominal}));
}

TEST_P(CpuProbeManagerTest,
       EnsureStartedCheckCalculateStateHysteresisDownByDelta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<CpuSample> samples = {
      // Value right after construction.
      CpuSample{1.0},
      // Value after first Update(), should be discarded.
      CpuSample{1.0},
      // kCritical value after should be reported.
      CpuSample{0.95},
      // kCritical value should be reported due to hysteresis.
      CpuSample{0.88},
      // kFair value should be reported.
      CpuSample{0.73},
      // kNominal value should be reported.
      CpuSample{0.56},
  };

  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), task_environment_.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  task_environment_.RunUntilQuit();

  EXPECT_THAT(samples_,
              ::testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kNominal}));
}

TEST_P(CpuProbeManagerTest,
       EnsureStartedCheckCalculateStateHysteresisDownByDeltaTwoState) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<CpuSample> samples = {
      // Value right after construction.
      CpuSample{1.0},
      // Value after first Update(), should be discarded.
      CpuSample{1.0},
      // kCritical value after should be reported.
      CpuSample{0.95},
      // kFair value should be reported.
      CpuSample{0.73},
      // kFair value should be reported due to hysteresis.
      CpuSample{0.58},
  };

  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), task_environment_.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  task_environment_.RunUntilQuit();

  EXPECT_THAT(samples_,
              ::testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kFair}));
}

TEST_P(CpuProbeManagerTest,
       EnsureStartedCheckCalculateStateHysteresisUpByDelta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<CpuSample> samples = {
      // Value right after construction.
      CpuSample{1.0},
      // Value after first Update(), should be discarded.
      CpuSample{1.0},
      // kNominal value after should be reported.
      CpuSample{0.6},
      // kFair value should be reported due to hysteresis.
      CpuSample{0.62},
      // kSerious value should be reported.
      CpuSample{0.77},
      // kCritical value should be reported.
      CpuSample{0.91},
  };

  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), task_environment_.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  task_environment_.RunUntilQuit();

  EXPECT_THAT(samples_,
              ::testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kNominal},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kSerious},
                  mojom::PressureState{mojom::PressureState::kCritical}));
}

TEST_P(CpuProbeManagerDelayedResponseTest, StopDelayedEnsureStartedImmediate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetProbeSample(CpuSample{0.1});
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  cpu_probe_manager_->Stop();

  samples_.clear();
  SetProbeSample(CpuSample{0.9});

  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_P(CpuProbeManagerDelayedResponseTest, StopDelayedEnsureStartedDelayed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetProbeSample(CpuSample{0.1});
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  cpu_probe_manager_->Stop();
  samples_.clear();
  SetProbeSample(CpuSample{0.9});

  task_environment_.FastForwardBy(TestTimeouts::action_timeout());
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_P(CpuProbeManagerDelayedResponseTest,
       StopImmediateEnsureStartedImmediate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetProbeSample(CpuSample{0.1});
  cpu_probe_manager_->EnsureStarted();
  cpu_probe_manager_->Stop();

  samples_.clear();
  SetProbeSample(CpuSample{0.9});

  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_P(CpuProbeManagerDelayedResponseTest, StopImmediateEnsureStartedDelayed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetProbeSample(CpuSample{0.1});
  cpu_probe_manager_->EnsureStarted();
  cpu_probe_manager_->Stop();

  samples_.clear();
  SetProbeSample(CpuSample{0.9});

  task_environment_.FastForwardBy(TestTimeouts::action_timeout());
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_P(CpuProbeManagerDelayedResponseTest, StopEnsureStartedNoRace) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Can't simulate the race if there's no delay in sending a response.
  if (GetParam() == ResponseDelay::kNone) {
    GTEST_SKIP();
  }

  SetProbeSample(CpuSample{0.9});

  cpu_probe_manager_->EnsureStarted();

  // This should send a sample request. Stop and restart the manager before the
  // response is received, to be sure it's correctly ignored.
  task_environment_.FastForwardBy(TestTimeouts::tiny_timeout());

  cpu_probe_manager_->Stop();
  EXPECT_THAT(samples_, ::testing::IsEmpty());
  SetProbeSample(CpuSample{0.65});
  cpu_probe_manager_->EnsureStarted();

  WaitForUpdate();

  // The 0.9 sample was sent before Stop(), so it should NOT be included in the
  // pressure calculation.
  EXPECT_THAT(samples_, ::testing::ElementsAre(
                            mojom::PressureState(mojom::PressureState::kFair)));
}

}  // namespace device
