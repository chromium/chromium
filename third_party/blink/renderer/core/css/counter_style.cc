// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/counter_style.h"

#include "third_party/blink/renderer/core/css/counter_style_map.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/css_value_pair.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// User agents must support representations at least 60 Unicode codepoints long,
// but they may choose to instead use the fallback style for representations
// that would be longer than 60 codepoints. Since WTF::String may use UTF-16, we
// limit string length at 120.
const wtf_size_t kCounterLengthLimit = 120;

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

String SymbolToString(const CSSValue& value) {
  if (const CSSStringValue* string = DynamicTo<CSSStringValue>(value))
    return string->Value();
  return To<CSSCustomIdentValue>(value).Value();
}

std::pair<int, int> BoundsToIntegerPair(const CSSValuePair& bounds) {
  int lower_bound, upper_bound;
  if (bounds.First().IsIdentifierValue()) {
    DCHECK_EQ(CSSValueID::kInfinite,
              To<CSSIdentifierValue>(bounds.First()).GetValueID());
    lower_bound = std::numeric_limits<int>::min();
  } else {
    DCHECK(bounds.First().IsPrimitiveValue());
    lower_bound = To<CSSPrimitiveValue>(bounds.First()).GetIntValue();
  }
  if (bounds.Second().IsIdentifierValue()) {
    DCHECK_EQ(CSSValueID::kInfinite,
              To<CSSIdentifierValue>(bounds.Second()).GetValueID());
    upper_bound = std::numeric_limits<int>::max();
  } else {
    DCHECK(bounds.Second().IsPrimitiveValue());
    upper_bound = To<CSSPrimitiveValue>(bounds.Second()).GetIntValue();
  }
  return std::make_pair(lower_bound, upper_bound);
}

// https://drafts.csswg.org/css-counter-styles/#cyclic-system
Vector<wtf_size_t> CyclicAlgorithm(int value, wtf_size_t num_symbols) {
  DCHECK(num_symbols);
  value %= static_cast<int>(num_symbols);
  value -= 1;
  if (value < 0)
    value += num_symbols;
  return {value};
}

// https://drafts.csswg.org/css-counter-styles/#fixed-system
Vector<wtf_size_t> FixedAlgorithm(int value,
                                  int first_symbol_value,
                                  wtf_size_t num_symbols) {
  if (value < first_symbol_value ||
      static_cast<unsigned>(value - first_symbol_value) >= num_symbols)
    return Vector<wtf_size_t>();
  return {value - first_symbol_value};
}

// https://drafts.csswg.org/css-counter-styles/#symbolic-system
Vector<wtf_size_t> SymbolicAlgorithm(unsigned value, wtf_size_t num_symbols) {
  DCHECK(num_symbols);
  if (!value)
    return Vector<wtf_size_t>();
  wtf_size_t index = (value - 1) % num_symbols;
  wtf_size_t repetitions = (value + num_symbols - 1) / num_symbols;
  if (repetitions > kCounterLengthLimit)
    return Vector<wtf_size_t>();
  return Vector<wtf_size_t>(repetitions, index);
}

// https://drafts.csswg.org/css-counter-styles/#alphabetic-system
Vector<wtf_size_t> AlphabeticAlgorithm(unsigned value, wtf_size_t num_symbols) {
  DCHECK(num_symbols);
  if (!value)
    return Vector<wtf_size_t>();
  Vector<wtf_size_t> result;
  while (value) {
    value -= 1;
    result.push_back(value % num_symbols);
    value /= num_symbols;

    // Since length is logarithmic to value, we won't exceed the length limit.
    DCHECK_LE(result.size(), kCounterLengthLimit);
  }
  std::reverse(result.begin(), result.end());
  return result;
}

// https://drafts.csswg.org/css-counter-styles/#numeric-system
Vector<wtf_size_t> NumericAlgorithm(unsigned value, wtf_size_t num_symbols) {
  DCHECK_GT(num_symbols, 1u);
  if (!value)
    return {0};

  Vector<wtf_size_t> result;
  while (value) {
    result.push_back(value % num_symbols);
    value /= num_symbols;

    // Since length is logarithmic to value, we won't exceed the length limit.
    DCHECK_LE(result.size(), kCounterLengthLimit);
  }
  std::reverse(result.begin(), result.end());
  return result;
}

// https://drafts.csswg.org/css-counter-styles/#additive-system
Vector<wtf_size_t> AdditiveAlgorithm(unsigned value,
                                     const Vector<unsigned>& weights) {
  DCHECK(weights.size());
  if (!value) {
    if (weights.back() == 0u)
      return {weights.size() - 1};
    return Vector<wtf_size_t>();
  }

  Vector<wtf_size_t> result;
  for (wtf_size_t index = 0; value && index < weights.size() && weights[index];
       ++index) {
    wtf_size_t repetitions = value / weights[index];
    if (repetitions) {
      if (result.size() + repetitions > kCounterLengthLimit)
        return Vector<wtf_size_t>();
      result.AppendVector(Vector<wtf_size_t>(repetitions, index));
    }
    value %= weights[index];
  }
  if (value)
    return Vector<wtf_size_t>();
  return result;
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
    } else if (system_ == CounterStyleSystem::kFixed && system->IsValuePair()) {
      const auto& second = To<CSSValuePair>(system)->Second();
      first_symbol_value_ = To<CSSPrimitiveValue>(second).GetIntValue();
    }
  }

  if (const CSSValue* fallback = rule.GetFallback())
    fallback_name_ = To<CSSCustomIdentValue>(fallback)->Value();

  if (system_ != CounterStyleSystem::kUnresolvedExtends) {
    if (system_ == CounterStyleSystem::kAdditive) {
      for (const CSSValue* symbol :
           To<CSSValueList>(*rule.GetAdditiveSymbols())) {
        const auto& pair = To<CSSValuePair>(*symbol);
        additive_weights_.push_back(
            To<CSSPrimitiveValue>(pair.First()).GetIntValue());
        symbols_.push_back(SymbolToString(pair.Second()));
      }
    } else {
      for (const CSSValue* symbol : To<CSSValueList>(*rule.GetSymbols()))
        symbols_.push_back(SymbolToString(*symbol));
    }
  }

  if (const CSSValue* negative = rule.GetNegative()) {
    if (const CSSValuePair* pair = DynamicTo<CSSValuePair>(negative)) {
      negative_prefix_ = SymbolToString(pair->First());
      negative_suffix_ = SymbolToString(pair->Second());
    } else {
      negative_prefix_ = SymbolToString(*negative);
    }
  }

  if (const CSSValue* pad = rule.GetPad()) {
    const CSSValuePair& pair = To<CSSValuePair>(*pad);
    pad_length_ = To<CSSPrimitiveValue>(pair.First()).GetIntValue();
    pad_symbol_ = SymbolToString(pair.Second());
  }

  if (const CSSValue* range = rule.GetRange()) {
    if (range->IsIdentifierValue()) {
      DCHECK_EQ(CSSValueID::kAuto, To<CSSIdentifierValue>(range)->GetValueID());
      // Empty |range_| already means 'auto'.
    } else {
      for (const CSSValue* bounds : To<CSSValueList>(*range))
        range_.push_back(BoundsToIntegerPair(To<CSSValuePair>(*bounds)));
    }
  }

  if (const CSSValue* prefix = rule.GetPrefix())
    prefix_ = SymbolToString(*prefix);
  if (const CSSValue* suffix = rule.GetSuffix())
    suffix_ = SymbolToString(*suffix);

  // TODO(crbug.com/687225): Implement 'speak-as'.
}

void CounterStyle::ResolveExtends(CounterStyle& extended) {
  DCHECK_NE(extended.system_, CounterStyleSystem::kUnresolvedExtends);
  extended_style_ = extended;

  system_ = extended.system_;

  if (system_ == CounterStyleSystem::kFixed)
    first_symbol_value_ = extended.first_symbol_value_;

  if (!style_rule_->GetFallback()) {
    fallback_name_ = extended.fallback_name_;
    fallback_style_ = nullptr;
  }

  symbols_ = extended.symbols_;
  if (system_ == CounterStyleSystem::kAdditive)
    additive_weights_ = extended.additive_weights_;

  if (!style_rule_->GetNegative()) {
    negative_prefix_ = extended.negative_prefix_;
    negative_suffix_ = extended.negative_suffix_;
  }

  if (!style_rule_->GetPad()) {
    pad_length_ = extended.pad_length_;
    pad_symbol_ = extended.pad_symbol_;
  }

  if (!style_rule_->GetRange())
    range_ = extended.range_;

  if (!style_rule_->GetPrefix())
    prefix_ = extended.prefix_;
  if (!style_rule_->GetSuffix())
    suffix_ = extended.suffix_;

  // TODO(crbug.com/687225): Implement 'speak-as'.
}

bool CounterStyle::RangeContains(int value) const {
  if (range_.size()) {
    for (const auto& bounds : range_) {
      if (value >= bounds.first && value <= bounds.second)
        return true;
    }
    return false;
  }

  // 'range' value is auto
  switch (system_) {
    case CounterStyleSystem::kCyclic:
    case CounterStyleSystem::kNumeric:
    case CounterStyleSystem::kFixed:
      return true;
    case CounterStyleSystem::kSymbolic:
    case CounterStyleSystem::kAlphabetic:
      return value >= 1;
    case CounterStyleSystem::kAdditive:
      return value >= 0;
    case CounterStyleSystem::kUnresolvedExtends:
      NOTREACHED();
      return false;
  }
}

bool CounterStyle::NeedsNegativeSign(int value) const {
  if (value >= 0)
    return false;
  switch (system_) {
    case CounterStyleSystem::kSymbolic:
    case CounterStyleSystem::kAlphabetic:
    case CounterStyleSystem::kNumeric:
    case CounterStyleSystem::kAdditive:
      return true;
    case CounterStyleSystem::kCyclic:
    case CounterStyleSystem::kFixed:
      return false;
    case CounterStyleSystem::kUnresolvedExtends:
      NOTREACHED();
      return false;
  }
}

String CounterStyle::GenerateFallbackRepresentation(int value) const {
  if (is_in_fallback_) {
    // We are in a fallback cycle. Use decimal instead.
    return GetDecimal().GenerateRepresentation(value);
  }

  base::AutoReset<bool> in_fallback_scope(&is_in_fallback_, true);
  return fallback_style_->GenerateRepresentation(value);
}

String CounterStyle::GenerateRepresentation(int value) const {
  if (pad_length_ > kCounterLengthLimit)
    return GenerateFallbackRepresentation(value);

  String initial_representation = GenerateInitialRepresentation(value);
  if (initial_representation.IsNull())
    return GenerateFallbackRepresentation(value);

  wtf_size_t initial_length = NumGraphemeClusters(initial_representation);
  if (NeedsNegativeSign(value)) {
    initial_length += NumGraphemeClusters(negative_prefix_);
    initial_length += NumGraphemeClusters(negative_suffix_);
  }
  wtf_size_t pad_copies =
      pad_length_ > initial_length ? pad_length_ - initial_length : 0;

  StringBuilder result;
  if (NeedsNegativeSign(value))
    result.Append(negative_prefix_);
  for (wtf_size_t i = 0; i < pad_copies; ++i)
    result.Append(pad_symbol_);
  result.Append(initial_representation);
  if (NeedsNegativeSign(value))
    result.Append(negative_suffix_);
  return result.ToString();
}

String CounterStyle::GenerateInitialRepresentation(int value) const {
  if (!RangeContains(value))
    return String();

  unsigned abs_value = value < 0 ? -value : value;

  Vector<wtf_size_t> symbol_indexes;
  switch (system_) {
    case CounterStyleSystem::kCyclic:
      symbol_indexes = CyclicAlgorithm(value, symbols_.size());
      break;
    case CounterStyleSystem::kFixed:
      symbol_indexes =
          FixedAlgorithm(value, first_symbol_value_, symbols_.size());
      break;
    case CounterStyleSystem::kNumeric:
      symbol_indexes = NumericAlgorithm(abs_value, symbols_.size());
      break;
    case CounterStyleSystem::kSymbolic:
      symbol_indexes = SymbolicAlgorithm(abs_value, symbols_.size());
      break;
    case CounterStyleSystem::kAlphabetic:
      symbol_indexes = AlphabeticAlgorithm(abs_value, symbols_.size());
      break;
    case CounterStyleSystem::kAdditive:
      symbol_indexes = AdditiveAlgorithm(abs_value, additive_weights_);
      break;
    case CounterStyleSystem::kUnresolvedExtends:
      NOTREACHED();
      break;
  }

  if (symbol_indexes.IsEmpty())
    return String();

  StringBuilder result;
  for (wtf_size_t index : symbol_indexes)
    result.Append(symbols_[index]);
  return result.ToString();
}

void CounterStyle::TraverseAndMarkDirtyIfNeeded(
    HeapHashSet<Member<CounterStyle>>& visited_counter_styles) {
  if (IsPredefined() || visited_counter_styles.Contains(this))
    return;
  visited_counter_styles.insert(this);

  if (has_inexistent_references_) {
    SetIsDirty();
    return;
  }

  if (extended_style_) {
    extended_style_->TraverseAndMarkDirtyIfNeeded(visited_counter_styles);
    if (extended_style_->IsDirty()) {
      SetIsDirty();
      return;
    }
  }

  if (fallback_style_) {
    fallback_style_->TraverseAndMarkDirtyIfNeeded(visited_counter_styles);
    if (fallback_style_->IsDirty()) {
      SetIsDirty();
      return;
    }
  }
}

void CounterStyle::Trace(Visitor* visitor) const {
  visitor->Trace(style_rule_);
  visitor->Trace(extended_style_);
  visitor->Trace(fallback_style_);
}

}  // namespace blink
