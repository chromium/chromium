// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_DOCUMENT_SPECULATION_RULES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_DOCUMENT_SPECULATION_RULES_H_

#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class SpeculationRuleLoader;
class HTMLAnchorElement;

// This corresponds to the document's list of speculation rule sets.
//
// Updates are pushed asynchronously.
class CORE_EXPORT DocumentSpeculationRules
    : public GarbageCollected<DocumentSpeculationRules>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];

  static DocumentSpeculationRules& From(Document&);
  static DocumentSpeculationRules* FromIfExists(Document&);

  explicit DocumentSpeculationRules(Document&);

  const HeapVector<Member<SpeculationRuleSet>>& rule_sets() const {
    return rule_sets_;
  }

  // Appends a newly added rule set.
  void AddRuleSet(SpeculationRuleSet*);

  // Removes a rule set from consideration.
  void RemoveRuleSet(SpeculationRuleSet*);

  void AddSpeculationRuleLoader(SpeculationRuleLoader*);
  void RemoveSpeculationRuleLoader(SpeculationRuleLoader*);

  void LinkInserted(HTMLAnchorElement* link);
  void LinkRemoved(HTMLAnchorElement* link);
  void HrefAttributeChanged(HTMLAnchorElement* link,
                            const AtomicString& old_value,
                            const AtomicString& new_value);
  void ReferrerPolicyAttributeChanged(HTMLAnchorElement* link);
  void RelAttributeChanged(HTMLAnchorElement* link);
  void DocumentReferrerPolicyChanged();
  void DocumentBaseURLChanged();
  void LinkMatchedSelectorsUpdated(HTMLAnchorElement* link);
  void LinkGainedOrLostComputedStyle(HTMLAnchorElement* link);
  void DocumentStyleUpdated();

  const HeapVector<Member<StyleRule>>& selectors() { return selectors_; }

  void Trace(Visitor*) const override;

 private:
  // Retrieves a valid proxy to the speculation host in the browser.
  // May be null if the execution context does not exist.
  mojom::blink::SpeculationHost* GetHost();

  // Requests a future call to UpdateSpeculationCandidates, if none is yet
  // scheduled.
  void QueueUpdateSpeculationCandidates();

  // Pushes the current speculation candidates to the browser, immediately.
  void UpdateSpeculationCandidates();

  // Appends all candidates populated from links in the document (based on
  // document rules in all the rule sets).
  void AddLinkBasedSpeculationCandidates(
      Vector<mojom::blink::SpeculationCandidatePtr>& candidates);

  // Initializes |link_map_| with all links in the document by traversing
  // through the document in shadow-including tree order.
  void InitializeIfNecessary();

  // Helper methods to modify |link_map_|.
  void AddLink(HTMLAnchorElement* link);
  void RemoveLink(HTMLAnchorElement* link);
  void InvalidateLink(HTMLAnchorElement* link);
  void InvalidateAllLinks();

  // Populates |selectors_| and notifies the StyleEngine.
  void UpdateSelectors();

  // Tracks the state of a pending update of speculation candidates
  // (UpdateSpeculationCandidates); and whether it requires style to be clean.
  enum class PendingUpdateState {
    // There is no update queued (either as a microtask or after the next style
    // update).
    kNoUpdatePending,
    // There is a microtask queued to perform an update. A style update will
    // not run UpdateSpeculationCandidates in this state.
    kUpdatePending,
    // An update will be performed after the next style update. We should
    // never reach this state unless there are 'selector_matches' predicates
    // present. There will be no microtask queued to perform an update in this
    // state.
    kUpdateWithCleanStylePending
  };
  void SetPendingUpdateState(PendingUpdateState state);

  // Checks the RuntimeEnabledFeature to see if the feature is enabled. If the
  // feature is found to be enabled once, it is considered to be enabled for the
  // rest of the document's lifetime.
  bool SelectorMatchesEnabled();

  HeapVector<Member<SpeculationRuleSet>> rule_sets_;
  HeapMojoRemote<mojom::blink::SpeculationHost> host_;
  HeapHashSet<Member<SpeculationRuleLoader>> speculation_rule_loaders_;

  // The following data structures together keep track of all the links in
  // the document. |matched_links_| contains links that match at least one
  // document rule, and also caches a list of speculation candidates created for
  // that link. |unmatched_links_| are links that are known to not match any
  // document rules. |pending_links_| are links that haven't been matched
  // against all the document rules yet.
  // TODO(crbug.com/1371522): Consider removing |unmatched_links_| and
  // re-traverse the document to find all links when a new ruleset is
  // added/removed.
  HeapHashMap<Member<HTMLAnchorElement>,
              Vector<mojom::blink::SpeculationCandidatePtr>>
      matched_links_;
  HeapHashSet<Member<HTMLAnchorElement>> unmatched_links_;
  HeapHashSet<Member<HTMLAnchorElement>> pending_links_;

  // Collects every CSS selector from every CSS selector document rule predicate
  // in this document's speculation rules.
  HeapVector<Member<StyleRule>> selectors_;

  bool initialized_ = false;
  bool sent_is_part_of_no_vary_search_trial_ = false;
  bool was_selector_matches_enabled_ = false;
  PendingUpdateState pending_update_state_ =
      PendingUpdateState::kNoUpdatePending;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_DOCUMENT_SPECULATION_RULES_H_
