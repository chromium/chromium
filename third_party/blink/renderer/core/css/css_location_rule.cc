// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_location_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_url_pattern_value.h"
#include "third_party/blink/renderer/core/css/style_rule_location.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSLocationRule::CSSLocationRule(StyleRuleLocation* location_rule,
                                 CSSStyleSheet* parent)
    : CSSRule(parent), location_rule_(location_rule) {}

CSSLocationRule::~CSSLocationRule() = default;

String CSSLocationRule::cssText() const {
  StringBuilder result;
  result.Append("@location ");
  SerializeIdentifier(location_rule_->GetName(), result);
  result.Append(" { ");
  if (const CSSURLPatternValue* pattern = location_rule_->GetPattern()) {
    result.Append("pattern: ");
    result.Append(pattern->CssText());
    result.Append("; ");
  }
  if (const CSSStringValue* protocol = location_rule_->GetProtocol()) {
    result.Append("protocol: ");
    result.Append(protocol->CssText());
    result.Append("; ");
  }
  if (const CSSStringValue* hostname = location_rule_->GetHostname()) {
    result.Append("hostname: ");
    result.Append(hostname->CssText());
    result.Append("; ");
  }
  if (const CSSStringValue* port = location_rule_->GetPort()) {
    result.Append("port: ");
    result.Append(port->CssText());
    result.Append("; ");
  }
  if (const CSSStringValue* pathname = location_rule_->GetPathname()) {
    result.Append("pathname: ");
    result.Append(pathname->CssText());
    result.Append("; ");
  }
  if (const CSSStringValue* search = location_rule_->GetSearch()) {
    result.Append("search: ");
    result.Append(search->CssText());
    result.Append("; ");
  }
  if (const CSSStringValue* hash = location_rule_->GetHash()) {
    result.Append("hash: ");
    result.Append(hash->CssText());
    result.Append("; ");
  }
  if (const CSSStringValue* base_url = location_rule_->GetBaseUrl()) {
    result.Append("base-url: ");
    result.Append(base_url->CssText());
    result.Append("; ");
  }
  result.Append('}');
  return result.ReleaseString();
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
