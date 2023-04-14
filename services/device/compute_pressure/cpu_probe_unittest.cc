// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe.h"

#include <cstddef>
#include <memory>
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
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/pressure_test_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class CpuProbeTest : public testing::Test {
 public:
  CpuProbeTest()
      : cpu_probe_(std::make_unique<FakeCpuProbe>(
            base::Milliseconds(1),
            base::BindRepeating(&CpuProbeTest::CollectorCallback,
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

  base::test::TaskEnvironment task_environment_;

  // This member is a std::unique_ptr instead of a plain CpuProbe
  // so it can be replaced inside tests.
  std::unique_ptr<CpuProbe> cpu_probe_;

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

TEST_F(CpuProbeTest, EnsureStarted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  static_cast<FakeCpuProbe*>(cpu_probe_.get())
      ->SetLastSample(PressureSample{0.9});
  cpu_probe_->EnsureStarted();
  WaitForUpdate();

  EXPECT_THAT(samples_, testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_F(CpuProbeTest, EnsureStartedSkipsFirstSample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PressureSample> samples = {
      // Value right after construction.
      PressureSample{0.6},
      // Value after first Update(), should be discarded.
      PressureSample{0.9},
      // Value after second Update(), should be reported.
      PressureSample{0.4},
  };

  base::RunLoop run_loop;
  cpu_probe_ = std::make_unique<StreamingCpuProbe>(
      base::Milliseconds(1),
      base::BindRepeating(&CpuProbeTest::CollectorCallback,
                          base::Unretained(this)),
      std::move(samples), run_loop.QuitClosure());
  cpu_probe_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_, testing::ElementsAre(
                            mojom::PressureState{mojom::PressureState::kFair}));
}

TEST_F(CpuProbeTest, EnsureStartedCheckCalculateStateWrongValue) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PressureSample> samples = {
      // Value right after construction.
      PressureSample{0.6},
      // Value after first Update(), should be discarded.
      PressureSample{0.9},
      // Crash expected.
      PressureSample{1.1},
  };

  base::RunLoop run_loop;
  cpu_probe_ = std::make_unique<StreamingCpuProbe>(
      base::Milliseconds(1),
      base::BindRepeating(&CpuProbeTest::CollectorCallback,
                          base::Unretained(this)),
      std::move(samples), run_loop.QuitClosure());
  cpu_probe_->EnsureStarted();

  EXPECT_DCHECK_DEATH_WITH(run_loop.Run(), "unexpected value: 1.1");
}

TEST_F(CpuProbeTest, EnsureStartedCheckCalculateStateHysteresisUp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PressureSample> samples = {
      // Value right after construction.
      PressureSample{0.6},
      // Value after first Update(), should be discarded.
      PressureSample{0.9},
      // kNominal value after should be reported.
      PressureSample{0.3},
      // kFair value should be reported.
      PressureSample{0.6},
      // kSerious value should be reported.
      PressureSample{0.9},
      // kCritical value should be reported.
      PressureSample{1.0},
  };

  base::RunLoop run_loop;
  cpu_probe_ = std::make_unique<StreamingCpuProbe>(
      base::Milliseconds(1),
      base::BindRepeating(&CpuProbeTest::CollectorCallback,
                          base::Unretained(this)),
      std::move(samples), run_loop.QuitClosure());
  cpu_probe_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_,
              testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kNominal},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kSerious},
                  mojom::PressureState{mojom::PressureState::kCritical}));
}

TEST_F(CpuProbeTest, EnsureStartedCheckCalculateStateHysteresisDown) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PressureSample> samples = {
      // Value right after construction.
      PressureSample{1.0},
      // Value after first Update(), should be discarded.
      PressureSample{0.85},
      // kCritical value after should be reported.
      PressureSample{1.0},
      // kSerious value should be reported.
      PressureSample{0.85},
      // kFair value should be reported.
      PressureSample{0.55},
      // kNominal value should be reported.
      PressureSample{0.25},
  };

  base::RunLoop run_loop;
  cpu_probe_ = std::make_unique<StreamingCpuProbe>(
      base::Milliseconds(1),
      base::BindRepeating(&CpuProbeTest::CollectorCallback,
                          base::Unretained(this)),
      std::move(samples), run_loop.QuitClosure());
  cpu_probe_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_,
              testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kSerious},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kNominal}));
}

TEST_F(CpuProbeTest, EnsureStartedCheckCalculateStateHysteresisDownByDelta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PressureSample> samples = {
      // Value right after construction.
      PressureSample{1.0},
      // Value after first Update(), should be discarded.
      PressureSample{1.0},
      // kCritical value after should be reported.
      PressureSample{0.95},
      // kCritical value should be reported due to hysteresis.
      PressureSample{0.88},
      // kFair value should be reported.
      PressureSample{0.58},
      // kNominal value should be reported.
      PressureSample{0.26},
  };

  base::RunLoop run_loop;
  cpu_probe_ = std::make_unique<StreamingCpuProbe>(
      base::Milliseconds(1),
      base::BindRepeating(&CpuProbeTest::CollectorCallback,
                          base::Unretained(this)),
      std::move(samples), run_loop.QuitClosure());
  cpu_probe_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_,
              testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kNominal}));
}

TEST_F(CpuProbeTest,
       EnsureStartedCheckCalculateStateHysteresisDownByDeltaTwoState) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PressureSample> samples = {
      // Value right after construction.
      PressureSample{1.0},
      // Value after first Update(), should be discarded.
      PressureSample{1.0},
      // kCritical value after should be reported.
      PressureSample{0.95},
      // kFair value should be reported.
      PressureSample{0.58},
      // kFair value should be reported due to hysteresis.
      PressureSample{0.28},
  };

  base::RunLoop run_loop;
  cpu_probe_ = std::make_unique<StreamingCpuProbe>(
      base::Milliseconds(1),
      base::BindRepeating(&CpuProbeTest::CollectorCallback,
                          base::Unretained(this)),
      std::move(samples), run_loop.QuitClosure());
  cpu_probe_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_,
              testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kCritical},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kFair}));
}

TEST_F(CpuProbeTest, EnsureStartedCheckCalculateStateHysteresisUpByDelta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<PressureSample> samples = {
      // Value right after construction.
      PressureSample{1.0},
      // Value after first Update(), should be discarded.
      PressureSample{1.0},
      // kNominal value after should be reported.
      PressureSample{0.3},
      // kFair value should be reported due to hysteresis.
      PressureSample{0.32},
      // kSerious value should be reported.
      PressureSample{0.62},
      // kCritical value should be reported.
      PressureSample{0.91},
  };

  base::RunLoop run_loop;
  cpu_probe_ = std::make_unique<StreamingCpuProbe>(
      base::Milliseconds(1),
      base::BindRepeating(&CpuProbeTest::CollectorCallback,
                          base::Unretained(this)),
      std::move(samples), run_loop.QuitClosure());
  cpu_probe_->EnsureStarted();
  run_loop.Run();

  EXPECT_THAT(samples_,
              testing::ElementsAre(
                  mojom::PressureState{mojom::PressureState::kNominal},
                  mojom::PressureState{mojom::PressureState::kFair},
                  mojom::PressureState{mojom::PressureState::kSerious},
                  mojom::PressureState{mojom::PressureState::kCritical}));
}

TEST_F(CpuProbeTest, StopDelayedEnsureStartedImmediate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_->EnsureStarted();
  WaitForUpdate();
  cpu_probe_->Stop();

  samples_.clear();
  static_cast<FakeCpuProbe*>(cpu_probe_.get())
      ->SetLastSample(PressureSample{0.9});

  cpu_probe_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_F(CpuProbeTest, StopDelayedEnsureStartedDelayed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_->EnsureStarted();
  WaitForUpdate();
  cpu_probe_->Stop();
  samples_.clear();
  static_cast<FakeCpuProbe*>(cpu_probe_.get())
      ->SetLastSample(PressureSample{0.9});
  // 10ms should be long enough to ensure that all the sampling tasks are done.
  base::PlatformThread::Sleep(base::Milliseconds(10));

  cpu_probe_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_F(CpuProbeTest, StopImmediateEnsureStartedImmediate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_->EnsureStarted();
  cpu_probe_->Stop();

  samples_.clear();
  static_cast<FakeCpuProbe*>(cpu_probe_.get())
      ->SetLastSample(PressureSample{0.9});

  cpu_probe_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

TEST_F(CpuProbeTest, StopImmediateEnsureStartedDelayed) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cpu_probe_->EnsureStarted();
  cpu_probe_->Stop();

  samples_.clear();
  static_cast<FakeCpuProbe*>(cpu_probe_.get())
      ->SetLastSample(PressureSample{0.9});
  // 10ms should be long enough to ensure that all the sampling tasks are done.
  base::PlatformThread::Sleep(base::Milliseconds(10));

  cpu_probe_->EnsureStarted();
  WaitForUpdate();
  EXPECT_THAT(samples_, testing::ElementsAre(mojom::PressureState(
                            mojom::PressureState::kSerious)));
}

}  // namespace device
