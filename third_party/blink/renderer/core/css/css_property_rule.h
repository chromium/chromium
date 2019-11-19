// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_RULE_H_

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSStyleDeclaration;
class StyleRuleProperty;
class StyleRuleCSSStyleDeclaration;

class CSSPropertyRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSPropertyRule(StyleRuleProperty*, CSSStyleSheet*);
  ~CSSPropertyRule() override;

  String cssText() const override;
  void Reattach(StyleRuleBase*) override;

  CSSStyleDeclaration* style() const;

  void Trace(blink::Visitor*) override;

 private:
  CSSRule::Type type() const override { return kPropertyRule; }

  Member<StyleRuleProperty> property_rule_;
  mutable Member<StyleRuleCSSStyleDeclaration> properties_cssom_wrapper_;
};

template <>
struct DowncastTraits<CSSPropertyRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.type() == CSSRule::kPropertyRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PROPERTY_RULE_H_
