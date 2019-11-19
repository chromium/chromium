// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PENDING_INTERPOLATION_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PENDING_INTERPOLATION_VALUE_H_

#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace cssvalue {

// A CSSPendingInterpolationValue represents a value which we don't yet know
// what is, but we know that it's the result of an ongoing interpolation.
// It is a way for interpolations to participate in the cascade, without
// knowing the exact value cascade-time.
//
// See StyleCascade::Animator for more information.
class CORE_EXPORT CSSPendingInterpolationValue : public CSSValue {
 public:
  enum class Type {
    kCSSProperty,
    kPresentationAttribute,
  };

  static CSSPendingInterpolationValue* Create(Type);
  CSSPendingInterpolationValue(Type);

  bool IsCSSProperty() const { return type_ == Type::kCSSProperty; }
  bool IsPresentationAttribute() const {
    return type_ == Type::kPresentationAttribute;
  }
  bool Equals(const CSSPendingInterpolationValue& v) const {
    return type_ == v.type_;
  }

  String CustomCSSText() const { return ""; }

  void TraceAfterDispatch(blink::Visitor* visitor) {
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  Type type_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSPendingInterpolationValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsPendingInterpolationValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PENDING_INTERPOLATION_VALUE_H_
