// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"

#include "base/memory/ptr_util.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"

namespace content {

namespace {
std::unique_ptr<NavigationEntryScreenshot> RemoveScreenshotFromEntry(
    NavigationEntry* entry) {
  CHECK(entry);
  std::unique_ptr<base::SupportsUserData::Data> data =
      entry->TakeUserData(NavigationEntryScreenshot::kUserDataKey);
  CHECK(data);
  auto* screenshot = static_cast<NavigationEntryScreenshot*>(data.release());
  CHECK(screenshot->is_cached());
  screenshot->set_cache(nullptr);
  return base::WrapUnique(screenshot);
}

}  // namespace

bool AreBackForwardTransitionsEnabled() {
  // TODO(https://crbug.com/1414164): We might want to disable this feature on
  // low-end devices.
  return base::FeatureList::IsEnabled(features::kBackForwardTransitions);
}

NavigationEntryScreenshotCache::NavigationEntryScreenshotCache(
    base::SafeRef<NavigationEntryScreenshotManager> manager,
    NavigationControllerImpl* nav_controller)
    : manager_(manager), nav_controller_(nav_controller) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(AreBackForwardTransitionsEnabled());
}

NavigationEntryScreenshotCache::~NavigationEntryScreenshotCache() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Purge();
}

void NavigationEntryScreenshotCache::SetScreenshot(
    NavigationEntry* entry,
    std::unique_ptr<NavigationEntryScreenshot> screenshot) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // We must be caching screenshot for a valid entry.
  CHECK(entry);
  // A navigation entry without a screenshot will be removed from the cache
  // first (thus not tracked). Impossible to overwrite for a cached entry.
  CHECK(!entry->GetUserData(NavigationEntryScreenshot::kUserDataKey));
  CHECK(cached_screenshots_.find(entry->GetUniqueID()) ==
        cached_screenshots_.end());
  CHECK(!screenshot->is_cached());
  screenshot->set_cache(this);
  const size_t size = screenshot->SizeInBytes();
  entry->SetUserData(NavigationEntryScreenshot::kUserDataKey,
                     std::move(screenshot));
  cached_screenshots_.insert(entry->GetUniqueID());
  manager_->OnScreenshotCached(this, size);

  if (new_screenshot_cached_callback_) {
    std::move(new_screenshot_cached_callback_).Run(entry->GetUniqueID());
  }
}

std::unique_ptr<NavigationEntryScreenshot>
NavigationEntryScreenshotCache::RemoveScreenshot(
    NavigationEntry* navigation_entry) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(navigation_entry);
  const int navigation_entry_id = navigation_entry->GetUniqueID();
  auto it = cached_screenshots_.find(navigation_entry_id);
  // `CHECK_NE` is not compatible with `base::flat_set`.
  CHECK(it != cached_screenshots_.end());

  // Remove the tracked nav entry id and the entry and update the metadata.
  cached_screenshots_.erase(it);
  auto screenshot = RemoveScreenshotFromEntry(navigation_entry);
  manager_->OnScreenshotRemoved(this, screenshot->SizeInBytes());
  return screenshot;
}

void NavigationEntryScreenshotCache::OnNavigationEntryGone(
    int navigation_entry_id,
    size_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = cached_screenshots_.find(navigation_entry_id);
  CHECK(it != cached_screenshots_.end());
  cached_screenshots_.erase(it);
  manager_->OnScreenshotRemoved(this, size);
}

void NavigationEntryScreenshotCache::EvictScreenshotsUntilUnderBudgetOrEmpty() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  CHECK(!IsEmpty());

  CHECK_GT(manager_->GetCurrentCacheSize(), manager_->GetMaxCacheSize());

  const int current_index = nav_controller_->GetCurrentEntryIndex();
  const int current_entry_id =
      nav_controller_->GetEntryAtIndex(current_index)->GetUniqueID();
  // It's impossible to have a screenshot for the current entry.
  CHECK(!cached_screenshots_.contains(current_entry_id));
  // Impossible to have just one entry (the current entry).
  CHECK_GT(nav_controller_->GetEntryCount(), 1);

  int distance_to_leftmost = current_index;
  int distance_to_rightmost =
      nav_controller_->GetEntryCount() - current_index - 1;

  // The eviction strategy is to priotrize keeping the screenshots for the
  // navigation entries that are closer to the "current entry" (last committed).
  // This strategy assumes the user is equally likely to go back/forward. This
  // is not true for Android where native OS gesture navigation only takes the
  // user back (even right-edge swipe).
  //
  // TODO(crbug.com/1415332): Iterate on the eviction strategy based on metrics
  // when this launches.
  //
  // Ex: [3, 4&, 5*, 6&, 7, 8&], where "*" means the last committed entry and
  // "&" means an entry with a screenshot. In this case `distance_to_leftmost` =
  // 2 and `distance_to_rightmost` = 3. The eviction order will be: entry8,
  // entry6 and entry4.
  //
  while (manager_->GetCurrentCacheSize() > manager_->GetMaxCacheSize() &&
         !IsEmpty()) {
    int candidate_nav_entry_id = -1;
    CHECK(distance_to_leftmost > 0 || distance_to_rightmost > 0);
    if (distance_to_leftmost > distance_to_rightmost) {
      candidate_nav_entry_id =
          nav_controller_->GetEntryAtIndex(current_index - distance_to_leftmost)
              ->GetUniqueID();
      --distance_to_leftmost;
    } else {
      candidate_nav_entry_id =
          nav_controller_
              ->GetEntryAtIndex(current_index + distance_to_rightmost)
              ->GetUniqueID();
      --distance_to_rightmost;
    }
    // Check whether this candidate entry has a screenshot to remove, or
    // continue to move closer to the current entry.
    auto* candidate_entry =
        nav_controller_->GetEntryWithUniqueID(candidate_nav_entry_id);
    CHECK(candidate_entry);
    if (auto it = cached_screenshots_.find(candidate_nav_entry_id);
        it != cached_screenshots_.end()) {
      std::unique_ptr<NavigationEntryScreenshot> evicted_screenshot =
          RemoveScreenshotFromEntry(candidate_entry);
      cached_screenshots_.erase(it);
      CHECK_LE(evicted_screenshot->SizeInBytes(),
               manager_->GetCurrentCacheSize());
      manager_->OnScreenshotRemoved(this, evicted_screenshot->SizeInBytes());
    }
  }
}

void NavigationEntryScreenshotCache::Purge() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = cached_screenshots_.begin();
  while (!IsEmpty()) {
    auto* evicted_entry = nav_controller_->GetEntryWithUniqueID(*it);
    CHECK(evicted_entry);
    auto purged = RemoveScreenshotFromEntry(evicted_entry);
    cached_screenshots_.erase(it);
    CHECK_LE(purged->SizeInBytes(), manager_->GetCurrentCacheSize());
    manager_->OnScreenshotRemoved(this, purged->SizeInBytes());
    it = cached_screenshots_.begin();
  }
}

bool NavigationEntryScreenshotCache::IsEmpty() const {
  return cached_screenshots_.empty();
}

void NavigationEntryScreenshotCache::SetNewScreenshotCachedCallbackForTesting(
    NewScreenshotCachedCallbackForTesting callback) {
  CHECK(!new_screenshot_cached_callback_);
  new_screenshot_cached_callback_ = std::move(callback);
}

}  // namespace content
