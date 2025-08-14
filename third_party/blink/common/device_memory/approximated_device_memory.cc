// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"

#include "base/check_op.h"
#include "base/system/sys_info.h"

namespace blink {

// static
float ApproximatedDeviceMemory::approximated_device_memory_gb_ = 0.0;
int64_t ApproximatedDeviceMemory::physical_memory_mb_ = 0;

// static
void ApproximatedDeviceMemory::Initialize() {
  if (approximated_device_memory_gb_ > 0.0)
    return;
  DCHECK_EQ(0, physical_memory_mb_);
  physical_memory_mb_ = ::base::SysInfo::AmountOfPhysicalMemoryMB();
  CalculateAndSetApproximatedDeviceMemory();
}

// static
float ApproximatedDeviceMemory::GetApproximatedDeviceMemory() {
  return approximated_device_memory_gb_;
}

// static
void ApproximatedDeviceMemory::CalculateAndSetApproximatedDeviceMemory() {
  // The calculations in this method are described in the specification:
  // https://w3c.github.io/device-memory/.
  DCHECK_GT(physical_memory_mb_, 0);
  int lower_bound = physical_memory_mb_;
  int power = 0;

  // Extract the most-significant-bit and its location.
  while (lower_bound > 1) {
    lower_bound >>= 1;
    power++;
  }
  // The remaining should always be equal to exactly 1.
  DCHECK_EQ(lower_bound, 1);

  int64_t upper_bound = lower_bound + 1;
  lower_bound = lower_bound << power;
  upper_bound = upper_bound << power;

  // Find the closest bound, and convert it to GB.
  if (physical_memory_mb_ - lower_bound <= upper_bound - physical_memory_mb_)
    approximated_device_memory_gb_ = static_cast<float>(lower_bound) / 1024.0;
  else
    approximated_device_memory_gb_ = static_cast<float>(upper_bound) / 1024.0;

  // Max-limit the reported value to 8GB to reduce fingerprintability of
  // high-spec machines.
  if (approximated_device_memory_gb_ > 8)
    approximated_device_memory_gb_ = 8.0;
}

// static
void ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(
    int64_t physical_memory_mb) {
  physical_memory_mb_ = physical_memory_mb;
  CalculateAndSetApproximatedDeviceMemory();
}

}  // namespace blink
