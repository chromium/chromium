// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MIXIN_PARAMETER_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MIXIN_PARAMETER_VALUE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// Marks a mixin parameter whose value is computed on the element using @apply.
class CORE_EXPORT CSSMixinParameterValue : public CSSValue {
 public:
  static CSSMixinParameterValue* Create();

  CSSMixinParameterValue() : CSSValue(kMixinParameterValueClass) {}

  String CustomCSSText() const;

  bool Equals(const CSSMixinParameterValue&) const { return true; }

  void TraceAfterDispatch(blink::Visitor* visitor) const {
    CSSValue::TraceAfterDispatch(visitor);
  }
};

template <>
struct DowncastTraits<CSSMixinParameterValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsMixinParameterValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MIXIN_PARAMETER_VALUE_H_
