// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_location_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/style_rule_location.h"

namespace blink {

CSSLocationRule::CSSLocationRule(StyleRuleLocation* location_rule,
                                 CSSStyleSheet* parent)
    : CSSRule(parent), location_rule_(location_rule) {}

CSSLocationRule::~CSSLocationRule() = default;

String CSSLocationRule::cssText() const {
  StringBuilder result;
  result.Append("@location ");
  SerializeIdentifier(location_rule_->GetName(), result);
  // TODO(crbug.com/436805487): Serialize descriptors. There are also
  // alternative spec proposals here, so better wait....
  result.Append(" {\n}");
  return result.ToString();
}

void CSSLocationRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  location_rule_ = To<StyleRuleLocation>(rule);
}

void CSSLocationRule::Trace(Visitor* visitor) const {
  visitor->Trace(location_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
