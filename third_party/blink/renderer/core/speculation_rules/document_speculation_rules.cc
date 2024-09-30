// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/document_speculation_rules.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/state_transitions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_including_tree_order_traversal.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/event_handler_registry.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/html/html_area_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/speculation_rule_loader.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/speculation_rules/document_rule_predicate.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_candidate.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
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
      NOTREACHED_IN_MIGRATION();
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
                           const Referrer& referrer,
                           bool has_link) {
  const String action_string = SpeculationActionAsString(action);

  const String suggested_fix =
      has_link ? "A stricter referrer policy may be set using the matched "
                 "link's \"referrerpolicy\" attribute, or it may be set "
                 "specifically for the " +
                     action_string +
                     " request using the \"referrer_policy\" key in the "
                     "speculation rule."
               : "A stricter referrer policy may be set for this specific " +
                     action_string +
                     " request using the \"referrer_policy\" key in the "
                     "speculation rule.";
  constexpr auto kExampleAcceptablePolicy =
      network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;

  return "Ignored attempt to " + action_string + " " + url.ElidedString() +
         " due to unacceptable referrer policy (" +
         SecurityPolicy::ReferrerPolicyAsString(referrer.referrer_policy) +
         "). " + suggested_fix + " For example, the policy \"" +
         SecurityPolicy::ReferrerPolicyAsString(kExampleAcceptablePolicy) +
         "\" is sufficiently strict.";
}

// Computes a referrer based on a Speculation Rule, and its URL or the link it
// is matched against. Return std::nullopt if the computed referrer policy is
// not acceptable (see AcceptableReferrerPolicy above).
std::optional<Referrer> GetReferrer(const SpeculationRule* rule,
                                    const SpeculationRuleSet& rule_set,
                                    Document& document,
                                    mojom::blink::SpeculationAction action,
                                    HTMLAnchorElementBase* link,
                                    std::optional<KURL> opt_url) {
  ExecutionContext* execution_context = document.GetExecutionContext();
  DCHECK(link || opt_url);
  network::mojom::ReferrerPolicy referrer_policy;
  if (rule->referrer_policy()) {
    referrer_policy = rule->referrer_policy().value();
  } else if (link && link->HasRel(kRelationNoReferrer)) {
    referrer_policy = network::mojom::ReferrerPolicy::kNever;
    UseCounter::Count(document,
                      WebFeature::kSpeculationRulesUsedLinkReferrerPolicy);
  } else if (link && link->FastHasAttribute(html_names::kReferrerpolicyAttr)) {
    // Override |referrer_policy| with value derived from link's
    // referrerpolicy attribute (if valid).
    bool valid = SecurityPolicy::ReferrerPolicyFromString(
        link->FastGetAttribute(html_names::kReferrerpolicyAttr),
        kSupportReferrerPolicyLegacyKeywords, &referrer_policy);
    if (valid) {
      UseCounter::Count(document,
                        WebFeature::kSpeculationRulesUsedLinkReferrerPolicy);
    } else {
      referrer_policy = execution_context->GetReferrerPolicy();
    }
  } else {
    referrer_policy = execution_context->GetReferrerPolicy();
  }

  String outgoing_referrer = execution_context->OutgoingReferrer();
  KURL url = link ? link->HrefURL() : opt_url.value();
  scoped_refptr<const SecurityOrigin> url_origin = SecurityOrigin::Create(url);
  const bool is_initially_same_site =
      url_origin->IsSameSiteWith(execution_context->GetSecurityOrigin());
  Referrer referrer =
      SecurityPolicy::GenerateReferrer(referrer_policy, url, outgoing_referrer);

  if (!AcceptableReferrerPolicy(referrer, is_initially_same_site)) {
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning,
        MakeReferrerWarning(action, url, referrer, link));
    Vector<DOMNodeId> nodes;
    if (rule_set.source()->GetNodeId()) {
      nodes.push_back(*rule_set.source()->GetNodeId());
    }
    if (link) {
      nodes.push_back(link->GetDomNodeId());
    }
    console_message->SetNodes(document.GetFrame(), std::move(nodes));
    execution_context->AddConsoleMessage(console_message);
    UseCounter::Count(document,
                      WebFeature::kSpeculationRulesRejectedLaxReferrerPolicy);
    return std::nullopt;
  }

  return referrer;
}

// The reason for calling |UpdateSpeculationCandidates| for metrics.
// Currently, this is designed to measure the impact of the project of
// retriggering preloading on BFCache restoration (crbug.com/1449163), so
// other update reasons (such as ruleset insertion/removal etc...) will be
// tentatively classified as |kOther|.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UpdateSpeculationCandidatesReason {
  kOther = 0,
  kRestoredFromBFCache = 1,
  kMaxValue = kRestoredFromBFCache,
};

}  // namespace

std::ostream& operator<<(
    std::ostream& o,
    const DocumentSpeculationRules::PendingUpdateState& s) {
  return o << static_cast<unsigned>(s);
}

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
    : Supplement(document), host_(document.GetExecutionContext()) {
  if (!base::FeatureList::IsEnabled(features::kLCPTimingPredictorPrerender2)) {
    return;
  }
  auto* frame = GetSupplementable()->GetFrame();
  if (!frame) {
    return;
  }
  // LCPP is supposed to be attached to outer-most-main-frame only.
  // This matches with the current implementation of prerender2.
  LCPCriticalPathPredictor* lcpp = frame->GetLCPP();
  if (!lcpp) {
    return;
  }
  lcpp->AddLCPPredictedCallback(WTF::BindOnce(
      &DocumentSpeculationRules::OnLCPPredicted, WrapPersistent(this)));
}

void DocumentSpeculationRules::OnLCPPredicted(const Element*) {
  CHECK(base::FeatureList::IsEnabled(features::kLCPTimingPredictorPrerender2));
  mojom::blink::SpeculationHost* host = GetHost();
  if (!host) {
    return;
  }
  host->OnLCPPredicted();
}

void DocumentSpeculationRules::AddRuleSet(SpeculationRuleSet* rule_set) {
  SpeculationRulesLoadOutcome outcome = SpeculationRulesLoadOutcome::kSuccess;
  if (rule_set->ShouldReportUMAForError()) {
    if (rule_set->source()->IsFromRequest()) {
      outcome = SpeculationRulesLoadOutcome::kParseErrorFetched;
    } else if (rule_set->source()->IsFromInlineScript()) {
      outcome = SpeculationRulesLoadOutcome::kParseErrorInline;
    } else if (rule_set->source()->IsFromBrowserInjected()) {
      outcome = SpeculationRulesLoadOutcome::kParseErrorBrowserInjected;
    } else {
      NOTREACHED_IN_MIGRATION() << "error with unknown rule source";
    }
  } else if (rule_set->source()->IsFromBrowserInjectedAndRespectsOptOut()) {
    // Don't insert browser-injected rule sets that respect the opt-out on pages
    // that have other rules.
    for (const auto& other_rule_set : rule_sets_) {
      if (!other_rule_set->source()->IsFromBrowserInjected()) {
        CountSpeculationRulesLoadOutcome(
            SpeculationRulesLoadOutcome::kAutoSpeculationRulesOptedOut);
        UseCounter::Count(GetSupplementable(),
                          WebFeature::kAutoSpeculationRulesOptedOut);
        return;
      }
    }
  }

  CountSpeculationRulesLoadOutcome(outcome);

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
  if (!wants_pointer_events_ && rule_set->requires_unfiltered_input()) {
    wants_pointer_events_ = true;
    Document& document = *GetSupplementable();
    if (auto* frame = document.GetFrame()) {
      frame->GetEventHandlerRegistry().DidAddEventHandler(
          document, EventHandlerRegistry::kPointerEvent);
    }
  }
  QueueUpdateSpeculationCandidates();

  probe::DidAddSpeculationRuleSet(*GetSupplementable(), *rule_set);

  // Record some use counters about the kinds of actions being proposed.
  if (rule_set->prefetch_rules().size()) {
    UseCounter::Count(GetSupplementable(),
                      rule_set->source()->IsFromBrowserInjected()
                          ? WebFeature::kSpeculationRulesBrowserPrefetchRule
                          : WebFeature::kSpeculationRulesAuthorPrefetchRule);
  }
  if (rule_set->prerender_rules().size()) {
    UseCounter::Count(GetSupplementable(),
                      rule_set->source()->IsFromBrowserInjected()
                          ? WebFeature::kSpeculationRulesBrowserPrerenderRule
                          : WebFeature::kSpeculationRulesAuthorPrerenderRule);
  }

  // If non-browser-injected speculation rules are injected, then remove all
  // opt-out respecting browser-injected speculation rules.
  if (!rule_set->source()->IsFromBrowserInjected()) {
    HeapVector<Member<SpeculationRuleSet>> to_remove;
    for (const auto& other_rule_set : rule_sets_) {
      if (other_rule_set->source()->IsFromBrowserInjectedAndRespectsOptOut()) {
        to_remove.push_back(other_rule_set);
      }
    }

    if (!to_remove.empty()) {
      UseCounter::Count(GetSupplementable(),
                        WebFeature::kAutoSpeculationRulesOptedOut);
      for (const auto& to_remove_rule_set : to_remove) {
        RemoveRuleSet(to_remove_rule_set);
      }
    }
  }
}

void DocumentSpeculationRules::RemoveRuleSet(SpeculationRuleSet* rule_set) {
  auto it = base::ranges::remove(rule_sets_, rule_set);
  CHECK(it != rule_sets_.end(), base::NotFatalUntil::M130)
      << "rule set was removed without existing";
  rule_sets_.erase(it, rule_sets_.end());
  if (rule_set->has_document_rule()) {
    InvalidateAllLinks();
    if (!rule_set->selectors().empty()) {
      UpdateSelectors();
    }
  }
  if (wants_pointer_events_ && rule_set->requires_unfiltered_input() &&
      base::ranges::none_of(rule_sets_,
                            &SpeculationRuleSet::requires_unfiltered_input)) {
    wants_pointer_events_ = false;
    Document& document = *GetSupplementable();
    if (auto* frame = document.GetFrame()) {
      frame->GetEventHandlerRegistry().DidRemoveEventHandler(
          document, EventHandlerRegistry::kPointerEvent);
    }
  }

  // When a rule set is removed, we want to assure that an update including the
  // removal is promptly processed, so that the browser can cancel any activity
  // that is no longer needed. This makes it more predictable when the author
  // can re-add those rules to start a new speculation (to freshen it), rather
  // than continuing an existing one.
  //
  // Since style doesn't necessarily become clean promptly enough for that (a
  // scheduled microtask is what we have in mind), we want style to be forced
  // clean by the deadline, if necessary.
  QueueUpdateSpeculationCandidates(/*force_style_update=*/true);

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

void DocumentSpeculationRules::LinkInserted(HTMLAnchorElementBase* link) {
  if (!initialized_)
    return;

  DCHECK(link->IsLink());
  DCHECK(link->isConnected());
  AddLink(link);
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::LinkRemoved(HTMLAnchorElementBase* link) {
  if (!initialized_)
    return;

  DCHECK(link->IsLink());
  RemoveLink(link);
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::HrefAttributeChanged(
    HTMLAnchorElementBase* link,
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
    HTMLAnchorElementBase* link) {
  LinkAttributeChanged(link);
}

void DocumentSpeculationRules::RelAttributeChanged(
    HTMLAnchorElementBase* link) {
  LinkAttributeChanged(link);
}

void DocumentSpeculationRules::TargetAttributeChanged(
    HTMLAnchorElementBase* link) {
  LinkAttributeChanged(link);
}

void DocumentSpeculationRules::DocumentReferrerPolicyChanged() {
  DocumentPropertyChanged();
}

void DocumentSpeculationRules::DocumentBaseURLChanged() {
  if (initialized_)
    InvalidateAllLinks();
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::DocumentBaseTargetChanged() {
  DocumentPropertyChanged();
}

void DocumentSpeculationRules::LinkMatchedSelectorsUpdated(
    HTMLAnchorElementBase* link) {
  DCHECK(initialized_);
  InvalidateLink(link);
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::LinkGainedOrLostComputedStyle(
    HTMLAnchorElementBase* link) {
  if (!initialized_) {
    return;
  }
  InvalidateLink(link);
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::DocumentStyleUpdated() {
  if (pending_update_state_ == PendingUpdateState::kOnNextStyleUpdate) {
    UpdateSpeculationCandidates();
  }
}

void DocumentSpeculationRules::ChildStyleRecalcBlocked(Element* root) {
  if (!initialized_) {
    return;
  }

  if (!elements_blocking_child_style_recalc_.insert(root).is_new_entry) {
    return;
  }

  bool queue_update = false;

  Node* node = FlatTreeTraversal::Next(*root, root);
  while (node) {
    if (node->IsLink() && (node->HasTagName(html_names::kATag) ||
                           node->HasTagName(html_names::kAreaTag))) {
      HTMLAnchorElementBase* anchor = To<HTMLAnchorElementBase>(node);
      if (stale_links_.insert(anchor).is_new_entry) {
        InvalidateLink(anchor);
        queue_update = true;
      }
    }

    // If |node| is an element that is already marked as blocking child style
    // recalc, we don't need to traverse its subtree (all of its children should
    // already be accounted for).
    if (auto* element = DynamicTo<Element>(node);
        element && elements_blocking_child_style_recalc_.Contains(element)) {
      node = FlatTreeTraversal::NextSkippingChildren(*node, root);
      continue;
    }

    node = FlatTreeTraversal::Next(*node, root);
  }

  if (queue_update) {
    QueueUpdateSpeculationCandidates();
  }
}

void DocumentSpeculationRules::DidStyleChildren(Element* root) {
  if (!initialized_) {
    return;
  }

  if (!elements_blocking_child_style_recalc_.Take(root)) {
    return;
  }

  bool queue_update = false;

  Node* node = FlatTreeTraversal::Next(*root, root);
  while (node) {
    if (node->IsLink() && (node->HasTagName(html_names::kATag) ||
                           node->HasTagName(html_names::kAreaTag))) {
      HTMLAnchorElementBase* anchor = To<HTMLAnchorElementBase>(node);
      if (auto it = stale_links_.find(anchor); it != stale_links_.end()) {
        stale_links_.erase(it);
        InvalidateLink(anchor);
        queue_update = true;
      }
    }

    // If |node| is a display-locked element that is already marked as blocking
    // child style recalc, we don't need to traverse its children.
    if (auto* element = DynamicTo<Element>(node);
        element && elements_blocking_child_style_recalc_.Contains(element)) {
      node = FlatTreeTraversal::NextSkippingChildren(*node, root);
      continue;
    }

    node = FlatTreeTraversal::Next(*node, root);
  }

  if (queue_update) {
    QueueUpdateSpeculationCandidates();
  }
}

void DocumentSpeculationRules::DisplayLockedElementDisconnected(Element* root) {
  elements_blocking_child_style_recalc_.erase(root);
  // Note: We don't queue an update or invalidate any links here because
  // |root|'s children will also be disconnected shortly after this.
}

void DocumentSpeculationRules::DocumentRestoredFromBFCache() {
  first_update_after_restored_from_bfcache_ = true;
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::InitiatePreview(const KURL& url) {
  CHECK(base::FeatureList::IsEnabled(features::kLinkPreview));

  auto* host = GetHost();
  if (host) {
    host->InitiatePreview(url);
  }
}

void DocumentSpeculationRules::QueueUpdateSpeculationCandidates(
    bool force_style_update) {
  const bool microtask_already_queued = IsMicrotaskQueued();

  bool needs_microtask = true;
  if (force_style_update) {
    SetPendingUpdateState(
        PendingUpdateState::kMicrotaskQueuedWithForcedStyleUpdate);
  } else if (pending_update_state_ == PendingUpdateState::kNoUpdate) {
    SetPendingUpdateState(PendingUpdateState::kMicrotaskQueued);
  } else {
    // An update of some kind is already scheduled, whether on a microtask or
    // the next style update. That's sufficient.
    needs_microtask = false;
  }

  auto* execution_context = GetSupplementable()->GetExecutionContext();
  if (needs_microtask && !microtask_already_queued && execution_context) {
    execution_context->GetAgent()->event_loop()->EnqueueMicrotask(WTF::BindOnce(
        &DocumentSpeculationRules::UpdateSpeculationCandidatesMicrotask,
        WrapWeakPersistent(this)));
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
  visitor->Trace(stale_links_);
  visitor->Trace(elements_blocking_child_style_recalc_);
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

void DocumentSpeculationRules::UpdateSpeculationCandidatesMicrotask() {
  DCHECK(IsMicrotaskQueued());

  // Wait for style to be clean before proceeding. Or force it, if this update
  // needs to happen promptly.
  Document& document = *GetSupplementable();
  if (document.NeedsLayoutTreeUpdate()) {
    if (pending_update_state_ ==
        PendingUpdateState::kMicrotaskQueuedWithForcedStyleUpdate) {
      document.UpdateStyleAndLayoutTree();
    } else {
      SetPendingUpdateState(PendingUpdateState::kOnNextStyleUpdate);
      return;
    }
  }

  UpdateSpeculationCandidates();
}

void DocumentSpeculationRules::UpdateSpeculationCandidates() {
  Document& document = *GetSupplementable();
  DCHECK_NE(pending_update_state_, PendingUpdateState::kNoUpdate);
  DCHECK(!document.NeedsLayoutTreeUpdate());

  // We are actually performing the update below, so mark as no update pending.
  SetPendingUpdateState(PendingUpdateState::kNoUpdate);

  mojom::blink::SpeculationHost* host = GetHost();
  auto* execution_context = document.GetExecutionContext();
  if (!host || !execution_context) {
    return;
  }

  HeapVector<Member<SpeculationCandidate>> candidates;
  auto push_candidates = [&candidates, &document](
                             mojom::blink::SpeculationAction action,
                             SpeculationRuleSet* rule_set,
                             const HeapVector<Member<SpeculationRule>>& rules) {
    for (SpeculationRule* rule : rules) {
      for (const KURL& url : rule->urls()) {
        std::optional<Referrer> referrer = GetReferrer(
            rule, *rule_set, document, action, /*link=*/nullptr, url);
        if (!referrer)
          continue;

        // Ensured by `SpeculationRuleSet`.
        CHECK(!rule->target_browsing_context_name_hint() ||
              action == mojom::blink::SpeculationAction::kPrerender);
        CHECK(!rule->requires_anonymous_client_ip_when_cross_origin() ||
              action == mojom::blink::SpeculationAction::kPrefetch);

        candidates.push_back(MakeGarbageCollected<SpeculationCandidate>(
            url, action, referrer.value(),
            rule->requires_anonymous_client_ip_when_cross_origin(),
            rule->target_browsing_context_name_hint().value_or(
                mojom::blink::SpeculationTargetHint::kNoHint),
            rule->eagerness(), rule->no_vary_search_expected().Clone(),
            rule->injection_type(), rule_set, /*anchor=*/nullptr));
      }
    }
  };

  for (SpeculationRuleSet* rule_set : rule_sets_) {
    push_candidates(mojom::blink::SpeculationAction::kPrefetch, rule_set,
                    rule_set->prefetch_rules());

    if (RuntimeEnabledFeatures::SpeculationRulesPrefetchWithSubresourcesEnabled(
            execution_context)) {
      push_candidates(
          mojom::blink::SpeculationAction::kPrefetchWithSubresources, rule_set,
          rule_set->prefetch_with_subresources_rules());
    }

    // If kPrerender2 is enabled, collect all prerender speculation rules.
    if (RuntimeEnabledFeatures::Prerender2Enabled(execution_context)) {
      push_candidates(mojom::blink::SpeculationAction::kPrerender, rule_set,
                      rule_set->prerender_rules());

      // Set the flag to evict the cached data of Session Storage when the
      // document is frozen or unload to avoid reusing old data in the cache
      // after the session storage has been modified by another renderer
      // process. See crbug.com/1215680 for more details.
      LocalFrame* frame = document.GetFrame();
      if (frame && frame->IsMainFrame()) {
        frame->SetEvictCachedSessionStorageOnFreezeOrUnload();
      }
    }
  }

  // Add candidates derived from document rule predicates.
  AddLinkBasedSpeculationCandidates(candidates);

  // Remove candidates for links to fragments in the current document. These are
  // unlikely to be useful to preload, because such navigations are likely to
  // trigger fragment navigation (see
  // |FrameLoader::ShouldPerformFragmentNavigation|).
  // Note that the document's URL is not necessarily the same as the base URL
  // (e,g., when a <base> element is present in the document).
  const KURL& document_url = document.Url();
  auto last = base::ranges::remove_if(candidates, [&](const auto& candidate) {
    const KURL& url = candidate->url();
    return url.HasFragmentIdentifier() &&
           EqualIgnoringFragmentIdentifier(url, document_url);
  });
  candidates.Shrink(base::checked_cast<wtf_size_t>(last - candidates.begin()));

  probe::SpeculationCandidatesUpdated(document, candidates);

  using SpeculationEagerness = blink::mojom::SpeculationEagerness;
  base::EnumSet<SpeculationEagerness, SpeculationEagerness::kMinValue,
                SpeculationEagerness::kMaxValue>
      eagerness_set;

  Vector<mojom::blink::SpeculationCandidatePtr> mojom_candidates;
  mojom_candidates.ReserveInitialCapacity(candidates.size());
  for (SpeculationCandidate* candidate : candidates) {
    eagerness_set.Put(candidate->eagerness());
    mojom_candidates.push_back(candidate->ToMojom());
  }

  host->UpdateSpeculationCandidates(std::move(mojom_candidates));

  if (eagerness_set.Has(SpeculationEagerness::kConservative)) {
    UseCounter::Count(document,
                      WebFeature::kSpeculationRulesEagernessConservative);
  }
  if (eagerness_set.Has(SpeculationEagerness::kModerate)) {
    UseCounter::Count(document, WebFeature::kSpeculationRulesEagernessModerate);
  }
  if (eagerness_set.Has(SpeculationEagerness::kEager)) {
    UseCounter::Count(document, WebFeature::kSpeculationRulesEagernessEager);
  }

  base::UmaHistogramEnumeration(
      "Preloading.Experimental.UpdateSpeculationCandidatesReason",
      first_update_after_restored_from_bfcache_
          ? UpdateSpeculationCandidatesReason::kRestoredFromBFCache
          : UpdateSpeculationCandidatesReason::kOther);

  first_update_after_restored_from_bfcache_ = false;
}

void DocumentSpeculationRules::AddLinkBasedSpeculationCandidates(
    HeapVector<Member<SpeculationCandidate>>& candidates) {
  // Match all the unmatched
  while (!pending_links_.empty()) {
    auto it = pending_links_.begin();
    HTMLAnchorElementBase* link = *it;
    HeapVector<Member<SpeculationCandidate>>* link_candidates =
        MakeGarbageCollected<HeapVector<Member<SpeculationCandidate>>>();
    Document& document = *GetSupplementable();
    ExecutionContext* execution_context = document.GetExecutionContext();
    CHECK(execution_context);

    const auto push_link_candidates =
        [&link, &link_candidates, &document, this](
            mojom::blink::SpeculationAction action,
            SpeculationRuleSet* rule_set,
            const HeapVector<Member<SpeculationRule>>& speculation_rules) {
          if (!link->HrefURL().ProtocolIsInHTTPFamily()) {
            return;
          }

          // We exclude links that don't have a ComputedStyle stored (or have
          // a ComputedStyle only because EnsureComputedStyle was called, and
          // otherwise wouldn't). This corresponds to links that are not in
          // the flat tree or links with a "display: none" inclusive-ancestor.
          if (ComputedStyle::IsNullOrEnsured(link->GetComputedStyle())) {
            return;
          }

          // Links with display locked ancestors can have a stale
          // ComputedStyle, i.e. a ComputedStyle that wasn't updated during a
          // style update because the element isn't currently being rendered,
          // but is not discarded either. We ignore these links as well.
          if (stale_links_.Contains(link)) {
            return;
          }

          for (SpeculationRule* rule : speculation_rules) {
            if (!rule->predicate())
              continue;
            if (!rule->predicate()->Matches(*link))
              continue;

            std::optional<Referrer> referrer =
                GetReferrer(rule, *rule_set, document, action, link,
                            /*opt_url=*/std::nullopt);
            if (!referrer)
              continue;

            mojom::blink::SpeculationTargetHint target_hint =
                mojom::blink::SpeculationTargetHint::kNoHint;
            if (action == mojom::blink::SpeculationAction::kPrerender) {
              if (rule->target_browsing_context_name_hint()) {
                target_hint = rule->target_browsing_context_name_hint().value();
              } else {
                // Obtain target hint from the link's target (if specified).
                target_hint =
                    SpeculationRuleSet::SpeculationTargetHintFromString(
                        link->GetEffectiveTarget());
              }
            }

            SpeculationCandidate* candidate =
                MakeGarbageCollected<SpeculationCandidate>(
                    link->HrefURL(), action, referrer.value(),
                    rule->requires_anonymous_client_ip_when_cross_origin(),
                    target_hint, rule->eagerness(),
                    rule->no_vary_search_expected().Clone(),
                    rule->injection_type(), rule_set, link);
            link_candidates->push_back(std::move(candidate));
          }
        };

    for (SpeculationRuleSet* rule_set : rule_sets_) {
      push_link_candidates(mojom::blink::SpeculationAction::kPrefetch, rule_set,
                           rule_set->prefetch_rules());

      if (RuntimeEnabledFeatures::
              SpeculationRulesPrefetchWithSubresourcesEnabled(
                  execution_context)) {
        push_link_candidates(
            mojom::blink::SpeculationAction::kPrefetchWithSubresources,
            rule_set, rule_set->prefetch_with_subresources_rules());
      }

      if (RuntimeEnabledFeatures::Prerender2Enabled(execution_context)) {
        push_link_candidates(mojom::blink::SpeculationAction::kPrerender,
                             rule_set, rule_set->prerender_rules());
      }
    }

    if (!link_candidates->empty()) {
      matched_links_.Set(link, link_candidates);
    } else {
      unmatched_links_.insert(link);
    }

    pending_links_.erase(it);
  }

  for (auto& it : matched_links_) {
    candidates.AppendVector(*(it.value));
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
    if (auto* anchor = DynamicTo<HTMLAnchorElementBase>(node)) {
      pending_links_.insert(anchor);
    }
  }
}

void DocumentSpeculationRules::LinkAttributeChanged(
    HTMLAnchorElementBase* link) {
  if (!initialized_) {
    return;
  }
  DCHECK(link->isConnected());
  InvalidateLink(link);
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::DocumentPropertyChanged() {
  if (!initialized_) {
    return;
  }
  InvalidateAllLinks();
  QueueUpdateSpeculationCandidates();
}

void DocumentSpeculationRules::AddLink(HTMLAnchorElementBase* link) {
  DCHECK(initialized_);
  DCHECK(link->IsLink());
  DCHECK(!base::Contains(unmatched_links_, link));
  DCHECK(!base::Contains(matched_links_, link));
  DCHECK(!base::Contains(pending_links_, link));
  DCHECK(!base::Contains(stale_links_, link));

  pending_links_.insert(link);
  // TODO(crbug.com/1371522): A stale link is guaranteed to not match, so we
  // should put it into |unmatched_links_| directly and skip queueing an update.
  if (DisplayLockUtilities::LockedAncestorPreventingStyle(*link)) {
    stale_links_.insert(link);
  }
}

void DocumentSpeculationRules::RemoveLink(HTMLAnchorElementBase* link) {
  DCHECK(initialized_);
  stale_links_.erase(link);

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
  CHECK(it != pending_links_.end(), base::NotFatalUntil::M130);
  pending_links_.erase(it);
}

void DocumentSpeculationRules::InvalidateLink(HTMLAnchorElementBase* link) {
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

  for (HTMLAnchorElementBase* link : unmatched_links_) {
    pending_links_.insert(link);
  }
  unmatched_links_.clear();
}

void DocumentSpeculationRules::UpdateSelectors() {
  HeapVector<Member<StyleRule>> selectors;
  for (SpeculationRuleSet* rule_set : rule_sets_) {
    selectors.AppendVector(rule_set->selectors());
  }

  selectors_ = std::move(selectors);
  GetSupplementable()->GetStyleEngine().DocumentRulesSelectorsChanged();
}

void DocumentSpeculationRules::SetPendingUpdateState(
    PendingUpdateState new_state) {
#if DCHECK_IS_ON()
  // TODO(jbroman): This could use "using enum" once that's allowed.
  using S = PendingUpdateState;
  DEFINE_STATIC_LOCAL(
      base::StateTransitions<S>, transitions,
      ({
          // When there is no update, we can only queue an update.
          {S::kNoUpdate,
           {S::kMicrotaskQueued, S::kMicrotaskQueuedWithForcedStyleUpdate}},
          // When an update is queued, it can complete, get upgraded to forcing
          // style, or need to wait for style (lazily).
          {S::kMicrotaskQueued,
           {S::kNoUpdate, S::kMicrotaskQueuedWithForcedStyleUpdate,
            S::kOnNextStyleUpdate}},
          // When waiting for style, this can complete, or we can realize we
          // need to queue another microtask to force an update, including
          // forcing style, by a predictable moment.
          {S::kOnNextStyleUpdate,
           {S::kNoUpdate, S::kMicrotaskQueuedWithForcedStyleUpdate}},
          // When a microtask with forced style has been queued, all it can do
          // is complete.
          {S::kMicrotaskQueuedWithForcedStyleUpdate, {S::kNoUpdate}},
      }));
  if (pending_update_state_ != new_state) {
    DCHECK_STATE_TRANSITION(&transitions, pending_update_state_, new_state);
  }
#endif
  pending_update_state_ = new_state;
}

}  // namespace blink
