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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_BASIC_SHAPE_VALUES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_BASIC_SHAPE_VALUES_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
namespace cssvalue {

class CSSBasicShapeCircleValue final : public CSSValue {
 public:
  CSSBasicShapeCircleValue() : CSSValue(kBasicShapeCircleClass) {}

  String CustomCSSText() const;
  bool Equals(const CSSBasicShapeCircleValue&) const;

  CSSValue* CenterX() const { return center_x_.Get(); }
  CSSValue* CenterY() const { return center_y_.Get(); }
  CSSValue* Radius() const { return radius_.Get(); }

  // TODO(sashab): Remove these and pass them as arguments in the constructor.
  void SetCenterX(CSSValue* center_x) { center_x_ = center_x; }
  void SetCenterY(CSSValue* center_y) { center_y_ = center_y; }
  void SetRadius(CSSValue* radius) { radius_ = radius; }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  Member<CSSValue> center_x_;
  Member<CSSValue> center_y_;
  Member<CSSValue> radius_;
};

class CSSBasicShapeEllipseValue final : public CSSValue {
 public:
  CSSBasicShapeEllipseValue() : CSSValue(kBasicShapeEllipseClass) {}

  String CustomCSSText() const;
  bool Equals(const CSSBasicShapeEllipseValue&) const;

  CSSValue* CenterX() const { return center_x_.Get(); }
  CSSValue* CenterY() const { return center_y_.Get(); }
  CSSValue* RadiusX() const { return radius_x_.Get(); }
  CSSValue* RadiusY() const { return radius_y_.Get(); }

  // TODO(sashab): Remove these and pass them as arguments in the constructor.
  void SetCenterX(CSSValue* center_x) { center_x_ = center_x; }
  void SetCenterY(CSSValue* center_y) { center_y_ = center_y; }
  void SetRadiusX(CSSValue* radius_x) { radius_x_ = radius_x; }
  void SetRadiusY(CSSValue* radius_y) { radius_y_ = radius_y; }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  Member<CSSValue> center_x_;
  Member<CSSValue> center_y_;
  Member<CSSValue> radius_x_;
  Member<CSSValue> radius_y_;
};

class CSSBasicShapePolygonValue final : public CSSValue {
 public:
  CSSBasicShapePolygonValue()
      : CSSValue(kBasicShapePolygonClass), wind_rule_(RULE_NONZERO) {}

  void AppendPoint(CSSPrimitiveValue* x, CSSPrimitiveValue* y) {
    values_.push_back(x);
    values_.push_back(y);
  }

  CSSPrimitiveValue* GetXAt(unsigned i) const { return values_.at(i * 2); }
  CSSPrimitiveValue* GetYAt(unsigned i) const { return values_.at(i * 2 + 1); }
  const HeapVector<Member<CSSPrimitiveValue>>& Values() const {
    return values_;
  }

  // TODO(sashab): Remove this and pass it as an argument in the constructor.
  void SetWindRule(WindRule w) { wind_rule_ = w; }
  WindRule GetWindRule() const { return wind_rule_; }

  String CustomCSSText() const;
  bool Equals(const CSSBasicShapePolygonValue&) const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  HeapVector<Member<CSSPrimitiveValue>> values_;
  WindRule wind_rule_;
};

class CSSBasicShapeInsetValue final : public CSSValue {
 public:
  CSSBasicShapeInsetValue() : CSSValue(kBasicShapeInsetClass) {}

  CSSPrimitiveValue* Top() const { return top_.Get(); }
  CSSPrimitiveValue* Right() const { return right_.Get(); }
  CSSPrimitiveValue* Bottom() const { return bottom_.Get(); }
  CSSPrimitiveValue* Left() const { return left_.Get(); }

  CSSValuePair* TopLeftRadius() const { return top_left_radius_.Get(); }
  CSSValuePair* TopRightRadius() const { return top_right_radius_.Get(); }
  CSSValuePair* BottomRightRadius() const { return bottom_right_radius_.Get(); }
  CSSValuePair* BottomLeftRadius() const { return bottom_left_radius_.Get(); }

  // TODO(sashab): Remove these and pass them as arguments in the constructor.
  void SetTop(CSSPrimitiveValue* top) { top_ = top; }
  void SetRight(CSSPrimitiveValue* right) { right_ = right; }
  void SetBottom(CSSPrimitiveValue* bottom) { bottom_ = bottom; }
  void SetLeft(CSSPrimitiveValue* left) { left_ = left; }

  void UpdateShapeSize4Values(CSSPrimitiveValue* top,
                              CSSPrimitiveValue* right,
                              CSSPrimitiveValue* bottom,
                              CSSPrimitiveValue* left) {
    SetTop(top);
    SetRight(right);
    SetBottom(bottom);
    SetLeft(left);
  }

  void UpdateShapeSize1Value(CSSPrimitiveValue* value1) {
    UpdateShapeSize4Values(value1, value1, value1, value1);
  }

  void UpdateShapeSize2Values(CSSPrimitiveValue* value1,
                              CSSPrimitiveValue* value2) {
    UpdateShapeSize4Values(value1, value2, value1, value2);
  }

  void UpdateShapeSize3Values(CSSPrimitiveValue* value1,
                              CSSPrimitiveValue* value2,
                              CSSPrimitiveValue* value3) {
    UpdateShapeSize4Values(value1, value2, value3, value2);
  }

  void SetTopLeftRadius(CSSValuePair* radius) { top_left_radius_ = radius; }
  void SetTopRightRadius(CSSValuePair* radius) { top_right_radius_ = radius; }
  void SetBottomRightRadius(CSSValuePair* radius) {
    bottom_right_radius_ = radius;
  }
  void SetBottomLeftRadius(CSSValuePair* radius) {
    bottom_left_radius_ = radius;
  }

  String CustomCSSText() const;
  bool Equals(const CSSBasicShapeInsetValue&) const;

  void TraceAfterDispatch(blink::Visitor*);

 private:
  Member<CSSPrimitiveValue> top_;
  Member<CSSPrimitiveValue> right_;
  Member<CSSPrimitiveValue> bottom_;
  Member<CSSPrimitiveValue> left_;

  Member<CSSValuePair> top_left_radius_;
  Member<CSSValuePair> top_right_radius_;
  Member<CSSValuePair> bottom_right_radius_;
  Member<CSSValuePair> bottom_left_radius_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSBasicShapeCircleValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsBasicShapeCircleValue();
  }
};

template <>
struct DowncastTraits<cssvalue::CSSBasicShapeEllipseValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsBasicShapeEllipseValue();
  }
};

template <>
struct DowncastTraits<cssvalue::CSSBasicShapePolygonValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsBasicShapePolygonValue();
  }
};

template <>
struct DowncastTraits<cssvalue::CSSBasicShapeInsetValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsBasicShapeInsetValue();
  }
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_BASIC_SHAPE_VALUES_H_
