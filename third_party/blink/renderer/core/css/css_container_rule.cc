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
  GetContainerQuerySet().Serialize(result);
  AppendCSSTextForItems(result);
  return result.ReleaseString();
}

const ContainerSelector& CSSContainerRule::SelectorForInspector() const {
  // TODO(41491726): This only considers a single query and returns the selector
  // for the first query in a comma separated list.
  return GetContainerQuerySet().Queries()[0]->Selector();
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

void CSSContainerRule::SetQueryText(const ExecutionContext* execution_context,
                                    String value) {
  StyleSheetContents* parent_contents =
      parentStyleSheet() ? parentStyleSheet()->Contents() : nullptr;
  CSSStyleSheet::RuleMutationScope mutation_scope(this);
  To<StyleRuleContainer>(group_rule_.Get())
      ->SetQueryText(execution_context, parent_contents, value);
  conditions_ = nullptr;
}

String CSSContainerRule::containerName() const {
  if (const ContainerQuery* query = SingleContainerQuery()) {
    String name = query->Selector().Name();
    if (!name.empty()) {
      StringBuilder result;
      SerializeIdentifier(name, result);
      return result.ReleaseString();
    }
  }
  return String();
}

String CSSContainerRule::containerQuery() const {
  if (const ContainerQuery* query = SingleContainerQuery()) {
    if (const ConditionalExpNode* query_exp = query->Query()) {
      return query_exp->Serialize();
    }
  }
  return String();
}

const ContainerQuerySet& CSSContainerRule::GetContainerQuerySet() const {
  return To<StyleRuleContainer>(group_rule_.Get())->GetContainerQuerySet();
}

const ContainerQuery* CSSContainerRule::SingleContainerQuery() const {
  return GetContainerQuerySet().SingleQuery();
}

const FrozenArray<CSSContainerCondition>& CSSContainerRule::conditions() {
  if (!conditions_) {
    FrozenArray<CSSContainerCondition>::VectorType condition_array;
    for (const ContainerQuery* query : GetContainerQuerySet().Queries()) {
      CSSContainerCondition* condition = CSSContainerCondition::Create();
      if (!query->Selector().Name().empty()) {
        condition->setName(query->Selector().Name());
      } else {
        condition->setName(g_empty_atom);
      }
      if (const ConditionalExpNode* query_exp = query->Query()) {
        condition->setQuery(query_exp->Serialize());
      } else {
        condition->setQuery(g_empty_string);
      }
      condition_array.push_back(condition);
    }
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
