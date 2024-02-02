// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/shaping/stretchy_operator_shaper.h"

#include <hb-ot.h>
#include <hb.h>
#include <unicode/uchar.h>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/fonts/canvas_rotation_in_vertical.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_inline_headers.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

namespace {

// HarfBuzz' hb_position_t is a 16.16 fixed-point value.
inline float HarfBuzzUnitsToFloat(hb_position_t value) {
  static const float kFloatToHbRatio = 1.0f / (1 << 16);
  return kFloatToHbRatio * value;
}

std::optional<OpenTypeMathStretchData::AssemblyParameters>
GetAssemblyParameters(const HarfBuzzFace* harfbuzz_face,
                      Glyph base_glyph,
                      OpenTypeMathStretchData::StretchAxis stretch_axis,
                      float target_size,
                      float* italic_correction) {
  Vector<OpenTypeMathStretchData::GlyphPartRecord> parts =
      OpenTypeMathSupport::GetGlyphPartRecords(harfbuzz_face, base_glyph,
                                               stretch_axis, italic_correction);
  if (parts.empty())
    return std::nullopt;

  hb_font_t* const hb_font = harfbuzz_face->GetScaledFont();

  auto hb_stretch_axis =
      stretch_axis == OpenTypeMathStretchData::StretchAxis::Horizontal
          ? HB_DIRECTION_LTR
          : HB_DIRECTION_BTT;

  // Go over the assembly parts and determine parameters used below.
  // https://w3c.github.io/mathml-core/#the-glyphassembly-table
  float min_connector_overlap = HarfBuzzUnitsToFloat(
      hb_ot_math_get_min_connector_overlap(hb_font, hb_stretch_axis));
  float max_connector_overlap = std::numeric_limits<float>::max();
  float non_extender_advance_sum = 0, extender_advance_sum = 0;
  unsigned non_extender_count = 0, extender_count = 0;

  for (auto& part : parts) {
    // Calculate the count and advance sums of extender and non-extender glyphs.
    if (part.is_extender) {
      extender_count++;
      extender_advance_sum += part.full_advance;
    } else {
      non_extender_count++;
      non_extender_advance_sum += part.full_advance;
    }

    // Take into account start connector length for all but the first glyph.
    if (part.is_extender || &part != &parts.front()) {
      max_connector_overlap =
          std::min(max_connector_overlap, part.start_connector_length);
    }

    // Take into account end connector length for all but the last glyph.
    if (part.is_extender || &part != &parts.back()) {
      max_connector_overlap =
          std::min(max_connector_overlap, part.end_connector_length);
    }
  }

  // Check validity conditions indicated in MathML core.
  float extender_non_overlapping_advance_sum =
      extender_advance_sum - min_connector_overlap * extender_count;
  if (extender_count == 0 || max_connector_overlap < min_connector_overlap ||
      extender_non_overlapping_advance_sum <= 0)
    return std::nullopt;

  // Calculate the minimal number of repetitions needed to obtain an assembly
  // size of size at least target size (r_min in MathML Core). Use a saturated
  // cast; if the value does not fit in unsigned, the kMaxGlyphs limit below
  // will take effect anyway.
  unsigned repetition_count = base::saturated_cast<unsigned>(std::max<float>(
      std::ceil((target_size - non_extender_advance_sum +
                 min_connector_overlap * (non_extender_count - 1)) /
                extender_non_overlapping_advance_sum),
      0));

  // Calculate the number of glyphs, limiting repetition_count to ensure the
  // assembly does not have more than HarfBuzzRunGlyphData::kMaxGlyphs.
  DCHECK_LE(non_extender_count, HarfBuzzRunGlyphData::kMaxGlyphs);
  repetition_count = std::min<unsigned>(
      repetition_count,
      (HarfBuzzRunGlyphData::kMaxGlyphs - non_extender_count) / extender_count);
  unsigned glyph_count = non_extender_count + repetition_count * extender_count;
  DCHECK_LE(glyph_count, HarfBuzzRunGlyphData::kMaxGlyphs);

  // Calculate the maximum overlap (called o_max in MathML Core) and the number
  // of glyph in such an assembly (called N in MathML Core).
  float connector_overlap = max_connector_overlap;
  if (glyph_count > 1) {
    float max_connector_overlap_theorical =
        (non_extender_advance_sum + repetition_count * extender_advance_sum -
         target_size) /
        (glyph_count - 1);
    connector_overlap =
        std::max(min_connector_overlap,
                 std::min(connector_overlap, max_connector_overlap_theorical));
  }

  // Calculate the assembly size (called  AssemblySize(o, r) in MathML Core).
  float stretch_size = non_extender_advance_sum +
                       repetition_count * extender_advance_sum -
                       connector_overlap * (glyph_count - 1);

  return std::optional<OpenTypeMathStretchData::AssemblyParameters>(
      {connector_overlap, repetition_count, glyph_count, stretch_size,
       std::move(parts)});
}

}  // namespace

const ShapeResult* StretchyOperatorShaper::Shape(const Font* font,
                                                 float target_size,
                                                 Metrics* metrics) const {
  const SimpleFontData* primary_font = font->PrimaryFont();
  const HarfBuzzFace* harfbuzz_face =
      primary_font->PlatformData().GetHarfBuzzFace();
  Glyph base_glyph = primary_font->GlyphForCharacter(stretchy_character_);
  float italic_correction = 0.0;
  if (metrics)
    *metrics = Metrics();

  Glyph glyph_variant;
  float glyph_variant_stretch_size;
  TextDirection direction = TextDirection::kLtr;

  // Try different glyph variants.
  for (auto& variant : OpenTypeMathSupport::GetGlyphVariantRecords(
           harfbuzz_face, base_glyph, stretch_axis_)) {
    glyph_variant = variant;
    gfx::RectF bounds = primary_font->BoundsForGlyph(glyph_variant);
    if (metrics) {
      italic_correction =
          OpenTypeMathSupport::MathItalicCorrection(harfbuzz_face, variant)
              .value_or(0);
      *metrics = {primary_font->WidthForGlyph(variant), -bounds.y(),
                  bounds.bottom(), italic_correction};
    }
    glyph_variant_stretch_size =
        stretch_axis_ == OpenTypeMathStretchData::StretchAxis::Horizontal
            ? bounds.width()
            : bounds.height();
    if (glyph_variant_stretch_size >= target_size) {
      return ShapeResult::CreateForStretchyMathOperator(
          font, direction, glyph_variant, glyph_variant_stretch_size);
    }
  }

  // Try a glyph assembly.
  auto params = GetAssemblyParameters(harfbuzz_face, base_glyph, stretch_axis_,
                                      target_size,
                                      metrics ? &italic_correction : nullptr);
  if (!params) {
    return ShapeResult::CreateForStretchyMathOperator(
        font, direction, glyph_variant, glyph_variant_stretch_size);
  }

  const ShapeResult* shape_result_for_glyph_assembly =
      ShapeResult::CreateForStretchyMathOperator(font, direction, stretch_axis_,
                                                 std::move(*params));
  if (metrics) {
    // The OpenType MATH specification does provide any distinction between
    // the advance width and ink width, so the latter is returned here.
    gfx::RectF bounds = shape_result_for_glyph_assembly->ComputeInkBounds();
    if (stretch_axis_ == OpenTypeMathStretchData::StretchAxis::Horizontal) {
      *metrics = {bounds.width(), -bounds.y(), bounds.bottom(),
                  italic_correction};
    } else {
      // For assemblies growing in the vertical direction, the distribution of
      // height between ascent and descent is not defined by the OpenType MATH
      // specification. This code uses MathML Core's convention of
      // ascent = height and descent = 0.
      // Additionally, ShapeResult::CreateForStretchyMathOperator uses a text
      // run that is HB_DIRECTION_TTB in order to stack the parts vertically but
      // the actual glyph assembly is still horizontal text, so height and width
      // are inverted.
      *metrics = {bounds.height(), bounds.width(), 0, italic_correction};
    }
  }
  return shape_result_for_glyph_assembly;
}

}  // namespace blink
