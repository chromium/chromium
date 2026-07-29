// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/compositor_animation_color_curve.h"

#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_color_mix_value.h"

namespace blink {

/* static */
bool CompositorAnimationColorCurve::ValidateColorValue(
    Element* element,
    const CSSValue* css_value,
    const TypedInterpolationValue* interpolation_value) {
  if (css_value) {
    if (css_value->IsIdentifierValue()) {
      CSSValueID value_id = To<CSSIdentifierValue>(css_value)->GetValueID();
      if (StyleColor::IsSystemColorIncludingDeprecated(value_id)) {
        // The color depends on the color-scheme. Though we can resolve the
        // color values, we presently lack a method to update the colors should
        // the color-scheme change during the course of the animation.
        // TODO(crbug.com/40795239): handle system color.
        return false;
      }
      if (value_id == CSSValueID::kCurrentcolor) {
        // Do not composite a background color animation that depends on
        // currentcolor until we have a mechanism to update the compositor
        // keyframes when currentcolor changes.
        return false;
      }
    } else if (css_value->IsColorMixValue()) {
      const cssvalue::CSSColorMixValue* color_mix =
          To<cssvalue::CSSColorMixValue>(css_value);
      if (!ValidateColorValue(element, &color_mix->Color1(), nullptr) ||
          !ValidateColorValue(element, &color_mix->Color2(), nullptr)) {
        // Unresolved color mix or a color mix with a system color dependency.
        // Either way, fall back to main.
        return false;
      }
    }

    const CSSPropertyName property_name =
        CSSPropertyName(CSSPropertyID::kBackgroundColor);
    const CSSValue* computed_value =
        StyleResolver::ComputeValue(element, property_name, *css_value);
    return computed_value->IsColorValue();
  } else if (interpolation_value) {
    const InterpolableValue* interpolable_value =
        interpolation_value->Value().interpolable_value.Get();
    // Transition keyframes store a pair of color values: one for the actual
    // color and one for the reported color (conditionally resolved). This is to
    // prevent JavaScript code from snooping the visited status of links. The
    // color to use for the animation is stored first in the list.
    // We need to further check that the color is a simple RGBA color and does
    // not require blending with other colors (e.g. currentcolor).
    if (!interpolable_value->IsList()) {
      return false;
    }

    const InterpolableList& list = To<InterpolableList>(*interpolable_value);
    return CSSColorInterpolationType::IsNonKeywordColor(*(list.Get(0)));
  }
  return false;
}

/* static */
scoped_refptr<CompositorAnimationColorCurve>
CompositorAnimationColorCurve::Create(Animation* animation,
                                      CSSPropertyName property_name) {
  scoped_refptr<CompositorAnimationColorCurve> curve =
      base::AdoptRef(new CompositorAnimationColorCurve(property_name));
  if (!curve->PopulateKeyframes(animation, ValidateColorValue)) {
    return nullptr;
  }
  const auto& keyframes = curve->GetKeyframes();
  for (const TypedKeyframe& item : keyframes) {
    if (!item.value.IsOpaque()) {
      curve->is_opaque_ = false;
      break;
    }
  }
  return curve;
}

/* static */
scoped_refptr<CompositorAnimationColorCurve>
CompositorAnimationColorCurve::CreateForTesting(
    const Vector<Color>& animated_colors,
    const Vector<double>& offsets,
    CSSPropertyName name) {
  scoped_refptr<CompositorAnimationColorCurve> curve =
      base::AdoptRef(new CompositorAnimationColorCurve(name));
  for (wtf_size_t i = 0; i < animated_colors.size(); i++) {
    curve->AddKeyframeForTesting(offsets[i], animated_colors[i]);
  }
  return curve;
}

scoped_refptr<CompositorAnimationCurve> CompositorAnimationColorCurve::Clone() {
  return base::AdoptRef(new CompositorAnimationColorCurve(*this));
}

Color CompositorAnimationColorCurve::ConvertCssValue(const CSSValue* value) {
  auto& color_value = To<cssvalue::CSSColor>(*value);
  return color_value.Value();
}

Color CompositorAnimationColorCurve::ConvertTypedInterpolationValue(
    const TypedInterpolationValue* interpolation_value) {
  const InterpolableValue* interpolable_value =
      interpolation_value->Value().interpolable_value.Get();
  const auto& list = To<InterpolableList>(*interpolable_value);
  DCHECK(CSSColorInterpolationType::IsNonKeywordColor(*(list.Get(0))));
  return CSSColorInterpolationType::GetColor(*(list.Get(0)));
}

Color CompositorAnimationColorCurve::InterpolateKeyframes(wtf_size_t index,
                                                          double progress) {
  auto& keyframes = GetKeyframes();
  Color first = keyframes[index].value;
  Color second = keyframes[index + 1].value;

  // Interpolation is in legacy srgb if and only if both endpoints are legacy
  // srgb. Otherwise, use OkLab for interpolation.
  if (first.GetColorSpace() != Color::ColorSpace::kSRGBLegacy ||
      second.GetColorSpace() != Color::ColorSpace::kSRGBLegacy) {
    first.ConvertToColorSpace(Color::ColorSpace::kOklab);
    second.ConvertToColorSpace(Color::ColorSpace::kOklab);
  }

  return Color::InterpolateColors(first.GetColorSpace(), std::nullopt, first,
                                  second, progress);
}

}  // namespace blink
