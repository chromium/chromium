// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_LINUX_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_LINUX_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "services/device/compute_pressure/core_times.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/pressure_sample.h"
#include "services/device/compute_pressure/procfs_stat_cpu_parser.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

class CpuProbeLinux : public CpuProbe {
 public:
  // Factory method for production instances.
  static std::unique_ptr<CpuProbeLinux> Create(
      base::TimeDelta,
      base::RepeatingCallback<void(mojom::PressureState)>);

  ~CpuProbeLinux() override;

  CpuProbeLinux(const CpuProbeLinux&) = delete;
  CpuProbeLinux& operator=(const CpuProbeLinux&) = delete;

 protected:
  CpuProbeLinux(base::TimeDelta,
                base::RepeatingCallback<void(mojom::PressureState)>,
                base::FilePath);

 private:
  FRIEND_TEST_ALL_PREFIXES(CpuProbeLinuxTest, ProductionDataNoCrash);
  FRIEND_TEST_ALL_PREFIXES(CpuProbeLinuxTest, OneCoreFullInfo);
  FRIEND_TEST_ALL_PREFIXES(CpuProbeLinuxTest, TwoCoresFullInfo);
  FRIEND_TEST_ALL_PREFIXES(CpuProbeLinuxTest, TwoCoresSecondCoreMissingStat);

  class BlockingTaskRunnerHelper;

  // CpuProbe implementation.
  void Update() override;

  base::SequenceBound<BlockingTaskRunnerHelper> helper_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<CpuProbeLinux> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_LINUX_H_
