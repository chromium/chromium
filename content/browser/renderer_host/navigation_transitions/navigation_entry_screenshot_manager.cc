// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"

#include "base/memory/ptr_util.h"
#include "base/system/sys_info.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "ui/display/screen.h"

namespace content {

namespace {
// TODO(https://crbug.com/1414164): Consult with Clank team to see if we have
// any metrics for this.
#if BUILDFLAG(IS_ANDROID)
constexpr static size_t kMaxNumThumbnails = 20U;
#else
constexpr static size_t kMaxNumThumbnails = 0U;
#endif

// TODO(https://crbug.com/1414164): Optimise the memory budget. This is fine for
// MVP, but we need to consult the Clank team for a more propriate budget size.
static size_t GetMemoryBudget() {
  // Assume 4 bytes per pixel. This value estimates the max number of bytes of
  // the physical screen's bitmap.
  const static size_t kDisplaySizeInBytes = 4 * display::Screen::GetScreen()
                                                    ->GetPrimaryDisplay()
                                                    .GetSizeInPixel()
                                                    .Area64();
  size_t physical_memory_budget = 0U;
#if BUILDFLAG(IS_ANDROID)
  if (base::SysInfo::IsLowEndDevice()) {
    // 64MB.
    physical_memory_budget = 64 * 1024 * 1024;
  } else {
    // For 8GB of RAM, this is ~ 215MB.
    physical_memory_budget = base::SysInfo::AmountOfPhysicalMemory() / 40;
  }
#endif
  // We should at least be able to cache one thumbnail.
  physical_memory_budget =
      std::max(kDisplaySizeInBytes, physical_memory_budget);
  return std::min(kDisplaySizeInBytes * kMaxNumThumbnails,
                  physical_memory_budget);
}
}  // namespace

NavigationEntryScreenshotManager::NavigationEntryScreenshotManager()
    :  // `NO_AUTO_EVICT` since we want to manually limit the global cache size
       // by the number of bytes of the thumbnails, rather than the number of
       // entries in the cache.
      managed_caches_(base::LRUCacheSet<int>::NO_AUTO_EVICT) {
  CHECK(AreBackForwardTransitionsEnabled());
  max_cache_size_in_bytes_ = GetMemoryBudget();
  listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE,
      base::BindRepeating(&NavigationEntryScreenshotManager::OnMemoryPressure,
                          base::Unretained(this)));
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

void NavigationEntryScreenshotManager::OnCacheBecameVisible(
    NavigationEntryScreenshotCacheEvictor* cache) {
  // Access the cache in the set to mark it as recently used.
  auto it = managed_caches_.Get(cache);
  CHECK(it != managed_caches_.end());
}

bool NavigationEntryScreenshotManager::IsEmpty() const {
  CHECK(managed_caches_.empty() == (current_cache_size_in_bytes_ == 0U));
  return managed_caches_.empty();
}

void NavigationEntryScreenshotManager::Register(
    NavigationEntryScreenshotCacheEvictor* cache) {
  CHECK(managed_caches_.Peek(cache) == managed_caches_.end());
  managed_caches_.Put(std::move(cache));
}

void NavigationEntryScreenshotManager::Unregister(
    NavigationEntryScreenshotCacheEvictor* cache) {
  auto it = managed_caches_.Peek(cache);
  CHECK(it != managed_caches_.end());
  managed_caches_.Erase(it);
}

// The current implementation iterates through the tabs in the LRU order and
// evict screenshots from each tab, until either the global size satisfies the
// budget, or the cache of the current tab is empty.
//
// One alternative is to always evict the navigation entries in LRU order,
// regardless of which tab the entry is from. The pro of this alternative is to
// have all the eviction logic inside the global manager.
//
// TODO(https://crbug.com/1420998): We need some metrics to understand if the
// currently implementation affects the cache hit rate. (I.e., would the
// alternative be a better approach?)
void NavigationEntryScreenshotManager::EvictIfOutOfMemoryBudget() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!IsEmpty());
  // Start with the least recently used.
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
    cache->Purge();
    CHECK(cache->IsEmpty());
    it = managed_caches_.begin();
  }
  CHECK(IsEmpty());
}

}  // namespace content
