// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COUNTER_STYLE_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COUNTER_STYLE_RULE_H_

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class StyleRuleCounterStyle;

class CSSCounterStyleRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSCounterStyleRule(StyleRuleCounterStyle*, CSSStyleSheet*);
  ~CSSCounterStyleRule() override;

  String cssText() const override;
  void Reattach(StyleRuleBase*) override;

  String name() const;
  String system() const;
  String symbols() const;
  String additiveSymbols() const;
  String negative() const;
  String prefix() const;
  String suffix() const;
  String range() const;
  String pad() const;
  String speakAs() const;
  String fallback() const;

  void setName(const String&);
  void setSystem(const String&);
  void setSymbols(const String&);
  void setAdditiveSymbols(const String&);
  void setNegative(const String&);
  void setPrefix(const String&);
  void setSuffix(const String&);
  void setRange(const String&);
  void setPad(const String&);
  void setSpeakAs(const String&);
  void setFallback(const String&);

  void Trace(Visitor*) const override;

 private:
  CSSRule::Type GetType() const override { return kCounterStyleRule; }

  Member<StyleRuleCounterStyle> counter_style_rule_;
};

template <>
struct DowncastTraits<CSSCounterStyleRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kCounterStyleRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_COUNTER_STYLE_RULE_H_
