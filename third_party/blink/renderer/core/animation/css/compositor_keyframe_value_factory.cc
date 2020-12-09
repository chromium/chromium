// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_value_factory.h"

#include "third_party/blink/renderer/core/animation/compositor_animations.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_color.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_double.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_filter_operations.h"
#include "third_party/blink/renderer/core/animation/css/compositor_keyframe_transform.h"
#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/properties/css_property.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static CompositorKeyframeValue* CreateFromTransformProperties(
    scoped_refptr<TransformOperation> transform,
    double zoom,
    scoped_refptr<TransformOperation> initial_transform) {
  TransformOperations operation;
  bool has_transform = static_cast<bool>(transform);
  if (has_transform || initial_transform) {
    operation.Operations().push_back(
        std::move(has_transform ? transform : initial_transform));
  }
  return MakeGarbageCollected<CompositorKeyframeTransform>(
      operation, has_transform ? zoom : 1);
}

CompositorKeyframeValue* CompositorKeyframeValueFactory::Create(
    const PropertyHandle& property,
    const ComputedStyle& style) {
  const CSSProperty& css_property = property.GetCSSProperty();
#if DCHECK_IS_ON()
  // Variables are conditionally interpolable and compositable.
  if (css_property.PropertyID() != CSSPropertyID::kVariable) {
    DCHECK(css_property.IsInterpolable());
    DCHECK(css_property.IsCompositableProperty());
  }
#endif
  switch (css_property.PropertyID()) {
    case CSSPropertyID::kOpacity:
      return MakeGarbageCollected<CompositorKeyframeDouble>(style.Opacity());
    case CSSPropertyID::kFilter:
      return MakeGarbageCollected<CompositorKeyframeFilterOperations>(
          style.Filter());
    case CSSPropertyID::kBackdropFilter:
      return MakeGarbageCollected<CompositorKeyframeFilterOperations>(
          style.BackdropFilter());
    case CSSPropertyID::kTransform:
      return MakeGarbageCollected<CompositorKeyframeTransform>(
          style.Transform(), style.EffectiveZoom());
    case CSSPropertyID::kTranslate: {
      return CreateFromTransformProperties(style.Translate(),
                                           style.EffectiveZoom(), nullptr);
    }
    case CSSPropertyID::kRotate: {
      return CreateFromTransformProperties(style.Rotate(),
                                           style.EffectiveZoom(), nullptr);
    }
    case CSSPropertyID::kScale: {
      return CreateFromTransformProperties(style.Scale(), style.EffectiveZoom(),
                                           nullptr);
    }
    case CSSPropertyID::kVariable: {
      if (!RuntimeEnabledFeatures::OffMainThreadCSSPaintEnabled()) {
        return nullptr;
      }
      const AtomicString& property_name = property.CustomPropertyName();
      const CSSValue* value = style.GetVariableValue(property_name);

      const auto* primitive_value = DynamicTo<CSSPrimitiveValue>(value);
      if (primitive_value && primitive_value->IsNumber()) {
        return MakeGarbageCollected<CompositorKeyframeDouble>(
            primitive_value->GetFloatValue());
      }

      // TODO: Add supported for interpolable color values from
      // CSSIdentifierValue when given a value of currentcolor
      if (const auto* color_value = DynamicTo<cssvalue::CSSColorValue>(value)) {
        Color color = color_value->Value();
        return MakeGarbageCollected<CompositorKeyframeColor>(SkColorSetARGB(
            color.Alpha(), color.Red(), color.Green(), color.Blue()));
      }

      return nullptr;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink
