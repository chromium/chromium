// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_function_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSFunctionRule::CSSFunctionRule(StyleRuleFunction* function_rule,
                                 CSSStyleSheet* parent)
    : CSSGroupingRule(function_rule, parent) {}

String CSSFunctionRule::name() const {
  return To<StyleRuleFunction>(*group_rule_).Name();
}

HeapVector<Member<FunctionParameter>> CSSFunctionRule::getParameters() const {
  HeapVector<Member<FunctionParameter>> out;

  for (const StyleRuleFunction::Parameter& param :
       To<StyleRuleFunction>(*group_rule_).GetParameters()) {
    auto* p = FunctionParameter::Create();
    p->setName(param.name);
    p->setType(param.type.ToString());
    if (param.default_value) {
      p->setDefaultValue(param.default_value->Serialize());
    }
    out.push_back(p);
  }

  return out;
}

String CSSFunctionRule::returnType() const {
  return To<StyleRuleFunction>(*group_rule_).GetReturnType().ToString();
}

// <css-type> = <syntax-component> | <type()>
// https://drafts.csswg.org/css-mixins-1/#typedef-css-type
void AppendCSSType(const CSSSyntaxDefinition& syntax, StringBuilder& builder) {
  CHECK(!syntax.IsUniversal());
  bool wrap_in_type = syntax.Components().size() != 1u;
  if (wrap_in_type) {
    builder.Append("type(");
  }
  builder.Append(syntax.ToString());
  if (wrap_in_type) {
    builder.Append(")");
  }
}

String CSSFunctionRule::cssText() const {
  const auto& rule = To<StyleRuleFunction>(*group_rule_);

  StringBuilder builder;
  builder.Append("@function ");
  SerializeIdentifier(rule.Name(), builder);
  builder.Append("(");

  bool first_param = true;
  for (const StyleRuleFunction::Parameter& param : rule.GetParameters()) {
    if (!first_param) {
      builder.Append(", ");
    }
    SerializeIdentifier(param.name, builder);
    if (!param.type.IsUniversal()) {
      builder.Append(" ");
      AppendCSSType(param.type, builder);
    }
    if (param.default_value) {
      builder.Append(": ");
      builder.Append(param.default_value->Serialize());
    }
    first_param = false;
  }

  builder.Append(")");

  if (!rule.GetReturnType().IsUniversal()) {
    builder.Append(" returns ");
    AppendCSSType(rule.GetReturnType(), builder);
  }

  builder.Append(" {");

  for (unsigned i = 0; i < length(); ++i) {
    CSSRule* child = ItemInternal(i);
    String child_text = child->cssText();
    if (!child_text.empty()) {
      builder.Append(" ");
      builder.Append(child_text);
    }
  }

  builder.Append(" }");

  return builder.ReleaseString();
}

}  // namespace blink
