// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/cpu.h"

#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "services/on_device_model/public/cpp/features.h"

namespace on_device_model {
namespace {

const base::FeatureParam<int> kRAMThreshold{&features::kOnDeviceModelCpuBackend,
                                            "on_device_cpu_ram_threshold_mb",
                                            15000};

const base::FeatureParam<int> kProcessorThreshold{
    &features::kOnDeviceModelCpuBackend,
    "on_device_cpu_processor_count_threshold", 4};

}  // namespace

bool IsCpuCapable() {
  if (base::FeatureList::IsEnabled(features::kOnDeviceModelForceCpuBackend)) {
    return true;
  }
  return base::FeatureList::IsEnabled(features::kOnDeviceModelCpuBackend) &&
         base::SysInfo::AmountOfPhysicalMemoryMB() >= kRAMThreshold.Get() &&
         base::SysInfo::NumberOfProcessors() >= kProcessorThreshold.Get();
}

}  // namespace on_device_model
