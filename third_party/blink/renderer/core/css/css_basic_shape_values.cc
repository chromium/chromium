/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_basic_shape_values.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

static String BuildCircleString(const String& radius,
                                const String& center_x,
                                const String& center_y) {
  char at[] = "at";
  char separator[] = " ";
  StringBuilder result;
  result.Append("circle(");
  if (!radius.IsNull())
    result.Append(radius);

  if (!center_x.IsNull() || !center_y.IsNull()) {
    if (!radius.IsNull())
      result.Append(separator);
    result.Append(at);
    result.Append(separator);
    result.Append(center_x);
    result.Append(separator);
    result.Append(center_y);
  }
  result.Append(')');
  return result.ToString();
}

static String SerializePositionOffset(const CSSValuePair& offset,
                                      const CSSValuePair& other) {
  if ((To<CSSIdentifierValue>(offset.First()).GetValueID() ==
           CSSValueID::kLeft &&
       To<CSSIdentifierValue>(other.First()).GetValueID() ==
           CSSValueID::kTop) ||
      (To<CSSIdentifierValue>(offset.First()).GetValueID() ==
           CSSValueID::kTop &&
       To<CSSIdentifierValue>(other.First()).GetValueID() == CSSValueID::kLeft))
    return offset.Second().CssText();
  return offset.CssText();
}

static CSSValuePair* BuildSerializablePositionOffset(CSSValue* offset,
                                                     CSSValueID default_side) {
  CSSValueID side = default_side;
  const CSSPrimitiveValue* amount = nullptr;

  if (!offset) {
    side = CSSValueID::kCenter;
  } else if (auto* offset_identifier_value =
                 DynamicTo<CSSIdentifierValue>(offset)) {
    side = offset_identifier_value->GetValueID();
  } else if (auto* offset_value_pair = DynamicTo<CSSValuePair>(offset)) {
    side = To<CSSIdentifierValue>(offset_value_pair->First()).GetValueID();
    amount = &To<CSSPrimitiveValue>(offset_value_pair->Second());
    if ((side == CSSValueID::kRight || side == CSSValueID::kBottom) &&
        amount->IsPercentage()) {
      side = default_side;
      amount = CSSNumericLiteralValue::Create(
          100 - amount->GetFloatValue(),
          CSSPrimitiveValue::UnitType::kPercentage);
    }
  } else {
    amount = To<CSSPrimitiveValue>(offset);
  }

  if (side == CSSValueID::kCenter) {
    side = default_side;
    amount = CSSNumericLiteralValue::Create(
        50, CSSPrimitiveValue::UnitType::kPercentage);
  } else if (!amount || (amount->IsLength() && amount->IsZero())) {
    if (side == CSSValueID::kRight || side == CSSValueID::kBottom)
      amount = CSSNumericLiteralValue::Create(
          100, CSSPrimitiveValue::UnitType::kPercentage);
    else
      amount = CSSNumericLiteralValue::Create(
          0, CSSPrimitiveValue::UnitType::kPercentage);
    side = default_side;
  }

  return MakeGarbageCollected<CSSValuePair>(CSSIdentifierValue::Create(side),
                                            amount,
                                            CSSValuePair::kKeepIdenticalValues);
}

String CSSBasicShapeCircleValue::CustomCSSText() const {
  CSSValuePair* normalized_cx =
      BuildSerializablePositionOffset(center_x_, CSSValueID::kLeft);
  CSSValuePair* normalized_cy =
      BuildSerializablePositionOffset(center_y_, CSSValueID::kTop);

  String radius;
  auto* radius_identifier_value = DynamicTo<CSSIdentifierValue>(radius_.Get());
  if (radius_ &&
      !(radius_identifier_value &&
        radius_identifier_value->GetValueID() == CSSValueID::kClosestSide))
    radius = radius_->CssText();

  return BuildCircleString(
      radius, SerializePositionOffset(*normalized_cx, *normalized_cy),
      SerializePositionOffset(*normalized_cy, *normalized_cx));
}

bool CSSBasicShapeCircleValue::Equals(
    const CSSBasicShapeCircleValue& other) const {
  return DataEquivalent(center_x_, other.center_x_) &&
         DataEquivalent(center_y_, other.center_y_) &&
         DataEquivalent(radius_, other.radius_);
}

void CSSBasicShapeCircleValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(center_x_);
  visitor->Trace(center_y_);
  visitor->Trace(radius_);
  CSSValue::TraceAfterDispatch(visitor);
}

static String BuildEllipseString(const String& radius_x,
                                 const String& radius_y,
                                 const String& center_x,
                                 const String& center_y) {
  char at[] = "at";
  char separator[] = " ";
  StringBuilder result;
  result.Append("ellipse(");
  bool needs_separator = false;
  if (!radius_x.IsNull()) {
    result.Append(radius_x);
    needs_separator = true;
  }
  if (!radius_y.IsNull()) {
    if (needs_separator)
      result.Append(separator);
    result.Append(radius_y);
    needs_separator = true;
  }

  if (!center_x.IsNull() || !center_y.IsNull()) {
    if (needs_separator)
      result.Append(separator);
    result.Append(at);
    result.Append(separator);
    result.Append(center_x);
    result.Append(separator);
    result.Append(center_y);
  }
  result.Append(')');
  return result.ToString();
}

String CSSBasicShapeEllipseValue::CustomCSSText() const {
  CSSValuePair* normalized_cx =
      BuildSerializablePositionOffset(center_x_, CSSValueID::kLeft);
  CSSValuePair* normalized_cy =
      BuildSerializablePositionOffset(center_y_, CSSValueID::kTop);

  String radius_x;
  String radius_y;
  if (radius_x_) {
    DCHECK(radius_y_);

    auto* radius_x_identifier_value =
        DynamicTo<CSSIdentifierValue>(radius_x_.Get());
    bool radius_x_closest_side =
        (radius_x_identifier_value &&
         radius_x_identifier_value->GetValueID() == CSSValueID::kClosestSide);

    auto* radius_y_identifier_value =
        DynamicTo<CSSIdentifierValue>(radius_y_.Get());
    bool radius_y_closest_side =
        (radius_y_identifier_value &&
         radius_y_identifier_value->GetValueID() == CSSValueID::kClosestSide);

    if (!radius_x_closest_side || !radius_y_closest_side) {
      radius_x = radius_x_->CssText();
      radius_y = radius_y_->CssText();
    }
  }

  return BuildEllipseString(
      radius_x, radius_y,
      SerializePositionOffset(*normalized_cx, *normalized_cy),
      SerializePositionOffset(*normalized_cy, *normalized_cx));
}

bool CSSBasicShapeEllipseValue::Equals(
    const CSSBasicShapeEllipseValue& other) const {
  return DataEquivalent(center_x_, other.center_x_) &&
         DataEquivalent(center_y_, other.center_y_) &&
         DataEquivalent(radius_x_, other.radius_x_) &&
         DataEquivalent(radius_y_, other.radius_y_);
}

void CSSBasicShapeEllipseValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(center_x_);
  visitor->Trace(center_y_);
  visitor->Trace(radius_x_);
  visitor->Trace(radius_y_);
  CSSValue::TraceAfterDispatch(visitor);
}

static String BuildPolygonString(const WindRule& wind_rule,
                                 const Vector<String>& points) {
  DCHECK(!(points.size() % 2));

  StringBuilder result;
  const char kEvenOddOpening[] = "polygon(evenodd, ";
  const char kNonZeroOpening[] = "polygon(";
  const char kCommaSeparator[] = ", ";
  static_assert(sizeof(kEvenOddOpening) > sizeof(kNonZeroOpening),
                "polygon string openings should be the same length");

  // Compute the required capacity in advance to reduce allocations.
  wtf_size_t length = sizeof(kEvenOddOpening) - 1;
  for (wtf_size_t i = 0; i < points.size(); i += 2) {
    if (i)
      length += (sizeof(kCommaSeparator) - 1);
    // add length of two strings, plus one for the space separator.
    length += points[i].length() + 1 + points[i + 1].length();
  }
  result.ReserveCapacity(length);

  if (wind_rule == RULE_EVENODD)
    result.Append(kEvenOddOpening);
  else
    result.Append(kNonZeroOpening);

  for (wtf_size_t i = 0; i < points.size(); i += 2) {
    if (i)
      result.Append(kCommaSeparator);
    result.Append(points[i]);
    result.Append(' ');
    result.Append(points[i + 1]);
  }

  result.Append(')');
  return result.ToString();
}

String CSSBasicShapePolygonValue::CustomCSSText() const {
  Vector<String> points;
  points.ReserveInitialCapacity(values_.size());

  for (wtf_size_t i = 0; i < values_.size(); ++i)
    points.push_back(values_.at(i)->CssText());

  return BuildPolygonString(wind_rule_, points);
}

bool CSSBasicShapePolygonValue::Equals(
    const CSSBasicShapePolygonValue& other) const {
  return CompareCSSValueVector(values_, other.values_);
}

void CSSBasicShapePolygonValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(values_);
  CSSValue::TraceAfterDispatch(visitor);
}

static bool BuildInsetRadii(Vector<String>& radii,
                            const String& top_left_radius,
                            const String& top_right_radius,
                            const String& bottom_right_radius,
                            const String& bottom_left_radius) {
  bool show_bottom_left = top_right_radius != bottom_left_radius;
  bool show_bottom_right =
      show_bottom_left || (bottom_right_radius != top_left_radius);
  bool show_top_right =
      show_bottom_right || (top_right_radius != top_left_radius);

  radii.push_back(top_left_radius);
  if (show_top_right)
    radii.push_back(top_right_radius);
  if (show_bottom_right)
    radii.push_back(bottom_right_radius);
  if (show_bottom_left)
    radii.push_back(bottom_left_radius);

  return radii.size() == 1 && radii[0] == "0px";
}

static String BuildInsetString(const String& top,
                               const String& right,
                               const String& bottom,
                               const String& left,
                               const String& top_left_radius_width,
                               const String& top_left_radius_height,
                               const String& top_right_radius_width,
                               const String& top_right_radius_height,
                               const String& bottom_right_radius_width,
                               const String& bottom_right_radius_height,
                               const String& bottom_left_radius_width,
                               const String& bottom_left_radius_height) {
  char opening[] = "inset(";
  char separator[] = " ";
  char corners_separator[] = "round";
  StringBuilder result;
  result.Append(opening);
  result.Append(top);
  bool show_left_arg = !left.IsNull() && left != right;
  bool show_bottom_arg = !bottom.IsNull() && (bottom != top || show_left_arg);
  bool show_right_arg = !right.IsNull() && (right != top || show_bottom_arg);
  if (show_right_arg) {
    result.Append(separator);
    result.Append(right);
  }
  if (show_bottom_arg) {
    result.Append(separator);
    result.Append(bottom);
  }
  if (show_left_arg) {
    result.Append(separator);
    result.Append(left);
  }

  if (!top_left_radius_width.IsNull() && !top_left_radius_height.IsNull()) {
    Vector<String> horizontal_radii;
    bool are_default_corner_radii = BuildInsetRadii(
        horizontal_radii, top_left_radius_width, top_right_radius_width,
        bottom_right_radius_width, bottom_left_radius_width);

    Vector<String> vertical_radii;
    are_default_corner_radii &= BuildInsetRadii(
        vertical_radii, top_left_radius_height, top_right_radius_height,
        bottom_right_radius_height, bottom_left_radius_height);

    if (!are_default_corner_radii) {
      result.Append(separator);
      result.Append(corners_separator);

      for (wtf_size_t i = 0; i < horizontal_radii.size(); ++i) {
        result.Append(separator);
        result.Append(horizontal_radii[i]);
      }
      if (horizontal_radii != vertical_radii) {
        result.Append(separator);
        result.Append('/');

        for (wtf_size_t i = 0; i < vertical_radii.size(); ++i) {
          result.Append(separator);
          result.Append(vertical_radii[i]);
        }
      }
    }
  }
  result.Append(')');

  return result.ToString();
}

static inline void UpdateCornerRadiusWidthAndHeight(
    const CSSValuePair* corner_radius,
    String& width,
    String& height) {
  if (!corner_radius)
    return;

  width = corner_radius->First().CssText();
  height = corner_radius->Second().CssText();
}

String CSSBasicShapeInsetValue::CustomCSSText() const {
  String top_left_radius_width;
  String top_left_radius_height;
  String top_right_radius_width;
  String top_right_radius_height;
  String bottom_right_radius_width;
  String bottom_right_radius_height;
  String bottom_left_radius_width;
  String bottom_left_radius_height;

  UpdateCornerRadiusWidthAndHeight(TopLeftRadius(), top_left_radius_width,
                                   top_left_radius_height);
  UpdateCornerRadiusWidthAndHeight(TopRightRadius(), top_right_radius_width,
                                   top_right_radius_height);
  UpdateCornerRadiusWidthAndHeight(BottomRightRadius(),
                                   bottom_right_radius_width,
                                   bottom_right_radius_height);
  UpdateCornerRadiusWidthAndHeight(BottomLeftRadius(), bottom_left_radius_width,
                                   bottom_left_radius_height);

  return BuildInsetString(
      top_ ? top_->CssText() : String(), right_ ? right_->CssText() : String(),
      bottom_ ? bottom_->CssText() : String(),
      left_ ? left_->CssText() : String(), top_left_radius_width,
      top_left_radius_height, top_right_radius_width, top_right_radius_height,
      bottom_right_radius_width, bottom_right_radius_height,
      bottom_left_radius_width, bottom_left_radius_height);
}

bool CSSBasicShapeInsetValue::Equals(
    const CSSBasicShapeInsetValue& other) const {
  return DataEquivalent(top_, other.top_) &&
         DataEquivalent(right_, other.right_) &&
         DataEquivalent(bottom_, other.bottom_) &&
         DataEquivalent(left_, other.left_) &&
         DataEquivalent(top_left_radius_, other.top_left_radius_) &&
         DataEquivalent(top_right_radius_, other.top_right_radius_) &&
         DataEquivalent(bottom_right_radius_, other.bottom_right_radius_) &&
         DataEquivalent(bottom_left_radius_, other.bottom_left_radius_);
}

void CSSBasicShapeInsetValue::TraceAfterDispatch(blink::Visitor* visitor) {
  visitor->Trace(top_);
  visitor->Trace(right_);
  visitor->Trace(bottom_);
  visitor->Trace(left_);
  visitor->Trace(top_left_radius_);
  visitor->Trace(top_right_radius_);
  visitor->Trace(bottom_right_radius_);
  visitor->Trace(bottom_left_radius_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace cssvalue
}  // namespace blink
