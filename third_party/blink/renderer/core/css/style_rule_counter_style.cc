// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"

#include "third_party/blink/renderer/core/css/css_counter_style_rule.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"

namespace blink {

StyleRuleCounterStyle::StyleRuleCounterStyle(const AtomicString& name,
                                             CSSPropertyValueSet* properties)
    : StyleRuleBase(kCounterStyle),
      name_(name),
      system_(properties->GetPropertyCSSValue(CSSPropertyID::kSystem)),
      negative_(properties->GetPropertyCSSValue(CSSPropertyID::kNegative)),
      prefix_(properties->GetPropertyCSSValue(CSSPropertyID::kPrefix)),
      suffix_(properties->GetPropertyCSSValue(CSSPropertyID::kSuffix)),
      range_(properties->GetPropertyCSSValue(CSSPropertyID::kRange)),
      pad_(properties->GetPropertyCSSValue(CSSPropertyID::kPad)),
      fallback_(properties->GetPropertyCSSValue(CSSPropertyID::kFallback)),
      symbols_(properties->GetPropertyCSSValue(CSSPropertyID::kSymbols)),
      additive_symbols_(
          properties->GetPropertyCSSValue(CSSPropertyID::kAdditiveSymbols)),
      speak_as_(properties->GetPropertyCSSValue(CSSPropertyID::kSpeakAs)) {
  DCHECK(properties);
}

StyleRuleCounterStyle::StyleRuleCounterStyle(const StyleRuleCounterStyle&) =
    default;

StyleRuleCounterStyle::~StyleRuleCounterStyle() = default;

void StyleRuleCounterStyle::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(system_);
  visitor->Trace(negative_);
  visitor->Trace(prefix_);
  visitor->Trace(suffix_);
  visitor->Trace(range_);
  visitor->Trace(pad_);
  visitor->Trace(fallback_);
  visitor->Trace(symbols_);
  visitor->Trace(additive_symbols_);
  visitor->Trace(speak_as_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
