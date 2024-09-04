// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_style_resolver.h"

#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

InspectorStyleResolver::InspectorStyleResolver(
    Element* element,
    PseudoId element_pseudo_id,
    const AtomicString& view_transition_name)
    : element_(element) {
  DCHECK(element_);

  // Update style and layout tree for collecting an up-to-date set of rules
  // and animations.
  element_->GetDocument().UpdateStyleAndLayoutTreeForElement(
      element_, DocumentUpdateReason::kInspector);

  // FIXME: It's really gross for the inspector to reach in and access
  // StyleResolver directly here. We need to provide the Inspector better APIs
  // to get this information without grabbing at internal style classes!
  StyleResolver& style_resolver = element_->GetDocument().GetStyleResolver();

  DCHECK(!IsTransitionPseudoElement(element_pseudo_id) ||
         element_->IsDocumentElement());
  matched_rules_ = style_resolver.PseudoCSSRulesForElement(
      element_, element_pseudo_id, view_transition_name,
      StyleResolver::kAllCSSRules);

  // Skip only if the pseudo element is not tree-abiding.
  // ::placeholder and ::file-selector-button are treated as regular elements
  // and hence don't need to be included here.
  if (element_pseudo_id && !(element_pseudo_id == kPseudoIdBefore ||
                             element_pseudo_id == kPseudoIdAfter ||
                             element_pseudo_id == kPseudoIdMarker ||
                             element_pseudo_id == kPseudoIdBackdrop)) {
    return;
  }

  const bool has_active_view_transition =
      element_->IsDocumentElement() &&
      !element_->GetDocument().GetStyleEngine().ViewTransitionTags().empty();
  for (PseudoId pseudo_id = kFirstPublicPseudoId;
       pseudo_id < kAfterLastInternalPseudoId;
       pseudo_id = static_cast<PseudoId>(pseudo_id + 1)) {
    if (!PseudoElement::IsWebExposed(pseudo_id, element_))
      continue;

    // The ::view-transition* pseudo elements are only generated for the root
    // element.
    if (IsTransitionPseudoElement(pseudo_id) && !has_active_view_transition) {
      continue;
    }

    const bool has_view_transition_names =
        IsTransitionPseudoElement(pseudo_id) &&
        PseudoElementHasArguments(pseudo_id);
    if (!has_view_transition_names) {
      AddPseudoElementRules(pseudo_id, g_null_atom);
      continue;
    }

    for (const auto& tag :
         element_->GetDocument().GetStyleEngine().ViewTransitionTags()) {
      AddPseudoElementRules(pseudo_id, tag);
    }
  }

  // Parent rules.
  Element* parent_element =
      element_pseudo_id ? element : FlatTreeTraversal::ParentElement(*element);
  while (parent_element) {
    RuleIndexList* parent_matched_rules = style_resolver.CssRulesForElement(
        parent_element, StyleResolver::kAllCSSRules);
    InspectorCSSMatchedRules* match =
        MakeGarbageCollected<InspectorCSSMatchedRules>();
    match->element = parent_element;
    match->matched_rules = parent_matched_rules;
    match->pseudo_id = kPseudoIdNone;
    parent_rules_.push_back(match);

    InspectorCSSMatchedPseudoElements* matched_pseudo_elements =
        MakeGarbageCollected<InspectorCSSMatchedPseudoElements>();
    matched_pseudo_elements->element = parent_element;

    for (PseudoId pseudo_id = kFirstPublicPseudoId;
         pseudo_id < kAfterLastInternalPseudoId;
         pseudo_id = static_cast<PseudoId>(pseudo_id + 1)) {
      // Only highlight pseudos can be inherited.
      if (!PseudoElement::IsWebExposed(pseudo_id, element_) ||
          !UsesHighlightPseudoInheritance(pseudo_id))
        continue;

      RuleIndexList* matched_rules = style_resolver.PseudoCSSRulesForElement(
          parent_element, pseudo_id, g_null_atom,
          StyleResolver::kAllButUACSSRules);
      if (matched_rules && matched_rules->size()) {
        InspectorCSSMatchedRules* pseudo_match =
            MakeGarbageCollected<InspectorCSSMatchedRules>();
        pseudo_match->element = parent_element;
        pseudo_match->matched_rules = matched_rules;
        pseudo_match->pseudo_id = pseudo_id;

        matched_pseudo_elements->pseudo_element_rules.push_back(pseudo_match);
      }
    }

    parent_pseudo_element_rules_.push_back(matched_pseudo_elements);
    parent_element = FlatTreeTraversal::ParentElement(*parent_element);
  }
}

void InspectorStyleResolver::AddPseudoElementRules(
    PseudoId pseudo_id,
    const AtomicString& view_transition_name) {
  StyleResolver& style_resolver = element_->GetDocument().GetStyleResolver();
  // If the pseudo-element doesn't exist, exclude UA rules to avoid cluttering
  // all elements.
  unsigned rules_to_include =
      element_->GetStyledPseudoElement(pseudo_id, view_transition_name)
          ? StyleResolver::kAllCSSRules
          : StyleResolver::kAllButUACSSRules;
  RuleIndexList* matched_rules = style_resolver.PseudoCSSRulesForElement(
      element_, pseudo_id, view_transition_name, rules_to_include);
  if (matched_rules && matched_rules->size()) {
    InspectorCSSMatchedRules* match =
        MakeGarbageCollected<InspectorCSSMatchedRules>();
    match->element = element_;
    match->matched_rules = matched_rules;
    match->pseudo_id = pseudo_id;
    match->view_transition_name = view_transition_name;
    pseudo_element_rules_.push_back(match);
  }
}

RuleIndexList* InspectorStyleResolver::MatchedRules() const {
  return matched_rules_;
}

HeapVector<Member<InspectorCSSMatchedRules>>
InspectorStyleResolver::PseudoElementRules() {
  return pseudo_element_rules_;
}

HeapVector<Member<InspectorCSSMatchedRules>>
InspectorStyleResolver::ParentRules() {
  return parent_rules_;
}

HeapVector<Member<InspectorCSSMatchedPseudoElements>>
InspectorStyleResolver::ParentPseudoElementRules() {
  return parent_pseudo_element_rules_;
}

}  // namespace blink
