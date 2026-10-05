// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_

#include <memory>

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/platform/fonts/custom_font_data.h"

namespace blink {

class FontResource;
class IftPatcher;
class IftSubsetDefinition;

// `CustomFontData` for an Incremental Font Transfer (IFT) font. See the W3 spec
// (https://www.w3.org/TR/IFT/).
class IftCustomFontData final : public CustomFontData {
 public:
  // Returns a new `IftCustomFontData` if `font_resource` holds an IFT font,
  // taking ownership of its patcher. Returns nullptr otherwise.
  static IftCustomFontData* MaybeCreate(FontResource& font_resource);

  IftCustomFontData(base::PassKey<IftCustomFontData>,
                    std::unique_ptr<IftPatcher> patcher);
  ~IftCustomFontData() override;

  // CustomFontData:
  bool IftRequireSubset(const StringView& text) const override;

 private:
  std::unique_ptr<IftPatcher> patcher_;
  // The codepoints requested through `IftRequireSubset()`. Heap allocated so
  // this header does not need the complete type, which would pull
  // Crubit-generated bindings into every includer.
  const std::unique_ptr<IftSubsetDefinition> requested_subset_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_
