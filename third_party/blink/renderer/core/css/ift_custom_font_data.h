// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_IFT_CUSTOM_FONT_DATA_H_

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/loader/resource/ift_font.h"
#include "third_party/blink/renderer/platform/fonts/custom_font_data.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class FontCustomPlatformData;
class FontDescription;
struct FontSelectionCapabilities;

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
