// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/compute_pressure/virtual_probes_manager.h"

#include <memory>

#include "base/time/time.h"
#include "services/device/compute_pressure/virtual_cpu_probe_manager.h"
#include "services/device/public/mojom/pressure_manager.mojom.h"

namespace device {

namespace {

// We need a shorter interval when using virtual probes because some rate
// obfuscation web tests require 50-100 updates to trigger the mitigation, and
// sampling once per second (per PressureManagerImpl::kDefaultSamplingInterval)
// takes too long.
constexpr base::TimeDelta kVirtualProbeSamplingInterval =
    base::Milliseconds(100);

}  // namespace

VirtualProbesManager::VirtualProbesManager(base::TimeDelta sampling_interval)
    : ProbesManager(sampling_interval) {}

VirtualProbesManager::~VirtualProbesManager() = default;

bool VirtualProbesManager::AddOverrideForSource(
    mojom::PressureSource source,
    mojom::VirtualPressureSourceMetadataPtr metadata) {
  if (overridden_sources_.Has(source)) {
    return false;
  }
  overridden_sources_.Put(source);

  switch (source) {
    case mojom::PressureSource::kCpu: {
      std::unique_ptr<CpuProbeManager> manager =
          metadata->available
              ? VirtualCpuProbeManager::Create(kVirtualProbeSamplingInterval,
                                               cpu_probe_sampling_callback())
              : nullptr;
      set_cpu_probe_manager(std::move(manager));
      break;
    }
  }

  return true;
}

void VirtualProbesManager::RemoveOverrideForSource(
    mojom::PressureSource source) {
  if (!overridden_sources_.Has(source)) {
    return;
  }
  overridden_sources_.Remove(source);

  switch (source) {
    case mojom::PressureSource::kCpu: {
      set_cpu_probe_manager(nullptr);
      break;
    }
  }
}

void VirtualProbesManager::AddUpdate(mojom::PressureSource source,
                                     mojom::PressureState state) {
  if (!overridden_sources_.Has(source)) {
    return;
  }

  switch (source) {
    case mojom::PressureSource::kCpu: {
      if (auto* manager = cpu_probe_manager()) {
        static_cast<VirtualCpuProbeManager*>(manager)->SetPressureState(state);
      }
      break;
    }
  }
}

bool VirtualProbesManager::IsOverriding(mojom::PressureSource source) const {
  return overridden_sources_.Has(source);
}

}  // namespace device
