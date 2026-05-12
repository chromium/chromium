// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CPU_PERFORMANCE_CPU_PERFORMANCE_H_
#define CONTENT_BROWSER_CPU_PERFORMANCE_CPU_PERFORMANCE_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/cpu_performance.h"
#include "third_party/blink/public/mojom/cpu_performance.mojom-shared.h"

namespace content::cpu_performance {

// Returns the CPU performance tier from an integer. The value must be
// within the valid range of PerformanceTier.
CONTENT_EXPORT Tier TierFromInt(int value);

enum class Manufacturer {
  kUnknown,
  // ---
  kAMD,
  kApple,
  kIntel,
  kMediaTek,
  kMicrosoft,
  kQualcomm,
  kSamsung,
};

CONTENT_EXPORT std::pair<Manufacturer, std::string> SplitCpuModel(
    std::string_view cpu_model);

CONTENT_EXPORT void Initialize();

CONTENT_EXPORT Tier GetTierFromCores(int cores);
CONTENT_EXPORT Tier GetTierFromCpuInfo(std::string_view cpu_model, int cores);

}  // namespace content::cpu_performance

#endif  // CONTENT_BROWSER_CPU_PERFORMANCE_CPU_PERFORMANCE_H_
