// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_mixin_rule.h"

#include "third_party/blink/renderer/core/css/css_function_rule.h"
#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class MediaQuerySetOwner;

CSSMixinRule::CSSMixinRule(StyleRuleMixin* mixin_rule, CSSStyleSheet* parent)
    : CSSGroupingRule(mixin_rule, parent) {}

String CSSMixinRule::name() const {
  return MixinRule().GetName();
}

String CSSMixinRule::cssText() const {
  StringBuilder result;
  result.Append("@mixin ");
  SerializeIdentifier(name(), result);
  result.Append('(');

  bool first_param = true;
  for (const StyleRuleFunction::Parameter& param :
       MixinRule().GetParameters()) {
    if (!first_param) {
      result.Append(", ");
    }
    if (param.name == "@contents") {
      result.Append(param.name);
    } else {
      SerializeIdentifier(param.name, result);
    }
    if (!param.type.IsUniversal()) {
      result.Append(" ");
      AppendCSSType(param.type, result);
    }
    if (param.default_value) {
      result.Append(": ");
      result.Append(param.default_value->Serialize());
    }
    first_param = false;
  }

  result.Append(')');
  AppendCSSTextForItems(result);
  return result.ReleaseString();
}

}  // namespace blink
