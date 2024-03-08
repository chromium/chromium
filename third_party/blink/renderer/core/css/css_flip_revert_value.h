// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FLIP_REVERT_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FLIP_REVERT_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

namespace cssvalue {

// This value behaves like CSSRevertLayerValue, except it reverts to the value
// of a different property, given by the property parameter, instead of
// reverting to a value for the same property like revert-layer does.
// This "cross-property" revert-layer behavior is useful for implementing
// "try tactics" [1] from CSS Anchor Positioning.
//
// Note that this class intentionally does not inherit from CSSFunctionValue,
// even though it's serialized as a function. This is because CSSFlipRevertValue
// is only created internally - there's no way to *parse* such a value,
// and therefore we have no CSSValueID for CSSFlipRevertValue.
//
// [1]
// https://drafts.csswg.org/css-anchor-position-1/#typedef-position-try-options-try-tactic
class CORE_EXPORT CSSFlipRevertValue : public CSSValue {
 public:
  explicit CSSFlipRevertValue(CSSPropertyID property_id)
      : CSSValue(kFlipRevertClass), property_id_(property_id) {
    CHECK_NE(property_id, CSSPropertyID::kInvalid);
    CHECK_NE(property_id, CSSPropertyID::kVariable);
  }

  CSSPropertyID PropertyID() const { return property_id_; }

  String CustomCSSText() const;

  bool Equals(const CSSFlipRevertValue& o) const {
    return property_id_ == o.property_id_;
  }

  void TraceAfterDispatch(blink::Visitor* visitor) const {
    CSSValue::TraceAfterDispatch(visitor);
  }

 private:
  // We revert to the value of this property, which is typically not the same
  // as the property holding this value.
  CSSPropertyID property_id_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSFlipRevertValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsFlipRevertValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_FLIP_REVERT_VALUE_H_
