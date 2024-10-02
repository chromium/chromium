// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_DATA_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_DATA_H_

#include <optional>

#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {

// Holds the relevant information about a navigation transition. Just like the
// `NavigationEntryScreenshot`, this struct is not persistent on the
// `NavigationEntry` (i.e. can't be restored).
class NavigationTransitionData {
 public:
  // Used for recording UMA for cache hit/miss.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CacheHitOrMissReason {
    // The screenshot is captured and placed in the cache.
    kCacheHit = 0,

    // Sent a request to capture a screenshot in
    // `CaptureNavigationEntryScreenshotForCrossDocumentNavigations`.
    kSentScreenshotRequest = 1,

    // Received an empty bitmap when capturing the screenshot.
    kCapturedEmptyBitmap = 2,

    // [DEPRECATED] Screenshot is not captured for subframes.
    // kCacheMissSubframe = 3,

    // Screenshot was evicted because of memory constraints.
    kCacheMissEvicted = 4,

    // Screenshot was purged along with all other screenshots because of memory
    // pressure.
    kCacheMissPurgedMemoryPressure = 5,

    // Screenshot is not available because the app was restarted/killed in the
    // background and we don't persist them to disk.
    // TODO(baranerf): Implement tracking of this case.
    kCacheMissColdStart = 6,

    // Screenshot is not captured for cloned navigations.
    kCacheMissClonedNavigationEntry = 7,

    // Screenshot is not captured for embedded pages.
    kCacheMissEmbeddedPages = 8,

    // Screenshot is not captured since the page has opted-out of BFCache.
    // Cache-Control: no-store
    kCacheMissCCNS = 9,

    // Screenshot was evicted because the tab was invisible for a long duration.
    kCacheMissInvisible = 10,

    // Screenshot was not captured because user had prefers-reduced-motion
    // turned on when the navigation committed.
    kCacheMissPrefersReducedMotion = 11,

    // Screenshot is not displayed since it is captured in a different
    // orientation (horizontal or vertical) compared to the current screen
    // orientation.
    kCacheMissScreenshotOrientation = 12,

    // Screenshot is not captured when the page is crashed.
    kNavigateAwayFromCrashedPage = 13,
    kNavigateAwayFromCrashedPageNoEarlySwap = 14,

    // Screenshot is not captured when the root window or compositor is
    // detached.
    kNoRootWindowOrCompositor = 15,

    // The browser isn't embedding a valid `viz::LocalSurfaceID` when we try
    // to capture the screenshot from the browser.
    kBrowserNotEmbeddingValidSurfaceId = 16,

    // We only cache screenshots for navigations targeting the primary main
    // frame.
    kCacheMissNonPrimaryMainFrame = 17,

    kMaxValue = kCacheMissNonPrimaryMainFrame
  };

  NavigationTransitionData() = default;
  ~NavigationTransitionData() = default;
  NavigationTransitionData(NavigationTransitionData&&) = delete;
  NavigationTransitionData& operator=(NavigationTransitionData&&) = default;
  NavigationTransitionData(const NavigationTransitionData&) = delete;
  NavigationTransitionData& operator=(const NavigationTransitionData&) = delete;

  void SetSameDocumentNavigationEntryScreenshotToken(
      const std::optional<blink::SameDocNavigationScreenshotDestinationToken>&
          token);

  const std::optional<blink::SameDocNavigationScreenshotDestinationToken>&
  same_document_navigation_entry_screenshot_token() const {
    return same_document_navigation_entry_screenshot_token_;
  }

  void set_is_copied_from_embedder(bool is_copied_from_embedder) {
    is_copied_from_embedder_ = is_copied_from_embedder;
  }
  bool is_copied_from_embedder() const { return is_copied_from_embedder_; }

  void set_cache_hit_or_miss_reason(
      std::optional<CacheHitOrMissReason> cache_hit_or_miss_reason) {
    cache_hit_or_miss_reason_ = cache_hit_or_miss_reason;
  }
  std::optional<CacheHitOrMissReason> cache_hit_or_miss_reason() const {
    return cache_hit_or_miss_reason_;
  }

  int copy_output_request_sequence() const {
    return copy_output_request_sequence_number_;
  }
  void increment_copy_output_request_sequence() {
    ++copy_output_request_sequence_number_;
  }

  const SkBitmap& favicon() const { return favicon_; }
  void set_favicon(const SkBitmap& favicon) { favicon_ = favicon; }

 private:
  // Whether this screenshot is supplied by the embedder.
  bool is_copied_from_embedder_ = false;

  // Used to map a screenshot for the last frame of this navigation entry
  // captured in Viz and sent back to the browser process. The token is set when
  // `DidCommitSameDocumentNavigation` is received in the browser process from
  // the renderer; and reset when its corresponding screenshot is received by
  // the browser process from Viz.
  std::optional<blink::SameDocNavigationScreenshotDestinationToken>
      same_document_navigation_entry_screenshot_token_;

  // Tracks copy output requests sent for this navigation entry. The embedder
  // increments this number when the entry is committed so that the results of
  // stale copy output requests will be ignored, preventing them from
  // interfering with future requests.
  int copy_output_request_sequence_number_ = 0;

  // TODO(https://crbug.com/40262175): We might want to move the
  // `NavigationEntryScreenshot` here as well when we make the screenshot
  // disk-persistent.

  // Used to record UMA in `BackForwardTransitionAnimator`
  std::optional<CacheHitOrMissReason> cache_hit_or_miss_reason_;

  // The favicon used to compose the fallback UX.
  SkBitmap favicon_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_DATA_H_
