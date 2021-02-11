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
#include "third_party/blink/renderer/core/layout/list_marker_text.h"
#include "third_party/blink/renderer/platform/text/text_break_iterator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// User agents must support representations at least 60 Unicode codepoints long,
// but they may choose to instead use the fallback style for representations
// that would be longer than 60 codepoints. Since WTF::String may use UTF-16, we
// limit string length at 120.
const wtf_size_t kCounterLengthLimit = 120;

bool HasSymbols(CounterStyleSystem system) {
  switch (system) {
    case CounterStyleSystem::kCyclic:
    case CounterStyleSystem::kFixed:
    case CounterStyleSystem::kSymbolic:
    case CounterStyleSystem::kAlphabetic:
    case CounterStyleSystem::kNumeric:
    case CounterStyleSystem::kAdditive:
      return true;
    case CounterStyleSystem::kUnresolvedExtends:
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
      return false;
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

// TODO(crbug.com/687225): After @counter-style is shipped and the legacy
// code paths are removed, remove everything else of list_marker_text and move
// the implementation of the special algorithms here.

String HebrewAlgorithm(unsigned value) {
  if (value > 999999)
    return String();
  return list_marker_text::GetText(EListStyleType::kHebrew, value);
}

int AbsoluteValueForLegacyCJKAlgorithms(int value) {
  // @counter-style algorithm works on absolute value, but the legacy
  // implementation works on the original value (and handles negative sign on
  // its own). Clamp to the signed int range before proceeding.
  if (UNLIKELY(value == std::numeric_limits<int>::min()))
    return std::numeric_limits<int>::max();
  else
    return std::abs(value);
}

String SimpChineseInformalAlgorithm(int value) {
  return list_marker_text::GetText(EListStyleType::kSimpChineseInformal,
                                   AbsoluteValueForLegacyCJKAlgorithms(value));
}

String SimpChineseFormalAlgorithm(int value) {
  return list_marker_text::GetText(EListStyleType::kSimpChineseFormal,
                                   AbsoluteValueForLegacyCJKAlgorithms(value));
}

String TradChineseInformalAlgorithm(int value) {
  return list_marker_text::GetText(EListStyleType::kTradChineseInformal,
                                   AbsoluteValueForLegacyCJKAlgorithms(value));
}

String TradChineseFormalAlgorithm(int value) {
  return list_marker_text::GetText(EListStyleType::kTradChineseFormal,
                                   AbsoluteValueForLegacyCJKAlgorithms(value));
}

String KoreanHangulFormalAlgorithm(int value) {
  return list_marker_text::GetText(EListStyleType::kKoreanHangulFormal,
                                   AbsoluteValueForLegacyCJKAlgorithms(value));
}

String KoreanHanjaInformalAlgorithm(int value) {
  return list_marker_text::GetText(EListStyleType::kKoreanHanjaInformal,
                                   AbsoluteValueForLegacyCJKAlgorithms(value));
}

String KoreanHanjaFormalAlgorithm(int value) {
  return list_marker_text::GetText(EListStyleType::kKoreanHanjaFormal,
                                   AbsoluteValueForLegacyCJKAlgorithms(value));
}

String LowerArmenianAlgorithm(unsigned value) {
  if (value > 99999999)
    return String();
  return list_marker_text::GetText(EListStyleType::kLowerArmenian, value);
}

String UpperArmenianAlgorithm(unsigned value) {
  if (value > 99999999)
    return String();
  return list_marker_text::GetText(EListStyleType::kUpperArmenian, value);
}

// https://drafts.csswg.org/css-counter-styles-3/#ethiopic-numeric-counter-style
String EthiopicNumericAlgorithm(unsigned value) {
  // Ethiopic characters for 1-9
  static const UChar units[9] = {0x1369, 0x136A, 0x136B, 0x136C, 0x136D,
                                 0x136E, 0x136F, 0x1370, 0x1371};
  // Ethiopic characters for 10, 20, ..., 90
  static const UChar tens[9] = {0x1372, 0x1373, 0x1374, 0x1375, 0x1376,
                                0x1377, 0x1378, 0x1379, 0x137A};
  if (!value)
    return String();
  if (value < 10u)
    return String(&units[value - 1], 1);

  // Generate characters in the reversed ordering
  Vector<UChar> result;
  for (bool odd_group = false; value; odd_group = !odd_group) {
    unsigned group_value = value % 100;
    value /= 100;
    if (!odd_group) {
      // This adds an extra character for group 0. We'll remove it in the end.
      result.push_back(kEthiopicNumberTenThousandCharacter);
    } else {
      if (group_value)
        result.push_back(kEthiopicNumberHundredCharacter);
    }
    bool most_significant_group = !value;
    bool remove_digits = !group_value ||
                         (group_value == 1 && most_significant_group) ||
                         (group_value == 1 && odd_group);
    if (!remove_digits) {
      if (unsigned unit = group_value % 10)
        result.push_back(units[unit - 1]);
      if (unsigned ten = group_value / 10)
        result.push_back(tens[ten - 1]);
    }
  }

  std::reverse(result.begin(), result.end());
  // Remove the extra character from group 0
  result.pop_back();
  return String(result.data(), result.size());
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

// static
CounterStyleSystem CounterStyle::ToCounterStyleSystemEnum(
    const CSSValue* value) {
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
    case CSSValueID::kInternalHebrew:
      return CounterStyleSystem::kHebrew;
    case CSSValueID::kInternalSimpChineseInformal:
      return CounterStyleSystem::kSimpChineseInformal;
    case CSSValueID::kInternalSimpChineseFormal:
      return CounterStyleSystem::kSimpChineseFormal;
    case CSSValueID::kInternalTradChineseInformal:
      return CounterStyleSystem::kTradChineseInformal;
    case CSSValueID::kInternalTradChineseFormal:
      return CounterStyleSystem::kTradChineseFormal;
    case CSSValueID::kInternalKoreanHangulFormal:
      return CounterStyleSystem::kKoreanHangulFormal;
    case CSSValueID::kInternalKoreanHanjaInformal:
      return CounterStyleSystem::kKoreanHanjaInformal;
    case CSSValueID::kInternalKoreanHanjaFormal:
      return CounterStyleSystem::kKoreanHanjaFormal;
    case CSSValueID::kInternalLowerArmenian:
      return CounterStyleSystem::kLowerArmenian;
    case CSSValueID::kInternalUpperArmenian:
      return CounterStyleSystem::kUpperArmenian;
    case CSSValueID::kInternalEthiopicNumeric:
      return CounterStyleSystem::kEthiopicNumeric;
    case CSSValueID::kExtends:
      return CounterStyleSystem::kUnresolvedExtends;
    default:
      NOTREACHED();
      return CounterStyleSystem::kSymbolic;
  }
}

CounterStyle::~CounterStyle() = default;

AtomicString CounterStyle::GetName() const {
  return style_rule_->GetName();
}

// static
CounterStyle* CounterStyle::Create(const StyleRuleCounterStyle& rule) {
  if (!rule.HasValidSymbols())
    return nullptr;

  return MakeGarbageCollected<CounterStyle>(rule);
}

CounterStyle::CounterStyle(const StyleRuleCounterStyle& rule)
    : style_rule_(rule), style_rule_version_(rule.GetVersion()) {
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

  if (HasSymbols(system_)) {
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
    case CounterStyleSystem::kSimpChineseInformal:
    case CounterStyleSystem::kSimpChineseFormal:
    case CounterStyleSystem::kTradChineseInformal:
    case CounterStyleSystem::kTradChineseFormal:
    case CounterStyleSystem::kKoreanHangulFormal:
    case CounterStyleSystem::kKoreanHanjaInformal:
    case CounterStyleSystem::kKoreanHanjaFormal:
      return true;
    case CounterStyleSystem::kSymbolic:
    case CounterStyleSystem::kAlphabetic:
    case CounterStyleSystem::kEthiopicNumeric:
      return value >= 1;
    case CounterStyleSystem::kAdditive:
      return value >= 0;
    case CounterStyleSystem::kHebrew:
      return value >= 0 && value <= 999999;
    case CounterStyleSystem::kLowerArmenian:
    case CounterStyleSystem::kUpperArmenian:
      return value >= 0 && value <= 99999999;
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
  DCHECK(!IsDirty());

  if (pad_length_ > kCounterLengthLimit)
    return GenerateFallbackRepresentation(value);

  String initial_representation = GenerateInitialRepresentation(value);
  if (initial_representation.IsNull())
    return GenerateFallbackRepresentation(value);

  wtf_size_t initial_length = NumGraphemeClusters(initial_representation);

  // TODO(crbug.com/687225): Spec requires us to further increment
  // |initial_length| by the length of the negative sign, but no current
  // implementation is doing that. For backward compatibility, we don't do that
  // for now. See https://github.com/w3c/csswg-drafts/issues/5906 for details.
  //
  // if (NeedsNegativeSign(value)) {
  //  initial_length += NumGraphemeClusters(negative_prefix_);
  //  initial_length += NumGraphemeClusters(negative_suffix_);
  // }

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

  unsigned abs_value =
      value == std::numeric_limits<int>::min()
          ? static_cast<unsigned>(std::numeric_limits<int>::max()) + 1u
          : std::abs(value);

  switch (system_) {
    case CounterStyleSystem::kCyclic:
      return IndexesToString(CyclicAlgorithm(value, symbols_.size()));
    case CounterStyleSystem::kFixed:
      return IndexesToString(
          FixedAlgorithm(value, first_symbol_value_, symbols_.size()));
    case CounterStyleSystem::kNumeric:
      return IndexesToString(NumericAlgorithm(abs_value, symbols_.size()));
    case CounterStyleSystem::kSymbolic:
      return IndexesToString(SymbolicAlgorithm(abs_value, symbols_.size()));
    case CounterStyleSystem::kAlphabetic:
      return IndexesToString(AlphabeticAlgorithm(abs_value, symbols_.size()));
    case CounterStyleSystem::kAdditive:
      return IndexesToString(AdditiveAlgorithm(abs_value, additive_weights_));
    case CounterStyleSystem::kHebrew:
      return HebrewAlgorithm(abs_value);
    case CounterStyleSystem::kSimpChineseInformal:
      return SimpChineseInformalAlgorithm(value);
    case CounterStyleSystem::kSimpChineseFormal:
      return SimpChineseFormalAlgorithm(value);
    case CounterStyleSystem::kTradChineseInformal:
      return TradChineseInformalAlgorithm(value);
    case CounterStyleSystem::kTradChineseFormal:
      return TradChineseFormalAlgorithm(value);
    case CounterStyleSystem::kKoreanHangulFormal:
      return KoreanHangulFormalAlgorithm(value);
    case CounterStyleSystem::kKoreanHanjaInformal:
      return KoreanHanjaInformalAlgorithm(value);
    case CounterStyleSystem::kKoreanHanjaFormal:
      return KoreanHanjaFormalAlgorithm(value);
    case CounterStyleSystem::kLowerArmenian:
      return LowerArmenianAlgorithm(abs_value);
    case CounterStyleSystem::kUpperArmenian:
      return UpperArmenianAlgorithm(abs_value);
    case CounterStyleSystem::kEthiopicNumeric:
      return EthiopicNumericAlgorithm(abs_value);
    case CounterStyleSystem::kUnresolvedExtends:
      NOTREACHED();
      return String();
  }
}

String CounterStyle::IndexesToString(
    const Vector<wtf_size_t>& symbol_indexes) const {
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

  if (has_inexistent_references_ ||
      style_rule_version_ != style_rule_->GetVersion()) {
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
