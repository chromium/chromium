// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_APPLY_MIXIN_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_APPLY_MIXIN_RULE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CSSStyleRule;
class StyleRuleApplyMixin;

// CSSOM wrapper for @apply (activating a mixin defined with @mixin).
class CORE_EXPORT CSSApplyMixinRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSApplyMixinRule(StyleRuleApplyMixin*, CSSStyleSheet* parent);
  String name() const;
  String cssText() const override;
  void Reattach(StyleRuleBase*) override;
  void Trace(Visitor*) const override;

 private:
  CSSRule::Type GetType() const override { return kApplyMixinRule; }
  Member<StyleRuleApplyMixin> apply_mixin_rule_;
  mutable Member<CSSStyleRule> fake_parent_cssom_;
};

template <>
struct DowncastTraits<CSSApplyMixinRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kApplyMixinRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_APPLY_MIXIN_RULE_H_
