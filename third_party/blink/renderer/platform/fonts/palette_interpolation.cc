// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/palette_interpolation.h"

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_cpal_lookup.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

Color PaletteInterpolation::InterpolateColor(Color start_color,
                                             Color end_color,
                                             double progress) {
  float alpha = ClampTo<double>(
      start_color.Alpha() * (1 - progress) + end_color.Alpha() * progress, 0,
      1);

  const auto interpolate_param = [&start_color, &end_color, &alpha, &progress](
                                     double start_param, double end_param) {
    return (start_param * start_color.Alpha() * (1 - progress) +
            end_param * end_color.Alpha() * progress) /
           alpha;
  };

  float param0 = interpolate_param(start_color.Param0(), end_color.Param0());
  float param1 = interpolate_param(start_color.Param1(), end_color.Param1());
  float param2 = interpolate_param(start_color.Param2(), end_color.Param2());

  Color result_color = Color::FromColorSpace(Color::ColorSpace::kOklab, param0,
                                             param1, param2, alpha);
  return result_color;
}

Color PaletteInterpolation::AddColors(Color start_color, Color end_color) {
  float alpha = ClampTo<double>(start_color.Alpha() + end_color.Alpha(), 0, 1);

  const auto add_params = [&start_color, &end_color, &alpha](double start_param,
                                                             double end_param) {
    return (start_param * start_color.Alpha() + end_param * end_color.Alpha()) /
           alpha;
  };

  float param0 = add_params(start_color.Param0(), end_color.Param0());
  float param1 = add_params(start_color.Param1(), end_color.Param1());
  float param2 = add_params(start_color.Param2(), end_color.Param2());

  Color result_color = Color::FromColorSpace(Color::ColorSpace::kOklab, param0,
                                             param1, param2, alpha);
  return result_color;
}

Color PaletteInterpolation::ScaleColor(Color color, double scale) {
  float alpha = ClampTo<double>(color.Alpha() * scale, 0, 1);

  float param0 = color.Param0() * scale;
  float param1 = color.Param1() * scale;
  float param2 = color.Param2() * scale;

  Color result_color = Color::FromColorSpace(Color::ColorSpace::kOklab, param0,
                                             param1, param2, alpha);
  return result_color;
}

Vector<FontPalette::FontPaletteOverride>
PaletteInterpolation::ApplyOperationToColorRecords(
    Vector<FontPalette::FontPaletteOverride>&& start_color_records,
    Vector<FontPalette::FontPaletteOverride>&& end_color_records,
    FontPalette::InterpolablePaletteOperation operation) {
  Vector<FontPalette::FontPaletteOverride> result_color_records;

  DCHECK_EQ(start_color_records.size(), end_color_records.size());

  wtf_size_t color_records_cnt = start_color_records.size();
  for (wtf_size_t i = 0; i < color_records_cnt; i++) {
    DCHECK_EQ(start_color_records[i].index, end_color_records[i].index);
    // Since there is no way for user to specify which color space should be
    // used for interpolation, it defaults to Oklab.
    // https://www.w3.org/TR/css-color-4/#interpolation-space
    Color start_color = start_color_records[i].color;
    start_color.ConvertToColorSpace(Color::ColorSpace::kOklab);
    Color end_color = end_color_records[i].color;
    end_color.ConvertToColorSpace(Color::ColorSpace::kOklab);
    Color result_color = start_color;
    switch (operation.type) {
      case FontPalette::kMixPalettes:
        result_color =
            InterpolateColor(start_color, end_color, operation.param);
        break;
      case FontPalette::kAddPalettes:
        result_color = AddColors(start_color, end_color);
        break;
      case FontPalette::kScalePalette:
        result_color = ScaleColor(start_color, operation.param);
        break;
      case FontPalette::kNoInterpolation:
        NOTREACHED();
    }

    FontPalette::FontPaletteOverride result_color_record(i, result_color);
    result_color_records.push_back(result_color_record);
  }
  return result_color_records;
}

absl::optional<uint16_t> PaletteInterpolation::RetrievePaletteIndex(
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
    return absl::nullopt;
  }
  return absl::nullopt;
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
  for (wtf_size_t i = 0; i < colors_size; i++) {
    color_records[i] = {static_cast<int>(i), colors[i]};
  }
  return color_records;
}

Vector<FontPalette::FontPaletteOverride>
PaletteInterpolation::ComputeInterpolableFontPalette(
    const FontPalette* palette) const {
  if (!palette->IsInterpolablePalette()) {
    absl::optional<uint16_t> retrieved_palette_index =
        RetrievePaletteIndex(palette);
    unsigned int new_palette_index =
        retrieved_palette_index.has_value() ? *retrieved_palette_index : 0;
    return RetrieveColorRecords(palette, new_palette_index);
  }

  Vector<FontPalette::FontPaletteOverride> start_color_records =
      ComputeInterpolableFontPalette(palette->GetStart().get());
  Vector<FontPalette::FontPaletteOverride> end_color_records =
      (palette->GetOperation().type != FontPalette::kScalePalette)
          ? ComputeInterpolableFontPalette(palette->GetEnd().get())
          : start_color_records;
  Vector<FontPalette::FontPaletteOverride> result_color_records =
      ApplyOperationToColorRecords(std::move(start_color_records),
                                   std::move(end_color_records),
                                   palette->GetOperation());
  return result_color_records;
}

}  // namespace blink
