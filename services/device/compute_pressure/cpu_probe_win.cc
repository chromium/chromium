// Copyright 2022 The Chromium Authors. All rights reserved.
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

  const auto& result = GetPdhData();
  if (result.has_value()) {
    last_sample_ = std::move(result.value());
  } else {
    last_sample_ = kUnsupportedValue;
    LOG(ERROR) << result.error();
  }
}

PressureSample CpuProbeWin::LastSample() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return last_sample_;
}

base::expected<PressureSample, std::string> CpuProbeWin::GetPdhData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PDH_STATUS pdh_status;

  if (!cpu_query_.is_valid()) {
    cpu_query_ = ScopedPdhQuery::Create();
    if (!cpu_query_.is_valid())
      return base::unexpected("PdhOpenQuery failed.");

    pdh_status = PdhAddEnglishCounter(cpu_query_.get(),
                                      L"\\Processor(_Total)\\% Processor Time",
                                      NULL, &cpu_percent_utilization_);
    if (pdh_status != ERROR_SUCCESS) {
      cpu_query_.reset();
      return base::unexpected("PdhAddEnglishCounter failed.");
    }
  }

  pdh_status = PdhCollectQueryData(cpu_query_.get());
  if (pdh_status != ERROR_SUCCESS)
    return base::unexpected("PdhCollectQueryData failed.");

  PDH_FMT_COUNTERVALUE counter_value;
  pdh_status = PdhGetFormattedCounterValue(
      cpu_percent_utilization_, PDH_FMT_DOUBLE, NULL, &counter_value);
  if (pdh_status != ERROR_SUCCESS)
    return base::unexpected("PdhGetFormattedCounterValue failed.");

  return PressureSample{counter_value.doubleValue / 100.0};
}

}  // namespace device
