// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_shape_value.h"

#include <memory>

#include "base/notreached.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_numeric_value.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/svg/svg_path_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink::cssvalue {

namespace {
void AppendControlPointCSSText(StringBuilder& builder,
                               const CSSShapeControlPoint& control_point,
                               bool is_relative) {
  builder.Append(control_point.second->CssText());
  if ((is_relative && control_point.first != CSSValueID::kStart) ||
      (!is_relative && control_point.first != CSSValueID::kOrigin)) {
    builder.Append(" from ");
    builder.Append(GetCSSValueNameAs<StringView>(control_point.first));
  }
}
}  // namespace
String CSSShapeCommand::CSSText() const {
  if (type_ == Type::kPathSegClosePath) {
    return GetCSSValueNameAs<String>(CSSValueID::kClose);
  }

  StringBuilder builder;

  switch (type_) {
    case Type::kPathSegMoveToAbs:
      builder.Append("move to ");
      break;
    case Type::kPathSegMoveToRel:
      builder.Append("move by ");
      break;
    case Type::kPathSegLineToAbs:
      builder.Append("line to ");
      break;
    case Type::kPathSegLineToRel:
      builder.Append("line by ");
      break;
    case Type::kPathSegLineToHorizontalAbs:
      builder.Append("hline to ");
      break;
    case Type::kPathSegLineToHorizontalRel:
      builder.Append("hline by ");
      break;
    case Type::kPathSegLineToVerticalAbs:
      builder.Append("vline to ");
      break;
    case Type::kPathSegLineToVerticalRel:
      builder.Append("vline by ");
      break;
    case Type::kPathSegCurveToCubicAbs:
    case Type::kPathSegCurveToQuadraticAbs:
      builder.Append("curve to ");
      break;
    case Type::kPathSegCurveToCubicRel:
    case Type::kPathSegCurveToQuadraticRel:
      builder.Append("curve by ");
      break;
    case Type::kPathSegCurveToCubicSmoothAbs:
    case Type::kPathSegCurveToQuadraticSmoothAbs:
      builder.Append("smooth to ");
      break;
    case Type::kPathSegCurveToCubicSmoothRel:
    case Type::kPathSegCurveToQuadraticSmoothRel:
      builder.Append("smooth by ");
      break;
    case Type::kPathSegArcAbs:
      builder.Append("arc to ");
      break;
    case Type::kPathSegArcRel:
      builder.Append("arc by ");
      break;
    default:
      NOTREACHED();
  }

  builder.Append(end_point_->CssText());

  switch (type_) {
    case Type::kPathSegCurveToCubicAbs:
    case Type::kPathSegCurveToCubicRel: {
      const auto& curve = static_cast<const CSSShapeCurveCommand<2>&>(*this);
      builder.Append(" with ");
      AppendControlPointCSSText(builder, curve.GetControlPoints().at(0),
                                type_ == Type::kPathSegCurveToCubicRel);
      builder.Append(" / ");
      AppendControlPointCSSText(builder, curve.GetControlPoints().at(1),
                                type_ == Type::kPathSegCurveToCubicRel);
      break;
    }

    case Type::kPathSegCurveToQuadraticAbs:
    case Type::kPathSegCurveToQuadraticRel:
    case Type::kPathSegCurveToCubicSmoothAbs:
    case Type::kPathSegCurveToCubicSmoothRel: {
      const auto& curve = static_cast<const CSSShapeCurveCommand<1>&>(*this);
      builder.Append(" with ");
      AppendControlPointCSSText(
          builder, curve.GetControlPoints().at(0),
          type_ == Type::kPathSegCurveToQuadraticRel ||
              type_ == Type::kPathSegCurveToCubicSmoothRel);
      break;
    }

    case Type::kPathSegArcAbs:
    case Type::kPathSegArcRel: {
      const CSSShapeArcCommand& arc =
          static_cast<const CSSShapeArcCommand&>(*this);
      builder.Append(" of ");
      builder.Append(arc.Radius().CssText());
      if (arc.Sweep() == CSSValueID::kCw) {
        builder.Append(" cw");
      }
      if (arc.Size() == CSSValueID::kLarge) {
        builder.Append(" large");
      }
      auto* numeric_angle = DynamicTo<CSSNumericLiteralValue>(arc.Angle());
      if (!numeric_angle || numeric_angle->ComputeDegrees() != 0) {
        builder.Append(" rotate ");
        builder.Append(arc.Angle().CssText());
      }
      break;
    }
    default:
      break;
  }

  return builder.ReleaseString();
}

bool CSSShapeCommand::operator==(const CSSShapeCommand& other) const {
  return type_ == other.type_ &&
         base::ValuesEquivalent(end_point_, other.end_point_);
}

String CSSShapeValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("shape(");
  if (wind_rule_ == RULE_EVENODD) {
    result.Append("evenodd ");
  }
  result.Append("from ");
  result.Append(origin_->CssText());
  result.Append(", ");
  bool first = true;
  for (const CSSShapeCommand* command : commands_) {
    if (!first) {
      result.Append(", ");
    }
    first = false;
    result.Append(command->CSSText());
  }
  result.Append(")");
  return result.ReleaseString();
}

void CSSShapeValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(origin_);
  visitor->Trace(commands_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink::cssvalue
