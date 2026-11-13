/*
 * Copyright (C) 2007 Apple Computer, Inc.
 * Copyright (c) 2007, 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CUSTOM_PLATFORM_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CUSTOM_PLATFORM_DATA_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/platform/bindings/v8_external_memory_accounter.h"
#include "third_party/blink/renderer/platform/fonts/font_optical_sizing.h"
#include "third_party/blink/renderer/platform/fonts/font_orientation.h"
#include "third_party/blink/renderer/platform/fonts/font_palette.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/fonts/opentype/variable_axes_names.h"
#include "third_party/blink/renderer/platform/fonts/resolved_font_features.h"
#include "third_party/blink/renderer/platform/fonts/text_rendering_mode.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkTypeface;

namespace blink {

class FontDescription;
class FontPlatformData;

class PLATFORM_EXPORT FontCustomPlatformData
    : public GarbageCollected<FontCustomPlatformData> {
 public:
  enum class VariationAxisMode {
    // Constrains axes against `base_typeface_`'s `fvar` table: returns empty if
    // `base_typeface_` is not a variable font, clamps `auto` descriptor ranges
    // to `fvar` `[min, max]`, filters `ital` against `fvar`, and populates
    // `fvar`'s default `opsz` for `font-optical-sizing: none`.
    kClampToTypeface,
    // Resolves coordinates from CSS and explicit `@font-face` descriptors
    // without clamping or filtering against `base_typeface_`'s `fvar` table
    // (for unpatched IFT fonts whose `fvar` may omit pinned axes or have
    // narrowed `[min, max]` ranges until patches are applied).
    kIgnoreTypefaceAxes,
  };

  static FontCustomPlatformData* Create(SharedBuffer*,
                                        String& ots_parse_message);
  static FontCustomPlatformData* Create(sk_sp<SkTypeface>, size_t data_size);

  using PassKey = base::PassKey<FontCustomPlatformData>;

  FontCustomPlatformData(PassKey, sk_sp<SkTypeface>, size_t data_size);
  FontCustomPlatformData(const FontCustomPlatformData&) = delete;
  FontCustomPlatformData& operator=(const FontCustomPlatformData&) = delete;
  ~FontCustomPlatformData();

  void Trace(Visitor*) const {}

  // Resolves unique variation axis coordinates (with `font-variation-settings`
  // overriding high-level CSS properties using last-wins precedence). Kept as
  // an instance method so future axis reconciliation (e.g., checking whether
  // `base_typeface_` already exposes a usable `ital` axis) can inspect
  // `base_typeface_`.
  Vector<FontVariationAxis, 5> ResolveVariationAxes(
      const FontDescription& font_description,
      const FontSelectionCapabilities& selection_capabilities,
      VariationAxisMode mode) const;

  // The size argument should come from EffectiveFontSize() and
  // adjusted_specified_size should come from AdjustedSpecifiedSize() of
  // FontDescription. The latter is needed for correctly applying
  // font-optical-sizing: auto; independent of zoom level.
  //
  // synthesize_italic - Requests faux (synthetic) italic slanting.
  // is_italic_keyword - True only for the `font-style: italic` keyword (not
  //     `oblique`); drives the OpenType `ital` variation axis.
  const FontPlatformData* GetFontPlatformData(
      float size,
      float adjusted_specified_size,
      bool bold,
      bool synthesize_italic,
      bool is_italic_keyword,
      const FontSelectionRequest&,
      const FontSelectionCapabilities&,
      const OpticalSizing& optical_sizing,
      TextRenderingMode text_rendering,
      const ResolvedFontFeatures& resolved_font_features,
      FontOrientation = FontOrientation::kHorizontal,
      const FontVariationSettings* = nullptr,
      const FontPalette* = nullptr) const;

  String FamilyNameForInspector() const;

  String GetPostScriptNameOrFamilyNameForInspector() const;

  Vector<VariationAxis> GetVariationAxes() const;

  size_t DataSize() const { return data_size_; }

 private:
  sk_sp<SkTypeface> base_typeface_;
  size_t data_size_;

  NO_UNIQUE_ADDRESS V8ExternalMemoryAccounterBase external_memory_accounter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_CUSTOM_PLATFORM_DATA_H_
