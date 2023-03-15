// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_WIN_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_WIN_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/public/mojom/pressure_update.mojom-shared.h"

namespace device {

class CpuProbeWin : public CpuProbe {
 public:
  // Factory method for production instances.
  static std::unique_ptr<CpuProbeWin> Create(
      base::TimeDelta,
      base::RepeatingCallback<void(mojom::PressureState)>);

  ~CpuProbeWin() override;

  CpuProbeWin(const CpuProbeWin&) = delete;
  CpuProbeWin& operator=(const CpuProbeWin&) = delete;

 private:
  class BlockingTaskRunnerHelper;

  CpuProbeWin(base::TimeDelta,
              base::RepeatingCallback<void(mojom::PressureState)>);

  // CpuProbe implementation.
  void Update() override;

  base::SequenceBound<BlockingTaskRunnerHelper> helper_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<CpuProbeWin> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_WIN_H_
