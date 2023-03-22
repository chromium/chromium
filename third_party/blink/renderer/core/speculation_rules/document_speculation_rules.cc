// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_including_tree_order_traversal.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/speculation_rule_loader.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/speculation_rules/document_rule_predicate.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// https://wicg.github.io/nav-speculation/prefetch.html#list-of-sufficiently-strict-speculative-navigation-referrer-policies
bool AcceptableReferrerPolicy(const Referrer& referrer,
                              bool is_initially_same_site) {
  // Lax referrer policies are acceptable for same-site. The browser is
  // responsible for aborting in the case of cross-site redirects with lax
  // referrer policies.
  if (is_initially_same_site)
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

String SpeculationActionAsString(mojom::blink::SpeculationAction action) {
  switch (action) {
    case mojom::blink::SpeculationAction::kPrefetch:
    case mojom::blink::SpeculationAction::kPrefetchWithSubresources:
      return "prefetch";
    case mojom::blink::SpeculationAction::kPrerender:
      return "prerender";
  }
}

String MakeReferrerWarning(mojom::blink::SpeculationAction action,
                           const KURL& url,
                           const Referrer& referrer) {
  return "Ignored attempt to " + SpeculationActionAsString(action) + " " +
         url.ElidedString() + " due to unacceptable referrer policy (" +
         SecurityPolicy::ReferrerPolicyAsString(referrer.referrer_policy) +
         ").";
}

// Computes a referrer based on a Speculation Rule, and its URL or the link it
// is matched against. Return absl::nullopt if the computed referrer policy is
// not acceptable (see AcceptableReferrerPolicy above).
absl::optional<Referrer> GetReferrer(SpeculationRule* rule,
                                     ExecutionContext* execution_context,
                                     mojom::blink::SpeculationAction action,
                                     HTMLAnchorElement* link,
                                     absl::optional<KURL> opt_url) {
  DCHECK(link || opt_url);
  bool using_link_referrer_policy = false;
  network::mojom::ReferrerPolicy referrer_policy;
  if (rule->referrer_policy()) {
    referrer_policy = rule->referrer_policy().value();
  } else {
    referrer_policy = execution_context->GetReferrerPolicy();
    if (link && link->HasRel(kRelationNoReferrer)) {
      using_link_referrer_policy = true;
      referrer_policy = network::mojom::ReferrerPolicy::kNever;
    } else if (link &&
               link->FastHasAttribute(html_names::kReferrerpolicyAttr)) {
      // Override |referrer_policy| with value derived from link's
      // referrerpolicy attribute (if valid).
      using_link_referrer_policy = SecurityPolicy::ReferrerPolicyFromString(
          link->FastGetAttribute(html_names::kReferrerpolicyAttr),
          kSupportReferrerPolicyLegacyKeywords, &referrer_policy);
    }
  }

  String outgoing_referrer = execution_context->OutgoingReferrer();
  KURL url = link ? link->HrefURL() : opt_url.value();
  scoped_refptr<const SecurityOrigin> url_origin = SecurityOrigin::Create(url);
  const bool is_initially_same_site =
      url_origin->IsSameSiteWith(execution_context->GetSecurityOrigin());
  Referrer referrer =
      SecurityPolicy::GenerateReferrer(referrer_policy, url, outgoing_referrer);

  // TODO(mcnee): Speculation rules initially shipped with a bug where a policy
  // of "no-referrer" would be assumed and the referrer policy restriction was
  // not enforced. We emulate that behaviour here as sites did not have a means
  // of specifying a suitable policy. SpeculationRulesReferrerPolicyKey shipped
  // in M111. This workaround should be removed when the flag is removed.
  // See https://crbug.com/1398772.
  if (!RuntimeEnabledFeatures::SpeculationRulesReferrerPolicyKeyEnabled(
          execution_context) &&
      !AcceptableReferrerPolicy(referrer, is_initially_same_site)) {
    referrer = SecurityPolicy::GenerateReferrer(
        network::mojom::ReferrerPolicy::kNever, url, outgoing_referrer);
    DCHECK(AcceptableReferrerPolicy(referrer, is_initially_same_site));
  }

  if (!AcceptableReferrerPolicy(referrer, is_initially_same_site)) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        MakeReferrerWarning(action, url, referrer));
    if (using_link_referrer_policy) {
      console_message->SetNodes(link->GetDocument().GetFrame(),
                                {DOMNodeIds::IdForNode(link)});
    }
    execution_context->AddConsoleMessage(console_message);
    return absl::nullopt;
  }

  return referrer;
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
  CountSpeculationRulesLoadOutcome(SpeculationRulesLoadOutcome::kSuccess);
  DCHECK(!base::Contains(rule_sets_, rule_set));
  rule_sets_.push_back(rule_set);
  if (rule_set->has_document_rule()) {
    UseCounter::Count(GetSupplementable(),
                      WebFeature::kSpeculationRulesDocumentRules);
    InitializeIfNecessary();
    InvalidateAllLinks();
    if (!rule_set->selectors().empty()) {
      UpdateSelectors();
    }
  }
  QueueUpdateSpeculationCandidates();

  probe::DidAddSpeculationRuleSet(*GetSupplementable(), *rule_set);
}

void DocumentSpeculationRules::RemoveRuleSet(SpeculationRuleSet* rule_set) {
  auto* it = base::ranges::remove(rule_sets_, rule_set);
  DCHECK(it != rule_sets_.end()) << "rule set was removed without existing";
  rule_sets_.erase(it, rule_sets_.end());
  if (rule_set->has_document_rule()) {
    InvalidateAllLinks();
    if (!rule_set->selectors().empty()) {
      UpdateSelectors();
    }
  }
  QueueUpdateSpeculationCandidates();

  probe::DidRemoveSpeculationRuleSet(*GetSupplementable(), *rule_set);
}

void DocumentSpeculationRules::AddSpeculationRuleLoader(
    SpeculationRuleLoader* speculation_rule_loader) {
  speculation_rule_loaders_.insert(speculation_rule_loader);
}

void DocumentSpeculationRules::RemoveSpeculationRuleLoader(
    SpeculationRuleLoader* speculation_rule_loader) {
  speculation_rule_loaders_.erase(speculation_rule_loader);
}

void DocumentSpeculationRules::LinkInserted(HTMLAnchorElement* link) {
  if (!initialized_)
    return;

  DCHECK(link->IsLink());
  DCHECK(link->isConnected());
  AddLink(link);
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::LinkRemoved(HTMLAnchorElement* link) {
  if (!initialized_)
    return;

  DCHECK(link->IsLink());
  RemoveLink(link);
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::HrefAttributeChanged(
    HTMLAnchorElement* link,
    const AtomicString& old_value,
    const AtomicString& new_value) {
  if (!initialized_)
    return;

  DCHECK_NE(old_value, new_value);
  DCHECK(link->isConnected());

  if (old_value.IsNull())
    AddLink(link);
  else if (new_value.IsNull())
    RemoveLink(link);
  else
    InvalidateLink(link);

  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::ReferrerPolicyAttributeChanged(
    HTMLAnchorElement* link) {
  if (!initialized_)
    return;

  DCHECK(link->isConnected());
  InvalidateLink(link);

  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::RelAttributeChanged(HTMLAnchorElement* link) {
  if (!initialized_)
    return;

  DCHECK(link->isConnected());
  InvalidateLink(link);

  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::DocumentReferrerPolicyChanged() {
  if (!initialized_)
    return;

  InvalidateAllLinks();
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::DocumentBaseURLChanged() {
  // Replace every existing rule set with a new copy that is parsed using the
  // updated document base URL.
  for (Member<SpeculationRuleSet>& rule_set : rule_sets_) {
    SpeculationRuleSet::Source* source = rule_set->source();
    rule_set = SpeculationRuleSet::Parse(
        source, GetSupplementable()->GetExecutionContext(),
        /*out_error=*/nullptr);
    // There should not be any parsing errors as these rule sets have already
    // been parsed once without errors, and an updated base URL should not cause
    // new errors. There may however still be warnings.
    DCHECK(rule_set);
  }
  if (initialized_)
    InvalidateAllLinks();
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::LinkMatchedSelectorsUpdated(
    HTMLAnchorElement* link) {
  DCHECK(initialized_);
  DCHECK(SelectorMatchesEnabled());

  InvalidateLink(link);
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::LinkGainedOrLostComputedStyle(
    HTMLAnchorElement* link) {
  if (!SelectorMatchesEnabled() || !initialized_) {
    return;
  }

  InvalidateLink(link);
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::DocumentStyleUpdated() {
  if (pending_update_state_ ==
      PendingUpdateState::kUpdateWithCleanStylePending) {
    UpdateSpeculationCandidates();
  }
}

void DocumentSpeculationRules::Trace(Visitor* visitor) const {
  Supplement::Trace(visitor);
  visitor->Trace(rule_sets_);
  visitor->Trace(host_);
  visitor->Trace(speculation_rule_loaders_);
  visitor->Trace(matched_links_);
  visitor->Trace(unmatched_links_);
  visitor->Trace(pending_links_);
  visitor->Trace(selectors_);
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
  if (pending_update_state_ != PendingUpdateState::kNoUpdatePending) {
    return;
  }

  // If "selector_matches" is enabled and style isn't clean, we don't need to
  // enqueue a microtask to run UpdateSpeculationCandidates, and instead wait
  // for DocumentStyleUpdated to be called.
  if (SelectorMatchesEnabled() &&
      GetSupplementable()->NeedsLayoutTreeUpdate()) {
    SetPendingUpdateState(PendingUpdateState::kUpdateWithCleanStylePending);
    return;
  }

  auto* execution_context = GetSupplementable()->GetExecutionContext();
  if (!execution_context)
    return;

  SetPendingUpdateState(PendingUpdateState::kUpdatePending);
  execution_context->GetAgent()->event_loop()->EnqueueMicrotask(
      WTF::BindOnce(&DocumentSpeculationRules::UpdateSpeculationCandidates,
                    WrapWeakPersistent(this)));
}

void DocumentSpeculationRules::UpdateSpeculationCandidates() {
  DCHECK_NE(pending_update_state_, PendingUpdateState::kNoUpdatePending);

  // Style may be invalidated after we enqueue a microtask, in which case we
  // wait for style to be clean before proceeding.
  if (SelectorMatchesEnabled() &&
      GetSupplementable()->NeedsLayoutTreeUpdate()) {
    SetPendingUpdateState(PendingUpdateState::kUpdateWithCleanStylePending);
    return;
  }

  // We are actually performing the update below, so mark as no update pending.
  SetPendingUpdateState(PendingUpdateState::kNoUpdatePending);

  mojom::blink::SpeculationHost* host = GetHost();
  auto* execution_context = GetSupplementable()->GetExecutionContext();
  if (!host || !execution_context)
    return;

  Vector<mojom::blink::SpeculationCandidatePtr> candidates;
  auto push_candidates = [&candidates, &execution_context](
                             mojom::blink::SpeculationAction action,
                             const HeapVector<Member<SpeculationRule>>& rules) {
    for (SpeculationRule* rule : rules) {
      for (const KURL& url : rule->urls()) {
        absl::optional<Referrer> referrer =
            GetReferrer(rule, execution_context, action, /*link=*/nullptr, url);
        if (!referrer)
          continue;

        auto referrer_ptr = mojom::blink::Referrer::New(
            KURL(referrer->referrer), referrer->referrer_policy);
        candidates.push_back(mojom::blink::SpeculationCandidate::New(
            url, action, std::move(referrer_ptr),
            rule->requires_anonymous_client_ip_when_cross_origin(),
            rule->target_browsing_context_name_hint().value_or(
                mojom::blink::SpeculationTargetHint::kNoHint),
            // The default Eagerness value for |"source": "list"| rules is
            // |kEager|. More info can be found here:
            // https://github.com/WICG/nav-speculation/blob/main/triggers.md#eagerness
            rule->eagerness().value_or(
                mojom::blink::SpeculationEagerness::kEager)));
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

  // Add candidates derived from document rule predicates.
  AddLinkBasedSpeculationCandidates(candidates);

  if (!sent_is_part_of_no_vary_search_trial_ &&
      RuntimeEnabledFeatures::NoVarySearchPrefetchEnabled(execution_context)) {
    sent_is_part_of_no_vary_search_trial_ = true;
    host->EnableNoVarySearchSupport();
  }

  host->UpdateSpeculationCandidates(std::move(candidates));
}

void DocumentSpeculationRules::AddLinkBasedSpeculationCandidates(
    Vector<mojom::blink::SpeculationCandidatePtr>& candidates) {
  // Match all the unmatched
  while (!pending_links_.empty()) {
    auto it = pending_links_.begin();
    HTMLAnchorElement* link = *it;
    Vector<mojom::blink::SpeculationCandidatePtr> link_candidates;
    ExecutionContext* execution_context =
        GetSupplementable()->GetExecutionContext();
    DCHECK(execution_context);
    const bool selector_matches_enabled = SelectorMatchesEnabled();

    const auto push_link_candidates =
        [&link, &link_candidates, &execution_context,
         &selector_matches_enabled](
            mojom::blink::SpeculationAction action,
            const HeapVector<Member<SpeculationRule>>& speculation_rules) {
          if (!link->HrefURL().ProtocolIsInHTTPFamily()) {
            return;
          }

          // We exclude links that don't have a ComputedStyle stored (or have a
          // ComputedStyle only because EnsureComputedStyle was called, and
          // otherwise wouldn't). This corresponds to links that are not in the
          // flat tree or links with a "display: none" inclusive-ancestor.
          if (selector_matches_enabled &&
              ComputedStyle::IsNullOrEnsured(link->GetComputedStyle())) {
            return;
          }

          for (SpeculationRule* rule : speculation_rules) {
            if (!rule->predicate())
              continue;
            if (!rule->predicate()->Matches(*link))
              continue;

            absl::optional<Referrer> referrer =
                GetReferrer(rule, execution_context, action, link,
                            /*opt_url=*/absl::nullopt);
            if (!referrer)
              continue;
            mojom::blink::ReferrerPtr referrer_ptr =
                mojom::blink::Referrer::New(KURL(referrer->referrer),
                                            referrer->referrer_policy);

            // TODO(crbug.com/1371522): We should be generating a target hint
            // based on the link's target.
            mojom::blink::SpeculationCandidatePtr candidate =
                mojom::blink::SpeculationCandidate::New(
                    link->HrefURL(), action, std::move(referrer_ptr),
                    rule->requires_anonymous_client_ip_when_cross_origin(),
                    rule->target_browsing_context_name_hint().value_or(
                        mojom::blink::SpeculationTargetHint::kNoHint),
                    // The default Eagerness value for |"source": "document"|
                    // rules is |kConservative|. More info can be found here:
                    // https://github.com/WICG/nav-speculation/blob/main/triggers.md#eagerness
                    rule->eagerness().value_or(
                        mojom::blink::SpeculationEagerness::kConservative));
            link_candidates.push_back(std::move(candidate));
          }
        };

    for (SpeculationRuleSet* rule_set : rule_sets_) {
      if (RuntimeEnabledFeatures::SpeculationRulesPrefetchProxyEnabled(
              execution_context)) {
        push_link_candidates(mojom::blink::SpeculationAction::kPrefetch,
                             rule_set->prefetch_rules());
      }

      if (RuntimeEnabledFeatures::
              SpeculationRulesPrefetchWithSubresourcesEnabled(
                  execution_context)) {
        push_link_candidates(
            mojom::blink::SpeculationAction::kPrefetchWithSubresources,
            rule_set->prefetch_with_subresources_rules());
      }

      if (RuntimeEnabledFeatures::Prerender2Enabled(execution_context)) {
        push_link_candidates(mojom::blink::SpeculationAction::kPrerender,
                             rule_set->prerender_rules());
      }
    }

    if (!link_candidates.empty())
      matched_links_.Set(link, std::move(link_candidates));
    else
      unmatched_links_.insert(link);

    pending_links_.erase(it);
  }

  for (auto& it : matched_links_) {
    for (const auto& candidate : it.value) {
      candidates.push_back(candidate.Clone());
    }
  }
}

void DocumentSpeculationRules::InitializeIfNecessary() {
  if (initialized_)
    return;
  initialized_ = true;
  for (Node& node :
       ShadowIncludingTreeOrderTraversal::DescendantsOf(*GetSupplementable())) {
    if (!node.IsLink())
      continue;
    if (auto* anchor = DynamicTo<HTMLAnchorElement>(node))
      pending_links_.insert(anchor);
    else if (auto* area = DynamicTo<HTMLAreaElement>(node))
      pending_links_.insert(area);
  }
}

void DocumentSpeculationRules::AddLink(HTMLAnchorElement* link) {
  DCHECK(initialized_);
  DCHECK(link->IsLink());
  DCHECK(!base::Contains(unmatched_links_, link));
  DCHECK(!base::Contains(matched_links_, link));
  DCHECK(!base::Contains(pending_links_, link));

  pending_links_.insert(link);
}

void DocumentSpeculationRules::RemoveLink(HTMLAnchorElement* link) {
  DCHECK(initialized_);

  if (auto it = matched_links_.find(link); it != matched_links_.end()) {
    matched_links_.erase(it);
    DCHECK(!base::Contains(unmatched_links_, link));
    DCHECK(!base::Contains(pending_links_, link));
    return;
  }
  // TODO(crbug.com/1371522): Removing a link that doesn't match anything isn't
  // going to change the candidate list, we could skip calling
  // QueueUpdateSpeculationCandidates in this scenario.
  if (auto it = unmatched_links_.find(link); it != unmatched_links_.end()) {
    unmatched_links_.erase(it);
    DCHECK(!base::Contains(pending_links_, link));
    return;
  }
  auto it = pending_links_.find(link);
  DCHECK(it != pending_links_.end());
  pending_links_.erase(it);
}

void DocumentSpeculationRules::InvalidateLink(HTMLAnchorElement* link) {
  DCHECK(initialized_);

  pending_links_.insert(link);
  if (auto it = matched_links_.find(link); it != matched_links_.end()) {
    matched_links_.erase(it);
    DCHECK(!base::Contains(unmatched_links_, link));
    return;
  }
  if (auto it = unmatched_links_.find(link); it != unmatched_links_.end())
    unmatched_links_.erase(it);
}

void DocumentSpeculationRules::InvalidateAllLinks() {
  DCHECK(initialized_);

  for (const auto& it : matched_links_)
    pending_links_.insert(it.key);
  matched_links_.clear();

  for (HTMLAnchorElement* link : unmatched_links_)
    pending_links_.insert(link);
  unmatched_links_.clear();
}

void DocumentSpeculationRules::UpdateSelectors() {
  if (!SelectorMatchesEnabled()) {
    return;
  }

  HeapVector<Member<StyleRule>> selectors;
  for (SpeculationRuleSet* rule_set : rule_sets_) {
    selectors.AppendVector(rule_set->selectors());
  }

  selectors_ = std::move(selectors);
  GetSupplementable()->GetStyleEngine().DocumentRulesSelectorsChanged();
}

void DocumentSpeculationRules::SetPendingUpdateState(
    PendingUpdateState new_state) {
  PendingUpdateState old_state = pending_update_state_;
  // This is the only invalid state transition.
  DCHECK(!(old_state == PendingUpdateState::kUpdateWithCleanStylePending &&
           new_state == PendingUpdateState::kUpdatePending));
  pending_update_state_ = new_state;
}

bool DocumentSpeculationRules::SelectorMatchesEnabled() {
  if (was_selector_matches_enabled_) {
    return true;
  }
  was_selector_matches_enabled_ = RuntimeEnabledFeatures::
      SpeculationRulesDocumentRulesSelectorMatchesEnabled(
          GetSupplementable()->GetExecutionContext());
  return was_selector_matches_enabled_;
}

}  // namespace blink
