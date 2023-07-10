// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_position_fallback_rule.h"

#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

StyleRulePositionFallback::StyleRulePositionFallback(
    const AtomicString& name,
    HeapVector<Member<StyleRuleBase>> rules)
    : StyleRuleGroup(kPositionFallback, std::move(rules)), name_(name) {}

StyleRulePositionFallback::StyleRulePositionFallback(
    const StyleRulePositionFallback&) = default;

StyleRulePositionFallback::~StyleRulePositionFallback() = default;

void StyleRulePositionFallback::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(layer_);
  StyleRuleGroup::TraceAfterDispatch(visitor);
}

CSSPositionFallbackRule::CSSPositionFallbackRule(
    StyleRulePositionFallback* position_fallback_rule,
    CSSStyleSheet* parent)
    : CSSGroupingRule(position_fallback_rule, parent) {}

CSSPositionFallbackRule::~CSSPositionFallbackRule() = default;

String CSSPositionFallbackRule::cssText() const {
  StringBuilder result;
  result.Append("@position-fallback ");
  result.Append(name());
  AppendCSSTextForItems(result);
  return result.ReleaseString();
}

}  // namespace blink
