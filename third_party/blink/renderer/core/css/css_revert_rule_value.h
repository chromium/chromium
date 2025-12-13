// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_REVERT_RULE_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_REVERT_RULE_VALUE_H_

#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSValuePool;

namespace cssvalue {

// Represents the 'revert-rule' keyword, which rolls back the cascade
// to the previous rule for a given declaration.
//
// https://github.com/w3c/csswg-drafts/issues/10443
class CORE_EXPORT CSSRevertRuleValue : public CSSValue {
 public:
  static CSSRevertRuleValue* Create();

  explicit CSSRevertRuleValue(base::PassKey<CSSValuePool>)
      : CSSValue(kRevertRuleClass) {}

  String CustomCSSText() const;

  bool Equals(const CSSRevertRuleValue&) const { return true; }

  void TraceAfterDispatch(blink::Visitor* visitor) const {
    CSSValue::TraceAfterDispatch(visitor);
  }
};

}  // namespace cssvalue

template <>
struct DowncastTraits<cssvalue::CSSRevertRuleValue> {
  static bool AllowFrom(const CSSValue& value) {
    return value.IsRevertRuleValue();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_REVERT_RULE_VALUE_H_
