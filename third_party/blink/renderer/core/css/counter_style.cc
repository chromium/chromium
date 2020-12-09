// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counter_style.h"

#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"

namespace blink {

namespace {

CounterStyleSystem ToCounterStyleSystemEnum(const CSSValue* value) {
  if (!value)
    return CounterStyleSystem::kSymbolic;

  CSSValueID system_keyword;
  if (const auto* id = DynamicTo<CSSIdentifierValue>(value)) {
    system_keyword = id->GetValueID();
  } else {
    // Either fixed or extends.
    DCHECK(value->IsValuePair());
    const CSSValuePair* pair = To<CSSValuePair>(value);
    DCHECK(pair->First().IsIdentifierValue());
    system_keyword = To<CSSIdentifierValue>(pair->First()).GetValueID();
  }

  switch (system_keyword) {
    case CSSValueID::kCyclic:
      return CounterStyleSystem::kCyclic;
    case CSSValueID::kFixed:
      return CounterStyleSystem::kFixed;
    case CSSValueID::kSymbolic:
      return CounterStyleSystem::kSymbolic;
    case CSSValueID::kAlphabetic:
      return CounterStyleSystem::kAlphabetic;
    case CSSValueID::kNumeric:
      return CounterStyleSystem::kNumeric;
    case CSSValueID::kAdditive:
      return CounterStyleSystem::kAdditive;
    case CSSValueID::kExtends:
      return CounterStyleSystem::kUnresolvedExtends;
    default:
      NOTREACHED();
      return CounterStyleSystem::kSymbolic;
  }
}

bool SymbolsAreValid(const StyleRuleCounterStyle& rule,
                     CounterStyleSystem system) {
  const CSSValueList* symbols = To<CSSValueList>(rule.GetSymbols());
  const CSSValueList* additive_symbols =
      To<CSSValueList>(rule.GetAdditiveSymbols());
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
  }
}

}  // namespace

// static
CounterStyle& CounterStyle::GetDecimal() {
  DEFINE_STATIC_LOCAL(
      Persistent<CounterStyle>, decimal,
      (CounterStyleMap::GetUACounterStyleMap()->FindCounterStyleAcrossScopes(
          "decimal")));
  DCHECK(decimal);
  return *decimal;
}

CounterStyle::~CounterStyle() = default;

AtomicString CounterStyle::GetName() const {
  return style_rule_->GetName();
}

// static
CounterStyle* CounterStyle::Create(const StyleRuleCounterStyle& rule) {
  CounterStyleSystem system = ToCounterStyleSystemEnum(rule.GetSystem());
  if (!SymbolsAreValid(rule, system))
    return nullptr;

  return MakeGarbageCollected<CounterStyle>(rule);
}

CounterStyle::CounterStyle(const StyleRuleCounterStyle& rule)
    : style_rule_(rule) {
  if (const CSSValue* system = rule.GetSystem()) {
    system_ = ToCounterStyleSystemEnum(system);

    if (system_ == CounterStyleSystem::kUnresolvedExtends) {
      const auto& second = To<CSSValuePair>(system)->Second();
      extends_name_ = To<CSSCustomIdentValue>(second).Value();
    }
  }

  if (const CSSValue* fallback = rule.GetFallback())
    fallback_name_ = To<CSSCustomIdentValue>(fallback)->Value();

  // TODO(crbug.com/687225): Implement and populate other fields.
}

void CounterStyle::ResolveExtends(const CounterStyle& extended) {
  DCHECK_NE(extended.system_, CounterStyleSystem::kUnresolvedExtends);
  extended_style_ = extended;

  system_ = extended.system_;

  if (!style_rule_->GetFallback()) {
    fallback_name_ = extended.fallback_name_;
    fallback_style_ = nullptr;
  }

  // TODO(crbug.com/687225): Implement and populate other fields.
}

void CounterStyle::ResetExtends() {
  if (extends_name_.IsNull() || extends_name_ == "decimal" ||
      extends_name_ == "disc")
    return;
  system_ = CounterStyleSystem::kUnresolvedExtends;
  extended_style_.Clear();
}

void CounterStyle::ResetFallback() {
  if (fallback_name_ == "decimal" || fallback_name_ == "disc")
    return;
  fallback_style_.Clear();
}

void CounterStyle::Trace(Visitor* visitor) const {
  visitor->Trace(style_rule_);
  visitor->Trace(extended_style_);
  visitor->Trace(fallback_style_);
}

}  // namespace blink
