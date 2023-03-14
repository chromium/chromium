// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_initial_rule.h"

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSInitialRule::CSSInitialRule(StyleRuleInitial* initial_rule,
                               CSSStyleSheet* parent)
    : CSSConditionRule(initial_rule, parent) {}

String CSSInitialRule::cssText() const {
  StringBuilder result;

  result.Append("@initial ");
  AppendCSSTextForItems(result);

  return result.ReleaseString();
}

}  // namespace blink
