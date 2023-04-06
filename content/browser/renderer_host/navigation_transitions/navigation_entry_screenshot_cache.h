// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_CACHE_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_CACHE_H_

#include "base/containers/flat_set.h"
#include "base/memory/safe_ref.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"
#include "content/common/content_export.h"

namespace content {

class NavigationEntry;
class NavigationEntryScreenshot;
class NavigationEntryScreenshotManager;

bool AreBackForwardTransitionsEnabled();

// This interface limits the access of the `NavigationEntryScreenshotManager` to
// the `NavigationEntryScreenshotCache`: we do not want the manager to
// accidentally perform any "Set" or "Take" operations on the cache. This is
// because the manager is owned by the `BrowserContext` who has access across
// the tabs, and we do not want any tab specific pixel data leaked across tabs.
class NavigationEntryScreenshotCacheEvictor {
 public:
  virtual ~NavigationEntryScreenshotCacheEvictor() = default;

  // Starting from the most distant entry of the last committed entry, erase the
  // screenshots from entries until either the memory watermark is below the
  // budget, or until no screenshots are tracked in this cache (this cache is
  // empty).
  virtual void EvictScreenshotsUntilUnderBudgetOrEmpty() = 0;

  // Deletes all the tracked screenshots in this cache, and notifies the global
  // manager to stop tracking this cache.
  virtual void Purge() = 0;

  virtual bool IsEmpty() const = 0;
};

// `NavigationEntryScreenshotCache` tracks `NavigationEntryScreenshot`s per
// `FrameTree`. It is owned by the `NavigationController` of the primary
// `FrameTree` of a `WebContents`.
class CONTENT_EXPORT NavigationEntryScreenshotCache
    : public NavigationEntryScreenshotCacheEvictor {
 public:
  explicit NavigationEntryScreenshotCache(
      base::SafeRef<NavigationEntryScreenshotManager> manager,
      NavigationControllerImpl* nav_controller);
  NavigationEntryScreenshotCache(const NavigationEntryScreenshotCache&) =
      delete;
  NavigationEntryScreenshotCache& operator=(
      const NavigationEntryScreenshotCache&) = delete;
  ~NavigationEntryScreenshotCache() override;

  // Used to assign a `NavigationEntryScreenshot` to a `NavigationEntry`, which
  // will own it. Also tracks the screenshot within this cache and notifies the
  // `NavigationEntryScreenshotManager` of size changes in case eviction is
  // needed.
  void SetScreenshot(NavigationEntry* navigation_entry,
                     std::unique_ptr<NavigationEntryScreenshot> screenshot);

  // Removes the `NavigationEntryScreenshot` from `NavigationEntry` and
  // transfers ownership to the caller, updating the relevant tracking in the
  // `NavigationEntryScreenshotManager`. The transfer of ownership is necessary
  // so that eviction does not occur while a screenshot is in use for an
  // animation. The caller is responsible for making sure `navigation_entry`
  // has a screenshot.
  std::unique_ptr<NavigationEntryScreenshot> RemoveScreenshot(
      NavigationEntry* navigation_entry);

  // Called by the `NavigationScreenshot` when the hosting navigation entry is
  // deleted.
  void OnNavigationEntryGone(int navigation_entry_id, size_t size);

  // `NavigationEntryScreenshotCacheEvictor`:
  //
  // The cost of `EvictScreenshotsUntilUnderBudgetOrEmpty` and `Purge` is linear
  // with respect to the number of navigation entries in the primary
  // `NavigationController`. This is because for each navigation entry's ID this
  // cache tracks, we need to query `NavigationController` using
  // `NavigationControllerImpl::GetEntryWithUniqueID`, which performs a linear
  // scan on all the navigation entries.
  void EvictScreenshotsUntilUnderBudgetOrEmpty() override;
  void Purge() override;
  bool IsEmpty() const override;

 private:
  // Tracks the unique IDs of the navigation entries, for which we have captured
  // screenshots.
  base::flat_set<int> cached_screenshots_;

  // The per-BrowserContext manager that manages the eviction. Guaranteed to
  // outlive `this`.
  base::SafeRef<NavigationEntryScreenshotManager> manager_;

  // The `NavigationController` that owns `this`. Guaranteed to outlive `this`.
  // We need the controller for cache eviction to query all the
  // `NavigationEntry`. See the impl for `EvictScreenshotsUntilInBudgetOrEmpty`.
  const raw_ptr<NavigationControllerImpl> nav_controller_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_CACHE_H_
