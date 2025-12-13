// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cpu_performance/cpu_performance.h"

#include "base/system/sys_info.h"

namespace content::cpu_performance {

static Tier CalculateTier() {
  int cores = base::SysInfo::NumberOfProcessors();
  return GetTierFromCores(cores);
}

Tier GetTier() {
  static Tier tier = CalculateTier();
  return tier;
}

Tier GetTierFromCores(int cores) {
  if (cores >= 1 && cores <= 2) {
    return Tier::kLow;
  } else if (cores >= 3 && cores <= 4) {
    return Tier::kMid;
  } else if (cores >= 5 && cores <= 12) {
    return Tier::kHigh;
  } else if (cores >= 13) {
    return Tier::kUltra;
  }
  return Tier::kUnknown;
}

}  // namespace content::cpu_performance
