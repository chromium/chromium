// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/palette_interpolation.h"

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_cpal_lookup.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

Vector<FontPalette::FontPaletteOverride> PaletteInterpolation::MixColorRecords(
    Vector<FontPalette::FontPaletteOverride>&& start_color_records,
    Vector<FontPalette::FontPaletteOverride>&& end_color_records,
    double percentage,
    double alpha_multiplier,
    Color::ColorSpace color_interpolation_space,
    std::optional<Color::HueInterpolationMethod> hue_interpolation_method) {
  Vector<FontPalette::FontPaletteOverride> result_color_records;

  DCHECK_EQ(start_color_records.size(), end_color_records.size());

  wtf_size_t color_records_cnt = start_color_records.size();
  for (wtf_size_t i = 0; i < color_records_cnt; i++) {
    DCHECK_EQ(start_color_records[i].index, end_color_records[i].index);

    Color start_color = start_color_records[i].color;
    Color end_color = end_color_records[i].color;

    Color result_color = Color::FromColorMix(
        color_interpolation_space, hue_interpolation_method, start_color,
        end_color, percentage, alpha_multiplier);

    FontPalette::FontPaletteOverride result_color_record(i, result_color);
    result_color_records.push_back(result_color_record);
  }
  return result_color_records;
}

std::optional<uint16_t> PaletteInterpolation::RetrievePaletteIndex(
    const FontPalette* palette) const {
  if (palette->GetPaletteNameKind() == FontPalette::kLightPalette ||
      palette->GetPaletteNameKind() == FontPalette::kDarkPalette) {
    OpenTypeCpalLookup::PaletteUse palette_use =
        palette->GetPaletteNameKind() == FontPalette::kLightPalette
            ? OpenTypeCpalLookup::kUsableWithLightBackground
            : OpenTypeCpalLookup::kUsableWithDarkBackground;
    return OpenTypeCpalLookup::FirstThemedPalette(typeface_, palette_use);
  } else if (palette->IsCustomPalette()) {
    FontPalette::BasePaletteValue base_palette_index =
        palette->GetBasePalette();

    switch (base_palette_index.type) {
      case FontPalette::kNoBasePalette: {
        return 0;
      }
      case FontPalette::kDarkBasePalette: {
        OpenTypeCpalLookup::PaletteUse palette_use =
            OpenTypeCpalLookup::kUsableWithDarkBackground;
        return OpenTypeCpalLookup::FirstThemedPalette(typeface_, palette_use);
      }
      case FontPalette::kLightBasePalette: {
        OpenTypeCpalLookup::PaletteUse palette_use =
            OpenTypeCpalLookup::kUsableWithLightBackground;
        return OpenTypeCpalLookup::FirstThemedPalette(typeface_, palette_use);
      }
      case FontPalette::kIndexBasePalette: {
        return base_palette_index.index;
      }
    }
    return std::nullopt;
  }
  return std::nullopt;
}

Vector<FontPalette::FontPaletteOverride>
PaletteInterpolation::RetrieveColorRecords(const FontPalette* palette,
                                           unsigned int palette_index) const {
  Vector<Color> colors =
      OpenTypeCpalLookup::RetrieveColorRecords(typeface_, palette_index);

  wtf_size_t colors_size = colors.size();
  if (!colors_size) {
    colors = OpenTypeCpalLookup::RetrieveColorRecords(typeface_, 0);
    colors_size = colors.size();
  } else {
    for (auto& override : *palette->GetColorOverrides()) {
      if (override.index < static_cast<int>(colors_size)) {
        colors[override.index] = override.color;
      }
    }
  }
  Vector<FontPalette::FontPaletteOverride> color_records(colors_size);
  DCHECK_LT(colors_size, std::numeric_limits<std::uint16_t>::max());
  for (wtf_size_t i = 0; i < colors_size; i++) {
    color_records[i] = {static_cast<uint16_t>(i), colors[i]};
  }
  return color_records;
}

Vector<FontPalette::FontPaletteOverride>
PaletteInterpolation::ComputeInterpolableFontPalette(
    const FontPalette* palette) const {
  if (!palette->IsInterpolablePalette()) {
    std::optional<uint16_t> retrieved_palette_index =
        RetrievePaletteIndex(palette);
    unsigned int new_palette_index =
        retrieved_palette_index.has_value() ? *retrieved_palette_index : 0;
    return RetrieveColorRecords(palette, new_palette_index);
  }

  Vector<FontPalette::FontPaletteOverride> start_color_records =
      ComputeInterpolableFontPalette(palette->GetStart().get());
  Vector<FontPalette::FontPaletteOverride> end_color_records =
      ComputeInterpolableFontPalette(palette->GetEnd().get());
  Vector<FontPalette::FontPaletteOverride> result_color_records =
      MixColorRecords(
          std::move(start_color_records), std::move(end_color_records),
          palette->GetNormalizedPercentage(), palette->GetAlphaMultiplier(),
          palette->GetColorInterpolationSpace(),
          palette->GetHueInterpolationMethod());
  return result_color_records;
}

}  // namespace blink
