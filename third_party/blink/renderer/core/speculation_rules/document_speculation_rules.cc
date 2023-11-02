// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/speculation_rule_loader.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// https://wicg.github.io/nav-speculation/prefetch.html#list-of-sufficiently-strict-speculative-navigation-referrer-policies
bool AcceptableReferrerPolicy(mojom::blink::SpeculationAction action,
                              const Referrer& referrer) {
  // TODO(1355146): This restriction should also apply to prerenders.
  if (action == mojom::blink::SpeculationAction::kPrerender)
    return true;

  switch (referrer.referrer_policy) {
    case network::mojom::ReferrerPolicy::kAlways:
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
    case network::mojom::ReferrerPolicy::kOrigin:
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return false;

    case network::mojom::ReferrerPolicy::kNever:
    case network::mojom::ReferrerPolicy::kSameOrigin:
    case network::mojom::ReferrerPolicy::kStrictOrigin:
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return true;

    case network::mojom::ReferrerPolicy::kDefault:
      NOTREACHED();
      return false;
  }
}

String MakeReferrerWarning(const KURL& url, const Referrer& referrer) {
  return "Ignored attempt to prefetch " + url.ElidedString() +
         " due to unacceptable referrer policy (" +
         SecurityPolicy::ReferrerPolicyAsString(referrer.referrer_policy) +
         ").";
}

}  // namespace

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
  DCHECK(!base::Contains(rule_sets_, rule_set));
  rule_sets_.push_back(rule_set);
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::RemoveRuleSet(SpeculationRuleSet* rule_set) {
  auto* it = base::ranges::remove(rule_sets_, rule_set);
  DCHECK(it != rule_sets_.end()) << "rule set was removed without existing";
  rule_sets_.erase(it, rule_sets_.end());
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::AddSpeculationRuleLoader(
    SpeculationRuleLoader* speculation_rule_loader) {
  speculation_rule_loaders_.insert(speculation_rule_loader);
}

void DocumentSpeculationRules::RemoveSpeculationRuleLoader(
    SpeculationRuleLoader* speculation_rule_loader) {
  speculation_rule_loaders_.erase(speculation_rule_loader);
}

void DocumentSpeculationRules::Trace(Visitor* visitor) const {
  Supplement::Trace(visitor);
  visitor->Trace(rule_sets_);
  visitor->Trace(host_);
  visitor->Trace(speculation_rule_loaders_);
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

void DocumentSpeculationRules::QueueUpdateSpeculationCandidates() {
  if (has_pending_update_)
    return;

  auto* execution_context = GetSupplementable()->GetExecutionContext();
  if (!execution_context)
    return;
  has_pending_update_ = true;
  execution_context->GetAgent()->event_loop()->EnqueueMicrotask(
      WTF::BindOnce(&DocumentSpeculationRules::UpdateSpeculationCandidates,
                    WrapWeakPersistent(this)));
}

void DocumentSpeculationRules::UpdateSpeculationCandidates() {
  has_pending_update_ = false;

  mojom::blink::SpeculationHost* host = GetHost();
  auto* execution_context = GetSupplementable()->GetExecutionContext();
  if (!host || !execution_context)
    return;

  network::mojom::ReferrerPolicy referrer_policy =
      execution_context->GetReferrerPolicy();
  String outgoing_referrer = execution_context->OutgoingReferrer();

  Vector<mojom::blink::SpeculationCandidatePtr> candidates;
  auto push_candidates = [&candidates, &referrer_policy, &outgoing_referrer,
                          &execution_context](
                             mojom::blink::SpeculationAction action,
                             const HeapVector<Member<SpeculationRule>>& rules) {
    for (SpeculationRule* rule : rules) {
      for (const KURL& url : rule->urls()) {
        Referrer referrer = SecurityPolicy::GenerateReferrer(
            referrer_policy, url, outgoing_referrer);

        if (!AcceptableReferrerPolicy(action, referrer)) {
          execution_context->AddConsoleMessage(
              mojom::blink::ConsoleMessageSource::kOther,
              mojom::blink::ConsoleMessageLevel::kWarning,
              MakeReferrerWarning(url, referrer));
          continue;
        }

        auto referrer_ptr = mojom::blink::Referrer::New(
            KURL(referrer.referrer), referrer.referrer_policy);
        candidates.push_back(mojom::blink::SpeculationCandidate::New(
            url, action, std::move(referrer_ptr),
            rule->requires_anonymous_client_ip_when_cross_origin(),
            rule->target_browsing_context_name_hint().value_or(
                mojom::blink::SpeculationTargetHint::kNoHint)));
      }
    }
  };

  for (SpeculationRuleSet* rule_set : rule_sets_) {
    // If kSpeculationRulesPrefetchProxy is enabled, collect all prefetch
    // speculation rules.
    if (RuntimeEnabledFeatures::SpeculationRulesPrefetchProxyEnabled(
            execution_context)) {
      push_candidates(mojom::blink::SpeculationAction::kPrefetch,
                      rule_set->prefetch_rules());
    }

    // Ditto for SpeculationRulesPrefetchWithSubresources.
    if (RuntimeEnabledFeatures::SpeculationRulesPrefetchWithSubresourcesEnabled(
            execution_context)) {
      push_candidates(
          mojom::blink::SpeculationAction::kPrefetchWithSubresources,
          rule_set->prefetch_with_subresources_rules());
    }

    // If kPrerender2 is enabled, collect all prerender speculation rules.
    if (RuntimeEnabledFeatures::Prerender2Enabled(execution_context)) {
      push_candidates(mojom::blink::SpeculationAction::kPrerender,
                      rule_set->prerender_rules());

      // Set the flag to evict the cached data of Session Storage when the
      // document is frozen or unload to avoid reusing old data in the cache
      // after the session storage has been modified by another renderer
      // process. See crbug.com/1215680 for more details.
      LocalFrame* frame = GetSupplementable()->GetFrame();
      if (frame && frame->IsMainFrame()) {
        frame->SetEvictCachedSessionStorageOnFreezeOrUnload();
      }
    }
  }

  host->UpdateSpeculationCandidates(std::move(candidates));
}

}  // namespace blink
