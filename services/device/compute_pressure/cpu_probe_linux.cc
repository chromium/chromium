// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe_linux.h"

#include <stdint.h>

#include <utility>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace device {

// Helper class that performs the actual I/O. It must run on a
// SequencedTaskRunner that is properly configured for blocking I/O
// operations.
class CpuProbeLinux::BlockingTaskRunnerHelper final {
 public:
  BlockingTaskRunnerHelper(base::FilePath,
                           base::WeakPtr<CpuProbeLinux>,
                           scoped_refptr<base::SequencedTaskRunner>);
  ~BlockingTaskRunnerHelper();

  BlockingTaskRunnerHelper(const BlockingTaskRunnerHelper&) = delete;
  BlockingTaskRunnerHelper& operator=(const BlockingTaskRunnerHelper&) = delete;

  void Update();

 private:
  // Called when a core is seen the first time in /proc/stat.
  //
  // For most systems, the cores listed in /proc/stat are static. However, it
  // is theoretically possible for cores to go online and offline.
  void InitializeCore(size_t, const CoreTimes&);

  SEQUENCE_CHECKER(sequence_checker_);

  // |owner_| can only be checked for validity on |owner_task_runner_|'s
  // sequence.
  base::WeakPtr<CpuProbeLinux> owner_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Task runner belonging to CpuProbeWin. Calls to it
  // will be done via this task runner.
  scoped_refptr<base::SequencedTaskRunner> owner_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // /proc/stat parser. Used to derive CPU utilization.
  ProcfsStatCpuParser stat_parser_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Most recent per-core times from /proc/stat.
  std::vector<CoreTimes> last_per_core_times_
      GUARDED_BY_CONTEXT(sequence_checker_);

  PressureSample last_sample_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kUnsupportedValue;
};

CpuProbeLinux::BlockingTaskRunnerHelper::BlockingTaskRunnerHelper(
    base::FilePath procfs_stat_path,
    base::WeakPtr<CpuProbeLinux> cpu_probe_linux,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : owner_(cpu_probe_linux),
      owner_task_runner_(std::move(task_runner)),
      stat_parser_(std::move(procfs_stat_path)) {}

CpuProbeLinux::BlockingTaskRunnerHelper::~BlockingTaskRunnerHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbeLinux::BlockingTaskRunnerHelper::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  stat_parser_.Update();
  const std::vector<CoreTimes>& per_core_times = stat_parser_.core_times();

  double utilization_sum = 0.0;
  int utilization_cores = 0;
  for (size_t i = 0; i < per_core_times.size(); ++i) {
    CHECK_GE(last_per_core_times_.size(), i);

    const CoreTimes& core_times = per_core_times[i];

    if (last_per_core_times_.size() == i) {
      InitializeCore(i, core_times);
      continue;
    }

    double core_utilization =
        core_times.TimeUtilization(last_per_core_times_[i]);
    if (core_utilization >= 0) {
      // Only overwrite `last_per_core_times_` if the /proc/stat counters are
      // monotonically increasing. Otherwise, discard the measurement.
      last_per_core_times_[i] = core_times;

      utilization_sum += core_utilization;
      ++utilization_cores;
    }
  }

  if (utilization_cores > 0) {
    last_sample_.cpu_utilization = utilization_sum / utilization_cores;
  } else {
    last_sample_ = kUnsupportedValue;
  }

  owner_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CpuProbeLinux::OnPressureSampleAvailable,
                                owner_, last_sample_));
}

void CpuProbeLinux::BlockingTaskRunnerHelper::InitializeCore(
    size_t core_index,
    const CoreTimes& initial_core_times) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(last_per_core_times_.size(), core_index);

  last_per_core_times_.push_back(initial_core_times);
}

// static
std::unique_ptr<CpuProbeLinux> CpuProbeLinux::Create(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback) {
  return base::WrapUnique(
      new CpuProbeLinux(sampling_interval, std::move(sampling_callback),
                        base::FilePath(ProcfsStatCpuParser::kProcfsStatPath)));
}

CpuProbeLinux::CpuProbeLinux(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureState)> sampling_callback,
    base::FilePath procfs_stat_path)
    : CpuProbe(sampling_interval, std::move(sampling_callback)) {
  helper_ = base::SequenceBound<BlockingTaskRunnerHelper>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
      std::move(procfs_stat_path), weak_factory_.GetWeakPtr(),
      base::SequencedTaskRunner::GetCurrentDefault());
}

CpuProbeLinux::~CpuProbeLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbeLinux::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  helper_.AsyncCall(&BlockingTaskRunnerHelper::Update);
}

}  // namespace device
