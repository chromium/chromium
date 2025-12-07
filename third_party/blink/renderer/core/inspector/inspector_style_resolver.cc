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
#include "third_party/blink/renderer/core/view_transition/view_transition_pseudo_element_base.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

InspectorStyleResolver::InspectorStyleResolver(
    Element* element,
    PseudoId element_pseudo_id,
    const AtomicString& pseudo_argument)
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

  matched_rules_ = style_resolver.PseudoCSSRulesForElement(
      element_, element_pseudo_id, pseudo_argument,
      StyleResolver::kAllCSSRules);

  // At this point, the pseudo-element id for scroll marker groups has been
  // translated to the external id, but the pseudo-element ids for scroll
  // buttons are still one of the internal ids, so we don't account for these
  // ids in the following if statement.
  DCHECK(element_pseudo_id != kPseudoIdScrollButton);
  DCHECK(element_pseudo_id != kPseudoIdScrollMarkerGroupBefore &&
         element_pseudo_id != kPseudoIdScrollMarkerGroupAfter);

  // Skip only if the pseudo-element is not tree-abiding.
  // ::placeholder and ::file-selector-button are treated as regular elements
  // and hence don't need to be included here.
  if (element_pseudo_id &&
      !(element_pseudo_id == kPseudoIdCheckMark ||
        element_pseudo_id == kPseudoIdBefore ||
        element_pseudo_id == kPseudoIdAfter ||
        element_pseudo_id == kPseudoIdPickerIcon ||
        element_pseudo_id == kPseudoIdInterestHint ||
        element_pseudo_id == kPseudoIdMarker ||
        element_pseudo_id == kPseudoIdBackdrop ||
        element_pseudo_id == kPseudoIdColumn ||
        element_pseudo_id == kPseudoIdScrollMarker ||
        element_pseudo_id == kPseudoIdScrollMarkerGroup ||
        element_pseudo_id == kPseudoIdScrollButtonBlockStart ||
        element_pseudo_id == kPseudoIdScrollButtonInlineStart ||
        element_pseudo_id == kPseudoIdScrollButtonInlineEnd ||
        element_pseudo_id == kPseudoIdScrollButtonBlockEnd)) {
    return;
  }

  const bool has_active_view_transition =
      !!ViewTransitionUtils::GetTransition(*element);

  for (PseudoId pseudo_id = kFirstPublicPseudoId;
       pseudo_id < kAfterLastInternalPseudoId;
       pseudo_id = static_cast<PseudoId>(pseudo_id + 1)) {
    if (!PseudoElement::IsWebExposed(pseudo_id, element_))
      continue;

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

    if (auto* view_transition_pseudo =
            DynamicTo<ViewTransitionPseudoElementBase>(
                element_->GetPseudoElement(kPseudoIdViewTransition))) {
      for (const auto& tag : view_transition_pseudo->GetViewTransitionNames()) {
        AddPseudoElementRules(pseudo_id, tag);
      }
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
          !IsHighlightPseudoElement(pseudo_id)) {
        continue;
      }

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
    const AtomicString& pseudo_argument) {
  StyleResolver& style_resolver = element_->GetDocument().GetStyleResolver();
  // If the pseudo-element doesn't exist, exclude UA rules to avoid cluttering
  // all elements.
  unsigned rules_to_include =
      element_->GetStyledPseudoElement(pseudo_id, pseudo_argument)
          ? StyleResolver::kAllCSSRules
          : StyleResolver::kAllButUACSSRules;
  RuleIndexList* matched_rules = style_resolver.PseudoCSSRulesForElement(
      element_, pseudo_id, pseudo_argument, rules_to_include);
  if (matched_rules && matched_rules->size()) {
    InspectorCSSMatchedRules* match =
        MakeGarbageCollected<InspectorCSSMatchedRules>();
    match->element = element_;
    match->matched_rules = matched_rules;
    match->pseudo_id = pseudo_id;
    match->pseudo_argument = pseudo_argument;
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
