// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rule.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/speculation_rules/document_rule_predicate.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

SpeculationRule::SpeculationRule(
    Vector<KURL> urls,
    DocumentRulePredicate* predicate,
    RequiresAnonymousClientIPWhenCrossOrigin requires_anonymous_client_ip,
    std::optional<mojom::blink::SpeculationTargetHint> target_hint,
    std::optional<network::mojom::ReferrerPolicy> referrer_policy,
    mojom::blink::SpeculationEagerness eagerness,
    network::mojom::blink::NoVarySearchPtr no_vary_search_expected,
    mojom::blink::SpeculationInjectionType injection_type)
    : urls_(std::move(urls)),
      predicate_(predicate),
      requires_anonymous_client_ip_(requires_anonymous_client_ip),
      target_browsing_context_name_hint_(target_hint),
      referrer_policy_(referrer_policy),
      eagerness_(eagerness),
      no_vary_search_expected_(std::move(no_vary_search_expected)),
      injection_type_(injection_type) {}

SpeculationRule::~SpeculationRule() = default;

void SpeculationRule::Trace(Visitor* visitor) const {
  visitor->Trace(predicate_);
}

}  // namespace blink
