// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_cpu_performance.h"

#include "base/system/sys_info.h"
#include "third_party/blink/renderer/core/frame/navigator.h"

namespace blink {

namespace {
// static
uint16_t GetTierFromCores(int cores) {
  if (cores >= 1 && cores <= 2) {
    return 1;  // low
  } else if (cores >= 3 && cores <= 4) {
    return 2;  // medium
  } else if (cores >= 5 && cores <= 12) {
    return 3;  // high
  } else if (cores >= 13) {
    return 4;  // ultra high
  }
  return 0;  // unknown
}
}  // anonymous namespace

// static
uint16_t NavigatorCPUPerformance::cpuPerformance(Navigator& navigator) {
  return GetTierFromCores(base::SysInfo::NumberOfProcessors());
}

}  // namespace blink
