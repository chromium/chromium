// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CPU_PERFORMANCE_H_
#define CONTENT_PUBLIC_BROWSER_CPU_PERFORMANCE_H_

#include <string>

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/cpu_performance.mojom-shared.h"

namespace content::cpu_performance {

using Tier = blink::mojom::PerformanceTier;

// Returns the nominal hardware CPU performance tier.
CONTENT_EXPORT Tier GetTier();

// Returns the CPU model string of the user device.
CONTENT_EXPORT std::string GetModel();

// Returns the number of logical cores of the user device.
CONTENT_EXPORT int GetCores();

}  // namespace content::cpu_performance

#endif  // CONTENT_PUBLIC_BROWSER_CPU_PERFORMANCE_H_
