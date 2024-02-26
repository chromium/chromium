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
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "components/system_cpu/pressure_test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using system_cpu::CpuSample;
using system_cpu::FakeCpuProbe;
using system_cpu::StreamingCpuProbe;

class CpuProbeManagerTest : public testing::Test {
 public:
  // Constructs CpuProbeManagerTest with |traits| being forwarded to its
  // TaskEnvironment.
  template <typename... TaskEnvironmentTraits>
  NOINLINE explicit CpuProbeManagerTest(TaskEnvironmentTraits&&... traits)
      : CpuProbeManagerTest(std::make_unique<base::test::TaskEnvironment>(
            std::forward<TaskEnvironmentTraits>(traits)...)) {}

  explicit CpuProbeManagerTest(
      std::unique_ptr<base::test::TaskEnvironment> task_environment)
      : task_environment_(std::move(task_environment)),
        cpu_probe_manager_(CpuProbeManager::CreateForTesting(
            std::make_unique<FakeCpuProbe>(),
            base::Milliseconds(1),
            base::BindRepeating(&CpuProbeManagerTest::CollectorCallback,
                                base::Unretained(this)))) {}

  void WaitForUpdate() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::RunLoop run_loop;
    SetNextUpdateCallback(run_loop.QuitClosure());
    run_loop.Run();
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

  std::unique_ptr<base::test::TaskEnvironment> task_environment_;

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

class CpuProbeManagerWithMockTimeTest : public CpuProbeManagerTest {
 public:
  CpuProbeManagerWithMockTimeTest()
      : CpuProbeManagerTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

TEST_F(CpuProbeManagerTest, CreateCpuProbeExists) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<CpuProbeManager> cpu_probe_manager =
      CpuProbeManager::Create(base::Milliseconds(1), base::DoNothing());
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

  EXPECT_THAT(samples_, testing::ElementsAre(mojom::PressureState(
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
      CpuSample{0.4},
  };

  base::RunLoop run_loop;
  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), run_loop.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_, testing::ElementsAre(
                            mojom::PressureState{mojom::PressureState::kFair}));
}

TEST_F(CpuProbeManagerTest, CalculateStateValueTooLarge) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  EXPECT_DCHECK_DEATH_WITH(cpu_probe_manager_->CalculateState(CpuSample{1.1}),
                           "unexpected value: 1.1");
}

TEST_F(CpuProbeManagerWithMockTimeTest,
       EnsureStartedCheckBreakCalibrationMitigation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // cpu_probe_manager_ is redefined with larger sampling time in seconds.
  // We noticed that cpu_probe_manager_, with fast sampling (ms), is slowing
  // down testing when using FastForwardBy(), especially on tsan and asan test
  // releases.
  cpu_probe_manager_ = CpuProbeManager::CreateForTesting(
      std::make_unique<FakeCpuProbe>(), base::Seconds(1),
      base::BindRepeating(&CpuProbeManagerTest::CollectorCallback,
                          base::Unretained(this)));

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
  task_environment_->FastForwardBy(
      cpu_probe_manager_->GetRandomizationTimeForTesting());
  EXPECT_THAT(samples_.back(),
              mojom::PressureState(mojom::PressureState::kCritical));
  // Second toggling.
  task_environment_->FastForwardBy(
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
      CpuSample{0.6},
      // kSerious value should be reported.
      CpuSample{0.9},
      // kCritical value should be reported.
      CpuSample{1.0},
  };

  base::RunLoop run_loop;
  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), run_loop.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_,
              testing::ElementsAre(
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
      CpuSample{0.55},
      // kNominal value should be reported.
      CpuSample{0.25},
  };

  base::RunLoop run_loop;
  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), run_loop.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_,
              testing::ElementsAre(
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
      CpuSample{0.58},
      // kNominal value should be reported.
      CpuSample{0.26},
  };

  base::RunLoop run_loop;
  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), run_loop.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_,
              testing::ElementsAre(
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
      CpuSample{0.58},
      // kFair value should be reported due to hysteresis.
      CpuSample{0.28},
  };

  base::RunLoop run_loop;
  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), run_loop.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_,
              testing::ElementsAre(
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
      CpuSample{0.3},
      // kFair value should be reported due to hysteresis.
      CpuSample{0.32},
      // kSerious value should be reported.
      CpuSample{0.62},
      // kCritical value should be reported.
      CpuSample{0.91},
  };

  base::RunLoop run_loop;
  cpu_probe_manager_->SetCpuProbeForTesting(std::make_unique<StreamingCpuProbe>(
      std::move(samples), run_loop.QuitClosure()));
  cpu_probe_manager_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_,
              testing::ElementsAre(
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
  EXPECT_THAT(samples_, testing::ElementsAre(mojom::PressureState(
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
  // 10ms should be long enough to ensure that all the sampling tasks are done.
  base::PlatformThread::Sleep(base::Milliseconds(10));

  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, testing::ElementsAre(mojom::PressureState(
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
  EXPECT_THAT(samples_, testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_F(CpuProbeManagerTest, StopImmediateEnsureStartedDelayed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_manager_->EnsureStarted();
  cpu_probe_manager_->Stop();

  samples_.clear();
  static_cast<FakeCpuProbe*>(cpu_probe_manager_->GetCpuProbeForTesting())
      ->SetLastSample(CpuSample{0.9});
  // 10ms should be long enough to ensure that all the sampling tasks are done.
  base::PlatformThread::Sleep(base::Milliseconds(10));

  cpu_probe_manager_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

}  // namespace device
