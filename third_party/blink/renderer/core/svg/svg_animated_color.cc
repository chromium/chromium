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

#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/svg/color_distance.h"
#include "third_party/blink/renderer/core/svg/svg_animate_element.h"

namespace blink {

SVGColorProperty::SVGColorProperty(const String& color_string)
    : style_color_(StyleColor::CurrentColor()) {
  Color color;
  if (CSSParser::ParseColor(color, color_string.StripWhiteSpace()))
    style_color_ = color;
}

String SVGColorProperty::ValueAsString() const {
  return style_color_.IsCurrentColor()
             ? "currentColor"
             : cssvalue::CSSColorValue::SerializeAsCSSComponentValue(
                   style_color_.GetColor());
}

SVGPropertyBase* SVGColorProperty::CloneForAnimation(const String&) const {
  // SVGAnimatedColor is deprecated. So No SVG DOM animation.
  NOTREACHED();
  return nullptr;
}

static inline Color FallbackColorForCurrentColor(SVGElement* target_element) {
  DCHECK(target_element);
  if (LayoutObject* target_layout_object = target_element->GetLayoutObject())
    return target_layout_object->ResolveColor(GetCSSPropertyColor());
  return Color::kTransparent;
}

void SVGColorProperty::Add(SVGPropertyBase* other,
                           SVGElement* context_element) {
  DCHECK(context_element);

  Color fallback_color = FallbackColorForCurrentColor(context_element);
  Color from_color =
      ToSVGColorProperty(other)->style_color_.Resolve(fallback_color);
  Color to_color = style_color_.Resolve(fallback_color);
  style_color_ = StyleColor(ColorDistance::AddColors(from_color, to_color));
}

void SVGColorProperty::CalculateAnimatedValue(
    const SVGAnimateElement& animation_element,
    float percentage,
    unsigned repeat_count,
    SVGPropertyBase* from_value,
    SVGPropertyBase* to_value,
    SVGPropertyBase* to_at_end_of_duration_value,
    SVGElement* context_element) {
  StyleColor from_style_color = ToSVGColorProperty(from_value)->style_color_;
  StyleColor to_style_color = ToSVGColorProperty(to_value)->style_color_;
  StyleColor to_at_end_of_duration_style_color =
      ToSVGColorProperty(to_at_end_of_duration_value)->style_color_;

  // Apply currentColor rules.
  DCHECK(context_element);
  Color fallback_color = FallbackColorForCurrentColor(context_element);
  Color from_color = from_style_color.Resolve(fallback_color);
  Color to_color = to_style_color.Resolve(fallback_color);
  Color to_at_end_of_duration_color =
      to_at_end_of_duration_style_color.Resolve(fallback_color);
  Color animated_color = style_color_.Resolve(fallback_color);

  float animated_red = animated_color.Red();
  animation_element.AnimateAdditiveNumber(
      percentage, repeat_count, from_color.Red(), to_color.Red(),
      to_at_end_of_duration_color.Red(), animated_red);

  float animated_green = animated_color.Green();
  animation_element.AnimateAdditiveNumber(
      percentage, repeat_count, from_color.Green(), to_color.Green(),
      to_at_end_of_duration_color.Green(), animated_green);

  float animated_blue = animated_color.Blue();
  animation_element.AnimateAdditiveNumber(
      percentage, repeat_count, from_color.Blue(), to_color.Blue(),
      to_at_end_of_duration_color.Blue(), animated_blue);

  float animated_alpha = animated_color.Alpha();
  animation_element.AnimateAdditiveNumber(
      percentage, repeat_count, from_color.Alpha(), to_color.Alpha(),
      to_at_end_of_duration_color.Alpha(), animated_alpha);

  style_color_ =
      StyleColor(MakeRGBA(roundf(animated_red), roundf(animated_green),
                          roundf(animated_blue), roundf(animated_alpha)));
}

float SVGColorProperty::CalculateDistance(SVGPropertyBase* to_value,
                                          SVGElement* context_element) {
  DCHECK(context_element);
  Color fallback_color = FallbackColorForCurrentColor(context_element);

  Color from_color = style_color_.Resolve(fallback_color);
  Color to_color =
      ToSVGColorProperty(to_value)->style_color_.Resolve(fallback_color);
  return ColorDistance::Distance(from_color, to_color);
}

}  // namespace blink
