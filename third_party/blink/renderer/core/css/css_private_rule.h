// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_RULE_H_

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class StyleRulePrivate;

class CSSPrivateRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSPrivateRule(StyleRulePrivate*, CSSStyleSheet* parent);

  String cssText() const override;
  void Reattach(StyleRuleBase*) override;

  void Trace(Visitor*) const override;

 private:
  CSSRule::Type GetType() const override { return kPrivateRule; }

  Member<StyleRulePrivate> private_rule_;
};

template <>
struct DowncastTraits<CSSPrivateRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kPrivateRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_PRIVATE_RULE_H_
