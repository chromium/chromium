// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_scroll_timeline_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSScrollTimelineRule::CSSScrollTimelineRule(
    StyleRuleScrollTimeline* scroll_timeline_rule,
    CSSStyleSheet* sheet)
    : CSSRule(sheet), scroll_timeline_rule_(scroll_timeline_rule) {}

CSSScrollTimelineRule::~CSSScrollTimelineRule() = default;

String CSSScrollTimelineRule::cssText() const {
  StringBuilder builder;
  builder.Append("@scroll-timeline ");
  SerializeIdentifier(name(), builder);
  builder.Append(" { ");
  if (const CSSValue* source = scroll_timeline_rule_->GetSource()) {
    builder.Append("source: ");
    builder.Append(source->CssText());
    builder.Append("; ");
  }
  if (const CSSValue* orientation = scroll_timeline_rule_->GetOrientation()) {
    builder.Append("orientation: ");
    builder.Append(orientation->CssText());
    builder.Append("; ");
  }
  if (const CSSValue* start = scroll_timeline_rule_->GetStart()) {
    builder.Append("start: ");
    builder.Append(start->CssText());
    builder.Append("; ");
  }
  if (const CSSValue* end = scroll_timeline_rule_->GetEnd()) {
    builder.Append("end: ");
    builder.Append(end->CssText());
    builder.Append("; ");
  }
  if (const CSSValue* time_range = scroll_timeline_rule_->GetTimeRange()) {
    builder.Append("time-range: ");
    builder.Append(time_range->CssText());
    builder.Append("; ");
  }
  builder.Append("}");
  return builder.ToString();
}

void CSSScrollTimelineRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  scroll_timeline_rule_ = To<StyleRuleScrollTimeline>(rule);
}

String CSSScrollTimelineRule::name() const {
  return scroll_timeline_rule_->GetName();
}

String CSSScrollTimelineRule::source() const {
  if (const CSSValue* source = scroll_timeline_rule_->GetSource())
    return source->CssText();
  return "none";
}

String CSSScrollTimelineRule::orientation() const {
  if (const CSSValue* orientation = scroll_timeline_rule_->GetOrientation())
    return orientation->CssText();
  return "auto";
}

String CSSScrollTimelineRule::start() const {
  if (const CSSValue* start = scroll_timeline_rule_->GetStart())
    return start->CssText();
  return "auto";
}

String CSSScrollTimelineRule::end() const {
  if (const CSSValue* end = scroll_timeline_rule_->GetEnd())
    return end->CssText();
  return "auto";
}

String CSSScrollTimelineRule::timeRange() const {
  if (const CSSValue* range = scroll_timeline_rule_->GetTimeRange())
    return range->CssText();
  return "auto";
}

void CSSScrollTimelineRule::Trace(Visitor* visitor) const {
  visitor->Trace(scroll_timeline_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
