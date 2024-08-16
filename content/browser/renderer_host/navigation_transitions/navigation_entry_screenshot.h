// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_H_

#include "base/supports_user_data.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "content/common/content_export.h"

class SkBitmap;

namespace content {

class NavigationEntryScreenshotCache;

// Wraps around a `cc::UIResourceBitmap`, which is used to show the user a
// preview of the previous page. This class is stored as user data on
// `NavigationEntry`.
//
// The screenshot is captured for the leaving page when the navigation is about
// to commit (see `CommitDeferringCondition`), subsequently stashed into the
// `NavigationEntry` that this screenshot is captured for. The capture is done
// in the browser process. The pixel data includes sensitive cross-origin data,
// so it must never be leaked to a renderer process.
//
// The screenshot is taken out of the `NavigationEntry` when it will be used for
// an animated transition for a gestured navigation.
//   - If the screenshot ends up being used, or deemed invalid (i.e. mismatches
//   with the current viewport size) for a preview, the caller is responsible
//   for destroying the screenshot.
//   - If the screenshot is not used for a preview but still valid (e.g. user
//   gesture cancels the animation thus no navigation, or the user initiates a
//   gesture to go back to multiple entries), the caller is responsible for
//   putting the screenshot back into the `NavigationEntryScreenshotCache`.
//
// If the user clears the navigation history, the screenshot is deleted when
// its owning `NavigationEntry` is destroyed. The screenshot is never recreated
// or cloned even when its `NavigationEntry` is cloned (tab clone) or restored
// (i.e., by restoring the last closed tab), because
// `base::SupportsUserData::Data::Clone()` is not implemented by
// `NavigationEntryScreenshot`.
class CONTENT_EXPORT NavigationEntryScreenshot
    : public cc::UIResourceClient,
      public base::SupportsUserData::Data {
 public:
  const static void* const kUserDataKey;

  static void SetDisableCompressionForTesting(bool disable);

  NavigationEntryScreenshot(const SkBitmap& bitmap,
                            int navigation_entry_id,
                            bool supports_etc_non_power_of_two);
  NavigationEntryScreenshot(const NavigationEntryScreenshot&) = delete;
  NavigationEntryScreenshot& operator=(const NavigationEntryScreenshot&) =
      delete;
  ~NavigationEntryScreenshot() override;

  // `cc::UIResourceClient`:
  cc::UIResourceBitmap GetBitmap(cc::UIResourceId uid,
                                 bool resource_lost) override;

  // Sets the `cache` managing the memory for this screenshot. When set, the
  // screenshot is stored on its associated NavigationEntry and is guaranteed to
  // not be displayed in the UI.
  //
  // Returns the memory occupied by the bitmap in bytes.
  size_t SetCache(NavigationEntryScreenshotCache* cache);

  // Returns true if the screenshot is being managed by a cache. This is not the
  // case when it's being displayed in the UI.
  bool is_cached() const { return cache_ != nullptr; }

  // Returns the bounds of the uncompressed bitmap.
  gfx::Size dimensions_without_compression() const {
    return dimensions_without_compression_;
  }

  int navigation_entry_id() const { return navigation_entry_id_; }

  SkBitmap GetBitmapForTesting() const;
  size_t CompressedSizeForTesting() const;

 private:
  void OnCompressionFinished(sk_sp<SkPixelRef> compressed_bitmap);

  void StartCompression(const SkBitmap& bitmap,
                        bool supports_etc_non_power_of_two);
  const cc::UIResourceBitmap& GetBitmap() const;

  // The uncompressed bitmap cached when navigating away from this navigation
  // entry.
  std::optional<cc::UIResourceBitmap> bitmap_;

  // The compressed bitmap generated on a worker thread. `bitmap_` is discarded
  // when the compressed bitmap is available and this screenshot is no longer
  // being displayed in the UI.
  std::optional<cc::UIResourceBitmap> compressed_bitmap_;

  // Set if this screenshot is being tracked by the `cache_`. The cache is
  // guaranteed to outlive the screenshot, if the screenshot is tracked.
  //
  // Since `this` is never restored/cloned (unlike its owning `NavigationEntry`,
  // per the class-level comments), we will never have a screenshot tracked in a
  // cache from a different `NavigationController`.
  raw_ptr<NavigationEntryScreenshotCache> cache_ = nullptr;

  // This screenshot is cached for the navigation entry of
  // `navigation_entry_id_`.
  const int navigation_entry_id_;

  const gfx::Size dimensions_without_compression_;

  base::WeakPtrFactory<NavigationEntryScreenshot> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_H_
