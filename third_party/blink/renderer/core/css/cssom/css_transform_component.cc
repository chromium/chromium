// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_transform_component.h"

#include "third_party/blink/renderer/core/css/cssom/css_matrix_component.h"
#include "third_party/blink/renderer/core/css/cssom/css_perspective.h"
#include "third_party/blink/renderer/core/css/cssom/css_rotate.h"
#include "third_party/blink/renderer/core/css/cssom/css_scale.h"
#include "third_party/blink/renderer/core/css/cssom/css_skew.h"
#include "third_party/blink/renderer/core/css/cssom/css_skew_x.h"
#include "third_party/blink/renderer/core/css/cssom/css_skew_y.h"
#include "third_party/blink/renderer/core/css/cssom/css_translate.h"

namespace blink {

CSSTransformComponent* CSSTransformComponent::FromCSSValue(
    const CSSValue& value) {
  const auto* function_value = DynamicTo<CSSFunctionValue>(value);
  if (!function_value) {
    return nullptr;
  }

  switch (function_value->FunctionType()) {
    case CSSValueID::kMatrix:
    case CSSValueID::kMatrix3d:
      return CSSMatrixComponent::FromCSSValue(*function_value);
    case CSSValueID::kPerspective:
      return CSSPerspective::FromCSSValue(*function_value);
    case CSSValueID::kRotate:
    case CSSValueID::kRotateX:
    case CSSValueID::kRotateY:
    case CSSValueID::kRotateZ:
    case CSSValueID::kRotate3d:
      return CSSRotate::FromCSSValue(*function_value);
    case CSSValueID::kScale:
    case CSSValueID::kScaleX:
    case CSSValueID::kScaleY:
    case CSSValueID::kScaleZ:
    case CSSValueID::kScale3d:
      return CSSScale::FromCSSValue(*function_value);
    case CSSValueID::kSkew:
      return CSSSkew::FromCSSValue(*function_value);
    case CSSValueID::kSkewX:
      return CSSSkewX::FromCSSValue(*function_value);
    case CSSValueID::kSkewY:
      return CSSSkewY::FromCSSValue(*function_value);
    case CSSValueID::kTranslate:
    case CSSValueID::kTranslateX:
    case CSSValueID::kTranslateY:
    case CSSValueID::kTranslateZ:
    case CSSValueID::kTranslate3d:
      return CSSTranslate::FromCSSValue(*function_value);
    default:
      return nullptr;
  }
}

String CSSTransformComponent::toString() const {
  const CSSValue* result = ToCSSValue();
  return result ? result->CssText() : "";
}

}  // namespace blink
