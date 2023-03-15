// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe_win.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "services/device/compute_pressure/pressure_sample.h"
#include "services/device/compute_pressure/scoped_pdh_query.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

// Helper class that performs the actual I/O. It must run on a
// SequencedTaskRunner that is properly configured for blocking I/O
// operations.
class CpuProbeWin::BlockingTaskRunnerHelper final {
 public:
  BlockingTaskRunnerHelper(base::WeakPtr<CpuProbeWin>,
                           scoped_refptr<base::SequencedTaskRunner>);
  ~BlockingTaskRunnerHelper();

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  void Update();

 private:
  absl::optional<PressureSample> GetPdhData();

  SEQUENCE_CHECKER(sequence_checker_);

  // |owner_| can only be checked for validity on |owner_task_runner_|'s
  // sequence.
  base::WeakPtr<CpuProbeWin> owner_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Task runner belonging to CpuProbeWin. Calls to it
  // will be done via this task runner.
  scoped_refptr<base::SequencedTaskRunner> owner_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to derive CPU utilization.
  ScopedPdhQuery cpu_query_ GUARDED_BY_CONTEXT(sequence_checker_);

  // This "handle" doesn't need to be freed but its lifetime is associated
  // with cpu_query_.
  PDH_HCOUNTER cpu_percent_utilization_ GUARDED_BY_CONTEXT(sequence_checker_);

  // True if PdhCollectQueryData has been called.
  //
  // It requires two data samples to calculate a formatted data value. So
  // PdhCollectQueryData should be called twice before calling
  // PdhGetFormattedCounterValue.
  // Detailed information can be found in the following website:
  // https://learn.microsoft.com/en-us/windows/win32/perfctrs/collecting-performance-data
  bool got_baseline_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  PressureSample last_sample_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kUnsupportedValue;
};

CpuProbeWin::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper(
    base::WeakPtr<CpuProbeWin> cpu_probe_win,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : owner_(cpu_probe_win), owner_task_runner_(std::move(task_runner)) {}

CpuProbeWin::BlockingTaskRunnerHelper::~BlockingTaskRunnerHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbeWin::BlockingTaskRunnerHelper::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto result = GetPdhData();
  last_sample_ = result ? *result : kUnsupportedValue;
  owner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CpuProbeWin::OnPressureSampleAvailable, owner_,
                                last_sample_));
}

absl::optional<PressureSample>
CpuProbeWin::BlockingTaskRunnerHelper::GetPdhData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PDH_STATUS pdh_status;

  if (!cpu_query_.is_valid()) {
    cpu_query_ = ScopedPdhQuery::Create();
    if (!cpu_query_.is_valid())
      return absl::nullopt;

    pdh_status = PdhAddEnglishCounter(cpu_query_.get(),
                                      L"\\Processor(_Total)\\% Processor Time",
                                      NULL, &cpu_percent_utilization_);
    if (pdh_status != ERROR_SUCCESS) {
      cpu_query_.reset();
      LOG(ERROR) << "PdhAddEnglishCounter failed: "
                 << logging::SystemErrorCodeToString(pdh_status);
      return absl::nullopt;
    }
  }

  pdh_status = PdhCollectQueryData(cpu_query_.get());
  if (pdh_status != ERROR_SUCCESS) {
    LOG(ERROR) << "PdhCollectQueryData failed: "
               << logging::SystemErrorCodeToString(pdh_status);
    return absl::nullopt;
  }

  if (!got_baseline_) {
    got_baseline_ = true;
    return absl::nullopt;
  }

  PDH_FMT_COUNTERVALUE counter_value;
  pdh_status = PdhGetFormattedCounterValue(
      cpu_percent_utilization_, PDH_FMT_DOUBLE, NULL, &counter_value);
  if (pdh_status != ERROR_SUCCESS) {
    LOG(ERROR) << "PdhGetFormattedCounterValue failed: "
               << logging::SystemErrorCodeToString(pdh_status);
    return absl::nullopt;
  }

  return PressureSample{counter_value.doubleValue / 100.0};
}

// static
std::unique_ptr<CpuProbeWin> CpuProbeWin::Create(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback) {
  return base::WrapUnique(
      new CpuProbeWin(sampling_interval, std::move(sampling_callback)));
}

CpuProbeWin::CpuProbeWin(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback)
    : CpuProbe(sampling_interval, std::move(sampling_callback)) {
  helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
      weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

CpuProbeWin::~CpuProbeWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbeWin::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  helper_.AsyncCall(&BlockingTaskRunnerHelper::Update);
}

}  // namespace device
