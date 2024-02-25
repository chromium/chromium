// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_DYNAMIC_RANGE_LIMIT_MIX_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_DYNAMIC_RANGE_LIMIT_MIX_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {
namespace cssvalue {

class CORE_EXPORT CSSDynamicRangeLimitMixValue : public CSSValue {
 public:
  CSSDynamicRangeLimitMixValue(const CSSValue* limit1,
                               const CSSValue* limit2,
                               const CSSPrimitiveValue* p)
      : CSSValue(kDynamicRangeLimitMixClass),
        limit1_(limit1),
        limit2_(limit2),
        percentage_(p) {}

  String CustomCSSText() const;

  void TraceAfterDispatch(blink::Visitor* visitor) const;

  bool Equals(const CSSDynamicRangeLimitMixValue& other) const;

  const CSSValue& Limit1() const { return *limit1_; }
  const CSSValue& Limit2() const { return *limit2_; }
  const CSSPrimitiveValue& Percentage() const { return *percentage_; }

 private:
  Member<const CSSValue> limit1_;
  Member<const CSSValue> limit2_;
  Member<const CSSPrimitiveValue> percentage_;
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSDynamicRangeLimitMixValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsDynamicRangeLimitMixValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_DYNAMIC_RANGE_LIMIT_MIX_VALUE_H_
