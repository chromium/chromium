// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"

#include "base/android/jni_callback.h"
#include "base/auto_reset.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/display/screen.h"

namespace content {
namespace {

const int kMaxScreenshotCount = 20;

static int g_min_required_physical_ram_mb = 7200;

const double kPercentageOfRamToUse = 0.5;

const base::TimeDelta kInvisibleCacheCleanupDelay = base::Minutes(7);

// SendResult is an expensive operation and the start of a navigation is a busy
// time. Delaying SendResult reduces chances of contention.
// The value can be based on human reaction times and LCP latencies and it can
// be adjusted based on the incidence of the value SentScreenshotRequest in
// Navigation.GestureTransition.CacheHitOrMissReason.
const base::TimeDelta kScreenshotSendResultDelay = base::Milliseconds(400);

size_t GetMaxCacheSizeInBytes() {
  constexpr int kLowEndMax = 32 * 1024 * 1024;  // 32MB
  constexpr int kOtherMax = 128 * 1024 * 1024;  // 128MB
  return base::SysInfo::IsLowEndDevice() ? kLowEndMax : kOtherMax;
}

}  // namespace

// static
bool NavigationTransitionConfig::SupportsBackForwardTransitions(
    base::PassKey<ContentBrowserClient>) {
  return base::SysInfo::AmountOfPhysicalMemory().InMiB() >=
         g_min_required_physical_ram_mb;
}

// static
size_t NavigationTransitionConfig::ComputeCacheSizeInBytes() {
  // TODO(crbug.com/429140103): Convert the return type to ByteCount.

  // Assume 4 bytes per pixel. This value estimates the max number of bytes of
  // the physical screen's uncompressed bitmap.
  // Assume one pixel for unit tests that don't have or need a screen.
  size_t display_size_in_bytes = 4;
  if (auto* screen = display::Screen::Get(); screen) {
    for (const auto& display : display::Screen::Get()->GetAllDisplays()) {
      display_size_in_bytes =
          std::max(display_size_in_bytes,
                   static_cast<size_t>(4 * display.GetSizeInPixel().Area64()));
    }
  }

  size_t memory_required_for_max_screenshots =
      display_size_in_bytes * kMaxScreenshotCount;

  size_t physical_memory_budget =
      (base::SysInfo::AmountOfPhysicalMemory().InBytes() *
       kPercentageOfRamToUse) /
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
  return kInvisibleCacheCleanupDelay;
}

// static
base::TimeDelta NavigationTransitionConfig::ScreenshotSendResultDelay() {
  return kScreenshotSendResultDelay;
}

// static
base::AutoReset<int>
NavigationTransitionConfig::SetMinRequiredPhysicalRamMbForTesting(int mb) {
  return base::AutoReset<int>(&g_min_required_physical_ram_mb, mb);
}

}  // namespace content
