// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MIXIN_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MIXIN_RULE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// CSSOM wrapper for a @mixin rule.
class CORE_EXPORT CSSMixinRule final : public CSSGroupingRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSMixinRule(StyleRuleMixin*, CSSStyleSheet* parent);
  String name() const;
  String cssText() const override;

 private:
  CSSRule::Type GetType() const override { return kMixinRule; }
  StyleRuleMixin& MixinRule() const { return To<StyleRuleMixin>(*group_rule_); }
};

template <>
struct DowncastTraits<CSSMixinRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kMixinRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_MIXIN_RULE_H_
