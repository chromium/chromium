// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SCROLL_TIMELINE_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SCROLL_TIMELINE_RULE_H_

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class StyleRuleScrollTimeline;

class CSSScrollTimelineRule final : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSScrollTimelineRule(StyleRuleScrollTimeline*, CSSStyleSheet*);
  ~CSSScrollTimelineRule() override;

  String cssText() const override;
  void Reattach(StyleRuleBase*) override;

  String name() const;
  String source() const;
  String orientation() const;
  String start() const;
  String end() const;
  String timeRange() const;

  void Trace(Visitor*) const override;

 private:
  CSSRule::Type GetType() const override { return kScrollTimelineRule; }

  Member<StyleRuleScrollTimeline> scroll_timeline_rule_;
};

template <>
struct DowncastTraits<CSSScrollTimelineRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kScrollTimelineRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_SCROLL_TIMELINE_RULE_H_
