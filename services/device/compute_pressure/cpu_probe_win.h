// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_WIN_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_WIN_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "services/device/compute_pressure/cpu_probe.h"
#include "services/device/compute_pressure/pressure_sample.h"
#include "services/device/compute_pressure/scoped_pdh_query.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class CpuProbeWin : public CpuProbe {
 public:
  // Factory method for production instances.
  static std::unique_ptr<CpuProbeWin> Create();

  ~CpuProbeWin() override;

  CpuProbeWin(const CpuProbeWin&) = delete;
  CpuProbeWin& operator=(const CpuProbeWin&) = delete;

  // CpuProbe implementation.
  void Update() override;
  PressureSample LastSample() override;

 private:
  CpuProbeWin();

  absl::optional<PressureSample> GetPdhData();

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to derive CPU utilization.
  ScopedPdhQuery cpu_query_;
  // This "handle" doesn't need to be freed but its lifetime is associated
  // with cpu_query_.
  PDH_HCOUNTER cpu_percent_utilization_;

  PressureSample last_sample_ GUARDED_BY_CONTEXT(sequence_checker_) =
      kUnsupportedValue;
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_CPU_PROBE_WIN_H_
