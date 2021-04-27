// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// static
const char DocumentSpeculationRules::kSupplementName[] =
    "DocumentSpeculationRules";

// static
DocumentSpeculationRules& DocumentSpeculationRules::From(Document& document) {
  if (DocumentSpeculationRules* self = FromIfExists(document))
    return *self;

  auto* self = MakeGarbageCollected<DocumentSpeculationRules>(document);
  ProvideTo(document, self);
  return *self;
}

// static
DocumentSpeculationRules* DocumentSpeculationRules::FromIfExists(
    Document& document) {
  return Supplement::From<DocumentSpeculationRules>(document);
}

DocumentSpeculationRules::DocumentSpeculationRules(Document& document)
    : Supplement(document), host_(document.GetExecutionContext()) {}

void DocumentSpeculationRules::AddRuleSet(SpeculationRuleSet* rule_set) {
  rule_sets_.push_back(rule_set);

  if (!has_pending_update_) {
    has_pending_update_ = true;
    auto task_runner =
        GetSupplementable()->GetExecutionContext()->GetTaskRunner(
            TaskType::kIdleTask);
    task_runner->PostTask(
        base::Location::Current(),
        WTF::Bind(&DocumentSpeculationRules::UpdateSpeculationCandidates,
                  WrapWeakPersistent(this)));
  }
}

void DocumentSpeculationRules::Trace(Visitor* visitor) const {
  Supplement::Trace(visitor);
  visitor->Trace(rule_sets_);
  visitor->Trace(host_);
}

mojom::blink::SpeculationHost* DocumentSpeculationRules::GetHost() {
  if (!host_.is_bound()) {
    auto* execution_context = GetSupplementable()->GetExecutionContext();
    if (!execution_context)
      return nullptr;
    execution_context->GetBrowserInterfaceBroker().GetInterface(
        host_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kInternalDefault)));
  }
  return host_.get();
}

void DocumentSpeculationRules::UpdateSpeculationCandidates() {
  has_pending_update_ = false;

  mojom::blink::SpeculationHost* host = GetHost();
  if (!host)
    return;

  Vector<mojom::blink::SpeculationCandidatePtr> candidates;
  auto push_candidates = [&candidates](
                             mojom::blink::SpeculationAction action,
                             const HeapVector<Member<SpeculationRule>>& rules) {
    for (SpeculationRule* rule : rules) {
      mojom::blink::SpeculationCandidate candidate;
      candidate.action = action;
      candidate.requires_anonymous_client_ip_when_cross_origin =
          rule->requires_anonymous_client_ip_when_cross_origin();
      for (const KURL& url : rule->urls()) {
        candidate.url = url;
        candidates.push_back(
            mojom::blink::SpeculationCandidate::New(candidate));
      }
    }
  };

  auto* execution_context = GetSupplementable()->GetExecutionContext();
  for (SpeculationRuleSet* rule_set : rule_sets_) {
    // If kSpeculationRulesPrefetchProxy is enabled, collect all prefetch
    // speculation rules.
    if (RuntimeEnabledFeatures::SpeculationRulesPrefetchProxyEnabled(
            execution_context)) {
      push_candidates(mojom::blink::SpeculationAction::kPrefetch,
                      rule_set->prefetch_rules());
      push_candidates(
          mojom::blink::SpeculationAction::kPrefetchWithSubresources,
          rule_set->prefetch_with_subresources_rules());
    }

    // If kPrerender2 is enabled, collect all prerender speculation rules.
    if (RuntimeEnabledFeatures::Prerender2Enabled(execution_context)) {
      push_candidates(mojom::blink::SpeculationAction::kPrerender,
                      rule_set->prerender_rules());
    }
  }

  host->UpdateSpeculationCandidates(std::move(candidates));
}

}  // namespace blink
