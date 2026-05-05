// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_COUNTER_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_COUNTER_STYLE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptors.h"
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
  static bool HasValidSymbols(const CSSValue* system,
                              const CSSValue* symbols,
                              const CSSValue* additive_symbols);

  AtomicString GetName() const { return name_; }

  // Do not hold onto this pointer. Requesting this updates the version,
  // causing layout to be invalidated. See
  // CSSCounterStyleRule::MutableStyleForInspector for more details.
  MutableCSSPropertyValueSet& MutableStyleForInspector();

  // For inspector use only. Do not modify.
  MutableCSSPropertyValueSet& Properties();

  const CSSValue* GetSystem() const;
  const CSSValue* GetNegative() const;
  const CSSValue* GetPrefix() const;
  const CSSValue* GetSuffix() const;
  const CSSValue* GetRange() const;
  const CSSValue* GetPad() const;
  const CSSValue* GetFallback() const;
  const CSSValue* GetSymbols() const;
  const CSSValue* GetAdditiveSymbols() const;
  const CSSValue* GetSpeakAs() const;

  // Returns false if the new value is invalid or equivalent to the old value.
  bool NewValueInvalidOrEqual(AtRuleDescriptorID, const CSSValue*);
  void SetDescriptorValue(AtRuleDescriptorID, const CSSValue*);

  void SetName(const AtomicString& name) {
    name_ = name;
    ++version_;
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

  // Tracks mutations due to setter functions.
  int version_ = 0;
  Member<CSSPropertyValueSet> properties_;
};

template <>
struct DowncastTraits<StyleRuleCounterStyle> {
  static bool AllowFrom(const StyleRuleBase& rule) {
    return rule.IsCounterStyleRule();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_RULE_COUNTER_STYLE_H_
