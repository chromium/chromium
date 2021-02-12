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

  int GetVersion() const { return version_; }

  // Different 'system' values have different requirements on 'symbols' and
  // 'additive-symbols'. Returns true if the requirement is met.
  // https://drafts.csswg.org/css-counter-styles-3/#counter-style-symbols
  bool HasValidSymbols() const;

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

  void SetName(const AtomicString& name) {
    name_ = name;
    ++version_;
  }

  // Returns false if the setter fails due to invalid new value.
  bool SetSystem(const CSSValue* system);
  bool SetNegative(const CSSValue* negative) {
    negative_ = negative;
    ++version_;
    return true;
  }
  bool SetPrefix(const CSSValue* prefix) {
    prefix_ = prefix;
    ++version_;
    return true;
  }
  bool SetSuffix(const CSSValue* suffix) {
    suffix_ = suffix;
    ++version_;
    return true;
  }
  bool SetRange(const CSSValue* range) {
    range_ = range;
    ++version_;
    return true;
  }
  bool SetPad(const CSSValue* pad) {
    pad_ = pad;
    ++version_;
    return true;
  }
  bool SetFallback(const CSSValue* fallback) {
    fallback_ = fallback;
    ++version_;
    return true;
  }
  bool SetSymbols(const CSSValue* symbols);
  bool SetAdditiveSymbols(const CSSValue* additive_symbols);
  bool SetSpeakAs(const CSSValue* speak_as) {
    speak_as_ = speak_as;
    ++version_;
    return true;
  }

  bool HasFailedOrCanceledSubresources() const {
    // TODO(crbug.com/1176323): Handle image symbols when we implement it.
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

  // Tracks mutations due to setter functions.
  int version_ = 0;
};

template <>
struct DowncastTraits<StyleRuleCounterStyle> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsCounterStyleRule();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_COUNTER_STYLE_H_
