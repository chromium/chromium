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

Member<const CSSValue>& StyleRuleCounterStyle::GetDescriptorReference(
    AtRuleDescriptorID descriptor_id) {
  switch (descriptor_id) {
    case AtRuleDescriptorID::System:
      return system_;
    case AtRuleDescriptorID::Negative:
      return negative_;
    case AtRuleDescriptorID::Prefix:
      return prefix_;
    case AtRuleDescriptorID::Suffix:
      return suffix_;
    case AtRuleDescriptorID::Range:
      return range_;
    case AtRuleDescriptorID::Pad:
      return pad_;
    case AtRuleDescriptorID::Fallback:
      return fallback_;
    case AtRuleDescriptorID::Symbols:
      return symbols_;
    case AtRuleDescriptorID::AdditiveSymbols:
      return additive_symbols_;
    case AtRuleDescriptorID::SpeakAs:
      return speak_as_;
    default:
      NOTREACHED_IN_MIGRATION();
      return speak_as_;
  }
}

bool StyleRuleCounterStyle::NewValueInvalidOrEqual(
    AtRuleDescriptorID descriptor_id,
    const CSSValue* new_value) {
  Member<const CSSValue>& original_value =
      GetDescriptorReference(descriptor_id);
  if (base::ValuesEquivalent(original_value.Get(), new_value)) {
    return false;
  }

  switch (descriptor_id) {
    case AtRuleDescriptorID::System:
      // If the attribute being set is system, and the new value would change
      // the algorithm used, do nothing and abort these steps.
      return CounterStyle::ToCounterStyleSystemEnum(system_) ==
             CounterStyle::ToCounterStyleSystemEnum(new_value);
    case AtRuleDescriptorID::Symbols:
    case AtRuleDescriptorID::AdditiveSymbols: {
      // If the returned value would cause the @counter-style rule to become
      // invalid, do nothing and abort these steps.
      base::AutoReset<Member<const CSSValue>> auto_reset(&original_value,
                                                         new_value);
      return HasValidSymbols();
    }
    default:
      return true;
  }
}

void StyleRuleCounterStyle::SetDescriptorValue(AtRuleDescriptorID descriptor_id,
                                               const CSSValue* new_value) {
  GetDescriptorReference(descriptor_id) = new_value;
  ++version_;
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
  visitor->Trace(layer_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
