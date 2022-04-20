// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_scope_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSScopeRule::CSSScopeRule(StyleRuleScope* scope_rule, CSSStyleSheet* parent)
    : CSSGroupingRule(scope_rule, parent) {}

CSSScopeRule::~CSSScopeRule() = default;

String CSSScopeRule::cssText() const {
  StringBuilder result;
  result.Append("@scope ");

  const StyleScope& scope =
      To<StyleRuleScope>(*group_rule_.Get()).GetStyleScope();

  result.Append('(');
  result.Append(scope.From().SelectorsText());
  result.Append(')');

  if (scope.To()) {
    result.Append(" to (");
    result.Append(scope.To()->SelectorsText());
    result.Append(')');
  }

  result.Append(' ');
  result.Append("{\n");
  AppendCSSTextForItems(result);
  result.Append('}');

  return result.ReleaseString();
}

}  // namespace blink
