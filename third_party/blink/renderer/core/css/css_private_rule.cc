// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_private_rule.h"

#include "third_party/blink/renderer/core/css/css_function_rule.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_private_variable.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSPrivateRule::CSSPrivateRule(StyleRulePrivate* private_rule,
                               CSSStyleSheet* parent)
    : CSSRule(parent), private_rule_(private_rule) {}

String CSSPrivateRule::cssText() const {
  StringBuilder result;
  result.Append("@private { ");
  for (const CSSPrivateVariable* variable :
       private_rule_->GetPrivateVariables()) {
    SerializeIdentifier(variable->Name(), result);
    if (!variable->Syntax().IsUniversal()) {
      result.Append(' ');
      AppendCSSType(variable->Syntax(), result);
    }
    if (variable->DefaultValue()) {
      result.Append(": ");
      result.Append(variable->DefaultValue()->Serialize());
    }
    result.Append("; ");
  }
  result.Append('}');
  return result.ReleaseString();
}

void CSSPrivateRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  private_rule_ = To<StyleRulePrivate>(rule);
}

void CSSPrivateRule::Trace(Visitor* visitor) const {
  visitor->Trace(private_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
