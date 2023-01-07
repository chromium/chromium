// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_condition_rule.h"

#include "third_party/blink/renderer/core/css/css_style_sheet.h"

namespace blink {

CSSConditionRule::CSSConditionRule(StyleRuleCondition* condition_rule,
                                   CSSStyleSheet* parent)
    : CSSGroupingRule(condition_rule, parent) {}

CSSConditionRule::~CSSConditionRule() = default;

String CSSConditionRule::conditionText() const {
  return ConditionTextInternal();
}

String CSSConditionRule::ConditionTextInternal() const {
  return static_cast<StyleRuleCondition*>(group_rule_.Get())->ConditionText();
}

}  // namespace blink
