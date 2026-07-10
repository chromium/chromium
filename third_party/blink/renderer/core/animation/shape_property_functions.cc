// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/shape_property_functions.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/shape_clip_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_offset_path_operation.h"
#include "third_party/blink/renderer/core/style/shape_value.h"

namespace blink {

namespace {

ShapeReferenceBox GetDefaultBox(CSSPropertyID property_id) {
  ShapeReferenceBox box;
  switch (property_id) {
    case CSSPropertyID::kClipPath:
      box.geometry = GeometryBox::kBorderBox;
      break;
    case CSSPropertyID::kOffsetPath:
      box.coord = CoordBox::kBorderBox;
      break;
    case CSSPropertyID::kShapeOutside:
      box.shape = ShapeBox::kMarginBox;
      break;
    default:
      break;
  }
  return box;
}

ShapeReferenceBox GetBox(CSSPropertyID property_id,
                         const CSSIdentifierValue& ident) {
  ShapeReferenceBox box;
  switch (property_id) {
    case CSSPropertyID::kClipPath:
      box.geometry = ident.ConvertTo<GeometryBox>();
      break;
    case CSSPropertyID::kOffsetPath:
      box.coord = ident.ConvertTo<CoordBox>();
      break;
    case CSSPropertyID::kShapeOutside:
      box.shape = ident.ConvertTo<ShapeBox>();
      break;
    default:
      break;
  }
  return box;
}

}  // namespace

BasicShapeInfo shape_property_functions::GetBasicShape(
    const CSSProperty& property,
    const ComputedStyle& style) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kClipPath: {
      auto* operation = DynamicTo<ShapeClipPathOperation>(style.ClipPath());
      if (!operation) {
        return {};
      }
      BasicShapeInfo info;
      info.shape = &operation->GetBasicShape();
      info.box.geometry = operation->GetGeometryBox();
      return info;
    }
    case CSSPropertyID::kD:
      return {style.D()};
    case CSSPropertyID::kObjectViewBox:
      return {style.ObjectViewBox()};
    case CSSPropertyID::kOffsetPath: {
      auto* operation = DynamicTo<ShapeOffsetPathOperation>(style.OffsetPath());
      if (!operation) {
        return {};
      }
      BasicShapeInfo info;
      info.shape = &operation->GetBasicShape();
      info.box.coord = operation->GetCoordBox();
      return info;
    }
    case CSSPropertyID::kShapeOutside: {
      const ShapeValue* shape_value = style.ShapeOutside();
      if (!shape_value || shape_value->GetType() != ShapeValue::kShape) {
        return {};
      }
      BasicShapeInfo info;
      info.shape = &shape_value->Shape();
      info.box.shape = shape_value->CssBox();
      return info;
    }
    default:
      NOTREACHED();
  }
}

void shape_property_functions::SetBasicShape(const CSSProperty& property,
                                             BasicShape& shape,
                                             ShapeReferenceBox box,
                                             ComputedStyleBuilder& builder) {
  switch (property.PropertyID()) {
    case CSSPropertyID::kClipPath:
      builder.SetClipPath(MakeGarbageCollected<ShapeClipPathOperation>(
          shape, box.geometry.value_or(GeometryBox::kBorderBox)));
      break;
    case CSSPropertyID::kD:
      builder.SetD(&To<StylePath>(shape));
      break;
    case CSSPropertyID::kObjectViewBox:
      builder.SetObjectViewBox(&shape);
      break;
    case CSSPropertyID::kOffsetPath:
      builder.SetOffsetPath(MakeGarbageCollected<ShapeOffsetPathOperation>(
          shape, box.coord.value_or(CoordBox::kBorderBox)));
      break;
    case CSSPropertyID::kShapeOutside:
      builder.SetShapeOutside(MakeGarbageCollected<ShapeValue>(
          shape, box.shape.value_or(ShapeBox::kMarginBox)));
      break;
    default:
      NOTREACHED();
  }
}

BasicShapeCssInfo shape_property_functions::GetCssBasicShape(
    const CSSProperty& property,
    const CSSValue& value) {
  BasicShapeCssInfo css_info;
  css_info.shape = &value;
  css_info.box = GetDefaultBox(property.PropertyID());
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    css_info.shape = &list->First();
    if (const auto* ident = DynamicTo<CSSIdentifierValue>(list->Last())) {
      css_info.box = GetBox(property.PropertyID(), *ident);
    }
  }
  return css_info;
}

}  // namespace blink
