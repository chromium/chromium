// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ROUTE_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ROUTE_RULE_H_

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSStyleSheet;
class StyleRuleRoute;

class CORE_EXPORT CSSRouteRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSRouteRule(StyleRuleRoute*, CSSStyleSheet*);
  ~CSSRouteRule() override;

  String cssText() const override;
  void Reattach(StyleRuleBase*) override;

  void Trace(Visitor*) const override;

 private:
  CSSRule::Type GetType() const override { return kRouteRule; }

  Member<StyleRuleRoute> route_rule_;
};

template <>
struct DowncastTraits<CSSRouteRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kRouteRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_ROUTE_RULE_H_
