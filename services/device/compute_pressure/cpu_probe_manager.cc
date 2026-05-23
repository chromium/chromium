// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/cpu_probe_manager.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "components/system_cpu/cpu_probe.h"
#include "services/device/public/cpp/device_features.h"

namespace device {

namespace {

using system_cpu::CpuProbe;
using system_cpu::CpuSample;

}  // namespace

// static
std::unique_ptr<CpuProbeManager> CpuProbeManager::Create(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureDataPtr)> sampling_callback) {
  std::unique_ptr<CpuProbe> system_cpu_probe = CpuProbe::Create();
  if (!system_cpu_probe) {
    return nullptr;
  }
  return base::WrapUnique(new CpuProbeManager(
      sampling_interval, sampling_callback, std::move(system_cpu_probe)));
}

// static
std::unique_ptr<CpuProbeManager> CpuProbeManager::CreateForTesting(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureDataPtr)> sampling_callback,
    std::unique_ptr<CpuProbe> system_cpu_probe) {
  if (!system_cpu_probe) {
    return nullptr;
  }
  return base::WrapUnique(new CpuProbeManager(
      sampling_interval, sampling_callback, std::move(system_cpu_probe)));
}

CpuProbeManager::CpuProbeManager(
    base::TimeDelta sampling_interval,
    base::RepeatingCallback<void(mojom::PressureDataPtr)> sampling_callback,
    std::unique_ptr<CpuProbe> system_cpu_probe)
    : sampling_interval_(sampling_interval),
      sampling_callback_(std::move(sampling_callback)),
      system_cpu_probe_(std::move(system_cpu_probe)) {
  CHECK(system_cpu_probe_);
  CHECK(sampling_callback_);
}

CpuProbeManager::~CpuProbeManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbeManager::EnsureStarted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (timer_.IsRunning()) {
    // Already started.
    return;
  }

  // The CpuSample reported by many CpuProbe implementations relies on the
  // differences observed between two Update() calls. For this reason, the
  // CpuSample reported from StartSampling() is not reported via
  // `sampling_callback_`.
  system_cpu_probe_->StartSampling(base::DoNothing());

  timer_.Start(FROM_HERE, sampling_interval_,
               base::BindRepeating(
                   &CpuProbe::RequestSample, system_cpu_probe_->GetWeakPtr(),
                   base::BindRepeating(&CpuProbeManager::OnCpuSampleAvailable,
                                       weak_factory_.GetWeakPtr())));
}

void CpuProbeManager::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timer_.Stop();
  weak_factory_.InvalidateWeakPtrs();
}

system_cpu::CpuProbe* CpuProbeManager::cpu_probe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return system_cpu_probe_.get();
}

void CpuProbeManager::OnCpuSampleAvailable(std::optional<CpuSample> sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If the timer was stopped, OnCpuSampleAvailable should have been cancelled
  // by InvalidateWeakPtrs().
  CHECK(timer_.IsRunning());
  if (sample.has_value()) {
    auto data = mojom::PressureData::New(sample.value().cpu_utilization,
                                         /*own_pressure_estimate=*/0.0);
    sampling_callback_.Run(std::move(data));
  }
}

void CpuProbeManager::SetCpuProbeForTesting(
    std::unique_ptr<system_cpu::CpuProbe> cpu_probe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!timer_.IsRunning());
  system_cpu_probe_ = std::move(cpu_probe);
}

}  // namespace device
