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
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class DOMWrapperWorld;
class FontResource;
class IftPatcher;
class IftSubsetDefinition;
class RemoteFontFaceSource;
class Resource;

// `CustomFontData` for an Incremental Font Transfer (IFT) font. See the W3 spec
// (https://www.w3.org/TR/IFT/).
class CORE_EXPORT IftCustomFontData final : public CustomFontData {
 public:
  // Returns a new `IftCustomFontData` if `font_resource` holds an IFT font,
  // taking ownership of its patcher. Returns nullptr otherwise.
  static IftCustomFontData* MaybeCreate(RemoteFontFaceSource* font_face_source,
                                        FontResource& font_resource);

  IftCustomFontData(base::PassKey<IftCustomFontData>,
                    RemoteFontFaceSource* font_face_source,
                    const KURL& font_url,
                    const DOMWrapperWorld* world_for_csp,
                    std::unique_ptr<IftPatcher> patcher);
  ~IftCustomFontData() override;

  void Trace(Visitor*) const override;

  // CustomFontData:
  bool IftRequireSubset(const StringView& text) const override;

  // TODO(wmedrano): Make `State` private once all behaviors are publicly
  // observable.
  enum class State {
    // The font is ready to use.
    kIdle,
    // The font is collecting subsets which will later be batched into a fetch
    // request.
    kCollecting,
    // The font is fetching the patches required for `fetched_subset_`.
    kFetching,
    // The font has failed in some way and is no longer usable.
    kFailed,
  };

  State GetStateForTesting() const { return state_; }
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
  Member<RemoteFontFaceSource> font_face_source_;
  // The URL of the font. Patch URLs are resolved relative to this.
  const KURL font_url_;
  Member<const DOMWrapperWorld> world_for_csp_;
  std::unique_ptr<IftPatcher> patcher_;
  // The codepoints requested through `IftRequireSubset()`. Heap allocated so
  // this header does not need the complete type, which would pull
  // Crubit-generated bindings into every includer.
  const std::unique_ptr<IftSubsetDefinition> requested_subset_;
  // The union of all subsets whose patches have been requested.
  const std::unique_ptr<IftSubsetDefinition> fetched_subset_;
  // Scratch subset reused across `IftRequireSubset()` calls.
  const std::unique_ptr<IftSubsetDefinition> scratch_subset_;
  // The in-flight patch fetches.
  mutable HeapHashSet<Member<PatchLoader>> patch_loaders_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_
