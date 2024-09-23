// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_config.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_utils.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/browser_metrics.h"
#include "ui/display/screen.h"

namespace content {

NavigationEntryScreenshotManager::NavigationEntryScreenshotManager()
    :  // `NO_AUTO_EVICT` since we want to manually limit the global cache size
       // by the number of bytes of the thumbnails, rather than the number of
       // entries in the cache.
      managed_caches_(base::LRUCacheSet<int>::NO_AUTO_EVICT),
      tick_clock_(base::DefaultTickClock::GetInstance()),
      cleanup_delay_(
          NavigationTransitionConfig::GetCleanupDelayForInvisibleCaches()) {
  CHECK(NavigationTransitionConfig::AreBackForwardTransitionsEnabled());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  max_cache_size_in_bytes_ =
      NavigationTransitionConfig::ComputeCacheSizeInBytes();
  listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&NavigationEntryScreenshotManager::OnMemoryPressure,
                          base::Unretained(this)));

  // Start recording memory usage.
  RecordScreenshotCacheSizeAfterDelay();
}

NavigationEntryScreenshotManager::~NavigationEntryScreenshotManager() = default;

void NavigationEntryScreenshotManager::OnScreenshotCached(
    NavigationEntryScreenshotCacheEvictor* cache,
    size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (managed_caches_.Get(cache) == managed_caches_.end()) {
    Register(cache);
  }
  // We shouldn't be able to capture anything greater than the budget.
  CHECK_LE(size, max_cache_size_in_bytes_);
  current_cache_size_in_bytes_ += size;

  EvictIfOutOfMemoryBudget();
}

void NavigationEntryScreenshotManager::OnScreenshotRemoved(
    NavigationEntryScreenshotCacheEvictor* cache,
    size_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!IsEmpty());
  auto it = managed_caches_.Get(cache);
  CHECK(it != managed_caches_.end());

  CHECK_GE(current_cache_size_in_bytes_, size);
  current_cache_size_in_bytes_ -= size;

  if (cache->IsEmpty()) {
    Unregister(cache);
  }
}

void NavigationEntryScreenshotManager::OnScreenshotCompressed(
    NavigationEntryScreenshotCacheEvictor* cache,
    size_t old_size,
    size_t new_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(!IsEmpty());
  CHECK(managed_caches_.Peek(cache) != managed_caches_.end());
  CHECK_GE(current_cache_size_in_bytes_, old_size);

  current_cache_size_in_bytes_ -= old_size;
  current_cache_size_in_bytes_ += new_size;
}

void NavigationEntryScreenshotManager::OnVisibilityChanged(
    NavigationEntryScreenshotCacheEvictor* cache) {
  if (cache->IsEmpty()) {
    CHECK(managed_caches_.Peek(cache) == managed_caches_.end());
    return;
  }

  auto last_visible_time = cache->GetLastVisibleTime();
  if (last_visible_time && !cleanup_task_.IsRunning()) {
    ScheduleCleanup(*last_visible_time);
  }
}

bool NavigationEntryScreenshotManager::IsEmpty() const {
  CHECK(managed_caches_.empty() == (current_cache_size_in_bytes_ == 0U));
  return managed_caches_.empty();
}

void NavigationEntryScreenshotManager::Register(
    NavigationEntryScreenshotCacheEvictor* cache) {
  CHECK(managed_caches_.Peek(cache) == managed_caches_.end());
  managed_caches_.Put(std::move(cache));
  OnVisibilityChanged(cache);
}

void NavigationEntryScreenshotManager::Unregister(
    NavigationEntryScreenshotCacheEvictor* cache) {
  auto it = managed_caches_.Peek(cache);
  CHECK(it != managed_caches_.end());
  managed_caches_.Erase(it);
}

base::TimeTicks NavigationEntryScreenshotManager::Now() const {
  return tick_clock_->NowTicks();
}

void NavigationEntryScreenshotManager::ScheduleCleanup(
    base::TimeTicks last_visible_time) {
  const auto delay = cleanup_delay_ - (Now() - last_visible_time);
  auto callback = base::BindOnce(&NavigationEntryScreenshotManager::RunCleanup,
                                 weak_factory_.GetWeakPtr());
  cleanup_task_.Start(FROM_HERE, delay, std::move(callback));
}

void NavigationEntryScreenshotManager::RunCleanup() {
  std::optional<base::TimeTicks> earliest_last_visible_time;
  const base::TimeTicks now = Now();

  auto it = managed_caches_.begin();
  while (it != managed_caches_.end()) {
    auto* cache = *it;
    it++;

    const auto last_visible_time = cache->GetLastVisibleTime();
    if (!last_visible_time) {
      continue;
    }

    CHECK_LE(*last_visible_time, now);
    if (now - *last_visible_time >= cleanup_delay_) {
      cache->Purge(
          NavigationEntryScreenshotCacheEvictor::PurgeReason::kInvisible);
      CHECK(cache->IsEmpty());
      CHECK(managed_caches_.Peek(cache) == managed_caches_.end());
    } else if (!earliest_last_visible_time) {
      earliest_last_visible_time = *last_visible_time;
    } else {
      earliest_last_visible_time =
          std::min(*earliest_last_visible_time, *last_visible_time);
    }
  }

  if (earliest_last_visible_time) {
    ScheduleCleanup(*earliest_last_visible_time);
  }
}

// The current implementation iterates through the tabs in the LRU order and
// evict screenshots from each tab, until either the global size satisfies the
// budget, or the cache of the current tab is empty.
//
// One alternative is to always evict the navigation entries in LRU order,
// regardless of which tab the entry is from. The pro of this alternative is to
// have all the eviction logic inside the global manager.
void NavigationEntryScreenshotManager::EvictIfOutOfMemoryBudget() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!IsEmpty());
  // Start with the least recently used.
  // TODO(khushalsagar): Evict from invisible caches first?
  auto it = managed_caches_.rbegin();
  while (current_cache_size_in_bytes_ > max_cache_size_in_bytes_) {
    CHECK(it != managed_caches_.rend());
    auto* cache = *it;
    cache->EvictScreenshotsUntilUnderBudgetOrEmpty();
    CHECK(cache->IsEmpty() ||
          current_cache_size_in_bytes_ <= max_cache_size_in_bytes_);
    if (cache->IsEmpty()) {
      // No need to unregister this cache -
      // `EvictScreenshotsUntilUnderBudgetOrEmpty` takes care of that.
      CHECK(managed_caches_.Peek(cache) == managed_caches_.end());
      it = managed_caches_.rbegin();
    }
  }
}

void NavigationEntryScreenshotManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (memory_pressure_level !=
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }
  // Using a while loop because `Purge` erases the iterator.
  auto it = managed_caches_.begin();
  while (it != managed_caches_.end()) {
    auto* cache = *it;
    cache->Purge(
        NavigationEntryScreenshotCacheEvictor::PurgeReason::kMemoryPressure);
    CHECK(cache->IsEmpty());
    it = managed_caches_.begin();
  }
  CHECK(IsEmpty());
}

void NavigationEntryScreenshotManager::RecordScreenshotCacheSizeAfterDelay() {
  GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &NavigationEntryScreenshotManager::RecordScreenshotCacheSize,
          weak_factory_.GetWeakPtr()),
      memory_instrumentation::GetDelayForNextMemoryLog());
}

void NavigationEntryScreenshotManager::RecordScreenshotCacheSize() {
  MEMORY_METRICS_HISTOGRAM_MB(
      "Navigation.GestureTransition.ScreenshotCacheSize",
      current_cache_size_in_bytes_ / (1024 * 1024));
  RecordScreenshotCacheSizeAfterDelay();
}

}  // namespace content
