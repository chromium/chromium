// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_result_rule.h"

#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MediaQuerySetOwner;

CSSResultRule::CSSResultRule(StyleRuleResult* mixin_rule, CSSStyleSheet* parent)
    : CSSGroupingRule(mixin_rule, parent) {}

String CSSResultRule::cssText() const {
  StringBuilder result;
  result.Append("@result");
  AppendCSSTextForItems(result);
  return result.ReleaseString();
}

}  // namespace blink
