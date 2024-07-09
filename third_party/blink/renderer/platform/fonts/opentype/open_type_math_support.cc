// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/fonts/opentype/open_type_math_support.h"

// clang-format off
#include <hb.h>
#include <hb-ot.h>
// clang-format on

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "third_party/blink/renderer/platform/fonts/shaping/harfbuzz_face.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {
// HarfBuzz' hb_position_t is a 16.16 fixed-point value.
float HarfBuzzUnitsToFloat(hb_position_t value) {
  static const float kFloatToHbRatio = 1.0f / (1 << 16);
  return kFloatToHbRatio * value;
}

// Latin Modern, STIX Two, XITS, Asana, Deja Vu, Libertinus and TeX Gyre fonts
// provide at most 13 size variant and 5 assembly parts.
// See https://chromium-review.googlesource.com/c/chromium/src/+/2074678
constexpr unsigned kMaxHarfBuzzRecords = 20;

hb_direction_t HarfBuzzDirection(
    blink::OpenTypeMathStretchData::StretchAxis stretch_axis) {
  return stretch_axis == blink::OpenTypeMathStretchData::StretchAxis::Horizontal
             ? HB_DIRECTION_LTR
             : HB_DIRECTION_BTT;
}

}  // namespace

namespace blink {

bool OpenTypeMathSupport::HasMathData(const HarfBuzzFace* harfbuzz_face) {
  if (!harfbuzz_face)
    return false;

  hb_font_t* font = harfbuzz_face->GetScaledFont();
  DCHECK(font);
  hb_face_t* face = hb_font_get_face(font);
  DCHECK(face);

  return hb_ot_math_has_data(face);
}

std::optional<float> OpenTypeMathSupport::MathConstant(
    const HarfBuzzFace* harfbuzz_face,
    MathConstants constant) {
  if (!HasMathData(harfbuzz_face))
    return std::nullopt;

  hb_font_t* const font = harfbuzz_face->GetScaledFont();
  DCHECK(font);

  hb_position_t harfbuzz_value = hb_ot_math_get_constant(
      font, static_cast<hb_ot_math_constant_t>(constant));

  switch (constant) {
    case kScriptPercentScaleDown:
    case kScriptScriptPercentScaleDown:
    case kRadicalDegreeBottomRaisePercent:
      return std::optional<float>(harfbuzz_value / 100.0);
    case kDelimitedSubFormulaMinHeight:
    case kDisplayOperatorMinHeight:
    case kMathLeading:
    case kAxisHeight:
    case kAccentBaseHeight:
    case kFlattenedAccentBaseHeight:
    case kSubscriptShiftDown:
    case kSubscriptTopMax:
    case kSubscriptBaselineDropMin:
    case kSuperscriptShiftUp:
    case kSuperscriptShiftUpCramped:
    case kSuperscriptBottomMin:
    case kSuperscriptBaselineDropMax:
    case kSubSuperscriptGapMin:
    case kSuperscriptBottomMaxWithSubscript:
    case kSpaceAfterScript:
    case kUpperLimitGapMin:
    case kUpperLimitBaselineRiseMin:
    case kLowerLimitGapMin:
    case kLowerLimitBaselineDropMin:
    case kStackTopShiftUp:
    case kStackTopDisplayStyleShiftUp:
    case kStackBottomShiftDown:
    case kStackBottomDisplayStyleShiftDown:
    case kStackGapMin:
    case kStackDisplayStyleGapMin:
    case kStretchStackTopShiftUp:
    case kStretchStackBottomShiftDown:
    case kStretchStackGapAboveMin:
    case kStretchStackGapBelowMin:
    case kFractionNumeratorShiftUp:
    case kFractionNumeratorDisplayStyleShiftUp:
    case kFractionDenominatorShiftDown:
    case kFractionDenominatorDisplayStyleShiftDown:
    case kFractionNumeratorGapMin:
    case kFractionNumDisplayStyleGapMin:
    case kFractionRuleThickness:
    case kFractionDenominatorGapMin:
    case kFractionDenomDisplayStyleGapMin:
    case kSkewedFractionHorizontalGap:
    case kSkewedFractionVerticalGap:
    case kOverbarVerticalGap:
    case kOverbarRuleThickness:
    case kOverbarExtraAscender:
    case kUnderbarVerticalGap:
    case kUnderbarRuleThickness:
    case kUnderbarExtraDescender:
    case kRadicalVerticalGap:
    case kRadicalDisplayStyleVerticalGap:
    case kRadicalRuleThickness:
    case kRadicalExtraAscender:
    case kRadicalKernBeforeDegree:
    case kRadicalKernAfterDegree:
      return std::optional<float>(HarfBuzzUnitsToFloat(harfbuzz_value));
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return std::nullopt;
}

std::optional<float> OpenTypeMathSupport::MathItalicCorrection(
    const HarfBuzzFace* harfbuzz_face,
    Glyph glyph) {
  if (!HasMathData(harfbuzz_face)) {
    return std::nullopt;
  }

  hb_font_t* const font = harfbuzz_face->GetScaledFont();

  return std::optional<float>(HarfBuzzUnitsToFloat(
      hb_ot_math_get_glyph_italics_correction(font, glyph)));
}

template <typename HarfBuzzRecordType>
using GetHarfBuzzMathRecordGetter =
    base::OnceCallback<unsigned int(hb_font_t* font,
                                    hb_codepoint_t glyph,
                                    hb_direction_t direction,
                                    unsigned int start_offset,
                                    unsigned int* record_count,
                                    HarfBuzzRecordType* record_array)>;

template <typename HarfBuzzRecordType, typename RecordType>
using HarfBuzzMathRecordConverter =
    base::RepeatingCallback<RecordType(HarfBuzzRecordType)>;

template <typename HarfBuzzRecordType, typename RecordType>
Vector<RecordType> GetHarfBuzzMathRecord(
    const HarfBuzzFace* harfbuzz_face,
    Glyph base_glyph,
    OpenTypeMathStretchData::StretchAxis stretch_axis,
    GetHarfBuzzMathRecordGetter<HarfBuzzRecordType> getter,
    HarfBuzzMathRecordConverter<HarfBuzzRecordType, RecordType> converter,
    std::optional<RecordType> prepended_record) {
  hb_font_t* const hb_font = harfbuzz_face->GetScaledFont();
  DCHECK(hb_font);

  hb_direction_t hb_stretch_axis = HarfBuzzDirection(stretch_axis);

  // In practice, math fonts have, for a given base glyph and stretch axis only
  // provide a few GlyphVariantRecords (size variants of increasing sizes) and
  // GlyphPartRecords (parts of a glyph assembly) so it is safe to truncate
  // the result vector to a small size.
  HarfBuzzRecordType chunk[kMaxHarfBuzzRecords];
  unsigned int count = kMaxHarfBuzzRecords;
  std::move(getter).Run(hb_font, base_glyph, hb_stretch_axis,
                        0 /* start_offset */, &count, chunk);

  // Create the vector to the determined size and initialize it with the results
  // converted from HarfBuzz's ones, prepending any optional record.
  Vector<RecordType> result;
  result.ReserveInitialCapacity(prepended_record ? count + 1 : count);
  if (prepended_record)
    result.push_back(*prepended_record);
  for (unsigned i = 0; i < count; i++) {
    result.push_back(converter.Run(chunk[i]));
  }
  return result;
}

Vector<OpenTypeMathStretchData::GlyphVariantRecord>
OpenTypeMathSupport::GetGlyphVariantRecords(
    const HarfBuzzFace* harfbuzz_face,
    Glyph base_glyph,
    OpenTypeMathStretchData::StretchAxis stretch_axis) {
  DCHECK(harfbuzz_face);
  DCHECK(base_glyph);

  auto getter = WTF::BindOnce(&hb_ot_math_get_glyph_variants);
  auto converter =
      WTF::BindRepeating([](hb_ot_math_glyph_variant_t record)
                             -> OpenTypeMathStretchData::GlyphVariantRecord {
        return record.glyph;
      });
  return GetHarfBuzzMathRecord(
      harfbuzz_face, base_glyph, stretch_axis, std::move(getter),
      std::move(converter),
      std::optional<OpenTypeMathStretchData::GlyphVariantRecord>(base_glyph));
}

Vector<OpenTypeMathStretchData::GlyphPartRecord>
OpenTypeMathSupport::GetGlyphPartRecords(
    const HarfBuzzFace* harfbuzz_face,
    Glyph base_glyph,
    OpenTypeMathStretchData::StretchAxis stretch_axis,
    float* italic_correction) {
  DCHECK(harfbuzz_face);
  DCHECK(base_glyph);

  auto getter = WTF::BindOnce(
      [](hb_font_t* font, hb_codepoint_t glyph, hb_direction_t direction,
         unsigned int start_offset, unsigned int* parts_count,
         hb_ot_math_glyph_part_t* parts) {
        hb_position_t italic_correction;
        return hb_ot_math_get_glyph_assembly(font, glyph, direction,
                                             start_offset, parts_count, parts,
                                             &italic_correction);
      });
  auto converter =
      WTF::BindRepeating([](hb_ot_math_glyph_part_t record)
                             -> OpenTypeMathStretchData::GlyphPartRecord {
        return {static_cast<Glyph>(record.glyph),
                HarfBuzzUnitsToFloat(record.start_connector_length),
                HarfBuzzUnitsToFloat(record.end_connector_length),
                HarfBuzzUnitsToFloat(record.full_advance),
                !!(record.flags & HB_MATH_GLYPH_PART_FLAG_EXTENDER)};
      });
  Vector<OpenTypeMathStretchData::GlyphPartRecord> parts =
      GetHarfBuzzMathRecord(
          harfbuzz_face, base_glyph, stretch_axis, std::move(getter),
          std::move(converter),
          std::optional<OpenTypeMathStretchData::GlyphPartRecord>());
  if (italic_correction && !parts.empty()) {
    hb_font_t* const hb_font = harfbuzz_face->GetScaledFont();
    // A GlyphAssembly subtable exists for the specified font, glyph and stretch
    // axis since it has been possible to retrieve the GlyphPartRecords. This
    // means that the following call is guaranteed to get an italic correction.
    hb_position_t harfbuzz_italic_correction;
    hb_ot_math_get_glyph_assembly(hb_font, base_glyph,
                                  HarfBuzzDirection(stretch_axis), 0, nullptr,
                                  nullptr, &harfbuzz_italic_correction);
    *italic_correction = HarfBuzzUnitsToFloat(harfbuzz_italic_correction);
  }
  return parts;
}

}  // namespace blink
