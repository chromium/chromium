// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/ift_custom_font_data.h"

#include <utility>

#include "base/check.h"
#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

IftCustomFontData* IftCustomFontData::Create(
    const IftFont* ift_font,
    const FontCustomPlatformData& font_platform_data,
    const FontDescription& font_description,
    const FontSelectionCapabilities& font_selection_capabilities) {
  // Resolve variation axis coordinates without clamping or filtering against
  // the base font's `fvar` table, which in an unpatched IFT font may omit
  // pinned axes or have narrowed `[min, max]` ranges until patches are applied.
  // Coordinates for axes the font does not have never match a patch; see
  // https://www.w3.org/TR/IFT/#abstract-opdef-check-entry-intersection.
  //
  // TODO(wmedrano): Clamp out-of-bounds CSS coordinates (e.g. `wght: 950`,
  // `slnt: -14`) to the full font / patch-map axis bounds so finite outer patch
  // segments still match, and include `fvar` defaults for omitted axes
  // (`font-optical-sizing: none` or custom axes) when needed.
  return MakeGarbageCollected<IftCustomFontData>(
      ift_font,
      font_platform_data.ResolveVariationAxes(
          font_description, font_selection_capabilities,
          FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes));
}

IftCustomFontData::IftCustomFontData(
    const IftFont* ift_font,
    Vector<FontVariationAxis, 5> variation_axes)
    : ift_font_(ift_font), variation_axes_(std::move(variation_axes)) {
  CHECK(ift_font_);
}

IftCustomFontData::~IftCustomFontData() = default;

void IftCustomFontData::Trace(Visitor* visitor) const {
  visitor->Trace(ift_font_);
  CustomFontData::Trace(visitor);
}

bool IftCustomFontData::IftRequireSubset(const StringView& text) const {
  return ift_font_->RequireSubset(text, variation_axes_);
}

}  // namespace blink
