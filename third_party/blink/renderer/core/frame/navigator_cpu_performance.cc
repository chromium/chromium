// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_cpu_performance.h"

#include "third_party/blink/public/mojom/cpu_performance.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/frame/navigator.h"

namespace blink {

// static
uint16_t NavigatorCPUPerformance::cpuPerformance(Navigator& navigator) {
  switch (Platform::Current()->GetCpuPerformanceTier()) {
    case mojom::blink::PerformanceTier::kLow:
      return 1;
    case mojom::blink::PerformanceTier::kMid:
      return 2;
    case mojom::blink::PerformanceTier::kHigh:
      return 3;
    case mojom::blink::PerformanceTier::kUltra:
      return 4;
    case mojom::blink::PerformanceTier::kUnknown:
    default:
      return 0;
  }
}

}  // namespace blink
