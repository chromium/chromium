/*
 * Copyright (C) 2007 Apple Computer, Inc.
 * Copyright (c) 2007, 2008, 2009, Google Inc. All rights reserved.
 * Copyright (C) 2010 Company 100, Inc.
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

#include "third_party/blink/renderer/platform/fonts/font_custom_platform_data.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_platform_data.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_format_check.h"
#include "third_party/blink/renderer/platform/fonts/opentype/font_settings.h"
#include "third_party/blink/renderer/platform/fonts/opentype/variable_axes_names.h"
#include "third_party/blink/renderer/platform/fonts/palette_interpolation.h"
#include "third_party/blink/renderer/platform/fonts/web_font_decoder.h"
#include "third_party/blink/renderer/platform/fonts/web_font_typeface_factory.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/skia/include/core/SkFourByteTag.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

constexpr SkFourByteTag kItalTag = SkSetFourByteTag('i', 't', 'a', 'l');
constexpr SkFourByteTag kOpszTag = SkSetFourByteTag('o', 'p', 's', 'z');
constexpr SkFourByteTag kSlntTag = SkSetFourByteTag('s', 'l', 'n', 't');
constexpr SkFourByteTag kWdthTag = SkSetFourByteTag('w', 'd', 't', 'h');
constexpr SkFourByteTag kWghtTag = SkSetFourByteTag('w', 'g', 'h', 't');

struct TypefaceAxes {
  std::optional<SkFontParameters::Variation::Axis> wght;
  std::optional<SkFontParameters::Variation::Axis> wdth;
  std::optional<SkFontParameters::Variation::Axis> ital;
  std::optional<SkFontParameters::Variation::Axis> slnt;
  std::optional<SkFontParameters::Variation::Axis> opsz;
};

TypefaceAxes GetTypefaceAxes(const SkTypeface& typeface) {
  TypefaceAxes result;
  const int axes_count = typeface.getVariationDesignParameters({});
  if (axes_count <= 0) {
    return result;
  }
  Vector<SkFontParameters::Variation::Axis, 5> axes(
      static_cast<wtf_size_t>(axes_count));
  const int axes_read = typeface.getVariationDesignParameters(axes);
  if (axes_read <= 0) {
    return result;
  }
  const int limit = std::min<int>(axes_read, axes.size());
  for (int i = 0; i < limit; ++i) {
    const SkFontParameters::Variation::Axis& axis = axes[i];
    if (axis.tag == kWghtTag && !result.wght) {
      result.wght = axis;
    } else if (axis.tag == kWdthTag && !result.wdth) {
      result.wdth = axis;
    } else if (axis.tag == kItalTag && !result.ital) {
      result.ital = axis;
    } else if (axis.tag == kSlntTag && !result.slnt) {
      result.slnt = axis;
    } else if (axis.tag == kOpszTag && !result.opsz) {
      result.opsz = axis;
    }
  }
  return result;
}

// Clamps `requested` to `descriptor_range` if set explicitly on `@font-face`,
// or to `typeface_axis` bounds if `descriptor_range` is `auto`.
float ClampAxisValue(
    FontSelectionValue requested,
    const FontSelectionRange& descriptor_range,
    const std::optional<SkFontParameters::Variation::Axis>& typeface_axis) {
  if (!descriptor_range.IsRangeSetFromAuto()) {
    return descriptor_range.IsValid() ? descriptor_range.ClampToRange(requested)
                                      : requested;
  }
  if (typeface_axis) {
    FontSelectionRange axis_range(FontSelectionValue(typeface_axis->min),
                                  FontSelectionValue(typeface_axis->max));
    if (axis_range.IsValid()) {
      return axis_range.ClampToRange(requested);
    }
  }
  return requested;
}

struct ResolvedVariations {
  Vector<FontVariationAxis, 5> axes;
  bool synthetic_bold = false;
  bool synthetic_italic = false;
};

ResolvedVariations ResolveVariations(
    const sk_sp<SkTypeface>& base_typeface,
    float adjusted_specified_size,
    bool bold,
    bool synthesize_italic,
    bool is_italic_keyword,
    const FontSelectionRequest& selection_request,
    const FontSelectionCapabilities& selection_capabilities,
    OpticalSizing optical_sizing,
    const FontVariationSettings* variation_settings,
    FontCustomPlatformData::VariationAxisMode mode) {
  ResolvedVariations result;
  result.synthetic_bold = bold;
  result.synthetic_italic = synthesize_italic;

  TypefaceAxes typeface_axes;
  if (mode == FontCustomPlatformData::VariationAxisMode::kClampToTypeface) {
    FontFormatCheck::VariableFontSubType font_sub_type =
        FontFormatCheck::ProbeVariableFont(base_typeface);
    bool is_variable_font =
        font_sub_type ==
            FontFormatCheck::VariableFontSubType::kVariableTrueType ||
        font_sub_type == FontFormatCheck::VariableFontSubType::kVariableCff2;
    if (!is_variable_font) {
      return result;
    }
    typeface_axes = GetTypefaceAxes(*base_typeface);
  }

  // Maximum axis count is maximum value for the OpenType USHORT,
  // which is a 16bit unsigned.
  // https://www.microsoft.com/typography/otspec/fvar.htm Variation
  // settings coming from CSS can have duplicate assignments and the
  // list can be longer than UINT16_MAX, but ignoring the length for
  // now, going with a reasonable upper limit. Deduplication gives
  // priority to the last occurring assignment.
  const wtf_size_t settings_count =
      (variation_settings &&
       variation_settings->size() < std::numeric_limits<uint16_t>::max())
          ? variation_settings->size()
          : 0;
  result.axes.reserve(5 + settings_count);

  result.axes.emplace_back(
      kWghtTag,
      ClampAxisValue(selection_request.weight, selection_capabilities.weight,
                     typeface_axes.wght));
  if (selection_capabilities.weight.IsRangeSetFromAuto() &&
      typeface_axes.wght) {
    FontSelectionRange wght_range(FontSelectionValue(typeface_axes.wght->min),
                                  FontSelectionValue(typeface_axes.wght->max));
    if (wght_range.IsValid()) {
      bool has_bold_variations = wght_range.maximum > kNormalWeightValue;
      result.synthetic_bold = bold && !has_bold_variations &&
                              selection_request.weight >= kBoldThreshold;
    }
  }

  result.axes.emplace_back(
      kWdthTag,
      ClampAxisValue(selection_request.width, selection_capabilities.width,
                     typeface_axes.wdth));

  const bool font_variable_italic_axis =
      RuntimeEnabledFeatures::FontVariableItalicAxisEnabled();
  const bool use_ital_axis =
      font_variable_italic_axis && is_italic_keyword && typeface_axes.ital &&
      typeface_axes.ital->min <= 1 && typeface_axes.ital->max >= 1;
  if (font_variable_italic_axis) {
    // The italic keyword instantiates the ital axis at 1. Any other
    // font-style value instantiates it at 0, as the matching algorithm
    // selects italic value 0 within the axis range; in particular, the
    // ital axis is not used to satisfy an oblique request. See
    // https://drafts.csswg.org/css-fonts-4/#font-style-matching. Values
    // outside the axis range are clamped by Skia.
    if (mode ==
        FontCustomPlatformData::VariationAxisMode::kIgnoreTypefaceAxes) {
      result.axes.emplace_back(kItalTag, is_italic_keyword ? 1.0f : 0.0f);
    } else if (typeface_axes.ital) {
      result.axes.emplace_back(kItalTag, use_ital_axis ? 1.0f : 0.0f);
    }
  }

  // CSS and OpenType have opposite definitions of direction of slant
  // angle. In OpenType positive values turn counter-clockwise, negative
  // values clockwise - in CSS positive values are clockwise rotations /
  // skew. See note in https://drafts.csswg.org/css-fonts/#font-style-prop -
  // map value from CSS to OpenType here.
  //
  // TODO(wmedrano): In `kIgnoreTypefaceAxes` mode, avoid requesting non-zero
  // `slnt` (`-14`) for `font-style: italic` when the font's `ital` axis
  // already satisfies it and `slnt` is not explicitly overridden.
  auto negate_slope = [](float slope) { return slope == 0.0f ? 0.0f : -slope; };
  float slnt = negate_slope(selection_request.slope);
  if (use_ital_axis) {
    result.synthetic_italic = false;
    if (typeface_axes.slnt) {
      slnt = typeface_axes.slnt->def;
    }
  } else if (!selection_capabilities.slope.IsRangeSetFromAuto()) {
    if (selection_capabilities.slope.IsValid()) {
      slnt = negate_slope(
          selection_capabilities.slope.ClampToRange(selection_request.slope));
    }
  } else if (typeface_axes.slnt) {
    FontSelectionRange slnt_range(FontSelectionValue(typeface_axes.slnt->min),
                                  FontSelectionValue(typeface_axes.slnt->max));
    if (slnt_range.IsValid()) {
      slnt = slnt_range.ClampToRange(-selection_request.slope);
      bool has_right_slanted_variations =
          slnt_range.minimum < kNormalSlopeValue;
      result.synthetic_italic = synthesize_italic &&
                                !has_right_slanted_variations &&
                                selection_request.slope >= kItalicSlopeValue;
    }
  }
  result.axes.emplace_back(kSlntTag, slnt);

  if (optical_sizing == OpticalSizing::kAuto) {
    // `FontDescription::AdjustedSpecifiedSize()` divides `0 / 0` (`NaN`) when
    // `font-size: 0px` is combined with a `size-adjust` descriptor.
    result.axes.emplace_back(kOpszTag, std::isfinite(adjusted_specified_size)
                                           ? adjusted_specified_size
                                           : 0.0f);
  } else if (optical_sizing == OpticalSizing::kNone && typeface_axes.opsz) {
    // Explicitly set default value to avoid automatic application of
    // optical sizing as it seems to happen on SkTypeface on Mac.
    result.axes.emplace_back(kOpszTag, typeface_axes.opsz->def);
  }

  // Override built-in axes in place (last assignment wins) so
  // `ResolveVariationAxes()` returns one coordinate per axis. CSS
  // `FontVariationSettings` are already sorted and deduplicated, so tracking
  // `max_tag` keeps scanning bounded to `built_in_count` (<= 5) in O(n) while
  // still deduplicating unsorted callers.
  const wtf_size_t built_in_count = result.axes.size();
  std::optional<uint32_t> max_tag;
  for (wtf_size_t i = 0; i < settings_count; ++i) {
    const FontVariationAxis& setting = (*variation_settings)[i];
    const wtf_size_t scan_limit = (!max_tag || *max_tag < setting.Tag())
                                      ? built_in_count
                                      : result.axes.size();
    max_tag = max_tag ? std::max(*max_tag, setting.Tag()) : setting.Tag();
    base::span<FontVariationAxis> scan_range =
        base::span(result.axes).first(scan_limit);
    auto existing =
        std::ranges::find(scan_range, setting.Tag(), &FontVariationAxis::Tag);
    if (existing != scan_range.end()) {
      *existing = setting;
    } else {
      result.axes.push_back(setting);
    }
  }

  return result;
}

}  // namespace

FontCustomPlatformData::FontCustomPlatformData(PassKey,
                                               sk_sp<SkTypeface> typeface,
                                               size_t data_size)
    : base_typeface_(std::move(typeface)), data_size_(data_size) {
  // The new instance of SkData was created while decoding. It stores data
  // from decoded font resource. GC is not aware of this allocation, so we
  // need to inform it.
  if (v8::Isolate* isolate = v8::Isolate::TryGetCurrent()) {
    external_memory_accounter_.Increase(isolate, data_size_);
  }
}

FontCustomPlatformData::~FontCustomPlatformData() {
  if (v8::Isolate* isolate = v8::Isolate::TryGetCurrent()) {
    // Safe cast since DecodedWebFont has max decompressed size of 128MB.
    external_memory_accounter_.Decrease(isolate, data_size_);
  }
}

Vector<FontVariationAxis, 5> FontCustomPlatformData::ResolveVariationAxes(
    const FontDescription& font_description,
    const FontSelectionCapabilities& selection_capabilities,
    VariationAxisMode mode) const {
  DCHECK(base_typeface_);
  return ResolveVariations(
             base_typeface_, font_description.AdjustedSpecifiedSize(),
             /*bold=*/false, /*synthesize_italic=*/false,
             font_description.GetStyleSyntax() ==
                 FontDescription::StyleSyntax::kItalicKeyword,
             font_description.GetFontSelectionRequest(), selection_capabilities,
             font_description.FontOpticalSizing(),
             font_description.VariationSettings(), mode)
      .axes;
}

const FontPlatformData* FontCustomPlatformData::GetFontPlatformData(
    float size,
    float adjusted_specified_size,
    bool bold,
    bool synthesize_italic,
    bool is_italic_keyword,
    const FontSelectionRequest& selection_request,
    const FontSelectionCapabilities& selection_capabilities,
    const OpticalSizing& optical_sizing,
    TextRenderingMode text_rendering,
    const ResolvedFontFeatures& resolved_font_features,
    FontOrientation orientation,
    const FontVariationSettings* variation_settings,
    const FontPalette* palette) const {
  DCHECK(base_typeface_);

  sk_sp<SkTypeface> return_typeface = base_typeface_;

  ResolvedVariations resolved = ResolveVariations(
      base_typeface_, adjusted_specified_size, bold, synthesize_italic,
      is_italic_keyword, selection_request, selection_capabilities,
      optical_sizing, variation_settings, VariationAxisMode::kClampToTypeface);
  bool synthetic_bold = resolved.synthetic_bold;
  bool synthetic_italic = resolved.synthetic_italic;
  if (!resolved.axes.empty()) {
    Vector<SkFontArguments::VariationPosition::Coordinate, 5> variation;
    variation.reserve(resolved.axes.size());
    for (const FontVariationAxis& axis : resolved.axes) {
      variation.push_back(SkFontArguments::VariationPosition::Coordinate{
          axis.Tag(), SkFloatToScalar(axis.Value())});
    }

    SkFontArguments font_args;
    font_args.setVariationDesignPosition(
        {variation.data(), static_cast<int>(variation.size())});
    sk_sp<SkTypeface> sk_variation_font(base_typeface_->makeClone(font_args));

    if (sk_variation_font) {
      return_typeface = std::move(sk_variation_font);
    } else {
      SkString family_name;
      base_typeface_->getFamilyName(&family_name);
      // TODO: Surface this as a console message?
      LOG(ERROR) << "Unable for apply variation axis properties for font: "
                 << family_name.c_str();
    }
  }

  if (palette && !palette->IsNormalPalette()) {
    // TODO: Check applicability of font-palette-values according to matching
    // font family name, or should that be done at the CSS family level?

    SkFontArguments font_args;
    SkFontArguments::Palette sk_palette{0, nullptr, 0};

    Vector<FontPalette::FontPaletteOverride> color_overrides;
    std::optional<uint16_t> palette_index = std::nullopt;
    PaletteInterpolation palette_interpolation(base_typeface_);
    if (palette->IsInterpolablePalette()) {
      color_overrides =
          palette_interpolation.ComputeInterpolableFontPalette(palette);
      palette_index = 0;
    } else {
      color_overrides = *palette->GetColorOverrides();
      palette_index = palette_interpolation.RetrievePaletteIndex(palette);
    }

    base::HeapArray<SkFontArguments::Palette::Override> sk_overrides;
    if (palette_index.has_value()) {
      sk_palette.index = *palette_index;

      if (color_overrides.size()) {
        sk_overrides =
            base::HeapArray<SkFontArguments::Palette::Override>::Uninit(
                color_overrides.size());
        for (wtf_size_t i = 0; i < color_overrides.size(); i++) {
          SkColor sk_color = color_overrides[i].color.toSkColor4f().toSkColor();
          sk_overrides[i] = {color_overrides[i].index, sk_color};
        }
        sk_palette.overrides = sk_overrides.data();
        sk_palette.overrideCount = color_overrides.size();
      }

      font_args.setPalette(sk_palette);
    }

    sk_sp<SkTypeface> palette_typeface(return_typeface->makeClone(font_args));
    if (palette_typeface) {
      return_typeface = palette_typeface;
    }
  }
  return MakeGarbageCollected<FontPlatformData>(
      std::move(return_typeface), std::string(), size,
      synthetic_bold && !base_typeface_->isBold(),
      synthetic_italic && !base_typeface_->isItalic(), text_rendering,
      resolved_font_features, orientation);
}

Vector<VariationAxis> FontCustomPlatformData::GetVariationAxes() const {
  return VariableAxesNames::GetVariationAxes(base_typeface_);
}

String FontCustomPlatformData::FamilyNameForInspector() const {
  SkTypeface::LocalizedStrings* font_family_iterator =
      base_typeface_->createFamilyNameIterator();
  SkTypeface::LocalizedString localized_string;
  while (font_family_iterator->next(&localized_string)) {
    // BCP 47 tags for English take precedent in font matching over other
    // localizations: https://drafts.csswg.org/css-fonts/#descdef-src.
    if (localized_string.fLanguage.equals("en") ||
        localized_string.fLanguage.equals("en-US")) {
      break;
    }
  }
  font_family_iterator->unref();
  return String::FromUtf8(base::as_byte_span(localized_string.fString));
}

String FontCustomPlatformData::GetPostScriptNameOrFamilyNameForInspector()
    const {
  SkString postscript_name;
  bool success = base_typeface_->getPostScriptName(&postscript_name);
  if (!success) {
    return FamilyNameForInspector();
  }

  return String(base::as_byte_span(postscript_name));
}

FontCustomPlatformData* FontCustomPlatformData::Create(
    SharedBuffer* buffer,
    String& ots_parse_message) {
  DCHECK(buffer);
  // TODO(wmedrano): Avoid creating unused `IftPatcher` within `DecodedWebFont`.
  base::expected<DecodedWebFont, String> decode_result = DecodeWebFont(buffer);
  if (!decode_result.has_value()) {
    ots_parse_message = std::move(decode_result).error();
    return nullptr;
  }
  return Create(std::move(decode_result->sk_typeface),
                decode_result->decoded_size);
}

FontCustomPlatformData* FontCustomPlatformData::Create(
    sk_sp<SkTypeface> typeface,
    size_t data_size) {
  return MakeGarbageCollected<FontCustomPlatformData>(
      PassKey(), std::move(typeface), data_size);
}

}  // namespace blink
