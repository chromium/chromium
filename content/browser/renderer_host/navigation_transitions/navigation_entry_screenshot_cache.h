// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_CACHE_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_CACHE_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/safe_ref.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_manager.h"
#include "content/common/content_export.h"

namespace content {

class NavigationEntry;
class NavigationEntryScreenshot;
class NavigationEntryScreenshotManager;

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
  enum class PurgeReason { kMemoryPressure, kInvisible };
  virtual void Purge(PurgeReason reason) = 0;

  virtual bool IsEmpty() const = 0;

  // Returns the time when this cache's tab was last visible or null if it is
  // currently visible.
  virtual std::optional<base::TimeTicks> GetLastVisibleTime() const = 0;
};

// `NavigationEntryScreenshotCache` tracks `NavigationEntryScreenshot`s per
// `FrameTree`. It is owned by the `NavigationController` of the primary
// `FrameTree` of a `WebContents`.
class CONTENT_EXPORT NavigationEntryScreenshotCache
    : public NavigationEntryScreenshotCacheEvictor {
 public:
  using CompressedCallback = base::OnceCallback<void(int nav_entry_index)>;
  static void SetCompressedCallbackForTesting(CompressedCallback callback);

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
  void SetScreenshot(base::WeakPtr<NavigationRequest> navigation_request,
                     std::unique_ptr<NavigationEntryScreenshot> screenshot,
                     bool is_copied_from_embedder);

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
  void OnNavigationEntryGone(int navigation_entry_id);

  // Called by `NavigationScreenshot` when the cached screenshot has been
  // compressed.
  void OnScreenshotCompressed(int navigation_entry_id, size_t new_size);

  // Called when a navigation request has finished.
  void OnNavigationFinished(const NavigationRequest& navigation_request);

  // Called when the visibility of the cache changes.
  void SetVisible(bool visible);

  // `NavigationEntryScreenshotCacheEvictor`:
  //
  // The cost of `EvictScreenshotsUntilUnderBudgetOrEmpty` and
  // `PurgeForMemoryPressure` is linear with respect to the number of navigation
  // entries in the primary `NavigationController`. This is because for each
  // navigation entry's ID this cache tracks, we need to query
  // `NavigationController` using
  // `NavigationControllerImpl::GetEntryWithUniqueID`, which performs a linear
  // scan on all the navigation entries.
  void EvictScreenshotsUntilUnderBudgetOrEmpty() override;
  void Purge(PurgeReason reason) override;
  bool IsEmpty() const override;
  std::optional<base::TimeTicks> GetLastVisibleTime() const override;

  // Allows the browsertests to be notified when a screenshot is cached.
  using NewScreenshotCachedCallbackForTesting = base::OnceCallback<void(int)>;
  void SetNewScreenshotCachedCallbackForTesting(
      NewScreenshotCachedCallbackForTesting callback);

 private:
  void SetScreenshotInternal(
      std::unique_ptr<NavigationEntryScreenshot> screenshot,
      bool is_copied_from_embedder);

  // Helper function to differentiate between purge because of memory pressure
  // and purge called by the destructor.
  void PurgeInternal(std::optional<PurgeReason> reason);

  // Tracks the unique IDs of the navigation entries, for which we have captured
  // screenshots, and the screenshot size in bytes.
  base::flat_map<int, size_t> cached_screenshots_;

  // Tracks the set of screenshots for ongoing navigations. These screenshots
  // are either added to `cached_screenshots_` or discarded when the navigation
  // finishes.
  struct PendingScreenshot {
    PendingScreenshot();
    PendingScreenshot(std::unique_ptr<NavigationEntryScreenshot> screenshot,
                      bool is_copied_from_embedder);
    ~PendingScreenshot();

    PendingScreenshot(PendingScreenshot&& other);
    PendingScreenshot& operator=(PendingScreenshot&& other);

    std::unique_ptr<NavigationEntryScreenshot> screenshot;
    bool is_copied_from_embedder;
  };
  base::flat_map<int64_t, PendingScreenshot> pending_screenshots_;

  // The per-BrowserContext manager that manages the eviction. Guaranteed to
  // outlive `this`.
  base::SafeRef<NavigationEntryScreenshotManager> manager_;

  // The `NavigationController` that owns `this`. Guaranteed to outlive `this`.
  // We need the controller for cache eviction to query all the
  // `NavigationEntry`. See the impl for `EvictScreenshotsUntilInBudgetOrEmpty`.
  const raw_ptr<NavigationControllerImpl> nav_controller_;

  // The last time this cache was visible or null if its currently visible.
  std::optional<base::TimeTicks> last_visible_timestamp_;

  NewScreenshotCachedCallbackForTesting new_screenshot_cached_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_CACHE_H_
