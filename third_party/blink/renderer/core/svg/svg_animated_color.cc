/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/svg/svg_animated_color.h"

#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"

namespace blink {

namespace {

struct RGBATuple {
  float red;
  float green;
  float blue;
  float alpha;
};

void Accumulate(RGBATuple& base, const RGBATuple& addend) {
  base.red += addend.red;
  base.green += addend.green;
  base.blue += addend.blue;
  base.alpha += addend.alpha;
}

RGBATuple ToRGBATuple(const StyleColor& color,
                      Color fallback_color,
                      mojom::blink::ColorScheme color_scheme) {
  const Color resolved = color.Resolve(fallback_color, color_scheme);
  RGBATuple tuple;
  resolved.GetRGBA(tuple.red, tuple.green, tuple.blue, tuple.alpha);
  return tuple;
}

StyleColor ToStyleColor(const RGBATuple& tuple) {
  return StyleColor(
      Color::FromRGBAFloat(tuple.red, tuple.green, tuple.blue, tuple.alpha));
}

Color FallbackColorForCurrentColor(const SVGElement& target_element) {
  if (const ComputedStyle* target_style = target_element.GetComputedStyle()) {
    return target_style->VisitedDependentColor(GetCSSPropertyColor());
  }
  return Color::kTransparent;
}

mojom::blink::ColorScheme ColorSchemeForSVGElement(
    const SVGElement& target_element) {
  if (const ComputedStyle* target_style = target_element.GetComputedStyle()) {
    return target_style->UsedColorScheme();
  }
  return mojom::blink::ColorScheme::kLight;
}

}  // namespace

SVGColorProperty::SVGColorProperty(const String& color_string)
    : style_color_(StyleColor::CurrentColor()) {
  Color color;
  if (CSSParser::ParseColor(color, color_string.StripWhiteSpace()))
    style_color_ = StyleColor(color);
}

String SVGColorProperty::ValueAsString() const {
  return style_color_.IsCurrentColor()
             ? "currentColor"
             : cssvalue::CSSColor::SerializeAsCSSComponentValue(
                   style_color_.GetColor());
}

SVGPropertyBase* SVGColorProperty::CloneForAnimation(const String&) const {
  // SVGAnimatedColor is deprecated. So No SVG DOM animation.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void SVGColorProperty::Add(const SVGPropertyBase* other,
                           const SVGElement* context_element) {
  DCHECK(context_element);

  Color fallback_color = FallbackColorForCurrentColor(*context_element);
  mojom::blink::ColorScheme color_scheme =
      ColorSchemeForSVGElement(*context_element);
  auto base = ToRGBATuple(To<SVGColorProperty>(other)->style_color_,
                          fallback_color, color_scheme);
  const auto addend = ToRGBATuple(style_color_, fallback_color, color_scheme);
  Accumulate(base, addend);
  style_color_ = ToStyleColor(base);
}

void SVGColorProperty::CalculateAnimatedValue(
    const SMILAnimationEffectParameters& parameters,
    float percentage,
    unsigned repeat_count,
    const SVGPropertyBase* from_value,
    const SVGPropertyBase* to_value,
    const SVGPropertyBase* to_at_end_of_duration_value,
    const SVGElement* context_element) {
  StyleColor from_style_color = To<SVGColorProperty>(from_value)->style_color_;
  StyleColor to_style_color = To<SVGColorProperty>(to_value)->style_color_;
  StyleColor to_at_end_of_duration_style_color =
      To<SVGColorProperty>(to_at_end_of_duration_value)->style_color_;

  // Apply currentColor rules.
  DCHECK(context_element);
  Color fallback_color = FallbackColorForCurrentColor(*context_element);
  mojom::blink::ColorScheme color_scheme =
      ColorSchemeForSVGElement(*context_element);
  const auto from = ToRGBATuple(from_style_color, fallback_color, color_scheme);
  const auto to = ToRGBATuple(to_style_color, fallback_color, color_scheme);
  const auto to_at_end_of_duration = ToRGBATuple(
      to_at_end_of_duration_style_color, fallback_color, color_scheme);

  // TODO(crbug.com/40249893): Don't clobber colorspace.
  RGBATuple result;
  result.red =
      ComputeAnimatedNumber(parameters, percentage, repeat_count, from.red,
                            to.red, to_at_end_of_duration.red);
  result.green =
      ComputeAnimatedNumber(parameters, percentage, repeat_count, from.green,
                            to.green, to_at_end_of_duration.green);
  result.blue =
      ComputeAnimatedNumber(parameters, percentage, repeat_count, from.blue,
                            to.blue, to_at_end_of_duration.blue);
  result.alpha =
      ComputeAnimatedNumber(parameters, percentage, repeat_count, from.alpha,
                            to.alpha, to_at_end_of_duration.alpha);

  if (parameters.is_additive) {
    Accumulate(result, ToRGBATuple(style_color_, fallback_color, color_scheme));
  }

  style_color_ = ToStyleColor(result);
}

float SVGColorProperty::CalculateDistance(
    const SVGPropertyBase* to_value,
    const SVGElement* context_element) const {
  DCHECK(context_element);
  Color fallback_color = FallbackColorForCurrentColor(*context_element);
  mojom::blink::ColorScheme color_scheme =
      ColorSchemeForSVGElement(*context_element);

  const auto from = ToRGBATuple(style_color_, fallback_color, color_scheme);
  const auto to = ToRGBATuple(To<SVGColorProperty>(to_value)->style_color_,
                              fallback_color, color_scheme);
  float red_diff = to.red - from.red;
  float green_diff = to.green - from.green;
  float blue_diff = to.blue - from.blue;
  // This is just a simple distance calculation, not respecting color spaces
  return sqrtf(red_diff * red_diff + blue_diff * blue_diff +
               green_diff * green_diff);
}

}  // namespace blink
