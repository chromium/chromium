/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/animation/css/css_animatable_value_factory.h"

#include "third_party/blink/renderer/core/animation/animatable/animatable_double.h"
#include "third_party/blink/renderer/core/animation/animatable/animatable_filter_operations.h"
#include "third_party/blink/renderer/core/animation/animatable/animatable_transform.h"
#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static AnimatableValue* CreateFromTransformProperties(
    scoped_refptr<TransformOperation> transform,
    double zoom,
    scoped_refptr<TransformOperation> initial_transform) {
  TransformOperations operation;
  bool has_transform = static_cast<bool>(transform);
  if (has_transform || initial_transform) {
    operation.Operations().push_back(
        std::move(has_transform ? transform : initial_transform));
  }
  return AnimatableTransform::Create(operation, has_transform ? zoom : 1);
}

AnimatableValue* CSSAnimatableValueFactory::Create(
    const PropertyHandle& property,
    const ComputedStyle& style) {
  const CSSProperty& css_property = property.GetCSSProperty();
#if DCHECK_IS_ON()
  // Variables are conditionally interpolable and compositable.
  if (css_property.PropertyID() != CSSPropertyVariable) {
    DCHECK(css_property.IsInterpolable());
    DCHECK(css_property.IsCompositableProperty());
  }
#endif
  switch (css_property.PropertyID()) {
    case CSSPropertyOpacity:
      return AnimatableDouble::Create(style.Opacity());
    case CSSPropertyFilter:
      return AnimatableFilterOperations::Create(style.Filter());
    case CSSPropertyBackdropFilter:
      return AnimatableFilterOperations::Create(style.BackdropFilter());
    case CSSPropertyTransform:
      return AnimatableTransform::Create(style.Transform(),
                                         style.EffectiveZoom());
    case CSSPropertyTranslate: {
      return CreateFromTransformProperties(style.Translate(),
                                           style.EffectiveZoom(), nullptr);
    }
    case CSSPropertyRotate: {
      return CreateFromTransformProperties(style.Rotate(),
                                           style.EffectiveZoom(), nullptr);
    }
    case CSSPropertyScale: {
      return CreateFromTransformProperties(style.Scale(), style.EffectiveZoom(),
                                           nullptr);
    }
    case CSSPropertyVariable: {
      if (!RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled()) {
        return nullptr;
      }
      const AtomicString& property_name = property.CustomPropertyName();
      const CSSValue* value = style.GetRegisteredVariable(property_name);
      if (!value || !value->IsPrimitiveValue() ||
          !ToCSSPrimitiveValue(*value).IsNumber()) {
        return nullptr;
      }
      return AnimatableDouble::Create(
          ToCSSPrimitiveValue(*value).GetFloatValue());
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink
