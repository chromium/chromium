// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VIEW_TRANSITIONS_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VIEW_TRANSITIONS_RULE_H_

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class ExecutionContext;
class StyleRuleViewTransitions;

class CSSViewTransitionsRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSViewTransitionsRule(StyleRuleViewTransitions*, CSSStyleSheet*);
  ~CSSViewTransitionsRule() override = default;

  String cssText() const override;
  void Reattach(StyleRuleBase*) override;

  void Trace(Visitor*) const override;

  String navigationTrigger() const;

  void setNavigationTrigger(const ExecutionContext*, const String&);

 private:
  CSSRule::Type GetType() const override { return kViewTransitionsRule; }

  Member<StyleRuleViewTransitions> view_transitions_rule_;
};

template <>
struct DowncastTraits<CSSViewTransitionsRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kViewTransitionsRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_VIEW_TRANSITIONS_RULE_H_
