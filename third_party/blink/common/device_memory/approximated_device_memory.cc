// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/system/sys_info.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

// static
float ApproximatedDeviceMemory::approximated_device_memory_gb_ = 0.0;
int64_t ApproximatedDeviceMemory::physical_memory_mb_ = 0;

// static
void ApproximatedDeviceMemory::Initialize() {
  if (approximated_device_memory_gb_ > 0.0)
    return;
  DCHECK_EQ(0, physical_memory_mb_);
  physical_memory_mb_ = ::base::SysInfo::AmountOfPhysicalMemory().InMiB();
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

  // Limit the values to reduce fingerprintability.
  float kMinMemory = 0.25f;
  float kMaxMemory = 8.0f;

  // We're rolling out improved limits. See: https://crbug.com/454354290.
  if (base::FeatureList::IsEnabled(
          blink::features::kUpdatedDeviceMemoryLimitsFor2026)) {
#if BUILDFLAG(IS_ANDROID)
    // Allow smaller lower limits on Android where lower RAM is still common.
    // Note: As of Jan-2026 some Google Search tests in our test suite
    // (GoogleAmpSXGStory2019 and BackgroundGoogleStory2019) serve different
    // content to 1GB and lower. So when increasing this lower limit you will
    // likely see memory regressions.
    kMinMemory = 1.0f;
#else
    // Increased limits on other platforms where higher RAM is more common.
    kMinMemory = 2.0f;
    kMaxMemory = 32.0f;
#endif
  }

  if (approximated_device_memory_gb_ < kMinMemory) {
    approximated_device_memory_gb_ = kMinMemory;
  } else if (approximated_device_memory_gb_ > kMaxMemory) {
    approximated_device_memory_gb_ = kMaxMemory;
  }
}

// static
void ApproximatedDeviceMemory::SetPhysicalMemoryMBForTesting(
    int64_t physical_memory_mb) {
  physical_memory_mb_ = physical_memory_mb;
  CalculateAndSetApproximatedDeviceMemory();
}

}  // namespace blink
