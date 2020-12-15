// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTER_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTER_STYLE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class StyleRuleCounterStyle;

enum class CounterStyleSystem {
  kCyclic,
  kFixed,
  kSymbolic,
  kAlphabetic,
  kNumeric,
  kAdditive,
  kUnresolvedExtends,
};

// Represents a valid counter style defined in a tree scope.
class CORE_EXPORT CounterStyle final : public GarbageCollected<CounterStyle> {
 public:
  static CounterStyle& GetDecimal();

  // Returns nullptr if the @counter-style rule is invalid.
  static CounterStyle* Create(const StyleRuleCounterStyle&);

  AtomicString GetName() const;
  CounterStyleSystem GetSystem() const { return system_; }

  // https://drafts.csswg.org/css-counter-styles/#generate-a-counter
  String GenerateRepresentation(int value) const;

  AtomicString GetExtendsName() const { return extends_name_; }
  const CounterStyle& GetExtendedStyle() const { return *extended_style_; }
  bool HasUnresolvedExtends() const {
    return system_ == CounterStyleSystem::kUnresolvedExtends;
  }
  void ResolveExtends(const CounterStyle& extended);

  AtomicString GetFallbackName() const { return fallback_name_; }
  const CounterStyle& GetFallbackStyle() const { return *fallback_style_; }
  bool HasUnresolvedFallback() const { return !fallback_style_; }
  void ResolveFallback(const CounterStyle& fallback) {
    fallback_style_ = &fallback;
  }

  // Resets the resolution of 'extends' and 'fallback' for recomputing it.
  void ResetExtends();
  void ResetFallback();

  void Trace(Visitor*) const;

  explicit CounterStyle(const StyleRuleCounterStyle& rule);
  ~CounterStyle();

 private:
  // https://drafts.csswg.org/css-counter-styles/#counter-style-range
  bool RangeContains(int value) const;

  // Returns true if a negative sign is needed for the value.
  // https://drafts.csswg.org/css-counter-styles/#counter-style-negative
  bool NeedsNegativeSign(int value) const;

  // https://drafts.csswg.org/css-counter-styles/#initial-representation-for-the-counter-value
  // Returns nullptr if the counter value cannot be represented with the given
  // 'system', 'range' and 'symbols'/'additive-symbols' descriptor values.
  String GenerateInitialRepresentation(int value) const;

  // Uses the fallback counter style to generate a representation for the value.
  // It may recurse, and if it enters a loop, it uses 'decimal' instead.
  String GenerateFallbackRepresentation(int value) const;

  // The corresponding style rule in CSS.
  Member<const StyleRuleCounterStyle> style_rule_;

  // The actual system of the counter style with 'extends' resolved. The value
  // is kUnresolvedExtends temporarily before the resolution.
  CounterStyleSystem system_ = CounterStyleSystem::kSymbolic;

  AtomicString extends_name_;
  Member<const CounterStyle> extended_style_;

  AtomicString fallback_name_ = "decimal";
  Member<const CounterStyle> fallback_style_;

  // True if we are looking for a fallback counter style to generate a counter
  // value. Supports cycle detection in fallback.
  mutable bool is_in_fallback_ = false;

  // Value of 'symbols' for non-additive systems; Or symbol values in
  // 'additive-symbols' for the 'additive' system.
  Vector<String> symbols_;

  // Additive weights, for the 'additive' system only.
  Vector<wtf_size_t> additive_weights_;

  // Value of 'range' descriptor. Empty vector means 'auto'.
  Vector<std::pair<int, int>> range_;

  String negative_prefix_ = "-";
  String negative_suffix_;

  String pad_symbol_;
  wtf_size_t pad_length_ = 0;

  // First symbol value, for 'fixed' system only.
  wtf_size_t first_symbol_value_ = 1;

  friend class CounterStyleMapTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTER_STYLE_H_
