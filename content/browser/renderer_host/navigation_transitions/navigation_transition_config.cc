// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"

#include "base/system/sys_info.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/screen.h"

namespace content {
namespace {

const base::FeatureParam<int> kMaxScreenshotCount{
    &blink::features::kBackForwardTransitions, "max-screenshot-count", 20};

const base::FeatureParam<int> kMaxCacheSize{
    &blink::features::kBackForwardTransitions, "max-cache-size", -1};

const base::FeatureParam<double> kPercentageOfRamToUse{
    &blink::features::kBackForwardTransitions, "percentage-of-ram-to-use", 2.5};

const base::FeatureParam<base::TimeDelta> kInvisibleCacheCleanupDelay{
    &blink::features::kBackForwardTransitions, "invisible-cache-cleanup-delay",
    base::Minutes(7)};

size_t GetMaxCacheSizeInBytes() {
  constexpr int kLowEndMax = 32 * 1024 * 1024;  // 32MB
  constexpr int kOtherMax = 128 * 1024 * 1024;  // 128MB
  const size_t default_size =
      base::SysInfo::IsLowEndDevice() ? kLowEndMax : kOtherMax;

  int size = kMaxCacheSize.Get();
  return size < 0 ? default_size : static_cast<size_t>(size);
}

}  // namespace

// static
bool NavigationTransitionConfig::AreBackForwardTransitionsEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kBackForwardTransitions);
}

// static
size_t NavigationTransitionConfig::ComputeCacheSizeInBytes() {
  // Assume 4 bytes per pixel. This value estimates the max number of bytes of
  // the physical screen's uncompressed bitmap.
  const size_t display_size_in_bytes = 4 * display::Screen::GetScreen()
                                               ->GetPrimaryDisplay()
                                               .GetSizeInPixel()
                                               .Area64();
  size_t memory_required_for_max_screenshots =
      display_size_in_bytes * kMaxScreenshotCount.Get();

  size_t physical_memory_budget =
      (base::SysInfo::AmountOfPhysicalMemory() * kPercentageOfRamToUse.Get()) /
      100;
  physical_memory_budget =
      std::min(physical_memory_budget, GetMaxCacheSizeInBytes());
  physical_memory_budget =
      std::min(physical_memory_budget, memory_required_for_max_screenshots);

  // We should at least be able to cache one uncompressed screenshot.
  physical_memory_budget =
      std::max(display_size_in_bytes, physical_memory_budget);

  return physical_memory_budget;
}

// static
base::TimeDelta
NavigationTransitionConfig::GetCleanupDelayForInvisibleCaches() {
  return kInvisibleCacheCleanupDelay.Get();
}

}  // namespace content
