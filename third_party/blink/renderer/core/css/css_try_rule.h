// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRY_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRY_RULE_H_

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

class CSSPositionFallbackRule;

class StyleRuleTry final : public StyleRuleBase {
 public:
  explicit StyleRuleTry(CSSPropertyValueSet*);
  ~StyleRuleTry();

  const CSSPropertyValueSet& Properties() const { return *properties_; }

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
  CSSTryRule(StyleRuleTry*, CSSPositionFallbackRule* parent);
  ~CSSTryRule() final;

  Type GetType() const final { return kTryRule; }

  String cssText() const final;
  void Reattach(StyleRuleBase*) final;

  void Trace(Visitor*) const final;

 private:
  Member<StyleRuleTry> try_rule_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_TRY_RULE_H_
