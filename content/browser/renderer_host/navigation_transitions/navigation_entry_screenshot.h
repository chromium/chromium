// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_H_

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/supports_user_data.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_client.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_data.h"
#include "content/common/content_export.h"

class SkBitmap;

namespace cc::slim {
class TextureLayer;
}

namespace gpu {
class ClientSharedImage;
}  // namespace gpu

namespace viz {
class RasterContextProvider;
}  // namespace viz

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
      public base::SupportsUserData::Data,
      public performance_scenarios::MatchingScenarioObserver,
      public viz::ContextLostObserver,
      private cc::TextureLayerClient {
 public:
  using ScreenshotCallback = base::RepeatingCallback<
      void(const SkBitmap& bitmap, bool requested, SkBitmap& out_override)>;

  const static void* const kUserDataKey;

  static void SetDisableCompressionForTesting(bool disable);

  NavigationEntryScreenshot(const SkBitmap& bitmap,
                            NavigationTransitionData::UniqueId unique_id,
                            bool supports_etc_non_power_of_two);
  NavigationEntryScreenshot(
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      NavigationTransitionData::UniqueId unique_id,
      bool supports_etc_non_power_of_two,
      scoped_refptr<viz::RasterContextProvider> context_provider,
      ScreenshotCallback callback);

  NavigationEntryScreenshot(const NavigationEntryScreenshot&) = delete;
  NavigationEntryScreenshot& operator=(const NavigationEntryScreenshot&) =
      delete;
  ~NavigationEntryScreenshot() override;

  // Whether the bitmap is ready or there is a shared image pending read back.
  bool IsValid() const;

  // Returns true when a bitmap (compressed or not) is ready for consumption.
  // A bitmap isn't ready when a read back is still pending or it failed.
  bool IsBitmapReady() const;

  // `cc::UIResourceClient`:
  cc::UIResourceBitmap GetBitmap(cc::UIResourceId uid,
                                 bool resource_lost) override;

  // Sets the `cache` managing the memory for this screenshot. When set, the
  // screenshot is stored on its associated NavigationEntry and is guaranteed to
  // not be displayed in the UI.
  //
  // Returns the memory occupied by the bitmap in bytes.
  size_t SetCache(NavigationEntryScreenshotCache* cache);

  void OnScenarioMatchChanged(performance_scenarios::ScenarioScope scope,
                              bool matches_pattern) override;

  void OnContextLost() override;

  // Creates a texture layer that uses the shared image in this screenshot.
  // This can't be called again until the returned closure runs.
  std::pair<scoped_refptr<cc::slim::TextureLayer>, base::ScopedClosureRunner>
  CreateTextureLayer();

  // Returns true if the screenshot is being managed by a cache. This is not the
  // case when it's being displayed in the UI.
  bool is_cached() const { return cache_ != nullptr; }

  // Returns the bounds of the uncompressed bitmap.
  gfx::Size dimensions_without_compression() const {
    return dimensions_without_compression_;
  }

  NavigationTransitionData::UniqueId unique_id() const { return unique_id_; }

  SkBitmap GetBitmapForTesting() const;
  size_t CompressedSizeForTesting() const;

 private:
  void ReadBack();
  void OnReadBack(SkBitmap bitmap, bool success);
  void OnCompressionFinished(sk_sp<SkPixelRef> compressed_bitmap);

  void SetupCompressionTask(const SkBitmap& bitmap,
                            bool supports_etc_non_power_of_two);
  void StartCompression();

  void ResetContextProvider();

  const cc::UIResourceBitmap& GetBitmap() const;

  // cc::TextureLayerClient
  // Prepares a transferable resource for the shared image in this screenshot.
  // This can only be called after running CreateTextureLayer and before the
  // returned closure runs.
  bool PrepareTransferableResource(
      viz::TransferableResource* transferable_resource,
      viz::ReleaseCallback* release_callback) override;

  void OnTextureLayerToBeDeleted();

  // The uncompressed bitmap cached when navigating away from this navigation
  // entry.
  std::optional<cc::UIResourceBitmap> bitmap_;

  scoped_refptr<gpu::ClientSharedImage> shared_image_;

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

  // This screenshot is cached for the navigation entry, whose
  // `navigation_transition_data()` has `unique_id_`.
  const NavigationTransitionData::UniqueId unique_id_;

  const gfx::Size dimensions_without_compression_;

  // Whether a readback is needed and wasn't issued.
  bool read_back_needed_ = false;

  const bool supports_etc_non_power_of_two_;

  scoped_refptr<viz::RasterContextProvider> context_provider_;

  base::OnceClosure compression_task_;

  ScreenshotCallback screenshot_callback_;

  viz::TransferableResource texture_transferable_resource_;
  viz::ReleaseCallback texture_release_callback_;

  base::WeakPtrFactory<NavigationEntryScreenshot> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_ENTRY_SCREENSHOT_H_
