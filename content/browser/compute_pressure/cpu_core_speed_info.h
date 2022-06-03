// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_CPU_CORE_SPEED_INFO_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_CPU_CORE_SPEED_INFO_H_

#include <stdint.h>

#include "content/common/content_export.h"

namespace content {

// Information used for computing the normalized speed of a single CPU core.
//
// Input frequencies are expected to be expressed in Hz. However, any unit
// works, as long as all frequencies use the same unit.
struct CONTENT_EXPORT CpuCoreSpeedInfo {
  // The minimum frequency in the CPU core's dynamic clocking range.
  //
  // Most CPU cores cannot be completely stopped, so this is rarely zero.
  //
  // Negative values are invalid, and will cause IsValid() to return false.
  int64_t min_frequency;

  // The maximum frequency in the CPU core's dynamic clocking range.
  //
  // Negative values are invalid, and will cause IsValid() to return false.
  int64_t max_frequency;

  // The maximum frequency where the CPU core can operate sustainably.
  //
  // For x86 CPUs, this frequency is reported on marketing materials, and may be
  // reported in the CPUID vendor string.
  //
  // Negative values represent a failure to obtain the core's base frequency.
  // Negative values will not cause IsValid() to return false.
  int64_t base_frequency;

  // The CPU core's current frequency.
  //
  // Negative values represent a failure to read the current frequency, and will
  // cause IsValid() to return false.
  //
  // NormalizedSpeed() caps values outside the expected range
  // [min_frequency, max_frequency] at the range's extremes.
  int64_t current_frequency;

  // Returns true if this info can be used to compute a normalized speed.
  //
  // For example, it doesn't make sense to reason about normalized speed if the
  // core's minimum frequency exceeds its maximum frequency
  bool IsValid() const;

  // Computes the normalized speed for the CPU core represented by this info.
  //
  // Returns a number in the range [0.0, 1.0].
  //
  // This info must be valid according to IsValid().
  double NormalizedSpeed() const;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_CPU_CORE_SPEED_INFO_H_
