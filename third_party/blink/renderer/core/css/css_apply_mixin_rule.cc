// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_apply_mixin_rule.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CSSApplyMixinRule::CSSApplyMixinRule(StyleRuleApplyMixin* apply_mixin_rule,
                                     CSSStyleSheet* sheet)
    : CSSRule(sheet), apply_mixin_rule_(apply_mixin_rule) {}

String CSSApplyMixinRule::cssText() const {
  StringBuilder result;
  result.Append("@apply ");
  SerializeIdentifier(name(), result);
  if (apply_mixin_rule_->HasContentsBlock()) {
    // Mostly follows CSSGroupingRule::AppendCSSTextForItems().
    result.Append(" {");
    const HeapVector<Member<StyleRuleBase>>& rules =
        apply_mixin_rule_->ChildRules();
    if (child_rule_cssom_wrappers_.size() != rules.size()) {
      child_rule_cssom_wrappers_.resize(rules.size());
    }
    for (unsigned i = 0; i < rules.size(); ++i) {
      if (!child_rule_cssom_wrappers_[i]) {
        child_rule_cssom_wrappers_[i] = rules[i]->CreateCSSOMWrapper(
            i, const_cast<CSSApplyMixinRule*>(this));
      }
      String rule_text = child_rule_cssom_wrappers_[i]->cssText();
      if (!rule_text.empty()) {
        result.Append(" ");
        result.Append(rule_text);
      }
    }
    result.Append(" }");
  } else {
    result.Append(';');
  }
  return result.ReleaseString();
}

void CSSApplyMixinRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  apply_mixin_rule_ = To<StyleRuleApplyMixin>(rule);
}

String CSSApplyMixinRule::name() const {
  return apply_mixin_rule_->GetName();
}

void CSSApplyMixinRule::Trace(Visitor* visitor) const {
  visitor->Trace(apply_mixin_rule_);
  visitor->Trace(child_rule_cssom_wrappers_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
