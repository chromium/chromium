// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe_win.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace device {

// static
std::unique_ptr<CpuProbeWin> CpuProbeWin::Create() {
  return base::WrapUnique(new CpuProbeWin());
}

CpuProbeWin::CpuProbeWin() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CpuProbeWin::~CpuProbeWin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbeWin::Update() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto result = GetPdhData();
  last_sample_ = result ? *result : kUnsupportedValue;
}

PressureSample CpuProbeWin::LastSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return last_sample_;
}

absl::optional<PressureSample> CpuProbeWin::GetPdhData() {
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

}  // namespace device
