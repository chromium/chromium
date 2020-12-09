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
  // The corresponding style rule in CSS.
  Member<const StyleRuleCounterStyle> style_rule_;

  // The actual system of the counter style with 'extends' resolved. The value
  // is kUnresolvedExtends temporarily before the resolution.
  CounterStyleSystem system_ = CounterStyleSystem::kSymbolic;

  AtomicString extends_name_;
  Member<const CounterStyle> extended_style_;

  AtomicString fallback_name_ = "decimal";
  Member<const CounterStyle> fallback_style_;

  friend class CounterStyleMapTest;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_COUNTER_STYLE_H_
