// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_starting_style_rule.h"

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSStartingStyleRule::CSSStartingStyleRule(
    StyleRuleStartingStyle* starting_style_rule,
    CSSStyleSheet* parent)
    : CSSGroupingRule(starting_style_rule, parent) {}

String CSSStartingStyleRule::cssText() const {
  StringBuilder result;

  result.Append("@starting-style");
  AppendCSSTextForItems(result);

  return result.ReleaseString();
}

}  // namespace blink
