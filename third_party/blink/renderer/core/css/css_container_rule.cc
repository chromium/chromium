// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_container_rule.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_css_container_condition.h"
#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/media_query_exp.h"
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
  result.Append(' ');
  result.Append(ContainerQuery().ToString());
  AppendCSSTextForItems(result);
  return result.ReleaseString();
}

const ContainerSelector& CSSContainerRule::SelectorForInspector() const {
  return ContainerQuery().Selector();
}

void CSSContainerRule::SetConditionText(
    const ExecutionContext* execution_context,
    String value) {
  StyleSheetContents* parent_contents =
      parentStyleSheet() ? parentStyleSheet()->Contents() : nullptr;
  CSSStyleSheet::RuleMutationScope mutation_scope(this);
  To<StyleRuleContainer>(group_rule_.Get())
      ->SetConditionText(execution_context, parent_contents, value);
  conditions_ = nullptr;
}

String CSSContainerRule::containerName() const {
  StringBuilder result;
  String name = ContainerQuery().Selector().Name();
  if (!name.empty()) {
    SerializeIdentifier(name, result);
  }
  return result.ReleaseString();
}

String CSSContainerRule::containerQuery() const {
  if (const ConditionalExpNode* query = ContainerQuery().Query()) {
    return query->Serialize();
  }
  return String();
}

const ContainerQuery& CSSContainerRule::ContainerQuery() const {
  return To<StyleRuleContainer>(group_rule_.Get())->GetContainerQuery();
}

const FrozenArray<CSSContainerCondition>& CSSContainerRule::conditions() {
  if (!conditions_) {
    FrozenArray<CSSContainerCondition>::VectorType condition_array;
    CSSContainerCondition* condition = CSSContainerCondition::Create();
    condition->setName(ContainerQuery().Selector().Name());
    if (const ConditionalExpNode* query = ContainerQuery().Query()) {
      condition->setQuery(query->Serialize());
    } else {
      condition->setQuery(g_empty_string);
    }
    condition_array.push_back(condition);
    conditions_ = MakeGarbageCollected<FrozenArray<CSSContainerCondition>>(
        std::move(condition_array));
  }
  return *conditions_;
}

void CSSContainerRule::Trace(Visitor* visitor) const {
  CSSConditionRule::Trace(visitor);
  visitor->Trace(conditions_);
}

}  // namespace blink
