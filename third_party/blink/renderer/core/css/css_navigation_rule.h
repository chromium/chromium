// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_NAVIGATION_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_NAVIGATION_RULE_H_

#include "third_party/blink/renderer/core/css/css_condition_rule.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSStyleSheet;
class Document;
class StyleRuleNavigation;

class CORE_EXPORT CSSNavigationRule final : public CSSConditionRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSNavigationRule(StyleRuleNavigation*, CSSStyleSheet*);
  ~CSSNavigationRule() override;

  String cssText() const override;
  // Prefer ConditionTextInternal for internal use.
  String conditionText() const override;
  String ConditionTextInternal() const override;
  void Reattach(StyleRuleBase*) override;

  void Trace(Visitor*) const override;

  bool Evaluate(Document* document);
  void SetConditionText(ExecutionContext* execution_context,
                        const String& text);

 private:
  CSSRule::Type GetType() const override { return kNavigationRule; }

  Member<StyleRuleNavigation> navigation_rule_;
};

template <>
struct DowncastTraits<CSSNavigationRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kNavigationRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_NAVIGATION_RULE_H_
