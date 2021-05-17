// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/cpu_probe.h"

#include <memory>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "content/browser/compute_pressure/compute_pressure_sample.h"
#include "content/browser/compute_pressure/cpu_probe_linux.h"

namespace content {

constexpr ComputePressureSample CpuProbe::kUnsupportedValue;

CpuProbe::CpuProbe() = default;
CpuProbe::~CpuProbe() = default;

// static
double CpuProbe::CoreSpeed(double min_frequency,
                           double max_frequency,
                           double base_frequency,
                           double current_frequency) {
  if (min_frequency >= max_frequency) {
    // Fail on cores that don't support scaling.
    return -1;
  }

  if (min_frequency < 0 || current_frequency < 0) {
    // Fail on cores with incomplete scaling information. Frequency readers
    // return -1 on failure, resulting in negative frequencies.
    return -1;
  }

  // We don't need to check for max_frequency < 0 because the two checks above
  // guarantee that 0 <= min_frequency and min_frequency < max_frequency.
  DCHECK_GE(min_frequency, 0);
  DCHECK_GT(max_frequency, min_frequency);
  DCHECK_GT(max_frequency, 0);

  // Cap the current frequency.
  current_frequency = std::max(min_frequency, current_frequency);
  current_frequency = std::min(max_frequency, current_frequency);
  DCHECK_GE(current_frequency, min_frequency);
  DCHECK_LE(current_frequency, max_frequency);

  if (base_frequency < min_frequency || base_frequency > max_frequency) {
    // Use a linear scale for cores that don't report base_frequency.
    base_frequency = (min_frequency + max_frequency) / 2;
  }
  DCHECK_GE(base_frequency, min_frequency);
  DCHECK_LE(base_frequency, max_frequency);

  if (current_frequency >= base_frequency && base_frequency != max_frequency) {
    double result = 0.5 + (current_frequency - base_frequency) /
                              ((max_frequency - base_frequency) * 2);
    DCHECK_GE(result, 0.5);
    DCHECK_LE(result, 1.0);
    return result;
  } else {
    double result = (current_frequency - min_frequency) /
                    ((base_frequency - min_frequency) * 2);
    DCHECK_GE(result, 0.0);
    DCHECK_LE(result, 0.5);
    return result;
  }
}

class NullCpuProbe : public CpuProbe {
 public:
  NullCpuProbe() { DETACH_FROM_SEQUENCE(sequence_checker_); }
  ~NullCpuProbe() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // CpuProbe implementation.
  void Update() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Checks that
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
  }
  ComputePressureSample LastSample() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return CpuProbe::kUnsupportedValue;
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

// Default implementation for platforms that don't have one.

// static
std::unique_ptr<CpuProbe> CpuProbe::Create() {
#if defined(OS_ANDROID)
  return nullptr;
#elif defined(OS_LINUX) || defined(OS_CHROMEOS)
  return CpuProbeLinux::Create();
#else
  return std::make_unique<NullCpuProbe>();
#endif  // defined(OS_ANDROID)
}

}  // namespace content
