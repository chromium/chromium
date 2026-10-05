// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_

#include <memory>

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/fonts/custom_font_data.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class FontResource;
class IftPatcher;
class IftSubsetDefinition;
class RemoteFontFaceSource;

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
    // The font is gathering the list of patches that should be fetched.
    kFetching,
    // The font has failed in some way and is no longer usable.
    kFailed,
  };

  State GetStateForTesting() const { return state_; }

 private:
  void BeginCollecting() const;
  void BeginFetching() const;
  void BeginFailed() const;

  mutable State state_ = State::kIdle;
  Member<RemoteFontFaceSource> font_face_source_;
  std::unique_ptr<IftPatcher> patcher_;
  // The codepoints requested through `IftRequireSubset()`. Heap allocated so
  // this header does not need the complete type, which would pull
  // Crubit-generated bindings into every includer.
  const std::unique_ptr<IftSubsetDefinition> requested_subset_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_
