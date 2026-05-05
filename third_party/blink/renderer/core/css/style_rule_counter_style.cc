// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"

#include "base/auto_reset.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/counter_style.h"
#include "third_party/blink/renderer/core/css/css_counter_style_rule.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"

namespace blink {

StyleRuleCounterStyle::StyleRuleCounterStyle(const AtomicString& name,
                                             CSSPropertyValueSet* properties)
    : StyleRuleBase(kCounterStyle), name_(name), properties_(properties) {
  DCHECK(properties);
}

StyleRuleCounterStyle::StyleRuleCounterStyle(const StyleRuleCounterStyle&) =
    default;

StyleRuleCounterStyle::~StyleRuleCounterStyle() = default;

const CSSValue* StyleRuleCounterStyle::GetSystem() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kSystem);
}
const CSSValue* StyleRuleCounterStyle::GetNegative() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kNegative);
}
const CSSValue* StyleRuleCounterStyle::GetPrefix() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kPrefix);
}
const CSSValue* StyleRuleCounterStyle::GetSuffix() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kSuffix);
}
const CSSValue* StyleRuleCounterStyle::GetRange() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kRange);
}
const CSSValue* StyleRuleCounterStyle::GetPad() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kPad);
}
const CSSValue* StyleRuleCounterStyle::GetFallback() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kFallback);
}
const CSSValue* StyleRuleCounterStyle::GetSymbols() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kSymbols);
}
const CSSValue* StyleRuleCounterStyle::GetAdditiveSymbols() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kAdditiveSymbols);
}
const CSSValue* StyleRuleCounterStyle::GetSpeakAs() const {
  return properties_->GetPropertyCSSValue(CSSPropertyID::kSpeakAs);
}

MutableCSSPropertyValueSet& StyleRuleCounterStyle::MutableStyleForInspector() {
  version_++;
  return Properties();
}

MutableCSSPropertyValueSet& StyleRuleCounterStyle::Properties() {
  if (!properties_->IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

bool StyleRuleCounterStyle::HasValidSymbols() const {
  return HasValidSymbols(GetSystem(), GetSymbols(), GetAdditiveSymbols());
}

bool StyleRuleCounterStyle::HasValidSymbols(
    const CSSValue* system_value,
    const CSSValue* symbols_value,
    const CSSValue* additive_symbols_value) {
  CounterStyleSystem system =
      CounterStyle::ToCounterStyleSystemEnum(system_value);
  const auto* symbols = To<CSSValueList>(symbols_value);
  const auto* additive_symbols = To<CSSValueList>(additive_symbols_value);
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

bool StyleRuleCounterStyle::NewValueInvalidOrEqual(
    AtRuleDescriptorID descriptor_id,
    const CSSValue* new_value) {
  const CSSValue* original_value = properties_->GetPropertyCSSValue(
      AtRuleDescriptorIDAsCSSPropertyID(descriptor_id));
  if (base::ValuesEquivalent(original_value, new_value)) {
    return false;
  }

  switch (descriptor_id) {
    case AtRuleDescriptorID::System:
      // If the attribute being set is system, and the new value would change
      // the algorithm used, do nothing and abort these steps.
      return CounterStyle::ToCounterStyleSystemEnum(GetSystem()) ==
             CounterStyle::ToCounterStyleSystemEnum(new_value);
    case AtRuleDescriptorID::Symbols:
      return HasValidSymbols(GetSystem(), new_value, GetAdditiveSymbols());
    case AtRuleDescriptorID::AdditiveSymbols:
      return HasValidSymbols(GetSystem(), GetSymbols(), new_value);
    default:
      return true;
  }
}

void StyleRuleCounterStyle::SetDescriptorValue(AtRuleDescriptorID descriptor_id,
                                               const CSSValue* new_value) {
  MutableStyleForInspector().SetProperty(
      AtRuleDescriptorIDAsCSSPropertyID(descriptor_id), *new_value);
}

void StyleRuleCounterStyle::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
