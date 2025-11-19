// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CPU_PERFORMANCE_CPU_PERFORMANCE_H_
#define CONTENT_BROWSER_CPU_PERFORMANCE_CPU_PERFORMANCE_H_

#include <cstdint>

#include "base/gtest_prod_util.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/cpu_performance.mojom.h"

namespace content::cpu_performance {

using Tier = blink::mojom::PerformanceTier;

// Returns the CPU performance tier, which exposes some information about
// how powerful the user device is. This function contains the default
// implementation for //content, which can be overridden by embedders.
CONTENT_EXPORT Tier GetTier();

CONTENT_EXPORT Tier GetTierFromCores(int cores);

}  // namespace content::cpu_performance

#endif  // CONTENT_BROWSER_CPU_PERFORMANCE_CPU_PERFORMANCE_H_
