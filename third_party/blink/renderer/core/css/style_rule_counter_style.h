// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_COUNTER_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_COUNTER_STYLE_H_

#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

class CORE_EXPORT StyleRuleCounterStyle : public StyleRuleBase {
 public:
  StyleRuleCounterStyle(const AtomicString&, CSSPropertyValueSet*);
  StyleRuleCounterStyle(const StyleRuleCounterStyle&);
  ~StyleRuleCounterStyle();

  AtomicString GetName() const { return name_; }
  const CSSValue* GetSystem() const { return system_; }
  const CSSValue* GetNegative() const { return negative_; }
  const CSSValue* GetPrefix() const { return prefix_; }
  const CSSValue* GetSuffix() const { return suffix_; }
  const CSSValue* GetRange() const { return range_; }
  const CSSValue* GetPad() const { return pad_; }
  const CSSValue* GetFallback() const { return fallback_; }
  const CSSValue* GetSymbols() const { return symbols_; }
  const CSSValue* GetAdditiveSymbols() const { return additive_symbols_; }
  const CSSValue* GetSpeakAs() const { return speak_as_; }

  void SetName(const AtomicString& name) { name_ = name; }
  void SetSystem(const CSSValue* system) { system_ = system; }
  void SetNegative(const CSSValue* negative) { negative_ = negative; }
  void SetPrefix(const CSSValue* prefix) { prefix_ = prefix; }
  void SetSuffix(const CSSValue* suffix) { suffix_ = suffix; }
  void SetRange(const CSSValue* range) { range_ = range; }
  void SetPad(const CSSValue* pad) { pad_ = pad; }
  void SetFallback(const CSSValue* fallback) { fallback_ = fallback; }
  void SetSymbols(const CSSValue* symbols) { symbols_ = symbols; }
  void SetAdditiveSymbols(const CSSValue* additive_symbols) {
    additive_symbols_ = additive_symbols;
  }
  void SetSpeakAs(const CSSValue* speak_as) { speak_as_ = speak_as; }

  bool HasFailedOrCanceledSubresources() const {
    // TODO(crbug.com/687225): Implement.
    return false;
  }

  StyleRuleCounterStyle* Copy() const {
    return MakeGarbageCollected<StyleRuleCounterStyle>(*this);
  }

  void TraceAfterDispatch(blink::Visitor*) const;

 private:
  AtomicString name_;
  Member<const CSSValue> system_;
  Member<const CSSValue> negative_;
  Member<const CSSValue> prefix_;
  Member<const CSSValue> suffix_;
  Member<const CSSValue> range_;
  Member<const CSSValue> pad_;
  Member<const CSSValue> fallback_;
  Member<const CSSValue> symbols_;
  Member<const CSSValue> additive_symbols_;
  Member<const CSSValue> speak_as_;
};

template <>
struct DowncastTraits<StyleRuleCounterStyle> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsCounterStyleRule();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_COUNTER_STYLE_H_
