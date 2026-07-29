// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_ANIMATION_COLOR_CURVE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_ANIMATION_COLOR_CURVE_H_

#include "third_party/blink/renderer/core/animation/compositor_animation_curve.h"

namespace blink {

class CORE_EXPORT CompositorAnimationColorCurve
    : public TypedCompositorAnimationCurve<Color> {
 public:
  // Returns if the color value is supported for interpolation via a paint
  // worklet.
  static bool ValidateColorValue(
      Element* element,
      const CSSValue* value,
      const TypedInterpolationValue* interpolation_value);

  static scoped_refptr<CompositorAnimationColorCurve> Create(
      Animation* animation,
      CSSPropertyName name);

  // Strictly used for unit testing.
  static scoped_refptr<CompositorAnimationColorCurve> CreateForTesting(
      const Vector<Color>& animated_colors,
      const Vector<double>& offsets,
      CSSPropertyName name);

  ~CompositorAnimationColorCurve() override = default;

  bool IsOpaque() { return is_opaque_; }

 protected:
  scoped_refptr<CompositorAnimationCurve> Clone() override;

  Color ConvertCssValue(const CSSValue* value) override;
  Color ConvertTypedInterpolationValue(
      const TypedInterpolationValue* value) override;
  Color InterpolateKeyframes(wtf_size_t index, double progress) override;

 private:
  explicit CompositorAnimationColorCurve(CSSPropertyName property_name)
      : TypedCompositorAnimationCurve<Color>(property_name) {}
  CompositorAnimationColorCurve(const CompositorAnimationColorCurve& other)
      : TypedCompositorAnimationCurve<Color>(other),
        is_opaque_(other.is_opaque_) {}  // NOLINT(modernize-use-equals-default)

  bool is_opaque_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_COMPOSITOR_ANIMATION_COLOR_CURVE_H_
