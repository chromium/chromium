// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_apply_mixin_rule.h"

#include "base/check.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
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
  if (apply_mixin_rule_->FakeParentRuleForDeclarations()) {
    // Mostly follows CSSGroupingRule::AppendCSSTextForItems().
    result.Append(" {");
    StyleRule* fake_parent = apply_mixin_rule_->FakeParentRuleForDeclarations();
    if (!fake_parent_cssom_) {
      fake_parent_cssom_ = To<CSSStyleRule>(fake_parent->CreateCSSOMWrapper(
          /*position_hint=*/0,
          /*parent_rule=*/const_cast<CSSApplyMixinRule*>(this)));
    }
    const GCedHeapVector<Member<StyleRuleBase>>* rules =
        fake_parent->ChildRules();
    for (unsigned i = 0; i < rules->size(); ++i) {
      CSSRule* rule = fake_parent_cssom_->ItemInternal(i);
      String rule_text = rule->cssText();
      if (!rule_text.empty()) {
        result.Append(" ");
        result.Append(rule_text);
      }
      result.Append(" }");
    }
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
  visitor->Trace(fake_parent_cssom_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
