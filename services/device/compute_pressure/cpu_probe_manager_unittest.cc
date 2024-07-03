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
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "components/system_cpu/pressure_test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using system_cpu::CpuSample;
using system_cpu::FakeCpuProbe;
using system_cpu::StreamingCpuProbe;

class CpuProbeManagerTest : public ::testing::Test {
 public:
  CpuProbeManagerTest()
      : cpu_probe_manager_(CpuProbeManager::CreateForTesting(
            std::make_unique<FakeCpuProbe>(),
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

TEST_F(CpuProbeManagerTest, CreateCpuProbeExists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<CpuProbeManager> cpu_probe_manager =
      CpuProbeManager::Create(TestTimeouts::tiny_timeout(), base::DoNothing());
  if (cpu_probe_manager) {
    EXPECT_TRUE(!!cpu_probe_manager->GetCpuProbeForTesting());
  }
}

TEST_F(CpuProbeManagerTest, EnsureStarted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static_cast<FakeCpuProbe*>(cpu_probe_manager_->GetCpuProbeForTesting())
      ->SetLastSample(std::make_optional(CpuSample{0.9}));
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();

  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_F(CpuProbeManagerTest, EnsureStartedSkipsFirstSample) {
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

TEST_F(CpuProbeManagerDeathTest, CalculateStateValueTooLarge) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EXPECT_DCHECK_DEATH_WITH(cpu_probe_manager_->CalculateState(CpuSample{1.1}),
                           "unexpected value: 1.1");
}

TEST_F(CpuProbeManagerTest, EnsureStartedCheckBreakCalibrationMitigation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static_cast<FakeCpuProbe*>(cpu_probe_manager_->GetCpuProbeForTesting())
      ->SetLastSample(CpuSample{0.86});
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_.back(),
              mojom::PressureState(mojom::PressureState::kSerious));

  cpu_probe_manager_->Stop();
  samples_.clear();

  static_cast<FakeCpuProbe*>(cpu_probe_manager_->GetCpuProbeForTesting())
      ->SetLastSample(CpuSample{0.86});
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

TEST_F(CpuProbeManagerTest, EnsureStartedCheckCalculateStateHysteresisUp) {
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

TEST_F(CpuProbeManagerTest, EnsureStartedCheckCalculateStateHysteresisDown) {
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

TEST_F(CpuProbeManagerTest,
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

TEST_F(CpuProbeManagerTest,
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

TEST_F(CpuProbeManagerTest,
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

TEST_F(CpuProbeManagerTest, StopDelayedEnsureStartedImmediate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  cpu_probe_manager_->Stop();

  samples_.clear();
  static_cast<FakeCpuProbe*>(cpu_probe_manager_->GetCpuProbeForTesting())
      ->SetLastSample(CpuSample{0.9});

  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_F(CpuProbeManagerTest, StopDelayedEnsureStartedDelayed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  cpu_probe_manager_->Stop();
  samples_.clear();
  static_cast<FakeCpuProbe*>(cpu_probe_manager_->GetCpuProbeForTesting())
      ->SetLastSample(CpuSample{0.9});

  task_environment_.FastForwardBy(TestTimeouts::action_timeout());
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_F(CpuProbeManagerTest, StopImmediateEnsureStartedImmediate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_manager_->EnsureStarted();
  cpu_probe_manager_->Stop();

  samples_.clear();
  static_cast<FakeCpuProbe*>(cpu_probe_manager_->GetCpuProbeForTesting())
      ->SetLastSample(CpuSample{0.9});

  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_F(CpuProbeManagerTest, StopImmediateEnsureStartedDelayed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_manager_->EnsureStarted();
  cpu_probe_manager_->Stop();

  samples_.clear();
  static_cast<FakeCpuProbe*>(cpu_probe_manager_->GetCpuProbeForTesting())
      ->SetLastSample(CpuSample{0.9});

  task_environment_.FastForwardBy(TestTimeouts::action_timeout());
  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, ::testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

}  // namespace device
