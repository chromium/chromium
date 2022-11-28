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
#include "third_party/blink/renderer/core/svg/color_distance.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"

namespace blink {

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
  NOTREACHED();
  return nullptr;
}

static inline Color FallbackColorForCurrentColor(
    const SVGElement* target_element) {
  DCHECK(target_element);
  if (const ComputedStyle* target_style = target_element->GetComputedStyle())
    return target_style->VisitedDependentColor(GetCSSPropertyColor());
  return Color::kTransparent;
}

static inline mojom::blink::ColorScheme ColorSchemeForSVGElement(
    const SVGElement* target_element) {
  DCHECK(target_element);
  if (const ComputedStyle* target_style = target_element->GetComputedStyle())
    return target_style->UsedColorScheme();
  return mojom::blink::ColorScheme::kLight;
}

void SVGColorProperty::Add(const SVGPropertyBase* other,
                           const SVGElement* context_element) {
  DCHECK(context_element);

  Color fallback_color = FallbackColorForCurrentColor(context_element);
  mojom::blink::ColorScheme color_scheme =
      ColorSchemeForSVGElement(context_element);
  Color from_color = To<SVGColorProperty>(other)->style_color_.Resolve(
      fallback_color, color_scheme);
  Color to_color = style_color_.Resolve(fallback_color, color_scheme);
  style_color_ = StyleColor(ColorDistance::AddColors(from_color, to_color));
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
  Color fallback_color = FallbackColorForCurrentColor(context_element);
  mojom::blink::ColorScheme color_scheme =
      ColorSchemeForSVGElement(context_element);
  Color from_color = from_style_color.Resolve(fallback_color, color_scheme);
  Color to_color = to_style_color.Resolve(fallback_color, color_scheme);
  Color to_at_end_of_duration_color =
      to_at_end_of_duration_style_color.Resolve(fallback_color, color_scheme);

  float animated_red = ComputeAnimatedNumber(
      parameters, percentage, repeat_count, from_color.Red(), to_color.Red(),
      to_at_end_of_duration_color.Red());
  float animated_green = ComputeAnimatedNumber(
      parameters, percentage, repeat_count, from_color.Green(),
      to_color.Green(), to_at_end_of_duration_color.Green());
  float animated_blue = ComputeAnimatedNumber(
      parameters, percentage, repeat_count, from_color.Blue(), to_color.Blue(),
      to_at_end_of_duration_color.Blue());
  float animated_alpha = ComputeAnimatedNumber(
      parameters, percentage, repeat_count, from_color.Alpha(),
      to_color.Alpha(), to_at_end_of_duration_color.Alpha());

  if (parameters.is_additive) {
    Color animated_color = style_color_.Resolve(fallback_color, color_scheme);
    animated_red += animated_color.Red();
    animated_green += animated_color.Green();
    animated_blue += animated_color.Blue();
    animated_alpha += animated_color.Alpha();
  }

  style_color_ = StyleColor(
      Color::FromRGBA(roundf(animated_red), roundf(animated_green),
                      roundf(animated_blue), roundf(animated_alpha)));
}

float SVGColorProperty::CalculateDistance(
    const SVGPropertyBase* to_value,
    const SVGElement* context_element) const {
  DCHECK(context_element);
  Color fallback_color = FallbackColorForCurrentColor(context_element);
  mojom::blink::ColorScheme color_scheme =
      ColorSchemeForSVGElement(context_element);

  Color from_color = style_color_.Resolve(fallback_color, color_scheme);
  Color to_color = To<SVGColorProperty>(to_value)->style_color_.Resolve(
      fallback_color, color_scheme);
  return ColorDistance::Distance(from_color, to_color);
}

}  // namespace blink
