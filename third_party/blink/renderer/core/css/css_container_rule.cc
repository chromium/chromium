// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSContainerRule::CSSContainerRule(StyleRuleContainer* container_rule,
                                   CSSStyleSheet* parent)
    : CSSConditionRule(container_rule, parent) {}

CSSContainerRule::~CSSContainerRule() = default;

String CSSContainerRule::cssText() const {
  StringBuilder result;
  result.Append("@container");

  String name = ContainerQuery().Selector().Name();
  if (!name.empty()) {
    result.Append(' ');
    SerializeIdentifier(name, result);
  }
  result.Append(' ');
  result.Append(ContainerQuery().ToString());
  AppendCSSTextForItems(result);
  return result.ReleaseString();
}

const AtomicString& CSSContainerRule::Name() const {
  return ContainerQuery().Selector().Name();
}

const ContainerSelector& CSSContainerRule::Selector() const {
  return ContainerQuery().Selector();
}

void CSSContainerRule::SetConditionText(
    const ExecutionContext* execution_context,
    String value) {
  CSSStyleSheet::RuleMutationScope mutation_scope(this);
  To<StyleRuleContainer>(group_rule_.Get())
      ->SetConditionText(execution_context, value);
}

const ContainerQuery& CSSContainerRule::ContainerQuery() const {
  return To<StyleRuleContainer>(group_rule_.Get())->GetContainerQuery();
}

}  // namespace blink
