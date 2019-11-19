/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_gradient_value.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_math_expression_node.h"
#include "third_party/blink/renderer/core/css/css_math_function_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/text_link_colors.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/color_blend.h"
#include "third_party/blink/renderer/platform/graphics/gradient.h"
#include "third_party/blink/renderer/platform/graphics/gradient_generated_image.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace cssvalue {

namespace {

bool ColorIsDerivedFromElement(const CSSIdentifierValue& value) {
  CSSValueID value_id = value.GetValueID();
  switch (value_id) {
    case CSSValueID::kInternalQuirkInherit:
    case CSSValueID::kWebkitLink:
    case CSSValueID::kWebkitActivelink:
    case CSSValueID::kCurrentcolor:
      return true;
    default:
      return false;
  }
}

bool AppendPosition(StringBuilder& result,
                    const CSSValue* x,
                    const CSSValue* y,
                    bool wrote_something) {
  if (!x && !y)
    return false;

  if (wrote_something)
    result.Append(' ');
  result.Append("at ");

  if (x) {
    result.Append(x->CssText());
    if (y)
      result.Append(' ');
  }

  if (y)
    result.Append(y->CssText());

  return true;
}

}  // anonymous ns

bool CSSGradientColorStop::IsCacheable() const {
  if (!IsHint()) {
    auto* identifier_value = DynamicTo<CSSIdentifierValue>(color_.Get());
    if (identifier_value && ColorIsDerivedFromElement(*identifier_value))
      return false;
  }

  // TODO(crbug.com/979895): This is the result of a refactoring, which might
  // have revealed an existing bug with calculated lengths. Investigate.
  return !offset_ || offset_->IsMathFunctionValue() ||
         !To<CSSNumericLiteralValue>(*offset_).IsFontRelativeLength();
}

void CSSGradientColorStop::Trace(blink::Visitor* visitor) {
  visitor->Trace(offset_);
  visitor->Trace(color_);
}

scoped_refptr<Image> CSSGradientValue::GetImage(
    const ImageResourceObserver& client,
    const Document& document,
    const ComputedStyle& style,
    const FloatSize& size) const {
  if (size.IsEmpty())
    return nullptr;

  if (is_cacheable_) {
    if (!Clients().Contains(&client))
      return nullptr;

    if (Image* result = CSSImageGeneratorValue::GetImage(&client, size))
      return result;
  }

  // We need to create an image.
  const ComputedStyle* root_style =
      document.documentElement()->GetComputedStyle();
  CSSToLengthConversionData conversion_data(
      &style, root_style, document.GetLayoutView(), style.EffectiveZoom());

  scoped_refptr<Gradient> gradient;
  switch (GetClassType()) {
    case kLinearGradientClass:
      gradient = To<CSSLinearGradientValue>(this)->CreateGradient(
          conversion_data, size, document, style);
      break;
    case kRadialGradientClass:
      gradient = To<CSSRadialGradientValue>(this)->CreateGradient(
          conversion_data, size, document, style);
      break;
    case kConicGradientClass:
      gradient = To<CSSConicGradientValue>(this)->CreateGradient(
          conversion_data, size, document, style);
      break;
    default:
      NOTREACHED();
  }

  scoped_refptr<Image> new_image =
      GradientGeneratedImage::Create(gradient, size);
  if (is_cacheable_)
    PutImage(size, new_image);

  return new_image;
}

// Should only ever be called for deprecated gradients.
static inline bool CompareStops(const CSSGradientColorStop& a,
                                const CSSGradientColorStop& b) {
  double a_val = a.offset_->GetDoubleValue();
  double b_val = b.offset_->GetDoubleValue();

  return a_val < b_val;
}

struct GradientStop {
  Color color;
  float offset;
  bool specified;

  GradientStop() : offset(0), specified(false) {}
};

struct CSSGradientValue::GradientDesc {
  STACK_ALLOCATED();

 public:
  GradientDesc(const FloatPoint& p0,
               const FloatPoint& p1,
               GradientSpreadMethod spread_method)
      : p0(p0), p1(p1), spread_method(spread_method) {}
  GradientDesc(const FloatPoint& p0,
               const FloatPoint& p1,
               float r0,
               float r1,
               GradientSpreadMethod spread_method)
      : p0(p0), p1(p1), r0(r0), r1(r1), spread_method(spread_method) {}

  Vector<Gradient::ColorStop> stops;
  FloatPoint p0, p1;
  float r0 = 0, r1 = 0;
  float start_angle = 0, end_angle = 360;
  GradientSpreadMethod spread_method;
};

static void ReplaceColorHintsWithColorStops(
    Vector<GradientStop>& stops,
    const HeapVector<CSSGradientColorStop, 2>& css_gradient_stops) {
  // This algorithm will replace each color interpolation hint with 9 regular
  // color stops. The color values for the new color stops will be calculated
  // using the color weighting formula defined in the spec. The new color
  // stops will be positioned in such a way that all the pixels between the two
  // user defined color stops have color values close to the interpolation
  // curve.
  // If the hint is closer to the left color stop, add 2 stops to the left and
  // 6 to the right, else add 6 stops to the left and 2 to the right.
  // The color stops on the side with more space start midway because
  // the curve approximates a line in that region.
  // Using this aproximation, it is possible to discern the color steps when
  // the gradient is large. If this becomes an issue, we can consider improving
  // the algorithm, or adding support for color interpolation hints to skia
  // shaders.

  int index_offset = 0;

  // The first and the last color stops cannot be color hints.
  for (wtf_size_t i = 1; i < css_gradient_stops.size() - 1; ++i) {
    if (!css_gradient_stops[i].IsHint())
      continue;

    // The current index of the stops vector.
    wtf_size_t x = i + index_offset;
    DCHECK_GE(x, 1u);

    // offsetLeft          offset                            offsetRight
    //   |-------------------|---------------------------------|
    //          leftDist                 rightDist

    float offset_left = stops[x - 1].offset;
    float offset_right = stops[x + 1].offset;
    float offset = stops[x].offset;
    float left_dist = offset - offset_left;
    float right_dist = offset_right - offset;
    float total_dist = offset_right - offset_left;

    Color left_color = stops[x - 1].color;
    Color right_color = stops[x + 1].color;

    DCHECK_LE(offset_left, offset);
    DCHECK_LE(offset, offset_right);

    if (WebCoreFloatNearlyEqual(left_dist, right_dist)) {
      stops.EraseAt(x);
      --index_offset;
      continue;
    }

    if (WebCoreFloatNearlyEqual(left_dist, .0f)) {
      stops[x].color = right_color;
      continue;
    }

    if (WebCoreFloatNearlyEqual(right_dist, .0f)) {
      stops[x].color = left_color;
      continue;
    }

    GradientStop new_stops[9];
    // Position the new color stops.
    if (left_dist > right_dist) {
      for (size_t y = 0; y < 7; ++y)
        new_stops[y].offset = offset_left + left_dist * (7 + y) / 13;
      new_stops[7].offset = offset + right_dist / 3;
      new_stops[8].offset = offset + right_dist * 2 / 3;
    } else {
      new_stops[0].offset = offset_left + left_dist / 3;
      new_stops[1].offset = offset_left + left_dist * 2 / 3;
      for (size_t y = 0; y < 7; ++y)
        new_stops[y + 2].offset = offset + right_dist * y / 13;
    }

    // calculate colors for the new color hints.
    // The color weighting for the new color stops will be
    // pointRelativeOffset^(ln(0.5)/ln(hintRelativeOffset)).
    float hint_relative_offset = left_dist / total_dist;
    for (size_t y = 0; y < 9; ++y) {
      float point_relative_offset =
          (new_stops[y].offset - offset_left) / total_dist;
      float weighting =
          powf(point_relative_offset, logf(.5f) / logf(hint_relative_offset));
      new_stops[y].color = Blend(left_color, right_color, weighting);
    }

    // Replace the color hint with the new color stops.
    stops.EraseAt(x);
    stops.insert(x, new_stops, 9);
    index_offset += 8;
  }
}

static Color ResolveStopColor(const CSSValue& stop_color,
                              const Document& document,
                              const ComputedStyle& style) {
  return document.GetTextLinkColors().ColorFromCSSValue(
      stop_color, style.VisitedDependentColor(GetCSSPropertyColor()),
      style.UsedColorScheme());
}

void CSSGradientValue::AddDeprecatedStops(GradientDesc& desc,
                                          const Document& document,
                                          const ComputedStyle& style) const {
  DCHECK(gradient_type_ == kCSSDeprecatedLinearGradient ||
         gradient_type_ == kCSSDeprecatedRadialGradient);

  // Performance here is probably not important because this is for deprecated
  // gradients.
  auto stops_sorted = stops_;
  std::stable_sort(stops_sorted.begin(), stops_sorted.end(), CompareStops);

  for (const auto& stop : stops_sorted) {
    float offset;
    if (stop.offset_->IsPercentage())
      offset = stop.offset_->GetFloatValue() / 100;
    else
      offset = stop.offset_->GetFloatValue();

    const Color color = ResolveStopColor(*stop.color_, document, style);
    desc.stops.emplace_back(offset, color);
  }
}

void CSSGradientValue::AddComputedStops(
    const ComputedStyle& style,
    bool allow_visited_style,
    const HeapVector<CSSGradientColorStop, 2>& stops) {
  for (unsigned index = 0; index < stops.size(); ++index) {
    CSSGradientColorStop stop = stops[index];
    CSSValueID value_id = CSSValueID::kInvalid;
    if (stop.color_ && stop.color_->IsIdentifierValue())
      value_id = To<CSSIdentifierValue>(*stop.color_).GetValueID();

    switch (value_id) {
      case CSSValueID::kInvalid:
      case CSSValueID::kInternalQuirkInherit:
      case CSSValueID::kWebkitLink:
      case CSSValueID::kWebkitActivelink:
      case CSSValueID::kWebkitFocusRingColor:
      case CSSValueID::kInternalRootColor:
        break;
      case CSSValueID::kCurrentcolor:
        if (allow_visited_style) {
          stop.color_ = CSSColorValue::Create(
              style.VisitedDependentColor(GetCSSPropertyColor()).Rgb());
        } else {
          stop.color_ =
              ComputedStyleUtils::CurrentColorOrValidColor(style, StyleColor());
        }
        break;
      default:
        stop.color_ = CSSColorValue::Create(
            StyleColor::ColorFromKeyword(
                value_id, ComputedStyle::InitialStyle().UsedColorScheme())
                .Rgb());
    }
    AddStop(stop);
  }
}

namespace {

bool RequiresStopsNormalization(const Vector<GradientStop>& stops,
                                CSSGradientValue::GradientDesc& desc) {
  // We need at least two stops to normalize
  if (stops.size() < 2)
    return false;

  // Repeating gradients are implemented using a normalized stop offset range
  // with the point/radius pairs aligned on the interval endpoints.
  if (desc.spread_method == kSpreadMethodRepeat)
    return true;

  // Degenerate stops
  if (stops.front().offset < 0 || stops.back().offset > 1)
    return true;

  return false;
}

// Redistribute the stops such that they fully cover [0 , 1] and add them to the
// gradient.
bool NormalizeAndAddStops(const Vector<GradientStop>& stops,
                          CSSGradientValue::GradientDesc& desc) {
  DCHECK_GT(stops.size(), 1u);

  const float first_offset = stops.front().offset;
  const float last_offset = stops.back().offset;
  const float span = last_offset - first_offset;

  if (fabs(span) < std::numeric_limits<float>::epsilon()) {
    // All stops are coincident -> use a single clamped offset value.
    const float clamped_offset = std::min(std::max(first_offset, 0.f), 1.f);

    // For repeating gradients, a coincident stop set defines a solid-color
    // image with the color of the last color-stop in the rule.
    // For non-repeating gradients, both the first color and the last color can
    // be significant (padding on both sides of the offset).
    if (desc.spread_method != kSpreadMethodRepeat)
      desc.stops.emplace_back(clamped_offset, stops.front().color);
    desc.stops.emplace_back(clamped_offset, stops.back().color);

    return false;
  }

  DCHECK_GT(span, 0);

  for (wtf_size_t i = 0; i < stops.size(); ++i) {
    const float normalized_offset = (stops[i].offset - first_offset) / span;

    // stop offsets should be monotonically increasing in [0 , 1]
    DCHECK_GE(normalized_offset, 0);
    DCHECK_LE(normalized_offset, 1);
    DCHECK(i == 0 ||
           normalized_offset >= (stops[i - 1].offset - first_offset) / span);

    desc.stops.emplace_back(normalized_offset, stops[i].color);
  }

  return true;
}

// Collapse all negative-offset stops to 0 and compute an interpolated color
// value for that point.
void ClampNegativeOffsets(Vector<GradientStop>& stops) {
  float last_negative_offset = 0;

  for (wtf_size_t i = 0; i < stops.size(); ++i) {
    const float current_offset = stops[i].offset;
    if (current_offset >= 0) {
      if (i > 0) {
        // We found the negative -> positive offset transition: compute an
        // interpolated color value for 0 and use it with the last clamped stop.
        DCHECK_LT(last_negative_offset, 0);
        float lerp_ratio =
            -last_negative_offset / (current_offset - last_negative_offset);
        stops[i - 1].color =
            Blend(stops[i - 1].color, stops[i].color, lerp_ratio);
      }

      break;
    }

    // Clamp all negative stops to 0.
    stops[i].offset = 0;
    last_negative_offset = current_offset;
  }
}

template <typename T>
std::tuple<T, T> AdjustedGradientDomainForOffsetRange(const T& v0,
                                                      const T& v1,
                                                      float first_offset,
                                                      float last_offset) {
  DCHECK_LE(first_offset, last_offset);

  const auto d = v1 - v0;

  // The offsets are relative to the [v0 , v1] segment.
  return std::make_tuple(v0 + d * first_offset, v0 + d * last_offset);
}

// Update the radial gradient radii to align with the given offset range.
void AdjustGradientRadiiForOffsetRange(CSSGradientValue::GradientDesc& desc,
                                       float first_offset,
                                       float last_offset) {
  DCHECK_LE(first_offset, last_offset);

  // Radial offsets are relative to the [0 , endRadius] segment.
  float adjusted_r0 = desc.r1 * first_offset;
  float adjusted_r1 = desc.r1 * last_offset;
  DCHECK_LE(adjusted_r0, adjusted_r1);
  // Unlike linear gradients (where we can adjust the points arbitrarily),
  // we cannot let our radii turn negative here.
  if (adjusted_r0 < 0) {
    // For the non-repeat case, this can never happen: clampNegativeOffsets()
    // ensures we don't have to deal with negative offsets at this point.

    DCHECK_EQ(desc.spread_method, kSpreadMethodRepeat);

    // When in repeat mode, we deal with it by repositioning both radii in the
    // positive domain - shifting them by a multiple of the radius span (which
    // is the period of our repeating gradient -> hence no visible side
    // effects).
    const float radius_span = adjusted_r1 - adjusted_r0;
    const float shift_to_positive =
        radius_span * ceilf(-adjusted_r0 / radius_span);
    adjusted_r0 += shift_to_positive;
    adjusted_r1 += shift_to_positive;
  }
  DCHECK_GE(adjusted_r0, 0);
  DCHECK_GE(adjusted_r1, adjusted_r0);

  desc.r0 = adjusted_r0;
  desc.r1 = adjusted_r1;
}

}  // anonymous ns

void CSSGradientValue::AddStops(
    CSSGradientValue::GradientDesc& desc,
    const CSSToLengthConversionData& conversion_data,
    const Document& document,
    const ComputedStyle& style) const {
  if (gradient_type_ == kCSSDeprecatedLinearGradient ||
      gradient_type_ == kCSSDeprecatedRadialGradient) {
    AddDeprecatedStops(desc, document, style);
    return;
  }

  wtf_size_t num_stops = stops_.size();

  Vector<GradientStop> stops(num_stops);

  float gradient_length;
  switch (GetClassType()) {
    case kLinearGradientClass:
      gradient_length = FloatSize(desc.p1 - desc.p0).DiagonalLength();
      break;
    case kRadialGradientClass:
      gradient_length = desc.r1;
      break;
    case kConicGradientClass:
      gradient_length = 1;
      break;
    default:
      NOTREACHED();
      gradient_length = 0;
  }

  bool has_hints = false;
  for (wtf_size_t i = 0; i < num_stops; ++i) {
    const CSSGradientColorStop& stop = stops_[i];

    if (stop.IsHint())
      has_hints = true;
    else
      stops[i].color = ResolveStopColor(*stop.color_, document, style);

    if (stop.offset_) {
      if (stop.offset_->IsPercentage()) {
        stops[i].offset = stop.offset_->GetFloatValue() / 100;
      } else if (stop.offset_->IsLength() ||
                 stop.offset_->IsCalculatedPercentageWithLength()) {
        float length;
        if (stop.offset_->IsLength()) {
          length = stop.offset_->ComputeLength<float>(conversion_data);
        } else {
          length = To<CSSMathFunctionValue>(stop.offset_.Get())
                       ->ToCalcValue(conversion_data)
                       ->Evaluate(gradient_length);
        }
        stops[i].offset = (gradient_length > 0) ? length / gradient_length : 0;
      } else if (stop.offset_->IsAngle()) {
        stops[i].offset = stop.offset_->ComputeDegrees() / 360.0f;
      } else {
        NOTREACHED();
        stops[i].offset = 0;
      }
      stops[i].specified = true;
    } else {
      // If the first color-stop does not have a position, its position defaults
      // to 0%. If the last color-stop does not have a position, its position
      // defaults to 100%.
      if (!i) {
        stops[i].offset = 0;
        stops[i].specified = true;
      } else if (num_stops > 1 && i == num_stops - 1) {
        stops[i].offset = 1;
        stops[i].specified = true;
      }
    }

    // If a color-stop has a position that is less than the specified position
    // of any color-stop before it in the list, its position is changed to be
    // equal to the largest specified position of any color-stop before it.
    if (stops[i].specified && i > 0) {
      wtf_size_t prev_specified_index;
      for (prev_specified_index = i - 1; prev_specified_index;
           --prev_specified_index) {
        if (stops[prev_specified_index].specified)
          break;
      }

      if (stops[i].offset < stops[prev_specified_index].offset)
        stops[i].offset = stops[prev_specified_index].offset;
    }
  }

  DCHECK(stops.front().specified);
  DCHECK(stops.back().specified);

  // If any color-stop still does not have a position, then, for each run of
  // adjacent color-stops without positions, set their positions so that they
  // are evenly spaced between the preceding and following color-stops with
  // positions.
  if (num_stops > 2) {
    wtf_size_t unspecified_run_start = 0;
    bool in_unspecified_run = false;

    for (wtf_size_t i = 0; i < num_stops; ++i) {
      if (!stops[i].specified && !in_unspecified_run) {
        unspecified_run_start = i;
        in_unspecified_run = true;
      } else if (stops[i].specified && in_unspecified_run) {
        wtf_size_t unspecified_run_end = i;

        if (unspecified_run_start < unspecified_run_end) {
          float last_specified_offset = stops[unspecified_run_start - 1].offset;
          float next_specified_offset = stops[unspecified_run_end].offset;
          float delta = (next_specified_offset - last_specified_offset) /
                        (unspecified_run_end - unspecified_run_start + 1);

          for (wtf_size_t j = unspecified_run_start; j < unspecified_run_end;
               ++j)
            stops[j].offset =
                last_specified_offset + (j - unspecified_run_start + 1) * delta;
        }

        in_unspecified_run = false;
      }
    }
  }

  DCHECK_EQ(stops.size(), stops_.size());
  if (has_hints) {
    ReplaceColorHintsWithColorStops(stops, stops_);
  }

  // At this point we have a fully resolved set of stops. Time to perform
  // adjustments for repeat gradients and degenerate values if needed.
  if (!RequiresStopsNormalization(stops, desc)) {
    // No normalization required, just add the current stops.
    for (const auto& stop : stops)
      desc.stops.emplace_back(stop.offset, stop.color);
    return;
  }

  switch (GetClassType()) {
    case kLinearGradientClass:
      if (NormalizeAndAddStops(stops, desc)) {
        std::tie(desc.p0, desc.p1) = AdjustedGradientDomainForOffsetRange(
            desc.p0, desc.p1, stops.front().offset, stops.back().offset);
      }
      break;
    case kRadialGradientClass:
      // Negative offsets are only an issue for non-repeating radial gradients:
      // linear gradient points can be repositioned arbitrarily, and for
      // repeating radial gradients we shift the radii into equivalent positive
      // values.
      if (!repeating_)
        ClampNegativeOffsets(stops);

      if (NormalizeAndAddStops(stops, desc)) {
        AdjustGradientRadiiForOffsetRange(desc, stops.front().offset,
                                          stops.back().offset);
      }
      break;
    case kConicGradientClass:
      if (NormalizeAndAddStops(stops, desc)) {
        std::tie(desc.start_angle, desc.end_angle) =
            AdjustedGradientDomainForOffsetRange(
                desc.start_angle, desc.end_angle, stops.front().offset,
                stops.back().offset);
      }
      break;
    default:
      NOTREACHED();
  }
}

static float PositionFromValue(const CSSValue* value,
                               const CSSToLengthConversionData& conversion_data,
                               const FloatSize& size,
                               bool is_horizontal) {
  float origin = 0;
  int sign = 1;
  float edge_distance = is_horizontal ? size.Width() : size.Height();

  // In this case the center of the gradient is given relative to an edge in the
  // form of: [ top | bottom | right | left ] [ <percentage> | <length> ].
  if (const auto* pair = DynamicTo<CSSValuePair>(*value)) {
    CSSValueID origin_id = To<CSSIdentifierValue>(pair->First()).GetValueID();
    value = &pair->Second();

    if (origin_id == CSSValueID::kRight || origin_id == CSSValueID::kBottom) {
      // For right/bottom, the offset is relative to the far edge.
      origin = edge_distance;
      sign = -1;
    }
  }

  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    switch (identifier_value->GetValueID()) {
      case CSSValueID::kTop:
        DCHECK(!is_horizontal);
        return 0;
      case CSSValueID::kLeft:
        DCHECK(is_horizontal);
        return 0;
      case CSSValueID::kBottom:
        DCHECK(!is_horizontal);
        return size.Height();
      case CSSValueID::kRight:
        DCHECK(is_horizontal);
        return size.Width();
      case CSSValueID::kCenter:
        return origin + sign * .5f * edge_distance;
      default:
        NOTREACHED();
        break;
    }
  }

  const CSSPrimitiveValue* primitive_value = To<CSSPrimitiveValue>(value);

  if (primitive_value->IsNumber())
    return origin +
           sign * primitive_value->GetFloatValue() * conversion_data.Zoom();

  if (primitive_value->IsPercentage())
    return origin +
           sign * primitive_value->GetFloatValue() / 100.f * edge_distance;

  if (primitive_value->IsCalculatedPercentageWithLength())
    return origin + sign * To<CSSMathFunctionValue>(primitive_value)
                               ->ToCalcValue(conversion_data)
                               ->Evaluate(edge_distance);

  return origin + sign * primitive_value->ComputeLength<float>(conversion_data);
}

// Resolve points/radii to front end values.
static FloatPoint ComputeEndPoint(
    const CSSValue* horizontal,
    const CSSValue* vertical,
    const CSSToLengthConversionData& conversion_data,
    const FloatSize& size) {
  FloatPoint result;

  if (horizontal)
    result.SetX(PositionFromValue(horizontal, conversion_data, size, true));

  if (vertical)
    result.SetY(PositionFromValue(vertical, conversion_data, size, false));

  return result;
}

bool CSSGradientValue::KnownToBeOpaque(const Document& document,
                                       const ComputedStyle& style) const {
  for (auto& stop : stops_) {
    if (!stop.IsHint() &&
        ResolveStopColor(*stop.color_, document, style).HasAlpha())
      return false;
  }
  return true;
}

Vector<Color> CSSGradientValue::GetStopColors(
    const Document& document,
    const ComputedStyle& style) const {
  Vector<Color> stop_colors;
  for (const auto& stop : stops_) {
    if (!stop.IsHint())
      stop_colors.push_back(ResolveStopColor(*stop.color_, document, style));
  }
  return stop_colors;
}

void CSSGradientValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(stops_);
  CSSImageGeneratorValue::TraceAfterDispatch(visitor);
}

String CSSLinearGradientValue::CustomCSSText() const {
  StringBuilder result;
  if (gradient_type_ == kCSSDeprecatedLinearGradient) {
    result.Append("-webkit-gradient(linear, ");
    result.Append(first_x_->CssText());
    result.Append(' ');
    result.Append(first_y_->CssText());
    result.Append(", ");
    result.Append(second_x_->CssText());
    result.Append(' ');
    result.Append(second_y_->CssText());
    AppendCSSTextForDeprecatedColorStops(result);
  } else if (gradient_type_ == kCSSPrefixedLinearGradient) {
    if (repeating_)
      result.Append("-webkit-repeating-linear-gradient(");
    else
      result.Append("-webkit-linear-gradient(");

    if (angle_)
      result.Append(angle_->CssText());
    else {
      if (first_x_ && first_y_) {
        result.Append(first_x_->CssText());
        result.Append(' ');
        result.Append(first_y_->CssText());
      } else if (first_x_ || first_y_) {
        if (first_x_)
          result.Append(first_x_->CssText());

        if (first_y_)
          result.Append(first_y_->CssText());
      }
    }

    constexpr bool kAppendSeparator = true;
    AppendCSSTextForColorStops(result, kAppendSeparator);
  } else {
    if (repeating_)
      result.Append("repeating-linear-gradient(");
    else
      result.Append("linear-gradient(");

    bool wrote_something = false;

    if (angle_ && angle_->ComputeDegrees() != 180) {
      result.Append(angle_->CssText());
      wrote_something = true;
    } else if ((first_x_ || first_y_) &&
               !(!first_x_ && first_y_ && first_y_->IsIdentifierValue() &&
                 To<CSSIdentifierValue>(first_y_.Get())->GetValueID() ==
                     CSSValueID::kBottom)) {
      result.Append("to ");
      if (first_x_ && first_y_) {
        result.Append(first_x_->CssText());
        result.Append(' ');
        result.Append(first_y_->CssText());
      } else if (first_x_)
        result.Append(first_x_->CssText());
      else
        result.Append(first_y_->CssText());
      wrote_something = true;
    }

    AppendCSSTextForColorStops(result, wrote_something);
  }

  result.Append(')');
  return result.ToString();
}

// Compute the endpoints so that a gradient of the given angle covers a box of
// the given size.
static void EndPointsFromAngle(float angle_deg,
                               const FloatSize& size,
                               FloatPoint& first_point,
                               FloatPoint& second_point,
                               CSSGradientType type) {
  // Prefixed gradients use "polar coordinate" angles, rather than "bearing"
  // angles.
  if (type == kCSSPrefixedLinearGradient)
    angle_deg = 90 - angle_deg;

  angle_deg = fmodf(angle_deg, 360);
  if (angle_deg < 0)
    angle_deg += 360;

  if (!angle_deg) {
    first_point.Set(0, size.Height());
    second_point.Set(0, 0);
    return;
  }

  if (angle_deg == 90) {
    first_point.Set(0, 0);
    second_point.Set(size.Width(), 0);
    return;
  }

  if (angle_deg == 180) {
    first_point.Set(0, 0);
    second_point.Set(0, size.Height());
    return;
  }

  if (angle_deg == 270) {
    first_point.Set(size.Width(), 0);
    second_point.Set(0, 0);
    return;
  }

  // angleDeg is a "bearing angle" (0deg = N, 90deg = E),
  // but tan expects 0deg = E, 90deg = N.
  float slope = tan(deg2rad(90 - angle_deg));

  // We find the endpoint by computing the intersection of the line formed by
  // the slope, and a line perpendicular to it that intersects the corner.
  float perpendicular_slope = -1 / slope;

  // Compute start corner relative to center, in Cartesian space (+y = up).
  float half_height = size.Height() / 2;
  float half_width = size.Width() / 2;
  FloatPoint end_corner;
  if (angle_deg < 90)
    end_corner.Set(half_width, half_height);
  else if (angle_deg < 180)
    end_corner.Set(half_width, -half_height);
  else if (angle_deg < 270)
    end_corner.Set(-half_width, -half_height);
  else
    end_corner.Set(-half_width, half_height);

  // Compute c (of y = mx + c) using the corner point.
  float c = end_corner.Y() - perpendicular_slope * end_corner.X();
  float end_x = c / (slope - perpendicular_slope);
  float end_y = perpendicular_slope * end_x + c;

  // We computed the end point, so set the second point, taking into account the
  // moved origin and the fact that we're in drawing space (+y = down).
  second_point.Set(half_width + end_x, half_height - end_y);
  // Reflect around the center for the start point.
  first_point.Set(half_width - end_x, half_height + end_y);
}

scoped_refptr<Gradient> CSSLinearGradientValue::CreateGradient(
    const CSSToLengthConversionData& conversion_data,
    const FloatSize& size,
    const Document& document,
    const ComputedStyle& style) const {
  DCHECK(!size.IsEmpty());

  FloatPoint first_point;
  FloatPoint second_point;
  if (angle_) {
    float angle = angle_->ComputeDegrees();
    EndPointsFromAngle(angle, size, first_point, second_point, gradient_type_);
  } else {
    switch (gradient_type_) {
      case kCSSDeprecatedLinearGradient:
        first_point = ComputeEndPoint(first_x_.Get(), first_y_.Get(),
                                      conversion_data, size);
        if (second_x_ || second_y_)
          second_point = ComputeEndPoint(second_x_.Get(), second_y_.Get(),
                                         conversion_data, size);
        else {
          if (first_x_)
            second_point.SetX(size.Width() - first_point.X());
          if (first_y_)
            second_point.SetY(size.Height() - first_point.Y());
        }
        break;
      case kCSSPrefixedLinearGradient:
        first_point = ComputeEndPoint(first_x_.Get(), first_y_.Get(),
                                      conversion_data, size);
        if (first_x_)
          second_point.SetX(size.Width() - first_point.X());
        if (first_y_)
          second_point.SetY(size.Height() - first_point.Y());
        break;
      case kCSSLinearGradient:
        if (first_x_ && first_y_) {
          // "Magic" corners, so the 50% line touches two corners.
          float rise = size.Width();
          float run = size.Height();
          auto* first_x_identifier_value =
              DynamicTo<CSSIdentifierValue>(first_x_.Get());
          if (first_x_identifier_value &&
              first_x_identifier_value->GetValueID() == CSSValueID::kLeft)
            run *= -1;
          auto* first_y_identifier_value =
              DynamicTo<CSSIdentifierValue>(first_y_.Get());
          if (first_y_identifier_value &&
              first_y_identifier_value->GetValueID() == CSSValueID::kBottom)
            rise *= -1;
          // Compute angle, and flip it back to "bearing angle" degrees.
          float angle = 90 - rad2deg(atan2(rise, run));
          EndPointsFromAngle(angle, size, first_point, second_point,
                             gradient_type_);
        } else if (first_x_ || first_y_) {
          second_point = ComputeEndPoint(first_x_.Get(), first_y_.Get(),
                                         conversion_data, size);
          if (first_x_)
            first_point.SetX(size.Width() - second_point.X());
          if (first_y_)
            first_point.SetY(size.Height() - second_point.Y());
        } else
          second_point.SetY(size.Height());
        break;
      default:
        NOTREACHED();
    }
  }

  GradientDesc desc(first_point, second_point,
                    repeating_ ? kSpreadMethodRepeat : kSpreadMethodPad);
  AddStops(desc, conversion_data, document, style);

  scoped_refptr<Gradient> gradient =
      Gradient::CreateLinear(desc.p0, desc.p1, desc.spread_method,
                             Gradient::ColorInterpolation::kPremultiplied);

  // Now add the stops.
  gradient->AddColorStops(desc.stops);

  return gradient;
}

bool CSSLinearGradientValue::Equals(const CSSLinearGradientValue& other) const {
  if (gradient_type_ == kCSSDeprecatedLinearGradient)
    return other.gradient_type_ == gradient_type_ &&
           DataEquivalent(first_x_, other.first_x_) &&
           DataEquivalent(first_y_, other.first_y_) &&
           DataEquivalent(second_x_, other.second_x_) &&
           DataEquivalent(second_y_, other.second_y_) && stops_ == other.stops_;

  if (repeating_ != other.repeating_)
    return false;

  if (angle_)
    return DataEquivalent(angle_, other.angle_) && stops_ == other.stops_;

  if (other.angle_)
    return false;

  bool equal_xand_y = false;
  if (first_x_ && first_y_) {
    equal_xand_y = DataEquivalent(first_x_, other.first_x_) &&
                   DataEquivalent(first_y_, other.first_y_);
  } else if (first_x_) {
    equal_xand_y = DataEquivalent(first_x_, other.first_x_) && !other.first_y_;
  } else if (first_y_) {
    equal_xand_y = DataEquivalent(first_y_, other.first_y_) && !other.first_x_;
  } else {
    equal_xand_y = !other.first_x_ && !other.first_y_;
  }

  return equal_xand_y && stops_ == other.stops_;
}

CSSLinearGradientValue* CSSLinearGradientValue::ComputedCSSValue(
    const ComputedStyle& style,
    bool allow_visited_style) {
  CSSLinearGradientValue* result = MakeGarbageCollected<CSSLinearGradientValue>(
      first_x_, first_y_, second_x_, second_y_, angle_,
      repeating_ ? kRepeating : kNonRepeating, GradientType());
  result->AddComputedStops(style, allow_visited_style, stops_);
  return result;
}

void CSSLinearGradientValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(first_x_);
  visitor->Trace(first_y_);
  visitor->Trace(second_x_);
  visitor->Trace(second_y_);
  visitor->Trace(angle_);
  CSSGradientValue::TraceAfterDispatch(visitor);
}

void CSSGradientValue::AppendCSSTextForColorStops(
    StringBuilder& result,
    bool requires_separator) const {
  for (const auto& stop : stops_) {
    if (requires_separator) {
      result.Append(", ");
    } else {
      requires_separator = true;
    }

    if (stop.color_)
      result.Append(stop.color_->CssText());
    if (stop.color_ && stop.offset_)
      result.Append(' ');
    if (stop.offset_)
      result.Append(stop.offset_->CssText());
  }
}

void CSSGradientValue::AppendCSSTextForDeprecatedColorStops(
    StringBuilder& result) const {
  for (unsigned i = 0; i < stops_.size(); i++) {
    const CSSGradientColorStop& stop = stops_[i];
    result.Append(", ");
    if (stop.offset_->GetDoubleValue() == 0) {
      result.Append("from(");
      result.Append(stop.color_->CssText());
      result.Append(')');
    } else if (stop.offset_->GetDoubleValue() == 1) {
      result.Append("to(");
      result.Append(stop.color_->CssText());
      result.Append(')');
    } else {
      result.Append("color-stop(");
      result.AppendNumber(stop.offset_->GetDoubleValue());
      result.Append(", ");
      result.Append(stop.color_->CssText());
      result.Append(')');
    }
  }
}

String CSSRadialGradientValue::CustomCSSText() const {
  StringBuilder result;

  if (gradient_type_ == kCSSDeprecatedRadialGradient) {
    result.Append("-webkit-gradient(radial, ");
    result.Append(first_x_->CssText());
    result.Append(' ');
    result.Append(first_y_->CssText());
    result.Append(", ");
    result.Append(first_radius_->CssText());
    result.Append(", ");
    result.Append(second_x_->CssText());
    result.Append(' ');
    result.Append(second_y_->CssText());
    result.Append(", ");
    result.Append(second_radius_->CssText());
    AppendCSSTextForDeprecatedColorStops(result);
  } else if (gradient_type_ == kCSSPrefixedRadialGradient) {
    if (repeating_)
      result.Append("-webkit-repeating-radial-gradient(");
    else
      result.Append("-webkit-radial-gradient(");

    if (first_x_ && first_y_) {
      result.Append(first_x_->CssText());
      result.Append(' ');
      result.Append(first_y_->CssText());
    } else if (first_x_)
      result.Append(first_x_->CssText());
    else if (first_y_)
      result.Append(first_y_->CssText());
    else
      result.Append("center");

    if (shape_ || sizing_behavior_) {
      result.Append(", ");
      if (shape_) {
        result.Append(shape_->CssText());
        result.Append(' ');
      } else {
        result.Append("ellipse ");
      }

      if (sizing_behavior_)
        result.Append(sizing_behavior_->CssText());
      else
        result.Append("cover");

    } else if (end_horizontal_size_ && end_vertical_size_) {
      result.Append(", ");
      result.Append(end_horizontal_size_->CssText());
      result.Append(' ');
      result.Append(end_vertical_size_->CssText());
    }

    constexpr bool kAppendSeparator = true;
    AppendCSSTextForColorStops(result, kAppendSeparator);
  } else {
    if (repeating_)
      result.Append("repeating-radial-gradient(");
    else
      result.Append("radial-gradient(");

    bool wrote_something = false;

    // The only ambiguous case that needs an explicit shape to be provided
    // is when a sizing keyword is used (or all sizing is omitted).
    if (shape_ && shape_->GetValueID() != CSSValueID::kEllipse &&
        (sizing_behavior_ || (!sizing_behavior_ && !end_horizontal_size_))) {
      result.Append("circle");
      wrote_something = true;
    }

    if (sizing_behavior_ &&
        sizing_behavior_->GetValueID() != CSSValueID::kFarthestCorner) {
      if (wrote_something)
        result.Append(' ');
      result.Append(sizing_behavior_->CssText());
      wrote_something = true;
    } else if (end_horizontal_size_) {
      if (wrote_something)
        result.Append(' ');
      result.Append(end_horizontal_size_->CssText());
      if (end_vertical_size_) {
        result.Append(' ');
        result.Append(end_vertical_size_->CssText());
      }
      wrote_something = true;
    }

    wrote_something |=
        AppendPosition(result, first_x_, first_y_, wrote_something);

    AppendCSSTextForColorStops(result, wrote_something);
  }

  result.Append(')');
  return result.ToString();
}

namespace {

// Resolve points/radii to front end values.
float ResolveRadius(const CSSPrimitiveValue* radius,
                    const CSSToLengthConversionData& conversion_data,
                    float* width_or_height = nullptr) {
  float result = 0;
  if (radius->IsNumber())
    result = radius->GetFloatValue() * conversion_data.Zoom();
  else if (width_or_height && radius->IsPercentage())
    result = *width_or_height * radius->GetFloatValue() / 100;
  else
    result = radius->ComputeLength<float>(conversion_data);

  return clampTo<float>(std::max(result, 0.0f));
}

enum EndShapeType { kCircleEndShape, kEllipseEndShape };

// Compute the radius to the closest/farthest side (depending on the compare
// functor).
FloatSize RadiusToSide(const FloatPoint& point,
                       const FloatSize& size,
                       EndShapeType shape,
                       bool (*compare)(float, float)) {
  float dx1 = clampTo<float>(fabs(point.X()));
  float dy1 = clampTo<float>(fabs(point.Y()));
  float dx2 = clampTo<float>(fabs(point.X() - size.Width()));
  float dy2 = clampTo<float>(fabs(point.Y() - size.Height()));

  float dx = compare(dx1, dx2) ? dx1 : dx2;
  float dy = compare(dy1, dy2) ? dy1 : dy2;

  if (shape == kCircleEndShape)
    return compare(dx, dy) ? FloatSize(dx, dx) : FloatSize(dy, dy);

  DCHECK_EQ(shape, kEllipseEndShape);
  return FloatSize(dx, dy);
}

// Compute the radius of an ellipse with center at 0,0 which passes through p,
// and has width/height given by aspectRatio.
inline FloatSize EllipseRadius(const FloatPoint& p, float aspect_ratio) {
  // If the aspectRatio is 0 or infinite, the ellipse is completely flat.
  // TODO(sashab): Implement Degenerate Radial Gradients, see crbug.com/635727.
  if (aspect_ratio == 0 || std::isinf(aspect_ratio))
    return FloatSize(0, 0);

  // x^2/a^2 + y^2/b^2 = 1
  // a/b = aspectRatio, b = a/aspectRatio
  // a = sqrt(x^2 + y^2/(1/r^2))
  float a = sqrtf(p.X() * p.X() + p.Y() * p.Y() * aspect_ratio * aspect_ratio);
  return FloatSize(clampTo<float>(a), clampTo<float>(a / aspect_ratio));
}

// Compute the radius to the closest/farthest corner (depending on the compare
// functor).
FloatSize RadiusToCorner(const FloatPoint& point,
                         const FloatSize& size,
                         EndShapeType shape,
                         bool (*compare)(float, float)) {
  const FloatRect rect(FloatPoint(), size);
  const FloatPoint corners[] = {rect.MinXMinYCorner(), rect.MaxXMinYCorner(),
                                rect.MaxXMaxYCorner(), rect.MinXMaxYCorner()};

  unsigned corner_index = 0;
  float distance = (point - corners[corner_index]).DiagonalLength();
  for (unsigned i = 1; i < base::size(corners); ++i) {
    float new_distance = (point - corners[i]).DiagonalLength();
    if (compare(new_distance, distance)) {
      corner_index = i;
      distance = new_distance;
    }
  }

  if (shape == kCircleEndShape) {
    distance = clampTo<float>(distance);
    return FloatSize(distance, distance);
  }

  DCHECK_EQ(shape, kEllipseEndShape);
  // If the end shape is an ellipse, the gradient-shape has the same ratio of
  // width to height that it would if closest-side or farthest-side were
  // specified, as appropriate.
  const FloatSize side_radius =
      RadiusToSide(point, size, kEllipseEndShape, compare);

  return EllipseRadius(FloatPoint(corners[corner_index] - point),
                       side_radius.AspectRatio());
}

}  // anonymous namespace

scoped_refptr<Gradient> CSSRadialGradientValue::CreateGradient(
    const CSSToLengthConversionData& conversion_data,
    const FloatSize& size,
    const Document& document,
    const ComputedStyle& style) const {
  DCHECK(!size.IsEmpty());

  FloatPoint first_point =
      ComputeEndPoint(first_x_.Get(), first_y_.Get(), conversion_data, size);
  if (!first_x_)
    first_point.SetX(size.Width() / 2);
  if (!first_y_)
    first_point.SetY(size.Height() / 2);

  FloatPoint second_point =
      ComputeEndPoint(second_x_.Get(), second_y_.Get(), conversion_data, size);
  if (!second_x_)
    second_point.SetX(size.Width() / 2);
  if (!second_y_)
    second_point.SetY(size.Height() / 2);

  float first_radius = 0;
  if (first_radius_)
    first_radius = ResolveRadius(first_radius_.Get(), conversion_data);

  FloatSize second_radius(0, 0);
  if (second_radius_) {
    second_radius.SetWidth(
        ResolveRadius(second_radius_.Get(), conversion_data));
    second_radius.SetHeight(second_radius.Width());
  } else if (end_horizontal_size_) {
    float width = size.Width();
    float height = size.Height();
    second_radius.SetWidth(
        ResolveRadius(end_horizontal_size_.Get(), conversion_data, &width));
    second_radius.SetHeight(
        end_vertical_size_
            ? ResolveRadius(end_vertical_size_.Get(), conversion_data, &height)
            : second_radius.Width());
  } else {
    EndShapeType shape =
        (shape_ && shape_->GetValueID() == CSSValueID::kCircle) ||
                (!shape_ && !sizing_behavior_ && end_horizontal_size_ &&
                 !end_vertical_size_)
            ? kCircleEndShape
            : kEllipseEndShape;

    switch (sizing_behavior_ ? sizing_behavior_->GetValueID()
                             : CSSValueID::kInvalid) {
      case CSSValueID::kContain:
      case CSSValueID::kClosestSide:
        second_radius = RadiusToSide(second_point, size, shape,
                                     [](float a, float b) { return a < b; });
        break;
      case CSSValueID::kFarthestSide:
        second_radius = RadiusToSide(second_point, size, shape,
                                     [](float a, float b) { return a > b; });
        break;
      case CSSValueID::kClosestCorner:
        second_radius = RadiusToCorner(second_point, size, shape,
                                       [](float a, float b) { return a < b; });
        break;
      default:
        second_radius = RadiusToCorner(second_point, size, shape,
                                       [](float a, float b) { return a > b; });
        break;
    }
  }

  DCHECK(std::isfinite(first_radius));
  DCHECK(std::isfinite(second_radius.Width()));
  DCHECK(std::isfinite(second_radius.Height()));

  bool is_degenerate = !second_radius.Width() || !second_radius.Height();
  GradientDesc desc(first_point, second_point, first_radius,
                    is_degenerate ? 0 : second_radius.Width(),
                    repeating_ ? kSpreadMethodRepeat : kSpreadMethodPad);
  AddStops(desc, conversion_data, document, style);

  scoped_refptr<Gradient> gradient = Gradient::CreateRadial(
      desc.p0, desc.r0, desc.p1, desc.r1,
      is_degenerate ? 1 : second_radius.AspectRatio(), desc.spread_method,
      Gradient::ColorInterpolation::kPremultiplied);

  // Now add the stops.
  gradient->AddColorStops(desc.stops);

  return gradient;
}

namespace {

bool EqualIdentifiersWithDefault(const CSSIdentifierValue* id_a,
                                 const CSSIdentifierValue* id_b,
                                 CSSValueID default_id) {
  CSSValueID value_a = id_a ? id_a->GetValueID() : default_id;
  CSSValueID value_b = id_b ? id_b->GetValueID() : default_id;
  return value_a == value_b;
}

}  // namespace

bool CSSRadialGradientValue::Equals(const CSSRadialGradientValue& other) const {
  if (gradient_type_ == kCSSDeprecatedRadialGradient)
    return other.gradient_type_ == gradient_type_ &&
           DataEquivalent(first_x_, other.first_x_) &&
           DataEquivalent(first_y_, other.first_y_) &&
           DataEquivalent(second_x_, other.second_x_) &&
           DataEquivalent(second_y_, other.second_y_) &&
           DataEquivalent(first_radius_, other.first_radius_) &&
           DataEquivalent(second_radius_, other.second_radius_) &&
           stops_ == other.stops_;

  if (repeating_ != other.repeating_)
    return false;

  if (!DataEquivalent(first_x_, other.first_x_) ||
      !DataEquivalent(first_y_, other.first_y_))
    return false;

  // There's either a size keyword or an explicit size specification.
  if (end_horizontal_size_) {
    // Explicit size specification. One <length> or two <length-percentage>.
    if (!DataEquivalent(end_horizontal_size_, other.end_horizontal_size_))
      return false;
    if (!DataEquivalent(end_vertical_size_, other.end_vertical_size_))
      return false;
  } else {
    if (other.end_horizontal_size_)
      return false;
    // There's a size keyword.
    if (!EqualIdentifiersWithDefault(sizing_behavior_, other.sizing_behavior_,
                                     CSSValueID::kFarthestCorner))
      return false;
    // Here the shape is 'ellipse' unless explicitly set to 'circle'.
    if (!EqualIdentifiersWithDefault(shape_, other.shape_,
                                     CSSValueID::kEllipse))
      return false;
  }
  return stops_ == other.stops_;
}

CSSRadialGradientValue* CSSRadialGradientValue::ComputedCSSValue(
    const ComputedStyle& style,
    bool allow_visited_style) {
  CSSRadialGradientValue* result = MakeGarbageCollected<CSSRadialGradientValue>(
      first_x_, first_y_, first_radius_, second_x_, second_y_, second_radius_,
      shape_, sizing_behavior_, end_horizontal_size_, end_vertical_size_,
      repeating_ ? kRepeating : kNonRepeating, GradientType());
  result->AddComputedStops(style, allow_visited_style, stops_);
  return result;
}

void CSSRadialGradientValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(first_x_);
  visitor->Trace(first_y_);
  visitor->Trace(second_x_);
  visitor->Trace(second_y_);
  visitor->Trace(first_radius_);
  visitor->Trace(second_radius_);
  visitor->Trace(shape_);
  visitor->Trace(sizing_behavior_);
  visitor->Trace(end_horizontal_size_);
  visitor->Trace(end_vertical_size_);
  CSSGradientValue::TraceAfterDispatch(visitor);
}

String CSSConicGradientValue::CustomCSSText() const {
  StringBuilder result;

  if (repeating_)
    result.Append("repeating-");
  result.Append("conic-gradient(");

  bool wrote_something = false;

  if (from_angle_) {
    result.Append("from ");
    result.Append(from_angle_->CssText());
    wrote_something = true;
  }

  wrote_something |= AppendPosition(result, x_, y_, wrote_something);

  AppendCSSTextForColorStops(result, wrote_something);

  result.Append(')');
  return result.ToString();
}

scoped_refptr<Gradient> CSSConicGradientValue::CreateGradient(
    const CSSToLengthConversionData& conversion_data,
    const FloatSize& size,
    const Document& document,
    const ComputedStyle& style) const {
  DCHECK(!size.IsEmpty());

  const float angle = from_angle_ ? from_angle_->ComputeDegrees() : 0;

  const FloatPoint position(
      x_ ? PositionFromValue(x_, conversion_data, size, true)
         : size.Width() / 2,
      y_ ? PositionFromValue(y_, conversion_data, size, false)
         : size.Height() / 2);

  GradientDesc desc(position, position,
                    repeating_ ? kSpreadMethodRepeat : kSpreadMethodPad);
  AddStops(desc, conversion_data, document, style);

  scoped_refptr<Gradient> gradient = Gradient::CreateConic(
      position, angle, desc.start_angle, desc.end_angle, desc.spread_method,
      Gradient::ColorInterpolation::kPremultiplied);
  gradient->AddColorStops(desc.stops);

  return gradient;
}

bool CSSConicGradientValue::Equals(const CSSConicGradientValue& other) const {
  return repeating_ == other.repeating_ && DataEquivalent(x_, other.x_) &&
         DataEquivalent(y_, other.y_) &&
         DataEquivalent(from_angle_, other.from_angle_) &&
         stops_ == other.stops_;
}

CSSConicGradientValue* CSSConicGradientValue::ComputedCSSValue(
    const ComputedStyle& style,
    bool allow_visited_style) {
  CSSConicGradientValue* result = CSSConicGradientValue::Create(
      x_, y_, from_angle_, repeating_ ? kRepeating : kNonRepeating);
  result->AddComputedStops(style, allow_visited_style, stops_);
  return result;
}

void CSSConicGradientValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(from_angle_);
  CSSGradientValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
