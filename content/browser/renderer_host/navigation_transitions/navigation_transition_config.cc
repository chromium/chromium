// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"

#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/screen.h"

namespace content {
namespace {

const base::FeatureParam<int> kMaxScreenshotCount{
    &blink::features::kBackForwardTransitions, "max-screenshot-count", 20};

const base::FeatureParam<int> kMaxCacheSize{
    &blink::features::kBackForwardTransitions, "max-cache-size", -1};

const base::FeatureParam<int> kMinRequiredPhysicalRamMb{
    &blink::features::kBackForwardTransitions, "min-required-physical-ram-mb",
    0};

const base::FeatureParam<double> kPercentageOfRamToUse{
    &blink::features::kBackForwardTransitions, "percentage-of-ram-to-use", 2.5};

const base::FeatureParam<base::TimeDelta> kInvisibleCacheCleanupDelay{
    &blink::features::kBackForwardTransitions, "invisible-cache-cleanup-delay",
    base::Minutes(7)};

// Compression can be done in a best-effort basis to reduce contention.
// Quiescence is defined as no visible page loading and no input being
// processed.
const base::FeatureParam<bool> kCompressScreenshotWhenQuiet{
    &blink::features::kBackForwardTransitions, "compress-screenshot-when-quiet",
    false};

// SendResult is an expensive operation and the start of a navigation is a busy
// time. Delaying SendResult reduces chances of contention.
// The value can be based on human reaction times and LCP latencies and it can
// be adjusted based on the incidence of the value SentScreenshotRequest in
// Navigation.GestureTransition.CacheHitOrMissReason.
const base::FeatureParam<int> kScreenshotSendResultDelayMs{
    &blink::features::kBackForwardTransitions,
    "screenshot-send-result-delay-ms", 0};

size_t GetMaxCacheSizeInBytes() {
  constexpr int kLowEndMax = 32 * 1024 * 1024;  // 32MB
  constexpr int kOtherMax = 128 * 1024 * 1024;  // 128MB
  const size_t default_size =
      base::SysInfo::IsLowEndDevice() ? kLowEndMax : kOtherMax;

  int size = kMaxCacheSize.Get();
  return size < 0 ? default_size : static_cast<size_t>(size);
}

int GetMinRequiredPhysicalRamMb() {
  return kMinRequiredPhysicalRamMb.Get();
}

}  // namespace

// static
bool NavigationTransitionConfig::AreBackForwardTransitionsEnabled() {
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <
      GetMinRequiredPhysicalRamMb()) {
    return false;
  }
  return base::FeatureList::IsEnabled(blink::features::kBackForwardTransitions);
}

// static
size_t NavigationTransitionConfig::ComputeCacheSizeInBytes() {
  // Assume 4 bytes per pixel. This value estimates the max number of bytes of
  // the physical screen's uncompressed bitmap.
  size_t display_size_in_bytes = 0;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    display_size_in_bytes =
        std::max(display_size_in_bytes,
                 static_cast<size_t>(4 * display.GetSizeInPixel().Area64()));
  }

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

// static
bool NavigationTransitionConfig::ShouldCompressScreenshotWhenQuiet() {
  return kCompressScreenshotWhenQuiet.Get();
}

// static
base::TimeDelta NavigationTransitionConfig::ScreenshotSendResultDelay() {
  return base::Milliseconds(kScreenshotSendResultDelayMs.Get());
}

}  // namespace content
