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
  result.Append(" {");
  AppendDescriptorIfNotEmpty(result, "pattern", location_rule_->GetPattern());
  AppendDescriptorIfNotEmpty(result, "protocol", location_rule_->GetProtocol());
  AppendDescriptorIfNotEmpty(result, "hostname", location_rule_->GetHostname());
  AppendDescriptorIfNotEmpty(result, "port", location_rule_->GetPort());
  AppendDescriptorIfNotEmpty(result, "pathname", location_rule_->GetPathname());
  AppendDescriptorIfNotEmpty(result, "search", location_rule_->GetSearch());
  AppendDescriptorIfNotEmpty(result, "hash", location_rule_->GetHash());
  AppendDescriptorIfNotEmpty(result, "base-url", location_rule_->GetBaseUrl());
  result.Append(" }");
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
