// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRY_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRY_RULE_H_

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

class StyleRuleCSSStyleDeclaration;

class StyleRuleTry final : public StyleRuleBase {
 public:
  explicit StyleRuleTry(CSSPropertyValueSet*);
  StyleRuleTry(const StyleRuleTry&) = default;
  ~StyleRuleTry();

  StyleRuleTry* Copy() const {
    return MakeGarbageCollected<StyleRuleTry>(*this);
  }

  const CSSPropertyValueSet& Properties() const { return *properties_; }
  MutableCSSPropertyValueSet& MutableProperties();

  void TraceAfterDispatch(Visitor*) const;

 private:
  Member<CSSPropertyValueSet> properties_;
};

template <>
struct DowncastTraits<StyleRuleTry> {
  static bool AllowFrom(const StyleRuleBase& rule) { return rule.IsTryRule(); }
};

class CSSTryRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit CSSTryRule(StyleRuleTry*);
  ~CSSTryRule() final;

  CSSStyleDeclaration* style() const;
  Type GetType() const final { return kTryRule; }

  String cssText() const final;
  void Reattach(StyleRuleBase*) final;

  void Trace(Visitor*) const final;

 private:
  Member<StyleRuleTry> try_rule_;
  mutable Member<StyleRuleCSSStyleDeclaration> properties_cssom_wrapper_;
};

template <>
struct DowncastTraits<CSSTryRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kTryRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRY_RULE_H_
