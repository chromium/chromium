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
    absl::optional<mojom::blink::SpeculationTargetHint> target_hint,
    absl::optional<network::mojom::ReferrerPolicy> referrer_policy,
    absl::optional<mojom::blink::SpeculationEagerness> eagerness)
    : urls_(std::move(urls)),
      predicate_(predicate),
      requires_anonymous_client_ip_(requires_anonymous_client_ip),
      target_browsing_context_name_hint_(target_hint),
      referrer_policy_(referrer_policy),
      eagerness_(eagerness) {}

SpeculationRule::~SpeculationRule() = default;

void SpeculationRule::Trace(Visitor* visitor) const {
  visitor->Trace(predicate_);
}

}  // namespace blink
