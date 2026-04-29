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

#include "base/check.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace cssvalue {

namespace {

bool MatchesIdentifier(const CSSValue& value, CSSValueID ident) {
  auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
  return identifier_value && identifier_value->GetValueID() == ident;
}

String SerializePositionOffset(const CSSValuePair& offset,
                               const CSSValuePair& other) {
  if ((To<CSSIdentifierValue>(offset.First()).GetValueID() ==
           CSSValueID::kLeft &&
       To<CSSIdentifierValue>(other.First()).GetValueID() ==
           CSSValueID::kTop) ||
      (To<CSSIdentifierValue>(offset.First()).GetValueID() ==
           CSSValueID::kTop &&
       To<CSSIdentifierValue>(other.First()).GetValueID() ==
           CSSValueID::kLeft)) {
    return offset.Second().CssText();
  }
  return offset.CssText();
}

const CSSValuePair* BuildSerializablePositionOffset(const CSSValue& offset,
                                                    CSSValueID default_side) {
  CSSValueID side = default_side;
  const CSSPrimitiveValue* amount = nullptr;

  if (auto* offset_identifier_value = DynamicTo<CSSIdentifierValue>(offset)) {
    side = offset_identifier_value->GetValueID();
  } else if (auto* offset_value_pair = DynamicTo<CSSValuePair>(offset)) {
    side = To<CSSIdentifierValue>(offset_value_pair->First()).GetValueID();
    amount = &To<CSSPrimitiveValue>(offset_value_pair->Second());
    if ((side == CSSValueID::kRight || side == CSSValueID::kBottom) &&
        amount->IsPercentage()) {
      side = default_side;
      amount =
          amount->SubtractFrom(100, CSSPrimitiveValue::UnitType::kPercentage);
    }
  } else {
    amount = &To<CSSPrimitiveValue>(offset);
  }

  if (side == CSSValueID::kCenter) {
    side = default_side;
    amount = CSSNumericLiteralValue::Create(
        50, CSSPrimitiveValue::UnitType::kPercentage);
  } else if (!amount ||
             (amount->IsLength() && amount->GetValueIfKnown() == 0.0)) {
    if (side == CSSValueID::kRight || side == CSSValueID::kBottom) {
      amount = CSSNumericLiteralValue::Create(
          100, CSSPrimitiveValue::UnitType::kPercentage);
    } else {
      amount = CSSNumericLiteralValue::Create(
          0, CSSPrimitiveValue::UnitType::kPercentage);
    }
    side = default_side;
  }

  return MakeGarbageCollected<CSSValuePair>(CSSIdentifierValue::Create(side),
                                            amount,
                                            CSSValuePair::kKeepIdenticalValues);
}

void SerializePosition(const CSSValue& center_x,
                       const CSSValue& center_y,
                       bool needs_separator,
                       StringBuilder& result) {
  if (needs_separator) {
    result.Append(' ');
  }
  const CSSValuePair* normalized_cx =
      BuildSerializablePositionOffset(center_x, CSSValueID::kLeft);
  const CSSValuePair* normalized_cy =
      BuildSerializablePositionOffset(center_y, CSSValueID::kTop);
  result.Append("at ");
  result.Append(SerializePositionOffset(*normalized_cx, *normalized_cy));
  result.Append(' ');
  result.Append(SerializePositionOffset(*normalized_cy, *normalized_cx));
}

}  // namespace

String CSSBasicShapeCircleValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("circle(");

  bool needs_separator = false;
  if (radius_ && !MatchesIdentifier(*radius_, CSSValueID::kClosestSide)) {
    result.Append(radius_->CssText());
    needs_separator = true;
  }

  const bool has_explicit_center = center_x_;
  if (has_explicit_center) {
    SerializePosition(*center_x_, *center_y_, needs_separator, result);
  }
  result.Append(')');
  return result.ReleaseString();
}

bool CSSBasicShapeCircleValue::Equals(
    const CSSBasicShapeCircleValue& other) const {
  return base::ValuesEquivalent(center_x_, other.center_x_) &&
         base::ValuesEquivalent(center_y_, other.center_y_) &&
         base::ValuesEquivalent(radius_, other.radius_);
}

void CSSBasicShapeCircleValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(center_x_);
  visitor->Trace(center_y_);
  visitor->Trace(radius_);
  CSSValue::TraceAfterDispatch(visitor);
}

String CSSBasicShapeEllipseValue::CustomCSSText() const {
  StringBuilder result;
  result.Append("ellipse(");

  bool needs_separator = false;
  if (radius_x_ && !(MatchesIdentifier(*radius_x_, CSSValueID::kClosestSide) &&
                     MatchesIdentifier(*radius_y_, CSSValueID::kClosestSide))) {
    result.Append(radius_x_->CssText());
    result.Append(' ');
    result.Append(radius_y_->CssText());
    needs_separator = true;
  }

  const bool has_explicit_center = center_x_;
  if (has_explicit_center) {
    SerializePosition(*center_x_, *center_y_, needs_separator, result);
  }
  result.Append(')');
  return result.ReleaseString();
}

bool CSSBasicShapeEllipseValue::Equals(
    const CSSBasicShapeEllipseValue& other) const {
  return base::ValuesEquivalent(center_x_, other.center_x_) &&
         base::ValuesEquivalent(center_y_, other.center_y_) &&
         base::ValuesEquivalent(radius_x_, other.radius_x_) &&
         base::ValuesEquivalent(radius_y_, other.radius_y_);
}

void CSSBasicShapeEllipseValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(center_x_);
  visitor->Trace(center_y_);
  visitor->Trace(radius_x_);
  visitor->Trace(radius_y_);
  CSSValue::TraceAfterDispatch(visitor);
}

static String BuildPolygonString(const WindRule& wind_rule,
                                 const CSSPrimitiveValue* rounding_radius,
                                 const Vector<String>& points) {
  DCHECK(!(points.size() % 2));

  const bool has_rounding_radius =
      rounding_radius &&
      !(rounding_radius->IsNumericLiteralValue() &&
        To<CSSNumericLiteralValue>(*rounding_radius).DoubleValue() == 0);
  const String rounding_radius_text =
      has_rounding_radius ? rounding_radius->CssText() : String();

  StringBuilder result;
  const char kEvenOddPrefix[] = "evenodd";
  const char kRoundPrefix[] = "round ";
  const char kCommaSeparator[] = ", ";

  // Compute the required capacity in advance to reduce allocations.
  wtf_size_t length = sizeof("polygon(") - 1 + 1;
  if (wind_rule == RULE_EVENODD) {
    length += sizeof(kEvenOddPrefix) - 1;
  }
  if (has_rounding_radius) {
    if (wind_rule == RULE_EVENODD) {
      length += 1;
    }
    length += sizeof(kRoundPrefix) - 1 + rounding_radius_text.length();
  }
  if (wind_rule == RULE_EVENODD || has_rounding_radius) {
    length += sizeof(kCommaSeparator) - 1;
  }
  for (wtf_size_t i = 0; i < points.size(); i += 2) {
    if (i) {
      length += (sizeof(kCommaSeparator) - 1);
    }
    // add length of two strings, plus one for the space separator.
    length += points[i].length() + 1 + points[i + 1].length();
  }
  result.ReserveCapacity(length);

  result.Append("polygon(");
  bool has_prefix = false;
  if (wind_rule == RULE_EVENODD) {
    result.Append(kEvenOddPrefix);
    has_prefix = true;
  }
  if (has_rounding_radius) {
    if (has_prefix) {
      result.Append(' ');
    }
    result.Append(kRoundPrefix);
    result.Append(rounding_radius_text);
    has_prefix = true;
  }
  if (has_prefix) {
    result.Append(kCommaSeparator);
  }

  for (wtf_size_t i = 0; i < points.size(); i += 2) {
    if (i) {
      result.Append(kCommaSeparator);
    }
    result.Append(points[i]);
    result.Append(' ');
    result.Append(points[i + 1]);
  }

  result.Append(')');
  return result.ReleaseString();
}

String CSSBasicShapePolygonValue::CustomCSSText() const {
  Vector<String> points(values_,
                        [](const CSSValue* value) { return value->CssText(); });

  return BuildPolygonString(wind_rule_, rounding_radius_, points);
}

bool CSSBasicShapePolygonValue::Equals(
    const CSSBasicShapePolygonValue& other) const {
  return wind_rule_ == other.wind_rule_ &&
         base::ValuesEquivalent(rounding_radius_, other.rounding_radius_) &&
         CompareCSSValueVector(values_, other.values_);
}

void CSSBasicShapePolygonValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(rounding_radius_);
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
  if (show_top_right) {
    radii.push_back(top_right_radius);
  }
  if (show_bottom_right) {
    radii.push_back(bottom_right_radius);
  }
  if (show_bottom_left) {
    radii.push_back(bottom_left_radius);
  }

  return radii.size() == 1 && radii[0] == "0px";
}

static void AppendRoundedCorners(const char* separator,
                                 const String& top_left_radius_width,
                                 const String& top_left_radius_height,
                                 const String& top_right_radius_width,
                                 const String& top_right_radius_height,
                                 const String& bottom_right_radius_width,
                                 const String& bottom_right_radius_height,
                                 const String& bottom_left_radius_width,
                                 const String& bottom_left_radius_height,
                                 StringBuilder& result) {
  char corners_separator[] = "round";
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
}

static String BuildRectStringCommon(const char* opening,
                                    bool show_left_arg,
                                    const String& top,
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
  char separator[] = " ";
  StringBuilder result;
  result.Append(opening);
  result.Append(top);
  show_left_arg |= !left.IsNull() && left != right;
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

  AppendRoundedCorners(separator, top_left_radius_width, top_left_radius_height,
                       top_right_radius_width, top_right_radius_height,
                       bottom_right_radius_width, bottom_right_radius_height,
                       bottom_left_radius_width, bottom_left_radius_height,
                       result);

  result.Append(')');

  return result.ReleaseString();
}

static String BuildXYWHString(const String& x,
                              const String& y,
                              const String& width,
                              const String& height,
                              const String& top_left_radius_width,
                              const String& top_left_radius_height,
                              const String& top_right_radius_width,
                              const String& top_right_radius_height,
                              const String& bottom_right_radius_width,
                              const String& bottom_right_radius_height,
                              const String& bottom_left_radius_width,
                              const String& bottom_left_radius_height) {
  const char opening[] = "xywh(";
  char separator[] = " ";
  StringBuilder result;

  result.Append(opening);
  result.Append(x);

  result.Append(separator);
  result.Append(y);

  result.Append(separator);
  result.Append(width);

  result.Append(separator);
  result.Append(height);

  AppendRoundedCorners(separator, top_left_radius_width, top_left_radius_height,
                       top_right_radius_width, top_right_radius_height,
                       bottom_right_radius_width, bottom_right_radius_height,
                       bottom_left_radius_width, bottom_left_radius_height,
                       result);

  result.Append(')');

  return result.ReleaseString();
}

static inline void UpdateCornerRadiusWidthAndHeight(
    const CSSValuePair* corner_radius,
    String& width,
    String& height) {
  if (!corner_radius) {
    return;
  }

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

  return BuildRectStringCommon(
      "inset(", false, top_ ? top_->CssText() : String(),
      right_ ? right_->CssText() : String(),
      bottom_ ? bottom_->CssText() : String(),
      left_ ? left_->CssText() : String(), top_left_radius_width,
      top_left_radius_height, top_right_radius_width, top_right_radius_height,
      bottom_right_radius_width, bottom_right_radius_height,
      bottom_left_radius_width, bottom_left_radius_height);
}

bool CSSBasicShapeInsetValue::Equals(
    const CSSBasicShapeInsetValue& other) const {
  return base::ValuesEquivalent(top_, other.top_) &&
         base::ValuesEquivalent(right_, other.right_) &&
         base::ValuesEquivalent(bottom_, other.bottom_) &&
         base::ValuesEquivalent(left_, other.left_) &&
         base::ValuesEquivalent(top_left_radius_, other.top_left_radius_) &&
         base::ValuesEquivalent(top_right_radius_, other.top_right_radius_) &&
         base::ValuesEquivalent(bottom_right_radius_,
                                other.bottom_right_radius_) &&
         base::ValuesEquivalent(bottom_left_radius_, other.bottom_left_radius_);
}

void CSSBasicShapeInsetValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
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

String CSSBasicShapeRectValue::CustomCSSText() const {
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

  return BuildRectStringCommon(
      "rect(", true, top_->CssText(), right_->CssText(), bottom_->CssText(),
      left_->CssText(), top_left_radius_width, top_left_radius_height,
      top_right_radius_width, top_right_radius_height,
      bottom_right_radius_width, bottom_right_radius_height,
      bottom_left_radius_width, bottom_left_radius_height);
}

bool CSSBasicShapeRectValue::Equals(const CSSBasicShapeRectValue& other) const {
  return base::ValuesEquivalent(top_, other.top_) &&
         base::ValuesEquivalent(right_, other.right_) &&
         base::ValuesEquivalent(bottom_, other.bottom_) &&
         base::ValuesEquivalent(left_, other.left_) &&
         base::ValuesEquivalent(top_left_radius_, other.top_left_radius_) &&
         base::ValuesEquivalent(top_right_radius_, other.top_right_radius_) &&
         base::ValuesEquivalent(bottom_right_radius_,
                                other.bottom_right_radius_) &&
         base::ValuesEquivalent(bottom_left_radius_, other.bottom_left_radius_);
}

void CSSBasicShapeRectValue::TraceAfterDispatch(blink::Visitor* visitor) const {
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

void CSSBasicShapeRectValue::Validate() const {
  auto validate_length = [](const CSSValue* length) {
    if (length->IsIdentifierValue()) {
      DCHECK(To<CSSIdentifierValue>(length)->GetValueID() == CSSValueID::kAuto);
      return;
    }
    DCHECK(length->IsPrimitiveValue());
  };

  validate_length(top_);
  validate_length(left_);
  validate_length(bottom_);
  validate_length(right_);
}

String CSSBasicShapeXYWHValue::CustomCSSText() const {
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

  return BuildXYWHString(x_->CssText(), y_->CssText(), width_->CssText(),
                         height_->CssText(), top_left_radius_width,
                         top_left_radius_height, top_right_radius_width,
                         top_right_radius_height, bottom_right_radius_width,
                         bottom_right_radius_height, bottom_left_radius_width,
                         bottom_left_radius_height);
}

bool CSSBasicShapeXYWHValue::Equals(const CSSBasicShapeXYWHValue& other) const {
  return base::ValuesEquivalent(x_, other.x_) &&
         base::ValuesEquivalent(y_, other.y_) &&
         base::ValuesEquivalent(width_, other.width_) &&
         base::ValuesEquivalent(height_, other.height_) &&
         base::ValuesEquivalent(top_left_radius_, other.top_left_radius_) &&
         base::ValuesEquivalent(top_right_radius_, other.top_right_radius_) &&
         base::ValuesEquivalent(bottom_right_radius_,
                                other.bottom_right_radius_) &&
         base::ValuesEquivalent(bottom_left_radius_, other.bottom_left_radius_);
}

void CSSBasicShapeXYWHValue::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(top_left_radius_);
  visitor->Trace(top_right_radius_);
  visitor->Trace(bottom_right_radius_);
  visitor->Trace(bottom_left_radius_);
  CSSValue::TraceAfterDispatch(visitor);
}

void CSSBasicShapeXYWHValue::Validate() const {
  DCHECK(x_);
  DCHECK(y_);
  DCHECK(width_);
  DCHECK(height_);

  // The spec requires non-negative width and height but we can only validate
  // numeric literals here.
  if (width_->IsNumericLiteralValue()) {
    DCHECK_GE(To<CSSNumericLiteralValue>(*width_).ClampedDoubleValue(), 0);
  }
  if (height_->IsNumericLiteralValue()) {
    DCHECK_GE(To<CSSNumericLiteralValue>(*height_).ClampedDoubleValue(), 0);
  }
}

}  // namespace cssvalue
}  // namespace blink
