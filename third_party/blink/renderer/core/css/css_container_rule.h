// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_RULE_H_

#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
#include "third_party/blink/renderer/core/css/css_condition_rule.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

class CSSContainerCondition;
class ContainerQuery;
class ContainerQuerySet;
class ContainerSelector;
class StyleRuleContainer;

class CORE_EXPORT CSSContainerRule final : public CSSConditionRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  CSSContainerRule(StyleRuleContainer*, CSSStyleSheet*);
  ~CSSContainerRule() override;

  String cssText() const override;
  String containerName() const;
  String containerQuery() const;
  const FrozenArray<CSSContainerCondition>& conditions();

  // TODO(crbug.com/41491726): Returns a single container selector used by
  // devtools to look up container candidates.
  const ContainerSelector& SelectorForInspector() const;
  void SetQueryText(const ExecutionContext*, String);
  void SetConditionText(const ExecutionContext*, String);

  void Trace(Visitor*) const override;

 private:
  CSSRule::Type GetType() const override { return kContainerRule; }
  const ContainerQuerySet& GetContainerQuerySet() const;
  const ContainerQuery* SingleContainerQuery() const;

  Member<FrozenArray<CSSContainerCondition>> conditions_;
};

template <>
struct DowncastTraits<CSSContainerRule> {
  static bool AllowFrom(const CSSRule& rule) {
    return rule.GetType() == CSSRule::kContainerRule;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_CONTAINER_RULE_H_
