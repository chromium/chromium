// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/fonts/custom_font_data.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DOMWrapperWorld;
class FontCustomPlatformData;
class FontDescription;
class FontResource;
struct FontSelectionCapabilities;
class IftPatcher;
class IftSubsetDefinition;
class Resource;
class ResourceFetcher;

// Observes updates to an `IftFont` and provides a `ResourceFetcher` for patch
// requests.
class CORE_EXPORT IftFontObserver : public GarbageCollectedMixin {
 public:
  // Returns the `ResourceFetcher` to use for patch requests, or nullptr if this
  // observer cannot fetch resources (e.g. its context is detached).
  virtual ResourceFetcher* GetIftResourceFetcher() const = 0;

  // Called when patches have been applied and `custom_font_data` has been
  // decoded from the extended font. `custom_font_data` is never null.
  virtual void OnIftFontUpdated(
      const FontCustomPlatformData* custom_font_data) = 0;
};

// The state of an Incremental Font Transfer (IFT) font. See the W3 spec
// (https://www.w3.org/TR/IFT/).
//
// A single instance that is shared by every `SimpleFontData` created from the
// same `RemoteFontFaceSource`.
class CORE_EXPORT IftFont final : public GarbageCollected<IftFont> {
 public:
  // Returns a new `IftFont` if `font_resource` holds an IFT font, cloning its
  // patcher. Returns nullptr otherwise. `observer` must be non-null.
  static IftFont* MaybeCreate(IftFontObserver* observer,
                              FontResource& font_resource);

  // `observer` and `patcher` must be non-null. `world_for_csp` may be null.
  IftFont(base::PassKey<IftFont>,
          IftFontObserver* observer,
          const KURL& font_url,
          const DOMWrapperWorld* world_for_csp,
          std::unique_ptr<IftPatcher> patcher);
  ~IftFont();

  void Trace(Visitor*) const;

  // Requests that the font is extended to support `text` for the given
  // `variation_axes`. Returns true if the rendered font already supports it, or
  // if the font has no patches for it. See
  // `CustomFontData::IftRequireSubset()`.
  bool RequireSubset(
      const StringView& text,
      base::span<const FontVariationAxis> variation_axes = {}) const;

  // TODO(wmedrano): Make `State` private once all behaviors are publicly
  // observable.
  enum class State {
    // The font is ready to use.
    kIdle,
    // The font is collecting subsets which will later be batched into a fetch
    // request. Entering this state from `kIdle` starts a new extension, which
    // lasts until the font returns to `kIdle`.
    kCollecting,
    // The font is fetching the patches required for `fetched_subset_`.
    kFetching,
    // The font has failed in some way and is no longer usable.
    kFailed,
  };

  State GetStateForTesting() const { return state_; }
  // Overrides the IFT spec patch limits enforced per extension.
  void SetPatchLimitsForTesting(uint32_t max_invalidating_patches,
                                uint32_t max_total_patches) const;
  // Returns the font data with all patches applied so far. The returned span is
  // invalidated when more patches are applied.
  base::span<const uint8_t> GetFontDataForTesting() const LIFETIME_BOUND;

 private:
  // Fetches a single patch and notifies its owner when done.
  class PatchLoader;

  void BeginIdle() const;
  void BeginCollecting() const;
  void BeginFetching() const;
  void BeginFailed() const;
  // Applies the fetched patches. Called once all patch fetches have finished.
  void ApplyPatches() const;
  // Called by `loader` when its fetch of `resource` has finished.
  void OnPatchLoaded(PatchLoader* loader, const Resource* resource) const;

  mutable State state_ = State::kIdle;
  Member<IftFontObserver> observer_;
  // The URL of the font. Patch URLs are resolved relative to this.
  const KURL font_url_;
  Member<const DOMWrapperWorld> world_for_csp_;
  std::unique_ptr<IftPatcher> patcher_;
  // The subsets requested through `RequireSubset()`. Heap allocated so this
  // header does not need the complete type, which would pull Crubit-generated
  // bindings into every includer.
  const std::unique_ptr<IftSubsetDefinition> requested_subset_;
  // The union of all subsets whose patches have been requested.
  const std::unique_ptr<IftSubsetDefinition> fetched_subset_;
  // Scratch subset reused across `RequireSubset()` calls.
  const std::unique_ptr<IftSubsetDefinition> scratch_subset_;
  // The in-flight patch fetches.
  mutable HeapHashSet<Member<PatchLoader>> patch_loaders_;
};

// The `CustomFontData` of a single `SimpleFontData` backed by an IFT font.
class CORE_EXPORT IftCustomFontData final : public CustomFontData {
 public:
  // Returns the `IftCustomFontData` for the given `ift_font`,
  // `font_platform_data`, `font_description`, and
  // `font_selection_capabilities`.
  static IftCustomFontData* Create(
      const IftFont* ift_font,
      const FontCustomPlatformData& font_platform_data,
      const FontDescription& font_description,
      const FontSelectionCapabilities& font_selection_capabilities);

  IftCustomFontData(const IftFont* ift_font,
                    Vector<FontVariationAxis, 5> variation_axes);
  ~IftCustomFontData() override;

  void Trace(Visitor*) const override;

  // CustomFontData:
  bool IftRequireSubset(const StringView& text) const override;

  const IftFont* GetIftFont() const { return ift_font_.Get(); }
  base::span<const FontVariationAxis> GetVariationAxesForTesting() const
      LIFETIME_BOUND {
    return variation_axes_;
  }

 private:
  Member<const IftFont> ift_font_;
  const Vector<FontVariationAxis, 5> variation_axes_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_
