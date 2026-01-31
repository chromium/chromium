// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_route_rule.h"

#include "third_party/blink/renderer/core/css/style_rule_route.h"

namespace blink {

CSSRouteRule::CSSRouteRule(StyleRuleRoute* route_rule, CSSStyleSheet* parent)
    : CSSRule(parent), route_rule_(route_rule) {}

CSSRouteRule::~CSSRouteRule() = default;

String CSSRouteRule::cssText() const {
  StringBuilder result;
  result.Append("@route ");
  NOTREACHED() << "Not implemented.";
}

void CSSRouteRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  route_rule_ = To<StyleRuleRoute>(rule);
}

void CSSRouteRule::Trace(Visitor* visitor) const {
  visitor->Trace(route_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
