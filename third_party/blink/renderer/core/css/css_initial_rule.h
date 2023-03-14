// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_INITIAL_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_INITIAL_RULE_H_

#include "third_party/blink/renderer/core/css/css_condition_rule.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class StyleRuleInitial;

class CSSInitialRule final : public CSSConditionRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSInitialRule(StyleRuleInitial*, CSSStyleSheet*);
  ~CSSInitialRule() override = default;

  String cssText() const override;

 private:
  CSSRule::Type GetType() const override { return kInitialRule; }
};

template <>
struct DowncastTraits<CSSInitialRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kInitialRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_INITIAL_RULE_H_
