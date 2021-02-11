// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"

#include "third_party/blink/renderer/core/css/counter_style.h"
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

bool StyleRuleCounterStyle::HasValidSymbols() const {
  CounterStyleSystem system =
      CounterStyle::ToCounterStyleSystemEnum(GetSystem());
  const auto* symbols = To<CSSValueList>(GetSymbols());
  const auto* additive_symbols = To<CSSValueList>(GetAdditiveSymbols());
  switch (system) {
    case CounterStyleSystem::kCyclic:
    case CounterStyleSystem::kFixed:
    case CounterStyleSystem::kSymbolic:
      return symbols && symbols->length();
    case CounterStyleSystem::kAlphabetic:
    case CounterStyleSystem::kNumeric:
      return symbols && symbols->length() > 1u;
    case CounterStyleSystem::kAdditive:
      return additive_symbols && additive_symbols->length();
    case CounterStyleSystem::kUnresolvedExtends:
      return !symbols && !additive_symbols;
    case CounterStyleSystem::kHebrew:
    case CounterStyleSystem::kSimpChineseInformal:
    case CounterStyleSystem::kSimpChineseFormal:
    case CounterStyleSystem::kTradChineseInformal:
    case CounterStyleSystem::kTradChineseFormal:
    case CounterStyleSystem::kKoreanHangulFormal:
    case CounterStyleSystem::kKoreanHanjaInformal:
    case CounterStyleSystem::kKoreanHanjaFormal:
    case CounterStyleSystem::kLowerArmenian:
    case CounterStyleSystem::kUpperArmenian:
    case CounterStyleSystem::kEthiopicNumeric:
      return true;
  }
}

bool StyleRuleCounterStyle::SetSystem(const CSSValue* system) {
  CounterStyleSystem old_system =
      CounterStyle::ToCounterStyleSystemEnum(system_);
  CounterStyleSystem new_system =
      CounterStyle::ToCounterStyleSystemEnum(system);

  // If the attribute being set is system, and the new value would change the
  // algorithm used, do nothing and abort these steps.
  if (old_system != new_system)
    return false;

  // Except 'fixed' and 'extends', other systems have nothing to modify.
  if (new_system != CounterStyleSystem::kFixed &&
      new_system != CounterStyleSystem::kUnresolvedExtends)
    return false;

  system_ = system;
  DCHECK(HasValidSymbols());

  ++version_;
  return true;
}

bool StyleRuleCounterStyle::SetSymbols(const CSSValue* symbols) {
  const CSSValue* original_symbols = symbols_;
  symbols_ = symbols;
  if (!HasValidSymbols()) {
    symbols_ = original_symbols;
    return false;
  }
  ++version_;
  return true;
}

bool StyleRuleCounterStyle::SetAdditiveSymbols(
    const CSSValue* additive_symbols) {
  const CSSValue* original_additive_symbols = additive_symbols_;
  additive_symbols_ = additive_symbols;
  if (!HasValidSymbols()) {
    additive_symbols_ = original_additive_symbols;
    return false;
  }
  ++version_;
  return true;
}

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
