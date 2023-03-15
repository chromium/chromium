// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/pressure_test_support.h"

#include "base/check_op.h"
#include "base/location.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"

namespace device {

constexpr PressureSample FakeCpuProbe::kInitialSample;

FakeCpuProbe::FakeCpuProbe(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback)
    : CpuProbe(sampling_interval, std::move(sampling_callback)),
      last_sample_(kInitialSample) {}

FakeCpuProbe::~FakeCpuProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FakeCpuProbe::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FakeCpuProbe::OnUpdate, base::Unretained(this)));
}

void FakeCpuProbe::OnUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::AutoLock auto_lock(lock_);
  OnPressureSampleAvailable(last_sample_);
}

void FakeCpuProbe::SetLastSample(PressureSample sample) {
  base::AutoLock auto_lock(lock_);
  last_sample_ = sample;
}

StreamingCpuProbe::StreamingCpuProbe(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback,
    std::vector<PressureSample> samples,
    base::OnceClosure callback)
    : CpuProbe(sampling_interval, std::move(sampling_callback)),
      samples_(std::move(samples)),
      callback_(std::move(callback)) {
  DCHECK_GT(samples_.size(), 0u);
}

StreamingCpuProbe::~StreamingCpuProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StreamingCpuProbe::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&StreamingCpuProbe::OnUpdate, base::Unretained(this)));
}

void StreamingCpuProbe::OnUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ++sample_index_;

  if (sample_index_ < samples_.size()) {
    OnPressureSampleAvailable(samples_.at(sample_index_));
    return;
  }

  if (!callback_.is_null()) {
    std::move(callback_).Run();
  }
}

}  // namespace device
