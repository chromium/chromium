// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compute_pressure/cpu_core_speed_info.h"

#include <algorithm>

#include "base/check_op.h"

namespace content {

bool CpuCoreSpeedInfo::IsValid() const {
  if (min_frequency >= max_frequency) {
    // Fail on cores that don't support scaling.
    return false;
  }

  if (min_frequency < 0 || current_frequency < 0) {
    // Fail on cores with incomplete scaling information. Frequency readers
    // return -1 on failure, resulting in negative frequencies.
    return false;
  }

  return true;
}

double CpuCoreSpeedInfo::NormalizedSpeed() const {
  DCHECK(IsValid());

  // We don't need to check for max_frequency < 0 because the two checks above
  // guarantee that 0 <= min_frequency and min_frequency < max_frequency.
  DCHECK_GE(min_frequency, 0);
  DCHECK_GT(max_frequency, min_frequency);
  DCHECK_GT(max_frequency, 0);

  // Cap the current frequency.
  double capped_current_frequency = std::max(min_frequency, current_frequency);
  capped_current_frequency =
      std::min<double>(max_frequency, capped_current_frequency);
  DCHECK_GE(capped_current_frequency, min_frequency);
  DCHECK_LE(capped_current_frequency, max_frequency);

  // Use a linear scale for cores that don't report base_frequency.
  double scaled_base_frequency =
      (base_frequency < min_frequency || base_frequency > max_frequency)
          ? (min_frequency + max_frequency) / 2.0
          : base_frequency;
  DCHECK_GE(scaled_base_frequency, min_frequency);
  DCHECK_LE(scaled_base_frequency, max_frequency);

  if (capped_current_frequency >= scaled_base_frequency &&
      scaled_base_frequency != max_frequency) {
    double result = 0.5 + (capped_current_frequency - scaled_base_frequency) /
                              ((max_frequency - scaled_base_frequency) * 2);
    DCHECK_GE(result, 0.5);
    DCHECK_LE(result, 1.0);
    return result;
  } else {
    double result = (capped_current_frequency - min_frequency) /
                    ((scaled_base_frequency - min_frequency) * 2);
    DCHECK_GE(result, 0.0);
    DCHECK_LE(result, 0.5);
    return result;
  }
}

}  // namespace content
