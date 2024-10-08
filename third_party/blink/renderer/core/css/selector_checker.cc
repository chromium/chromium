/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/selector_checker.h"

#include "base/auto_reset.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_argument_context.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_cache_scope.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/part_names.h"
#include "third_party/blink/renderer/core/css/post_style_update_scope.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_scope_data.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/nth_index_cache.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/dom/scroll_marker_pseudo_element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_field_set_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_details_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_permission_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/media/html_audio_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/view_transition/view_transition.h"
#include "third_party/blink/renderer/core/view_transition/view_transition_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

static bool IsFrameFocused(const Element& element) {
  return element.GetDocument().GetFrame() && element.GetDocument()
                                                 .GetFrame()
                                                 ->Selection()
                                                 .FrameIsFocusedAndActive();
}

static bool MatchesSpatialNavigationFocusPseudoClass(const Element& element) {
  auto* option_element = DynamicTo<HTMLOptionElement>(element);
  return option_element && option_element->SpatialNavigationFocused() &&
         IsFrameFocused(element);
}

static bool MatchesHasDatalistPseudoClass(const Element& element) {
  auto* html_input_element = DynamicTo<HTMLInputElement>(element);
  return html_input_element && html_input_element->DataList();
}

static bool MatchesListBoxPseudoClass(const Element& element) {
  auto* html_select_element = DynamicTo<HTMLSelectElement>(element);
  return html_select_element && !html_select_element->UsesMenuList();
}

static bool MatchesMultiSelectFocusPseudoClass(const Element& element) {
  auto* option_element = DynamicTo<HTMLOptionElement>(element);
  return option_element && option_element->IsMultiSelectFocused() &&
         IsFrameFocused(element);
}

static bool MatchesTagName(const Element& element,
                           const QualifiedName& tag_q_name) {
  if (tag_q_name == AnyQName()) {
    return true;
  }
  const AtomicString& local_name = tag_q_name.LocalName();
  if (local_name != CSSSelector::UniversalSelectorAtom() &&
      local_name != element.localName()) {
    if (element.IsHTMLElement() || !IsA<HTMLDocument>(element.GetDocument())) {
      return false;
    }
    // Non-html elements in html documents are normalized to their camel-cased
    // version during parsing if applicable. Yet, type selectors are lower-cased
    // for selectors in html documents. Compare the upper case converted names
    // instead to allow matching SVG elements like foreignObject.
    if (element.TagQName().LocalNameUpper() != tag_q_name.LocalNameUpper()) {
      return false;
    }
  }
  const AtomicString& namespace_uri = tag_q_name.NamespaceURI();
  return namespace_uri == g_star_atom ||
         namespace_uri == element.namespaceURI();
}

static Element* ParentElement(
    const SelectorChecker::SelectorCheckingContext& context) {
  // - If context.scope is a shadow root, we should walk up to its shadow host.
  // - If context.scope is some element in some shadow tree and querySelector
  //   initialized the context, e.g. shadowRoot.querySelector(':host *'),
  //   (a) context.element has the same treescope as context.scope, need to walk
  //       up to its shadow host.
  //   (b) Otherwise, should not walk up from a shadow root to a shadow host.
  if (context.scope &&
      (context.scope == context.element->ContainingShadowRoot() ||
       context.scope->GetTreeScope() == context.element->GetTreeScope())) {
    return context.element->ParentOrShadowHostElement();
  }
  return context.element->parentElement();
}

// If context has scope, return slot that matches the scope, otherwise return
// the assigned slot for scope-less matching of ::slotted pseudo element.
static const HTMLSlotElement* FindSlotElementInScope(
    const SelectorChecker::SelectorCheckingContext& context) {
  if (!context.scope) {
    return context.element->AssignedSlot();
  }

  for (const HTMLSlotElement* slot = context.element->AssignedSlot(); slot;
       slot = slot->AssignedSlot()) {
    if (slot->GetTreeScope() == context.scope->GetTreeScope()) {
      return slot;
    }
  }
  return nullptr;
}

static inline bool NextSelectorExceedsScope(
    const SelectorChecker::SelectorCheckingContext& context) {
  if (context.scope && context.scope->IsInShadowTree()) {
    return context.element == context.scope->OwnerShadowHost();
  }

  return false;
}

static bool ShouldMatchHoverOrActive(
    const SelectorChecker::SelectorCheckingContext& context) {
  // If we're in quirks mode, then :hover and :active should never match anchors
  // with no href and *:hover and *:active should not match anything. This is
  // specified in https://quirks.spec.whatwg.org/#the-:active-and-:hover-quirk
  if (!context.element->GetDocument().InQuirksMode()) {
    return true;
  }
  if (context.is_sub_selector) {
    return true;
  }
  if (context.element->IsLink()) {
    return true;
  }
  const CSSSelector* selector = context.selector;
  while (selector->Relation() == CSSSelector::kSubSelector &&
         selector->NextSimpleSelector()) {
    selector = selector->NextSimpleSelector();
    if (selector->Match() != CSSSelector::kPseudoClass) {
      return true;
    }
    if (selector->GetPseudoType() != CSSSelector::kPseudoHover &&
        selector->GetPseudoType() != CSSSelector::kPseudoActive) {
      return true;
    }
  }
  return false;
}

static bool Impacts(const SelectorChecker::SelectorCheckingContext& context,
                    SelectorChecker::Impact impact) {
  return static_cast<int>(context.impact) & static_cast<int>(impact);
}

static bool ImpactsSubject(
    const SelectorChecker::SelectorCheckingContext& context) {
  return Impacts(context, SelectorChecker::Impact::kSubject);
}

static bool ImpactsNonSubject(
    const SelectorChecker::SelectorCheckingContext& context) {
  return Impacts(context, SelectorChecker::Impact::kNonSubject);
}

static bool IsFirstChild(const Element& element) {
  return !ElementTraversal::PreviousSibling(element);
}

static bool IsLastChild(const Element& element) {
  return !ElementTraversal::NextSibling(element);
}

static bool IsFirstOfType(const Element& element, const QualifiedName& type) {
  return !ElementTraversal::PreviousSibling(element, HasTagName(type));
}

static bool IsLastOfType(const Element& element, const QualifiedName& type) {
  return !ElementTraversal::NextSibling(element, HasTagName(type));
}

static void DisallowMatchVisited(
    SelectorChecker::SelectorCheckingContext& context) {
  context.had_match_visited |= context.match_visited;
  context.match_visited = false;
}

bool SelectorChecker::Match(const SelectorCheckingContext& context,
                            MatchResult& result) const {
  DCHECK(context.selector);
  DCHECK(!context.had_match_visited);
#if DCHECK_IS_ON()
  DCHECK(!inside_match_) << "Do not re-enter Match: use MatchSelector instead";
  base::AutoReset<bool> reset_inside_match(&inside_match_, true);
#endif  // DCHECK_IS_ON()

  if (context.vtt_originating_element) [[unlikely]] {
    // A kUAShadow combinator is required for VTT matching.
    if (context.selector->IsLastInComplexSelector()) {
      return false;
    }
  }
  return MatchSelector(context, result) == kSelectorMatches;
}

// Recursive check of selectors and combinators
// It can return 4 different values:
// * SelectorMatches          - the selector matches the element e
// * SelectorFailsLocally     - the selector fails for the element e
// * SelectorFailsAllSiblings - the selector fails for e and any sibling of e
// * SelectorFailsCompletely  - the selector fails for e and any sibling or
//   ancestor of e
SelectorChecker::MatchStatus SelectorChecker::MatchSelector(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  SubResult sub_result(result);
  bool is_covered_by_bucketing =
      context.selector->IsCoveredByBucketing() &&
      !context
           .is_sub_selector &&  // Don't trust bucketing in sub-selectors; we
                                // may be in a child selector (a nested rule).
      !context.scope;           // May be featureless; see CheckOne().
#if DCHECK_IS_ON()
  SubResult dummy_result(result);
  if (is_covered_by_bucketing) {
    DCHECK(CheckOne(context, dummy_result))
        << context.selector->SimpleSelectorTextForDebug()
        << " unexpectedly didn't match element " << context.element;
    DCHECK_EQ(0, dummy_result.flags);
  }
#endif
  if (!is_covered_by_bucketing && !CheckOne(context, sub_result)) {
    return kSelectorFailsLocally;
  }

  if (sub_result.dynamic_pseudo != kPseudoIdNone) {
    result.dynamic_pseudo = sub_result.dynamic_pseudo;
    result.custom_highlight_name = std::move(sub_result.custom_highlight_name);
  }

  if (context.selector->IsLastInComplexSelector()) {
    return kSelectorMatches;
  }

  switch (context.selector->Relation()) {
    case CSSSelector::kSubSelector:
      return MatchForSubSelector(context, result);
    case CSSSelector::kScopeActivation:
      // The kScopeActivation relation type does not change the current element
      // being matched: it behaves like a special kSubSelector in this context.
      return MatchForScopeActivation(context, result);
    default: {
      if (NextSelectorExceedsScope(context)) {
        return kSelectorFailsCompletely;
      }

      if (context.pseudo_id != kPseudoIdNone &&
          context.pseudo_id != result.dynamic_pseudo) {
        return kSelectorFailsCompletely;
      }

      base::AutoReset<PseudoId> dynamic_pseudo_scope(&result.dynamic_pseudo,
                                                     kPseudoIdNone);
      return MatchForRelation(context, result);
    }
  }
}

static inline SelectorChecker::SelectorCheckingContext
PrepareNextContextForRelation(
    const SelectorChecker::SelectorCheckingContext& context) {
  SelectorChecker::SelectorCheckingContext next_context(context);
  DCHECK(context.selector->NextSimpleSelector());
  next_context.selector = context.selector->NextSimpleSelector();
  return next_context;
}

SelectorChecker::MatchStatus SelectorChecker::MatchForSubSelector(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  SelectorCheckingContext next_context = PrepareNextContextForRelation(context);

  PseudoId dynamic_pseudo = result.dynamic_pseudo;
  next_context.has_scrollbar_pseudo =
      dynamic_pseudo != kPseudoIdNone &&
      (scrollbar_ || dynamic_pseudo == kPseudoIdScrollbarCorner ||
       dynamic_pseudo == kPseudoIdResizer);

  // If we saw a pseudo element while not computing pseudo element styles, do
  // not try to match any simple selectors after the pseudo element as those
  // selectors need to match the actual pseudo element.
  //
  // Examples:
  //
  // span::selection:window-inactive {}
  // #id::before:initial {}
  // .class::before:hover {}
  //
  // In all of those cases we need to skip matching the pseudo classes after the
  // pseudo element on the originating element.
  // But we allow the ::column::scroll-marker case to keep matching when we are
  // at ::column part.
  if (context.in_rightmost_compound && dynamic_pseudo != kPseudoIdNone &&
      context.pseudo_id == kPseudoIdNone && dynamic_pseudo != kPseudoIdColumn) {
    // We are in the rightmost compound and have matched a pseudo element
    // (dynamic_pseudo is not kPseudoIdNone), which means we are looking at
    // pseudo classes after the pseudo element. We are also matching the
    // originating element (context.pseudo_id is kPseudoIdnone), which means we
    // are matching for tracking the existence of such pseudo elements which
    // results in SetHasPseudoElementStyle() on the originating element's
    // ComputedStyle.
    if (!next_context.has_scrollbar_pseudo &&
        dynamic_pseudo == kPseudoIdScrollbar) {
      // Fail ::-webkit-scrollbar:hover because HasPseudoElementStyle for
      // scrollbars will remove the native scrollbar. Having only
      // ::-webkit-scrollbar rules that have pseudo class modifiers will end up
      // with not adding a custom scrollbar which means we end up with no
      // scrollbar.
      return kSelectorFailsCompletely;
    }
    // This means we will end up with false positives for pseudo elements like
    // ::before with only pseudo class modifiers where we end up trying to
    // create the pseudo element but end up not doing it because we have no
    // matching rules without modifiers. That is also already the case if you
    // have ::before elements without content properties.
    return kSelectorMatches;
  }

  next_context.previously_matched_pseudo_element = dynamic_pseudo;
  next_context.is_sub_selector = true;
  return MatchSelector(next_context, result);
}

SelectorChecker::MatchStatus SelectorChecker::MatchForScopeActivation(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  SelectorCheckingContext next_context = PrepareNextContextForRelation(context);
  next_context.is_sub_selector = true;

  if (context.style_scope) {
    const StyleScopeActivations& activations =
        EnsureActivations(context, *context.style_scope);
    if (ImpactsSubject(context)) {
      // For e.g. @scope (:hover) { :scope { ...} },
      // the StyleScopeActivations may have stored MatchFlags that we
      // need to propagate. However, this is only needed if :scope
      // appears in the subject position, since MatchFlags are only
      // used for subject invalidation. Non-subject flags are set on
      // Elements directly (e.g. SetChildrenOrSiblingsAffectedByHover)
      result.flags |= activations.match_flags;
    }
    if (activations.vector.empty()) {
      return kSelectorFailsCompletely;
    }
    for (const StyleScopeActivation& activation : activations.vector) {
      next_context.match_visited = context.match_visited;
      next_context.impact = context.impact;
      next_context.style_scope = nullptr;
      next_context.scope = activation.root;
      if (MatchSelector(next_context, result) == kSelectorMatches) {
        result.proximity = activation.proximity;
        return kSelectorMatches;
      }
    }
    return kSelectorFailsLocally;
  }

  return MatchSelector(next_context, result);
}

SelectorChecker::MatchStatus SelectorChecker::MatchForRelation(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  SelectorCheckingContext next_context = PrepareNextContextForRelation(context);

  CSSSelector::RelationType relation = context.selector->Relation();

  // Disable :visited matching when we see the first link or try to match
  // anything else than an ancestor.
  if ((!context.is_sub_selector || context.in_nested_complex_selector) &&
      (context.element->IsLink() || (relation != CSSSelector::kDescendant &&
                                     relation != CSSSelector::kChild))) {
    DisallowMatchVisited(next_context);
  }

  next_context.in_rightmost_compound = false;
  next_context.impact = Impact::kNonSubject;
  next_context.is_sub_selector = false;
  next_context.previous_element = context.element;
  next_context.pseudo_id = kPseudoIdNone;

  switch (relation) {
    case CSSSelector::kRelativeDescendant:
      DCHECK(result.has_argument_leftmost_compound_matches);
      result.has_argument_leftmost_compound_matches->push_back(context.element);
      [[fallthrough]];
    case CSSSelector::kDescendant:
      if (next_context.selector->GetPseudoType() == CSSSelector::kPseudoScope) {
        if (next_context.selector->IsLastInComplexSelector()) {
          if (context.scope && context.scope->IsDocumentFragment()) {
            return kSelectorMatches;
          }
        }
      }
      for (next_context.element = ParentElement(next_context);
           next_context.element;
           next_context.element = ParentElement(next_context)) {
        MatchStatus match = MatchSelector(next_context, result);
        if (match == kSelectorMatches || match == kSelectorFailsCompletely) {
          return match;
        }
        if (NextSelectorExceedsScope(next_context)) {
          return kSelectorFailsCompletely;
        }
        if (next_context.element->IsLink()) {
          DisallowMatchVisited(next_context);
        }
      }
      return kSelectorFailsCompletely;
    case CSSSelector::kRelativeChild:
      DCHECK(result.has_argument_leftmost_compound_matches);
      result.has_argument_leftmost_compound_matches->push_back(context.element);
      [[fallthrough]];
    case CSSSelector::kChild: {
      next_context.element = ParentElement(next_context);
      if (!next_context.element) {
        return kSelectorFailsCompletely;
      }
      return MatchSelector(next_context, result);
    }
    case CSSSelector::kRelativeDirectAdjacent:
      DCHECK(result.has_argument_leftmost_compound_matches);
      result.has_argument_leftmost_compound_matches->push_back(context.element);
      [[fallthrough]];
    case CSSSelector::kDirectAdjacent:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent =
                context.element->ParentElementOrShadowRoot()) {
          parent->SetChildrenAffectedByDirectAdjacentRules();
        }
      }
      next_context.element =
          ElementTraversal::PreviousSibling(*context.element);
      if (!next_context.element) {
        return kSelectorFailsAllSiblings;
      }
      return MatchSelector(next_context, result);
    case CSSSelector::kRelativeIndirectAdjacent:
      DCHECK(result.has_argument_leftmost_compound_matches);
      result.has_argument_leftmost_compound_matches->push_back(context.element);
      [[fallthrough]];
    case CSSSelector::kIndirectAdjacent:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent =
                context.element->ParentElementOrShadowRoot()) {
          parent->SetChildrenAffectedByIndirectAdjacentRules();
        }
      }
      next_context.element =
          ElementTraversal::PreviousSibling(*context.element);
      for (; next_context.element;
           next_context.element =
               ElementTraversal::PreviousSibling(*next_context.element)) {
        MatchStatus match = MatchSelector(next_context, result);
        if (match == kSelectorMatches || match == kSelectorFailsAllSiblings ||
            match == kSelectorFailsCompletely) {
          return match;
        }
      }
      return kSelectorFailsAllSiblings;

    case CSSSelector::kUAShadow: {
      // Note: context.scope should be non-null unless we're checking user or
      // UA origin rules, or VTT rules.  (We could CHECK() this if it weren't
      // for the user rules part.)

      // If we're in the same tree-scope as the scoping element, then following
      // a kUAShadow combinator would escape that and thus the scope.
      if (context.scope && context.scope->OwnerShadowHost() &&
          context.scope->OwnerShadowHost()->GetTreeScope() ==
              context.element->GetTreeScope()) {
        return kSelectorFailsCompletely;
      }

      Element* shadow_host = context.element->OwnerShadowHost();
      if (!shadow_host) {
        return kSelectorFailsCompletely;
      }
      // Match against featureless-like Element described by spec:
      // https://w3c.github.io/webvtt/#obtaining-css-boxes
      if (context.vtt_originating_element) {
        shadow_host = context.vtt_originating_element;
      }
      next_context.element = shadow_host;

      // If this is the *last* time that we cross shadow scopes, then make
      // sure that we've crossed *enough* shadow scopes.  This prevents
      // ::pseudo1 from matching in a scope where it shouldn't match but where
      // ::part(p)::pseudo1 or where ::pseudo2::pseudo1 should match.
      if (context.scope &&
          context.scope->GetTreeScope() !=
              next_context.element->GetTreeScope() &&
          !next_context.selector->CrossesTreeScopes()) {
        return kSelectorFailsCompletely;
      }

      return MatchSelector(next_context, result);
    }

    case CSSSelector::kShadowSlot: {
      if (ToHTMLSlotElementIfSupportsAssignmentOrNull(*context.element)) {
        return kSelectorFailsCompletely;
      }
      const HTMLSlotElement* slot = FindSlotElementInScope(context);
      if (!slot) {
        return kSelectorFailsCompletely;
      }

      next_context.element = const_cast<HTMLSlotElement*>(slot);
      return MatchSelector(next_context, result);
    }

    case CSSSelector::kShadowPart:
      // We ascend through ancestor shadow host elements until we reach the host
      // in the TreeScope associated with the style rule. We then match against
      // that host.
      while (true) {
        next_context.element = next_context.element->OwnerShadowHost();
        if (!next_context.element) {
          return kSelectorFailsCompletely;
        }

        // Generally a ::part() rule needs to be in the host’s tree scope, but
        // if (and only if) we are preceded by :host or :host(), then the rule
        // could also be in the same scope as the subject.
        TreeScope& host_tree_scope =
            next_context.selector->IsHostPseudoClass()
                ? *context.scope->GetTreeScope().ParentTreeScope()
                : context.scope->GetTreeScope();

        if (next_context.element->GetTreeScope() == host_tree_scope) {
          return MatchSelector(next_context, result);
        }
      }
    case CSSSelector::kSubSelector:
    case CSSSelector::kScopeActivation:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return kSelectorFailsCompletely;
}

static bool AttributeValueMatches(const Attribute& attribute_item,
                                  CSSSelector::MatchType match,
                                  const AtomicString& selector_value,
                                  bool case_insensitive) {
  const AtomicString& value = attribute_item.Value();
  switch (match) {
    case CSSSelector::kAttributeExact:
      // Comparing AtomicStrings for equality is very cheap,
      // so even for a case-insensitive match, we test that first.
      return selector_value == value ||
             (case_insensitive &&
              EqualIgnoringASCIICase(selector_value, value));
    case CSSSelector::kAttributeSet:
      return true;
    case CSSSelector::kAttributeList: {
      // Ignore empty selectors or selectors containing HTML spaces
      if (selector_value.empty() ||
          selector_value.Find(&IsHTMLSpace<UChar>) != kNotFound) {
        return false;
      }

      unsigned start_search_at = 0;
      while (true) {
        wtf_size_t found_pos = value.Find(
            selector_value, start_search_at,
            case_insensitive ? kTextCaseASCIIInsensitive : kTextCaseSensitive);
        if (found_pos == kNotFound) {
          return false;
        }
        if (!found_pos || IsHTMLSpace<UChar>(value[found_pos - 1])) {
          unsigned end_str = found_pos + selector_value.length();
          if (end_str == value.length() || IsHTMLSpace<UChar>(value[end_str])) {
            break;  // We found a match.
          }
        }

        // No match. Keep looking.
        start_search_at = found_pos + 1;
      }
      return true;
    }
    case CSSSelector::kAttributeContain:
      if (selector_value.empty()) {
        return false;
      }
      return value.Contains(selector_value, case_insensitive
                                                ? kTextCaseASCIIInsensitive
                                                : kTextCaseSensitive);
    case CSSSelector::kAttributeBegin:
      if (selector_value.empty()) {
        return false;
      }
      return value.StartsWith(selector_value, case_insensitive
                                                  ? kTextCaseASCIIInsensitive
                                                  : kTextCaseSensitive);
    case CSSSelector::kAttributeEnd:
      if (selector_value.empty()) {
        return false;
      }
      return value.EndsWith(selector_value, case_insensitive
                                                ? kTextCaseASCIIInsensitive
                                                : kTextCaseSensitive);
    case CSSSelector::kAttributeHyphen:
      if (value.length() < selector_value.length()) {
        return false;
      }
      if (!value.StartsWith(selector_value, case_insensitive
                                                ? kTextCaseASCIIInsensitive
                                                : kTextCaseSensitive)) {
        return false;
      }
      // It they start the same, check for exact match or following '-':
      if (value.length() != selector_value.length() &&
          value[selector_value.length()] != '-') {
        return false;
      }
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool AnyAttributeMatches(Element& element,
                                CSSSelector::MatchType match,
                                const CSSSelector& selector) {
  const QualifiedName& selector_attr = selector.Attribute();
  // Should not be possible from the CSS grammar.
  DCHECK_NE(selector_attr.LocalName(), CSSSelector::UniversalSelectorAtom());

  // Synchronize the attribute in case it is lazy-computed.
  // Currently all lazy properties have a null namespace, so only pass
  // localName().
  element.SynchronizeAttribute(selector_attr.LocalName());

  // NOTE: For kAttributeSet, this is a bogus pointer but never used.
  const AtomicString& selector_value = selector.Value();

  // Legacy dictates that values of some attributes should be compared in
  // a case-insensitive manner regardless of whether the case insensitive
  // flag is set or not (but an explicit case sensitive flag will override
  // that, by causing LegacyCaseInsensitiveMatch() never to be set).
  const bool case_insensitive =
      selector.AttributeMatch() ==
          CSSSelector::AttributeMatchType::kCaseInsensitive ||
      (selector.LegacyCaseInsensitiveMatch() &&
       IsA<HTMLDocument>(element.GetDocument()));

  AttributeCollection attributes = element.AttributesWithoutUpdate();
  for (const auto& attribute_item : attributes) {
    if (!attribute_item.Matches(selector_attr)) {
      if (element.IsHTMLElement() ||
          !IsA<HTMLDocument>(element.GetDocument())) {
        continue;
      }
      // Non-html attributes in html documents are normalized to their camel-
      // cased version during parsing if applicable. Yet, attribute selectors
      // are lower-cased for selectors in html documents. Compare the selector
      // and the attribute local name insensitively to e.g. allow matching SVG
      // attributes like viewBox.
      //
      // NOTE: If changing this behavior, be sure to also update the bucketing
      // in ElementRuleCollector::CollectMatchingRules() accordingly.
      if (!attribute_item.MatchesCaseInsensitive(selector_attr)) {
        continue;
      }
    }

    if (AttributeValueMatches(attribute_item, match, selector_value,
                              case_insensitive)) {
      return true;
    }

    if (selector_attr.NamespaceURI() != g_star_atom) {
      return false;
    }
  }

  return false;
}

ALWAYS_INLINE bool SelectorChecker::CheckOne(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  DCHECK(context.element);
  Element& element = *context.element;
  DCHECK(context.selector);
  const CSSSelector& selector = *context.selector;

  // When considered within its own shadow trees, the shadow host is
  // featureless. Only the :host, :host(), and :host-context() pseudo-classes
  // are allowed to match it. [1]
  //
  // However, the :scope pseudo-class may also match the host if the host is the
  // scoping root. [2]
  //
  // Also, we need to descend into selectors that contain lists instead of
  // just returning false, such that :is(:host, .doesnotmatch) (see [3]),
  // or similar via nesting, is handled correctly. (This also deals with
  // :not().) Such cases will eventually recurse back into CheckOne(),
  // so should do not get false positives from doing this; the featurelessness
  // will be checked on a lower level.
  //
  // [1] https://drafts.csswg.org/css-scoping/#host-element-in-tree
  // [2] https://github.com/w3c/csswg-drafts/issues/9025
  // [3] https://drafts.csswg.org/selectors-4/#data-model
  if (context.scope && context.scope->OwnerShadowHost() == element &&
      (!selector.IsHostPseudoClass() && !selector.SelectorListOrParent() &&
       selector.GetPseudoType() != CSSSelector::kPseudoTrue &&
       selector.GetPseudoType() != CSSSelector::kPseudoScope &&
       !context.treat_shadow_host_as_normal_scope &&
       selector.Match() != CSSSelector::kPseudoElement)) {
    return false;
  }

  switch (selector.Match()) {
    case CSSSelector::kTag:
      return MatchesTagName(element, selector.TagQName());
    case CSSSelector::kClass:
      return element.HasClass() &&
             element.ClassNames().Contains(selector.Value());
    case CSSSelector::kId:
      return element.HasID() &&
             element.IdForStyleResolution() == selector.Value();

    // Attribute selectors
    case CSSSelector::kAttributeExact:
    case CSSSelector::kAttributeSet:
    case CSSSelector::kAttributeHyphen:
    case CSSSelector::kAttributeList:
    case CSSSelector::kAttributeContain:
    case CSSSelector::kAttributeBegin:
    case CSSSelector::kAttributeEnd:
      return AnyAttributeMatches(element, selector.Match(), selector);

    case CSSSelector::kPseudoClass:
      return CheckPseudoClass(context, result);
    case CSSSelector::kPseudoElement:
      return CheckPseudoElement(context, result);

    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool SelectorChecker::CheckPseudoNot(const SelectorCheckingContext& context,
                                     MatchResult& result) const {
  return !MatchesAnyInList(context, context.selector->SelectorList()->First(),
                           result);
}

bool SelectorChecker::MatchesAnyInList(const SelectorCheckingContext& context,
                                       const CSSSelector* selector_list,
                                       MatchResult& result) const {
  SelectorCheckingContext sub_context(context);
  sub_context.is_sub_selector = true;
  sub_context.in_nested_complex_selector = true;
  sub_context.pseudo_id = kPseudoIdNone;
  for (sub_context.selector = selector_list; sub_context.selector;
       sub_context.selector = CSSSelectorList::Next(*sub_context.selector)) {
    SubResult sub_result(result);
    if (MatchSelector(sub_context, sub_result) == kSelectorMatches) {
      return true;
    }
  }
  return false;
}

namespace {

Element* TraverseToParentOrShadowHost(Element* element) {
  return element->ParentOrShadowHostElement();
}

Element* TraverseToPreviousSibling(Element* element) {
  return ElementTraversal::PreviousSibling(*element);
}

inline bool CacheMatchedElementsAndReturnMatchedResultForIndirectRelation(
    Element* has_anchor_element,
    HeapVector<Member<Element>>& has_argument_leftmost_compound_matches,
    CheckPseudoHasCacheScope::Context& cache_scope_context,
    Element* (*next)(Element*)) {
  if (cache_scope_context.CacheAllowed()) {
    bool selector_matched = false;
    for (auto leftmost : has_argument_leftmost_compound_matches) {
      for (Element* has_matched_element = next(leftmost); has_matched_element;
           has_matched_element = next(has_matched_element)) {
        if (has_matched_element == has_anchor_element) {
          selector_matched = true;
        }
        uint8_t old_result =
            cache_scope_context.SetMatchedAndGetOldResult(has_matched_element);
        if (old_result == kCheckPseudoHasResultNotCached) {
          continue;
        }
        if (old_result & kCheckPseudoHasResultMatched) {
          break;
        }
      }
    }
    return selector_matched;
  }

  for (auto leftmost : has_argument_leftmost_compound_matches) {
    for (Element* has_matched_element = next(leftmost); has_matched_element;
         has_matched_element = next(has_matched_element)) {
      if (has_matched_element == has_anchor_element) {
        return true;
      }
    }
  }
  return false;
}

inline bool CacheMatchedElementsAndReturnMatchedResultForDirectRelation(
    Element* has_anchor_element,
    HeapVector<Member<Element>>& has_argument_leftmost_compound_matches,
    CheckPseudoHasCacheScope::Context& cache_scope_context,
    Element* (*next)(Element*)) {
  if (cache_scope_context.CacheAllowed()) {
    bool selector_matched = false;
    for (auto leftmost : has_argument_leftmost_compound_matches) {
      if (Element* has_matched_element = next(leftmost)) {
        cache_scope_context.SetMatchedAndGetOldResult(has_matched_element);
        if (has_matched_element == has_anchor_element) {
          selector_matched = true;
        }
      }
    }
    return selector_matched;
  }

  for (auto leftmost : has_argument_leftmost_compound_matches) {
    if (Element* has_matched_element = next(leftmost)) {
      if (has_matched_element == has_anchor_element) {
        return true;
      }
    }
  }
  return false;
}

inline bool CacheMatchedElementsAndReturnMatchedResult(
    CSSSelector::RelationType leftmost_relation,
    Element* has_anchor_element,
    HeapVector<Member<Element>>& has_argument_leftmost_compound_matches,
    CheckPseudoHasCacheScope::Context& cache_scope_context) {
  switch (leftmost_relation) {
    case CSSSelector::kRelativeDescendant:
      return CacheMatchedElementsAndReturnMatchedResultForIndirectRelation(
          has_anchor_element, has_argument_leftmost_compound_matches,
          cache_scope_context, TraverseToParentOrShadowHost);
    case CSSSelector::kRelativeChild:
      return CacheMatchedElementsAndReturnMatchedResultForDirectRelation(
          has_anchor_element, has_argument_leftmost_compound_matches,
          cache_scope_context, TraverseToParentOrShadowHost);
    case CSSSelector::kRelativeDirectAdjacent:
      return CacheMatchedElementsAndReturnMatchedResultForDirectRelation(
          has_anchor_element, has_argument_leftmost_compound_matches,
          cache_scope_context, TraverseToPreviousSibling);
    case CSSSelector::kRelativeIndirectAdjacent:
      return CacheMatchedElementsAndReturnMatchedResultForIndirectRelation(
          has_anchor_element, has_argument_leftmost_compound_matches,
          cache_scope_context, TraverseToPreviousSibling);
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

inline bool ContextForSubjectHasInMatchesArgument(
    const SelectorChecker::SelectorCheckingContext& has_checking_context) {
  return has_checking_context.element == has_checking_context.scope &&
         has_checking_context.in_rightmost_compound;
}

uint8_t SetHasAnchorElementAsCheckedAndGetOldResult(
    const SelectorChecker::SelectorCheckingContext& has_checking_context,
    CheckPseudoHasCacheScope::Context& cache_scope_context) {
  DCHECK_EQ(has_checking_context.selector->GetPseudoType(),
            CSSSelector::kPseudoHas);
  Element* has_anchor_element = has_checking_context.element;
  uint8_t previous_result = cache_scope_context.GetResult(has_anchor_element);
  if (previous_result & kCheckPseudoHasResultChecked) {
    return previous_result;
  }

  // If the selector checking context is for the subject :has() in the argument
  // of the JavaScript API 'matches()', skip to check whether the :has() anchor
  // element was already checked or not.
  if (!ContextForSubjectHasInMatchesArgument(has_checking_context) &&
      cache_scope_context.AlreadyChecked(has_anchor_element)) {
    // If the element already have cache item, set the element as checked.
    // Otherwise, skip to set to prevent increasing unnecessary cache item.
    if (previous_result != kCheckPseudoHasResultNotCached) {
      cache_scope_context.SetChecked(has_anchor_element);
    }

    // If the :has() anchor element was already checked previously, return the
    // previous result with the kCheckPseudoHasResultChecked flag set.
    return previous_result | kCheckPseudoHasResultChecked;
  }

  cache_scope_context.SetChecked(has_anchor_element);
  return previous_result;
}

void SetAffectedByHasFlagsForElementAtDepth(
    CheckPseudoHasArgumentContext& argument_context,
    Element* element,
    int depth) {
  if (depth > 0) {
    element->SetAncestorsOrAncestorSiblingsAffectedByHas();
  } else {
    element->SetSiblingsAffectedByHasFlags(
        argument_context.GetSiblingsAffectedByHasFlags());
  }
}

void SetAffectedByHasFlagsForHasAnchorElement(
    CheckPseudoHasArgumentContext& argument_context,
    Element* has_anchor_element) {
  switch (argument_context.LeftmostRelation()) {
    case CSSSelector::kRelativeChild:
    case CSSSelector::kRelativeDescendant:
      has_anchor_element->SetAncestorsOrAncestorSiblingsAffectedByHas();
      break;
    case CSSSelector::kRelativeDirectAdjacent:
    case CSSSelector::kRelativeIndirectAdjacent:
      has_anchor_element->SetSiblingsAffectedByHasFlags(
          argument_context.GetSiblingsAffectedByHasFlags());
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void SetAffectedByHasFlagsForHasAnchorSiblings(
    CheckPseudoHasArgumentContext& argument_context,
    Element* has_anchor_element) {
  if (argument_context.AdjacentDistanceLimit() == 0) {
    return;
  }
  int distance = 1;
  for (Element* sibling = ElementTraversal::NextSibling(*has_anchor_element);
       sibling && distance <= argument_context.AdjacentDistanceLimit();
       sibling = ElementTraversal::NextSibling(*sibling), distance++) {
    sibling->SetSiblingsAffectedByHasFlags(
        argument_context.GetSiblingsAffectedByHasFlags());
  }
}

void SetAffectedByHasForArgumentMatchedElement(
    CheckPseudoHasArgumentContext& argument_context,
    Element* has_anchor_element,
    Element* argument_matched_element,
    int argument_matched_depth) {
  // Iterator class to traverse siblings, ancestors and ancestor siblings of the
  // CheckPseudoHasArgumentTraversalIterator's current element until reach to
  // the :has() anchor element to set the SiblingsAffectedByHasFlags or
  // AncestorsOrAncestorSiblingsAffectedByHas flag.
  class AffectedByHasIterator {
    STACK_ALLOCATED();

   public:
    AffectedByHasIterator(CheckPseudoHasArgumentContext& argument_context,
                          Element* has_anchor_element,
                          Element* argument_matched_element,
                          int argument_matched_depth)
        : argument_context_(argument_context),
          has_anchor_element_(has_anchor_element),
          argument_matched_depth_(argument_matched_depth),
          current_depth_(argument_matched_depth),
          current_element_(argument_matched_element) {
      DCHECK_GE(current_depth_, 0);
      // affected-by flags of the matched element were already set.
      // So, this iterator traverses from the next of the matched element.
      ++*this;
    }

    Element* CurrentElement() const { return current_element_; }
    bool AtEnd() const {
      DCHECK_GE(current_depth_, 0);
      return current_element_ == has_anchor_element_;
    }
    int CurrentDepth() const { return current_depth_; }
    void operator++() {
      DCHECK(current_element_);

      if (current_depth_ == 0) {
        current_element_ = ElementTraversal::PreviousSibling(*current_element_);
        DCHECK(current_element_);
        return;
      }

      Element* previous = nullptr;
      if (NeedsTraverseSiblings() &&
          (previous = ElementTraversal::PreviousSibling(*current_element_))) {
        current_element_ = previous;
        DCHECK(current_element_);
        return;
      }

      DCHECK_GT(current_depth_, 0);
      current_depth_--;
      current_element_ = current_element_->ParentOrShadowHostElement();
      DCHECK(current_element_);
    }

   private:
    inline bool NeedsTraverseSiblings() {
      // When the current element is at the same depth of the argument selector
      // matched element, we can determine whether the sibling traversal is
      // needed or not by checking whether the rightmost combinator is an
      // adjacent combinator. When the current element is not at the same depth
      // of the argument selector matched element, we can determine whether the
      // sibling traversal is needed or not by checking whether an adjacent
      // combinator is between child or descendant combinator.
      DCHECK_LE(current_depth_, argument_matched_depth_);
      return argument_matched_depth_ == current_depth_
                 ? argument_context_.SiblingCombinatorAtRightmost()
                 : argument_context_
                       .SiblingCombinatorBetweenChildOrDescendantCombinator();
    }

    const CheckPseudoHasArgumentContext& argument_context_;
    Element* has_anchor_element_;
    const int argument_matched_depth_;
    int current_depth_;
    Element* current_element_;
  } affected_by_has_iterator(argument_context, has_anchor_element,
                             argument_matched_element, argument_matched_depth);

  // Set AncestorsOrAncestorSiblingsAffectedByHas flag on the elements at
  // upward (previous siblings, ancestors, ancestors' previous siblings) of the
  // argument matched element.
  for (; !affected_by_has_iterator.AtEnd(); ++affected_by_has_iterator) {
    SetAffectedByHasFlagsForElementAtDepth(
        argument_context, affected_by_has_iterator.CurrentElement(),
        affected_by_has_iterator.CurrentDepth());
  }
}

bool SkipCheckingHasArgument(
    CheckPseudoHasArgumentContext& context,
    CheckPseudoHasArgumentTraversalIterator& iterator) {
  // Siblings of the :has() anchor element cannot be a subject of :has()
  // argument if the argument selector has child or descendant combinator.
  if (context.DepthLimit() > 0 && iterator.CurrentDepth() == 0) {
    return true;
  }

  // The current element of the iterator cannot be a subject of :has() argument
  // if the :has() argument selector only matches on the elements at a fixed
  // depth and the current element of the iterator is not at the certain depth.
  // (e.g. For the style rule '.a:has(> .b > .c) {}', a child of '.a' or a great
  // grand child of '.a' cannot be a subject of the argument '> .b > .c'. Only
  // the grand child of '.a' can be a subject of the argument)
  if (context.DepthFixed() &&
      (iterator.CurrentDepth() != context.DepthLimit())) {
    return true;
  }

  return false;
}

void AddElementIdentifierHashesInTraversalScopeAndSetAffectedByHasFlags(
    CheckPseudoHasFastRejectFilter& fast_reject_filter,
    Element& has_anchor_element,
    CheckPseudoHasArgumentContext& argument_context,
    bool update_affected_by_has_flags) {
  for (CheckPseudoHasArgumentTraversalIterator iterator(has_anchor_element,
                                                        argument_context);
       !iterator.AtEnd(); ++iterator) {
    fast_reject_filter.AddElementIdentifierHashes(*iterator.CurrentElement());
    if (update_affected_by_has_flags) {
      SetAffectedByHasFlagsForElementAtDepth(
          argument_context, iterator.CurrentElement(), iterator.CurrentDepth());
    }
  }
}

void SetAllElementsInTraversalScopeAsChecked(
    Element* has_anchor_element,
    CheckPseudoHasArgumentContext& argument_context,
    CheckPseudoHasCacheScope::Context& cache_scope_context) {
  // Find last element and last depth of the argument traversal iterator.
  Element* last_element = has_anchor_element;
  int last_depth = 0;
  if (argument_context.AdjacentDistanceLimit() > 0) {
    last_element = ElementTraversal::NextSibling(*last_element);
  }
  if (last_element) {
    if (argument_context.DepthLimit() > 0) {
      last_element = ElementTraversal::FirstChild(*last_element);
      last_depth = 1;
    }
  }
  if (!last_element) {
    return;
  }
  cache_scope_context.SetAllTraversedElementsAsChecked(last_element,
                                                       last_depth);
}

enum EarlyBreakOnHasArgumentChecking {
  kBreakEarlyAndReturnAsMatched,
  kBreakEarlyAndMoveToNextArgument,
  kNoEarlyBreak,
};

EarlyBreakOnHasArgumentChecking CheckEarlyBreakForHasArgument(
    const SelectorChecker::SelectorCheckingContext& context,
    Element* has_anchor_element,
    CheckPseudoHasArgumentContext& argument_context,
    CheckPseudoHasCacheScope::Context& cache_scope_context,
    bool& update_affected_by_has_flags) {
  if (!cache_scope_context.CacheAllowed()) {
    return kNoEarlyBreak;
  }

  // Get the cached :has() checking result of the element to skip :has()
  // argument checking.
  //  - If the element was already marked as matched, break :has() argument
  //    checking early and return as matched.
  //  - If the element was already checked but not matched, break :has()
  //    argument checking early and move to the next argument selector.
  //  - Otherwise, check :has() argument.
  uint8_t previous_result =
      SetHasAnchorElementAsCheckedAndGetOldResult(context, cache_scope_context);
  if (previous_result & kCheckPseudoHasResultChecked) {
    if (update_affected_by_has_flags) {
      SetAffectedByHasFlagsForHasAnchorSiblings(argument_context,
                                                has_anchor_element);
    }
    return previous_result & kCheckPseudoHasResultMatched
               ? kBreakEarlyAndReturnAsMatched
               : kBreakEarlyAndMoveToNextArgument;
  }

  // Check fast reject filter to reject :has() argument checking early.

  bool is_new_entry;
  CheckPseudoHasFastRejectFilter& fast_reject_filter =
      cache_scope_context.EnsureFastRejectFilter(has_anchor_element,
                                                 is_new_entry);

  // Filter is not actually created on the first check to avoid unnecessary
  // filter creation overhead. If the :has() anchor element has the
  // AffectedByMultipleHas flag set, use fast reject filter even if on the first
  // check since there can be more checks on the anchor element.
  if (is_new_entry && !has_anchor_element->AffectedByMultipleHas()) {
    return kNoEarlyBreak;
  }

  // The bloom filter in the fast reject filter is allocated and initialized on
  // the second check. We can check fast rejection with the filter after the
  // allocation and initialization.
  if (!fast_reject_filter.BloomFilterAllocated()) {
    if (update_affected_by_has_flags) {
      // Mark the :has() anchor element as affected by multiple :has() pseudo
      // classes so that we can always use fast reject filter for the anchor
      // element.
      has_anchor_element->SetAffectedByMultipleHas();
    }

    fast_reject_filter.AllocateBloomFilter();
    AddElementIdentifierHashesInTraversalScopeAndSetAffectedByHasFlags(
        fast_reject_filter, *has_anchor_element, argument_context,
        update_affected_by_has_flags);
  }

  // affected-by-has flags were already set while adding element identifier
  // hashes (AddElementIdentifierHashesInTraversalScopeAndSetAffectedByHasFlags)
  update_affected_by_has_flags = false;

  if (fast_reject_filter.FastReject(
          argument_context.GetPseudoHasArgumentHashes())) {
    SetAllElementsInTraversalScopeAsChecked(
        has_anchor_element, argument_context, cache_scope_context);
    return kBreakEarlyAndMoveToNextArgument;
  }

  return kNoEarlyBreak;
}

bool MatchesExternalSVGUseTarget(Element& element) {
  const auto* svg_element = DynamicTo<SVGElement>(element);
  return svg_element && svg_element->IsResourceTarget();
}

}  // namespace

bool SelectorChecker::CheckPseudoHas(const SelectorCheckingContext& context,
                                     MatchResult& result) const {
  if (context.element->GetDocument().InPseudoHasChecking()) {
    // :has() within :has() would normally be rejected parse-time, but we can
    // end up in this situation nevertheless, due to nesting. We just return
    // a not-matched for now; it is possible that we should fail the entire rule
    // (consider what happens if it is e.g. within :not()), but we would have to
    // have some way to propagate that up the stack, and consider interactions
    // with the forgiveness of :is().
    return false;
  }
  CheckPseudoHasCacheScope check_pseudo_has_cache_scope(
      &context.element->GetDocument(), /*within_selector_checking=*/true);

  Element* has_anchor_element = context.element;
  Document& document = has_anchor_element->GetDocument();
  DCHECK(document.GetCheckPseudoHasCacheScope());
  SelectorCheckingContext sub_context(has_anchor_element);
  sub_context.scope = context.scope;
  // sub_context.match_visited is false (by default) to disable
  // :visited matching when it is in the :has argument
  sub_context.is_inside_has_pseudo_class = true;
  sub_context.pseudo_has_in_rightmost_compound = context.in_rightmost_compound;
  bool update_affected_by_has_flags = mode_ == kResolvingStyle;
  bool match_in_shadow_tree = context.selector->HasArgumentMatchInShadowTree();

  if (match_in_shadow_tree && !has_anchor_element->GetShadowRoot()) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  DCHECK(context.selector->SelectorList());
  for (const CSSSelector* selector = context.selector->SelectorList()->First();
       selector; selector = CSSSelectorList::Next(*selector)) {
    CheckPseudoHasArgumentContext argument_context(selector,
                                                   match_in_shadow_tree);

    // In case of matching a :has() argument on a shadow root subtree, skip
    // matching if the argument contains the sibling relationship to the :has()
    // anchor element because the shadow root cannot have sibling element.
    if (argument_context.TraversalScope() == kInvalidShadowRootTraversalScope) {
      continue;
    }

    CSSSelector::RelationType leftmost_relation =
        argument_context.LeftmostRelation();
    CheckPseudoHasCacheScope::Context cache_scope_context(&document,
                                                          argument_context);

    // In case that the :has() pseudo class checks a relationship to a sibling
    // element at fixed distance (e.g. '.a:has(+ .b)') or a sibling subtree at
    // fixed distance (e.g. '.a:has(+ .b .c)'), set the parent of the :has()
    // anchor element as ChildrenAffectedByDirectAdjacentRules to indicate
    // that removing a child from the parent may affect a :has() testing result
    // on a child of the parent.
    // (e.g. When we have a style rule '.a:has(+ .b) {}' we always need :has()
    // invalidation if the preceding element of '.b' is removed)
    // Please refer the :has() invalidation for element removal:
    //  - StyleEngine::ScheduleInvalidationsForHasPseudoAffectedByRemoval()
    if (argument_context.AdjacentDistanceLimit() > 0 &&
        argument_context.AdjacentDistanceFixed()) {
      if (ContainerNode* parent =
              has_anchor_element->ParentElementOrShadowRoot()) {
        parent->SetChildrenAffectedByDirectAdjacentRules();
      }
    }

    if (update_affected_by_has_flags) {
      SetAffectedByHasFlagsForHasAnchorElement(argument_context,
                                               has_anchor_element);
    }

    EarlyBreakOnHasArgumentChecking early_break = CheckEarlyBreakForHasArgument(
        context, has_anchor_element, argument_context, cache_scope_context,
        update_affected_by_has_flags);
    if (early_break == kBreakEarlyAndReturnAsMatched) {
      return true;
    } else if (early_break == kBreakEarlyAndMoveToNextArgument) {
      continue;
    }

    sub_context.selector = selector;
    sub_context.relative_anchor_element = has_anchor_element;

    bool selector_matched = false;
    Element* last_argument_checked_element = nullptr;
    int last_argument_checked_depth = -1;
    for (CheckPseudoHasArgumentTraversalIterator iterator(*has_anchor_element,
                                                          argument_context);
         !iterator.AtEnd(); ++iterator) {
      if (update_affected_by_has_flags) {
        SetAffectedByHasFlagsForElementAtDepth(argument_context,
                                               iterator.CurrentElement(),
                                               iterator.CurrentDepth());
      }

      if (SkipCheckingHasArgument(argument_context, iterator)) {
        continue;
      }

      sub_context.element = iterator.CurrentElement();
      HeapVector<Member<Element>> has_argument_leftmost_compound_matches;
      SubResult sub_result(result);
      sub_result.has_argument_leftmost_compound_matches =
          &has_argument_leftmost_compound_matches;

      MatchSelector(sub_context, sub_result);

      last_argument_checked_element = iterator.CurrentElement();
      last_argument_checked_depth = iterator.CurrentDepth();

      selector_matched = CacheMatchedElementsAndReturnMatchedResult(
          leftmost_relation, has_anchor_element,
          has_argument_leftmost_compound_matches, cache_scope_context);

      if (selector_matched) {
        break;
      }
    }

    if (cache_scope_context.CacheAllowed() && last_argument_checked_element) {
      cache_scope_context.SetAllTraversedElementsAsChecked(
          last_argument_checked_element, last_argument_checked_depth);
    }

    if (!selector_matched) {
      continue;
    }

    if (update_affected_by_has_flags) {
      SetAffectedByHasForArgumentMatchedElement(
          argument_context, has_anchor_element, last_argument_checked_element,
          last_argument_checked_depth);
    }
    return true;
  }
  return false;
}

bool SelectorChecker::CheckPseudoClass(const SelectorCheckingContext& context,
                                       MatchResult& result) const {
  Element& element = *context.element;
  const CSSSelector& selector = *context.selector;
  bool force_pseudo_state = false;

  if (context.has_scrollbar_pseudo) {
    // CSS scrollbars match a specific subset of pseudo classes, and they have
    // specialized rules for each
    // (since there are no elements involved).
    return CheckScrollbarPseudoClass(context, result);
  }

  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoNot:
      return CheckPseudoNot(context, result);
    case CSSSelector::kPseudoEmpty: {
      bool is_empty = true;
      bool has_whitespace = false;
      for (Node* n = element.firstChild(); n; n = n->nextSibling()) {
        if (n->IsElementNode()) {
          is_empty = false;
          break;
        }
        if (auto* text_node = DynamicTo<Text>(n)) {
          if (!text_node->data().empty()) {
            if (text_node->ContainsOnlyWhitespaceOrEmpty()) {
              has_whitespace = true;
            } else {
              is_empty = false;
              break;
            }
          }
        }
      }
      if (is_empty && has_whitespace) {
        UseCounter::Count(context.element->GetDocument(),
                          WebFeature::kCSSSelectorEmptyWhitespaceOnlyFail);
        is_empty = false;
      }
      if (mode_ == kResolvingStyle) {
        element.SetStyleAffectedByEmpty();
      }
      return is_empty;
    }
    case CSSSelector::kPseudoFirstChild:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent = element.ParentElementOrDocumentFragment()) {
          parent->SetChildrenAffectedByFirstChildRules();
        }
        element.SetAffectedByFirstChildRules();
      }
      return IsFirstChild(element);
    case CSSSelector::kPseudoFirstOfType:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent = element.ParentElementOrDocumentFragment()) {
          parent->SetChildrenAffectedByForwardPositionalRules();
        }
      }
      return IsFirstOfType(element, element.TagQName());
    case CSSSelector::kPseudoLastChild: {
      ContainerNode* parent = element.ParentElementOrDocumentFragment();
      if (mode_ == kResolvingStyle) {
        if (parent) {
          parent->SetChildrenAffectedByLastChildRules();
        }
        element.SetAffectedByLastChildRules();
      }
      if (mode_ != kQueryingRules && parent &&
          !parent->IsFinishedParsingChildren()) {
        return false;
      }
      return IsLastChild(element);
    }
    case CSSSelector::kPseudoLastOfType: {
      ContainerNode* parent = element.ParentElementOrDocumentFragment();
      if (mode_ == kResolvingStyle) {
        if (parent) {
          parent->SetChildrenAffectedByBackwardPositionalRules();
        }
      }
      if (mode_ != kQueryingRules && parent &&
          !parent->IsFinishedParsingChildren()) {
        return false;
      }
      return IsLastOfType(element, element.TagQName());
    }
    case CSSSelector::kPseudoOnlyChild: {
      if (IsTransitionPseudoElement(context.pseudo_id)) {
        DCHECK(element.IsDocumentElement());
        DCHECK(context.pseudo_argument);

        auto* transition =
            ViewTransitionUtils::GetTransition(element.GetDocument());
        DCHECK(transition);
        return transition->MatchForOnlyChild(context.pseudo_id,
                                             *context.pseudo_argument);
      }

      ContainerNode* parent = element.ParentElementOrDocumentFragment();
      if (mode_ == kResolvingStyle) {
        if (parent) {
          parent->SetChildrenAffectedByFirstChildRules();
          parent->SetChildrenAffectedByLastChildRules();
        }
        element.SetAffectedByFirstChildRules();
        element.SetAffectedByLastChildRules();
      }
      if (mode_ != kQueryingRules && parent &&
          !parent->IsFinishedParsingChildren()) {
        return false;
      }
      return IsFirstChild(element) && IsLastChild(element);
    }
    case CSSSelector::kPseudoOnlyOfType: {
      // FIXME: This selector is very slow.
      ContainerNode* parent = element.ParentElementOrDocumentFragment();
      if (mode_ == kResolvingStyle && parent) {
        parent->SetChildrenAffectedByForwardPositionalRules();
        parent->SetChildrenAffectedByBackwardPositionalRules();
      }
      if (mode_ != kQueryingRules && parent &&
          !parent->IsFinishedParsingChildren()) {
        return false;
      }
      return IsFirstOfType(element, element.TagQName()) &&
             IsLastOfType(element, element.TagQName());
    }
    case CSSSelector::kPseudoPlaceholderShown: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoPlaceholderShown,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      if (auto* text_control = ToTextControlOrNull(element)) {
        return text_control->IsPlaceholderVisible();
      }
      break;
    }
    case CSSSelector::kPseudoNthChild:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent = element.ParentElementOrDocumentFragment()) {
          parent->SetChildrenAffectedByForwardPositionalRules();
        }
      }
      if (selector.SelectorList()) {
        // Check if the element itself matches the “of” selector.
        // Note that this will also propagate the correct MatchResult flags,
        // so NthIndexCache does not have to do that.
        if (!MatchesAnyInList(context, selector.SelectorList()->First(),
                              result)) {
          return false;
        }
      }
      return selector.MatchNth(NthIndexCache::NthChildIndex(
          element, selector.SelectorList(), this, &context));
    case CSSSelector::kPseudoNthOfType:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent = element.ParentElementOrDocumentFragment()) {
          parent->SetChildrenAffectedByForwardPositionalRules();
        }
      }
      return selector.MatchNth(NthIndexCache::NthOfTypeIndex(element));
    case CSSSelector::kPseudoNthLastChild: {
      ContainerNode* parent = element.ParentElementOrDocumentFragment();
      if (mode_ == kResolvingStyle && parent) {
        parent->SetChildrenAffectedByBackwardPositionalRules();
      }
      if (mode_ != kQueryingRules && parent &&
          !parent->IsFinishedParsingChildren()) {
        return false;
      }
      if (selector.SelectorList()) {
        // Check if the element itself matches the “of” selector.
        if (!MatchesAnyInList(context, selector.SelectorList()->First(),
                              result)) {
          return false;
        }
      }
      return selector.MatchNth(NthIndexCache::NthLastChildIndex(
          element, selector.SelectorList(), this, &context));
    }
    case CSSSelector::kPseudoNthLastOfType: {
      ContainerNode* parent = element.ParentElementOrDocumentFragment();
      if (mode_ == kResolvingStyle && parent) {
        parent->SetChildrenAffectedByBackwardPositionalRules();
      }
      if (mode_ != kQueryingRules && parent &&
          !parent->IsFinishedParsingChildren()) {
        return false;
      }
      return selector.MatchNth(NthIndexCache::NthLastOfTypeIndex(element));
    }
    case CSSSelector::kPseudoSelectorFragmentAnchor:
      return MatchesSelectorFragmentAnchorPseudoClass(element);
    case CSSSelector::kPseudoTarget:
      probe::ForcePseudoState(&element, CSSSelector::kPseudoTarget,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element == element.GetDocument().CssTarget() ||
             MatchesExternalSVGUseTarget(element);
    case CSSSelector::kPseudoIs:
    case CSSSelector::kPseudoWhere:
    case CSSSelector::kPseudoAny:
      return MatchesAnyInList(context, selector.SelectorListOrParent(), result);
    case CSSSelector::kPseudoParent: {
      const CSSSelector* parent = selector.SelectorListOrParent();
      if (parent == nullptr) {
        // & at top level matches like :scope.
        return CheckPseudoScope(context, result);
      } else {
        return MatchesAnyInList(context, parent, result);
      }
    }
    case CSSSelector::kPseudoAutofill:
    case CSSSelector::kPseudoWebKitAutofill:
    case CSSSelector::kPseudoAutofillPreviewed:
    case CSSSelector::kPseudoAutofillSelected:
      return CheckPseudoAutofill(selector.GetPseudoType(), element);
    case CSSSelector::kPseudoAnyLink:
    case CSSSelector::kPseudoWebkitAnyLink:
      return element.IsLink();
    case CSSSelector::kPseudoLink:
      return element.IsLink() && !context.match_visited;
    case CSSSelector::kPseudoVisited:
      return element.IsLink() && context.match_visited;
    case CSSSelector::kPseudoDrag:
      if (mode_ == kResolvingStyle) {
        if (ImpactsNonSubject(context)) {
          element.SetChildrenOrSiblingsAffectedByDrag();
        }
      }
      if (ImpactsSubject(context)) {
        result.SetFlag(MatchFlag::kAffectedByDrag);
      }
      return element.IsDragged();
    case CSSSelector::kPseudoFocus:
      if (mode_ == kResolvingStyle) {
        if (context.is_inside_has_pseudo_class) [[unlikely]] {
          element.SetAncestorsOrSiblingsAffectedByFocusInHas();
        } else {
          if (ImpactsNonSubject(context)) {
            element.SetChildrenOrSiblingsAffectedByFocus();
          }
        }
      }
      return MatchesFocusPseudoClass(element,
                                     context.previously_matched_pseudo_element);
    case CSSSelector::kPseudoFocusVisible:
      if (mode_ == kResolvingStyle) {
        if (context.is_inside_has_pseudo_class) [[unlikely]] {
          element.SetAncestorsOrSiblingsAffectedByFocusVisibleInHas();
        } else {
          if (ImpactsNonSubject(context)) {
            element.SetChildrenOrSiblingsAffectedByFocusVisible();
          }
        }
      }
      return MatchesFocusVisiblePseudoClass(element);
    case CSSSelector::kPseudoFocusWithin:
      if (mode_ == kResolvingStyle) {
        if (context.is_inside_has_pseudo_class) [[unlikely]] {
          element.SetAncestorsOrSiblingsAffectedByFocusInHas();
        } else if (ImpactsNonSubject(context)) {
          element.SetChildrenOrSiblingsAffectedByFocusWithin();
        }
      }
      if (ImpactsSubject(context)) {
        result.SetFlag(MatchFlag::kAffectedByFocusWithin);
      }
      probe::ForcePseudoState(&element, CSSSelector::kPseudoFocusWithin,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.HasFocusWithin();
    case CSSSelector::kPseudoHasSlotted:
      DCHECK(RuntimeEnabledFeatures::CSSPseudoHasSlottedEnabled());
      if (auto* slot = DynamicTo<HTMLSlotElement>(element)) {
        return slot->HasAssignedNodesNoRecalc();
      }
      return false;
    case CSSSelector::kPseudoHover:
      if (mode_ == kResolvingStyle) {
        if (context.is_inside_has_pseudo_class) [[unlikely]] {
          element.SetAncestorsOrSiblingsAffectedByHoverInHas();
        } else if (ImpactsNonSubject(context)) {
          element.SetChildrenOrSiblingsAffectedByHover();
        }
      }
      if (ImpactsSubject(context)) {
        result.SetFlag(MatchFlag::kAffectedByHover);
      }
      if (!ShouldMatchHoverOrActive(context)) {
        return false;
      }
      probe::ForcePseudoState(&element, CSSSelector::kPseudoHover,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.IsHovered();
    case CSSSelector::kPseudoActive:
      if (mode_ == kResolvingStyle) {
        if (context.is_inside_has_pseudo_class) [[unlikely]] {
          element.SetAncestorsOrSiblingsAffectedByActiveInHas();
        } else if (ImpactsNonSubject(context)) {
          element.SetChildrenOrSiblingsAffectedByActive();
        }
      }
      if (ImpactsSubject(context)) {
        result.SetFlag(MatchFlag::kAffectedByActive);
      }
      if (!ShouldMatchHoverOrActive(context)) {
        return false;
      }
      probe::ForcePseudoState(&element, CSSSelector::kPseudoActive,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.IsActive();
    case CSSSelector::kPseudoEnabled: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoEnabled,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.MatchesEnabledPseudoClass();
    }
    case CSSSelector::kPseudoFullPageMedia:
      return element.GetDocument().IsMediaDocument();
    case CSSSelector::kPseudoDefault:
      return element.MatchesDefaultPseudoClass();
    case CSSSelector::kPseudoDisabled:
      probe::ForcePseudoState(&element, CSSSelector::kPseudoDisabled,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      if (auto* fieldset = DynamicTo<HTMLFieldSetElement>(element)) {
        // <fieldset> should never be considered disabled, but should still
        // match the :enabled or :disabled pseudo-classes according to whether
        // the attribute is set or not. See here for context:
        // https://github.com/whatwg/html/issues/5886#issuecomment-1582410112
        return fieldset->IsActuallyDisabled();
      }
      return element.IsDisabledFormControl();
    case CSSSelector::kPseudoReadOnly: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoReadOnly,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.MatchesReadOnlyPseudoClass();
    }
    case CSSSelector::kPseudoReadWrite: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoReadWrite,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.MatchesReadWritePseudoClass();
    }
    case CSSSelector::kPseudoOptional: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoOptional,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.IsOptionalFormControl();
    }
    case CSSSelector::kPseudoRequired: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoRequired,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.IsRequiredFormControl();
    }
    case CSSSelector::kPseudoUserInvalid: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoUserInvalid,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      if (auto* form_control =
              DynamicTo<HTMLFormControlElementWithState>(element)) {
        return form_control->MatchesUserInvalidPseudo();
      }
      return false;
    }
    case CSSSelector::kPseudoUserValid: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoUserValid,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      if (auto* form_control =
              DynamicTo<HTMLFormControlElementWithState>(element)) {
        return form_control->MatchesUserValidPseudo();
      }
      return false;
    }
    case CSSSelector::kPseudoValid:
      probe::ForcePseudoState(&element, CSSSelector::kPseudoValid,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.MatchesValidityPseudoClasses() && element.IsValidElement();
    case CSSSelector::kPseudoInvalid: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoInvalid,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.MatchesValidityPseudoClasses() &&
             !element.IsValidElement();
    }
    case CSSSelector::kPseudoChecked: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoChecked,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      if (auto* input_element = DynamicTo<HTMLInputElement>(element)) {
        // Even though WinIE allows checked and indeterminate to
        // co-exist, the CSS selector spec says that you can't be
        // both checked and indeterminate. We will behave like WinIE
        // behind the scenes and just obey the CSS spec here in the
        // test for matching the pseudo.
        if (input_element->ShouldAppearChecked() &&
            !input_element->ShouldAppearIndeterminate()) {
          return true;
        }
      } else if (auto* option_element = DynamicTo<HTMLOptionElement>(element)) {
        if (option_element->Selected()) {
          return true;
        }
      } else if (PseudoElement* scroll_marker =
                     element.GetPseudoElement(kPseudoIdScrollMarker);
                 context.pseudo_id == kPseudoIdScrollMarker && scroll_marker) {
        return To<ScrollMarkerPseudoElement>(scroll_marker)->IsSelected();
      }
      break;
    }
    case CSSSelector::kPseudoIndeterminate: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoIndeterminate,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.ShouldAppearIndeterminate();
    }
    case CSSSelector::kPseudoRoot:
      return element == element.GetDocument().documentElement();
    case CSSSelector::kPseudoLang: {
      auto* vtt_element = DynamicTo<VTTElement>(element);
      AtomicString value = vtt_element ? vtt_element->Language()
                                       : element.ComputeInheritedLanguage();
      const AtomicString& argument = selector.Argument();
      if (value.empty() ||
          !value.StartsWith(argument, kTextCaseASCIIInsensitive)) {
        break;
      }
      if (value.length() != argument.length() &&
          value[argument.length()] != '-') {
        break;
      }
      return true;
    }
    case CSSSelector::kPseudoDir: {
      const AtomicString& argument = selector.Argument();
      if (argument.empty()) {
        break;
      }

      TextDirection direction;
      if (EqualIgnoringASCIICase(argument, "ltr")) {
        direction = TextDirection::kLtr;
      } else if (EqualIgnoringASCIICase(argument, "rtl")) {
        direction = TextDirection::kRtl;
      } else {
        break;
      }

      // Recomputing the slot assignment can update cached directionality.  In
      // most cases it's OK for this code to be run when slot assignments are
      // dirty; however for API calls like Element.matches() we should recalc
      // them now.
      Document& document = element.GetDocument();
      if (mode_ == kQueryingRules && document.IsSlotAssignmentDirty()) {
        document.GetSlotAssignmentEngine().RecalcSlotAssignments();
      }

      return element.CachedDirectionality() == direction;
    }
    case CSSSelector::kPseudoDialogInTopLayer:
      if (auto* dialog = DynamicTo<HTMLDialogElement>(element)) {
        if (dialog->IsModal() &&
            dialog->FastHasAttribute(html_names::kOpenAttr)) {
          DCHECK(dialog->GetDocument().TopLayerElements().Contains(dialog));
          return true;
        }
        // When the dialog is transitioning to closed, we have to check the
        // elements which are in the top layer but are pending removal to see if
        // this element used to be open as a dialog.
        std::optional<Document::TopLayerReason> top_layer_reason =
            dialog->GetDocument().IsScheduledForTopLayerRemoval(dialog);
        return top_layer_reason &&
               *top_layer_reason == Document::TopLayerReason::kDialog;
      }
      return false;
    case CSSSelector::kPseudoPopoverInTopLayer:
      if (auto* html_element = DynamicTo<HTMLElement>(element);
          html_element && html_element->HasPopoverAttribute()) {
        // When the popover is open and is not transitioning to closed,
        // popoverOpen will return true.
        if (html_element->popoverOpen()) {
          DCHECK(html_element->GetDocument().TopLayerElements().Contains(
              html_element));
          return true;
        }
        // When the popover is transitioning to closed, popoverOpen won't return
        // true and we have to check the elements which are in the top layer but
        // are pending removal to see if this element used to be popoverOpen.
        std::optional<Document::TopLayerReason> top_layer_reason =
            html_element->GetDocument().IsScheduledForTopLayerRemoval(
                html_element);
        return top_layer_reason &&
               *top_layer_reason == Document::TopLayerReason::kPopover;
      }
      return false;
    case CSSSelector::kPseudoPopoverOpen:
      if (auto* html_element = DynamicTo<HTMLElement>(element);
          html_element && html_element->HasPopoverAttribute()) {
        return html_element->popoverOpen();
      }
      return false;
    case CSSSelector::kPseudoOpen:
      if (auto* dialog = DynamicTo<HTMLDialogElement>(element)) {
        return dialog->FastHasAttribute(html_names::kOpenAttr);
      } else if (auto* details = DynamicTo<HTMLDetailsElement>(element)) {
        return details->FastHasAttribute(html_names::kOpenAttr);
      } else if (auto* select = DynamicTo<HTMLSelectElement>(element)) {
        return select->PopupIsVisible();
      }
      return false;
    case CSSSelector::kPseudoClosed:
      if (auto* dialog = DynamicTo<HTMLDialogElement>(element)) {
        return !dialog->FastHasAttribute(html_names::kOpenAttr);
      } else if (auto* details = DynamicTo<HTMLDetailsElement>(element)) {
        return !details->FastHasAttribute(html_names::kOpenAttr);
      } else if (auto* select = DynamicTo<HTMLSelectElement>(element)) {
        return select->UsesMenuList() && !select->PopupIsVisible();
      }
      return false;
    case CSSSelector::kPseudoFullscreen:
    // fall through
    case CSSSelector::kPseudoFullScreen:
      return Fullscreen::IsFullscreenFlagSetFor(element);
    case CSSSelector::kPseudoFullScreenAncestor:
      return element.ContainsFullScreenElement();
    case CSSSelector::kPseudoPaused: {
      DCHECK(RuntimeEnabledFeatures::CSSPseudoPlayingPausedEnabled());
      auto* media_element = DynamicTo<HTMLMediaElement>(element);
      return media_element && media_element->paused();
    }
    case CSSSelector::kPseudoPermissionGranted: {
      CHECK(RuntimeEnabledFeatures::PermissionElementEnabled(
          element.GetExecutionContext()));
      auto* permission_element = DynamicTo<HTMLPermissionElement>(element);
      return permission_element && permission_element->granted();
    }
    case CSSSelector::kPseudoPermissionElementInvalidStyle: {
      CHECK(RuntimeEnabledFeatures::PermissionElementEnabled(
          element.GetExecutionContext()));
      auto* permission_element = DynamicTo<HTMLPermissionElement>(element);
      return permission_element && permission_element->HasInvalidStyle();
    }
    case CSSSelector::kPseudoPermissionElementOccluded: {
      CHECK(RuntimeEnabledFeatures::PermissionElementEnabled(
          element.GetExecutionContext()));
      auto* permission_element = DynamicTo<HTMLPermissionElement>(element);
      return permission_element && permission_element->IsOccluded();
    }
    case CSSSelector::kPseudoPictureInPicture:
      return PictureInPictureController::IsElementInPictureInPicture(&element);
    case CSSSelector::kPseudoPlaying: {
      DCHECK(RuntimeEnabledFeatures::CSSPseudoPlayingPausedEnabled());
      auto* media_element = DynamicTo<HTMLMediaElement>(element);
      return media_element && !media_element->paused();
    }
    case CSSSelector::kPseudoVideoPersistent: {
      DCHECK(is_ua_rule_);
      auto* video_element = DynamicTo<HTMLVideoElement>(element);
      return video_element && video_element->IsPersistent();
    }
    case CSSSelector::kPseudoVideoPersistentAncestor:
      DCHECK(is_ua_rule_);
      return element.ContainsPersistentVideo();
    case CSSSelector::kPseudoXrOverlay:
      // In immersive AR overlay mode, apply a pseudostyle to the DOM Overlay
      // element. This is the same as the fullscreen element in the current
      // implementation, but could be different for AR headsets.
      return element.GetDocument().IsXrOverlay() &&
             Fullscreen::IsFullscreenElement(element);
    case CSSSelector::kPseudoInRange: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoInRange,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.IsInRange();
    }
    case CSSSelector::kPseudoOutOfRange: {
      probe::ForcePseudoState(&element, CSSSelector::kPseudoOutOfRange,
                              &force_pseudo_state);
      if (force_pseudo_state) {
        return true;
      }
      return element.IsOutOfRange();
    }
    case CSSSelector::kPseudoFutureCue: {
      auto* vtt_element = DynamicTo<VTTElement>(element);
      return vtt_element && !vtt_element->IsPastNode();
    }
    case CSSSelector::kPseudoPastCue: {
      auto* vtt_element = DynamicTo<VTTElement>(element);
      return vtt_element && vtt_element->IsPastNode();
    }
    case CSSSelector::kPseudoScope:
      return CheckPseudoScope(context, result);
    case CSSSelector::kPseudoDefined:
      return element.IsDefined();
    case CSSSelector::kPseudoHostContext:
      UseCounter::Count(
          context.element->GetDocument(),
          mode_ == kQueryingRules
              ? WebFeature::kCSSSelectorHostContextInSnapshotProfile
              : WebFeature::kCSSSelectorHostContextInLiveProfile);
      [[fallthrough]];
    case CSSSelector::kPseudoHost:
      return CheckPseudoHost(context, result);
    case CSSSelector::kPseudoSpatialNavigationFocus:
      DCHECK(is_ua_rule_);
      return MatchesSpatialNavigationFocusPseudoClass(element);
    case CSSSelector::kPseudoHasDatalist:
      DCHECK(is_ua_rule_);
      return MatchesHasDatalistPseudoClass(element);
    case CSSSelector::kPseudoIsHtml:
      DCHECK(is_ua_rule_);
      return IsA<HTMLDocument>(element.GetDocument());
    case CSSSelector::kPseudoListBox:
      DCHECK(is_ua_rule_);
      return MatchesListBoxPseudoClass(element);
    case CSSSelector::kPseudoSelectHasChildButton:
      DCHECK(is_ua_rule_);
      if (!RuntimeEnabledFeatures::CustomizableSelectEnabled()) {
        return false;
      }
      if (auto* select = DynamicTo<HTMLSelectElement>(element)) {
        return select->SlottedButton();
      }
      return false;
    case CSSSelector::kPseudoMultiSelectFocus:
      DCHECK(is_ua_rule_);
      return MatchesMultiSelectFocusPseudoClass(element);
    case CSSSelector::kPseudoHostHasNonAutoAppearance:
      DCHECK(is_ua_rule_);
      if (ShadowRoot* root = element.ContainingShadowRoot()) {
        if (!root->IsUserAgent()) {
          return false;
        }
        const ComputedStyle* style = root->host().GetComputedStyle();
        return style && style->HasEffectiveAppearance();
      }
      return false;
    case CSSSelector::kPseudoWindowInactive:
      if (context.previously_matched_pseudo_element != kPseudoIdSelection) {
        return false;
      }
      return !element.GetDocument().GetPage()->GetFocusController().IsActive();
    case CSSSelector::kPseudoStateDeprecatedSyntax: {
      CHECK(RuntimeEnabledFeatures::CSSCustomStateDeprecatedSyntaxEnabled());
      return element.DidAttachInternals() &&
             element.EnsureElementInternals().HasState(selector.Value());
    }
    case CSSSelector::kPseudoState: {
      CHECK(RuntimeEnabledFeatures::CSSCustomStateNewSyntaxEnabled());
      return element.DidAttachInternals() &&
             element.EnsureElementInternals().HasState(selector.Argument());
    }
    case CSSSelector::kPseudoHorizontal:
    case CSSSelector::kPseudoVertical:
    case CSSSelector::kPseudoDecrement:
    case CSSSelector::kPseudoIncrement:
    case CSSSelector::kPseudoStart:
    case CSSSelector::kPseudoEnd:
    case CSSSelector::kPseudoDoubleButton:
    case CSSSelector::kPseudoSingleButton:
    case CSSSelector::kPseudoNoButton:
    case CSSSelector::kPseudoCornerPresent:
      return false;
    case CSSSelector::kPseudoModal:
      if (Fullscreen::IsFullscreenElement(element)) {
        return true;
      }
      if (const auto* dialog_element = DynamicTo<HTMLDialogElement>(element)) {
        return dialog_element->IsModal();
      }
      return false;
    case CSSSelector::kPseudoHas:
      if (mode_ == kResolvingStyle) {
        // Set 'AffectedBySubjectHas' or 'AffectedByNonSubjectHas' flag to
        // indicate that the element is affected by a subject or non-subject
        // :has() state change. It means that, when we have a mutation on
        // an element, and the element is in the :has() argument checking scope
        // of a :has() anchor element, we may need to invalidate the subject
        // element of the style rule containing the :has() pseudo class because
        // the mutation can affect the state of the :has().
        if (ImpactsSubject(context)) {
          element.SetAffectedBySubjectHas();
        }
        if (ImpactsNonSubject(context)) {
          element.SetAffectedByNonSubjectHas();
        }

        if (selector.ContainsPseudoInsideHasPseudoClass()) {
          element.SetAffectedByPseudoInHas();
        }

        if (selector.ContainsComplexLogicalCombinationsInsideHasPseudoClass()) {
          element.SetAffectedByLogicalCombinationsInHas();
        }
      }
      return CheckPseudoHas(context, result);
    case CSSSelector::kPseudoRelativeAnchor:
      DCHECK(context.relative_anchor_element);
      return context.relative_anchor_element == &element;
    case CSSSelector::kPseudoActiveViewTransition: {
      // :active-view-transition is only valid on the document element.
      if (!element.IsDocumentElement()) {
        return false;
      }

      // The pseudo is only valid if there is a transition.
      auto* transition =
          ViewTransitionUtils::GetTransition(element.GetDocument());
      if (!transition) {
        return false;
      }

      // Ask the transition to match for active-view-transition.
      return transition->MatchForActiveViewTransition();
    }
    case CSSSelector::kPseudoActiveViewTransitionType: {
      // :active-view-transition-type is only valid on the document element.
      if (!element.IsDocumentElement()) {
        return false;
      }

      // The pseudo is only valid if there is a transition.
      auto* transition =
          ViewTransitionUtils::GetTransition(element.GetDocument());
      if (!transition) {
        return false;
      }

      // Ask the transition to match based on the argument list.
      return transition->MatchForActiveViewTransitionType(selector.IdentList());
    }
    case CSSSelector::kPseudoUnparsed:
      // Only kept around for parsing; can never match anything
      // (because we don't know what it's supposed to mean).
      return false;
    case CSSSelector::kPseudoTrue:
      return true;
    case CSSSelector::kPseudoCurrent:
      if (context.previously_matched_pseudo_element != kPseudoIdSearchText) {
        return false;
      }
      return context.search_text_request_is_current;
    case CSSSelector::kPseudoUnknown:
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return false;
}

static bool MatchesUAShadowElement(Element& element, const AtomicString& id) {
  ShadowRoot* root = element.ContainingShadowRoot();
  return root && root->IsUserAgent() && element.ShadowPseudoId() == id;
}

bool SelectorChecker::CheckPseudoAutofill(CSSSelector::PseudoType pseudo_type,
                                          Element& element) const {
  bool force_pseudo_state = false;
  probe::ForcePseudoState(&element, CSSSelector::kPseudoAutofill,
                          &force_pseudo_state);
  if (force_pseudo_state) {
    return true;
  }
  HTMLFormControlElement* form_control_element =
      DynamicTo<HTMLFormControlElement>(&element);
  if (!form_control_element) {
    return false;
  }
  switch (pseudo_type) {
    case CSSSelector::kPseudoAutofill:
    case CSSSelector::kPseudoWebKitAutofill:
      return form_control_element->IsAutofilled() ||
             form_control_element->IsPreviewed();
    case CSSSelector::kPseudoAutofillPreviewed:
      return form_control_element->GetAutofillState() ==
             WebAutofillState::kPreviewed;
    case CSSSelector::kPseudoAutofillSelected:
      return form_control_element->IsAutofilled();
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return false;
}

bool SelectorChecker::CheckPseudoElement(const SelectorCheckingContext& context,
                                         MatchResult& result) const {
  const CSSSelector& selector = *context.selector;
  Element& element = *context.element;

  if (context.in_nested_complex_selector) {
    // This would normally be rejected parse-time, but can happen
    // with the & selector, so reject it match-time.
    // See https://github.com/w3c/csswg-drafts/issues/7912.
    return false;
  }

  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoCue: {
      SelectorCheckingContext sub_context(context);
      sub_context.is_sub_selector = true;
      sub_context.scope = nullptr;
      sub_context.treat_shadow_host_as_normal_scope = false;

      for (sub_context.selector = selector.SelectorList()->First();
           sub_context.selector; sub_context.selector = CSSSelectorList::Next(
                                     *sub_context.selector)) {
        SubResult sub_result(result);
        if (MatchSelector(sub_context, sub_result) == kSelectorMatches) {
          return true;
        }
      }
      return false;
    }
    case CSSSelector::kPseudoPart:
      if (!part_names_) {
        return false;
      }
      for (const auto& part_name : selector.IdentList()) {
        if (!part_names_->Contains(part_name)) {
          return false;
        }
      }
      return true;
    case CSSSelector::kPseudoFileSelectorButton:
      return MatchesUAShadowElement(
          element, shadow_element_names::kPseudoFileUploadButton);
    case CSSSelector::kPseudoPicker:
      if (selector.Argument() == "select") {
        return MatchesUAShadowElement(element,
                                      shadow_element_names::kPickerSelect);
      } else {
        return false;
      }
    case CSSSelector::kPseudoPlaceholder:
      return MatchesUAShadowElement(
          element, shadow_element_names::kPseudoInputPlaceholder);
    case CSSSelector::kPseudoDetailsContent:
      return MatchesUAShadowElement(element,
                                    shadow_element_names::kIdDetailsContent);
    case CSSSelector::kPseudoWebKitCustomElement:
      return MatchesUAShadowElement(element, selector.Value());
    case CSSSelector::kPseudoBlinkInternalElement:
      DCHECK(is_ua_rule_);
      return MatchesUAShadowElement(element, selector.Value());
    case CSSSelector::kPseudoSlotted: {
      SelectorCheckingContext sub_context(context);
      sub_context.is_sub_selector = true;
      sub_context.scope = nullptr;
      sub_context.treat_shadow_host_as_normal_scope = false;

      // ::slotted() only allows one compound selector.
      DCHECK(selector.SelectorList()->First());
      DCHECK(!CSSSelectorList::Next(*selector.SelectorList()->First()));
      sub_context.selector = selector.SelectorList()->First();
      SubResult sub_result(result);
      if (MatchSelector(sub_context, sub_result) != kSelectorMatches) {
        return false;
      }
      return true;
    }
    case CSSSelector::kPseudoHighlight: {
      result.dynamic_pseudo = PseudoId::kPseudoIdHighlight;
      // A null pseudo_argument_ means we are matching rules on the originating
      // element. We keep track of which pseudo elements may match for the
      // element through result.dynamic_pseudo. For ::highlight() pseudo
      // elements we have a single flag for tracking whether an element may
      // match _any_ ::highlight() element (kPseudoIdHighlight).
      if (!pseudo_argument_ || pseudo_argument_ == selector.Argument()) {
        result.custom_highlight_name = selector.Argument().Impl();
        return true;
      }
      return false;
    }
    case CSSSelector::kPseudoViewTransition:
    case CSSSelector::kPseudoViewTransitionGroup:
    case CSSSelector::kPseudoViewTransitionImagePair:
    case CSSSelector::kPseudoViewTransitionOld:
    case CSSSelector::kPseudoViewTransitionNew: {
      const PseudoId selector_pseudo_id =
          CSSSelector::GetPseudoId(selector.GetPseudoType());
      if (element.IsDocumentElement() && context.pseudo_id == kPseudoIdNone) {
        // We don't strictly need to use dynamic_pseudo since we don't rely on
        // SetHasPseudoElementStyle but we need to return a match to invalidate
        // the originating element and set dynamic_pseudo to avoid collecting
        // it as a matched rule in ElementRuleCollector.
        result.dynamic_pseudo = selector_pseudo_id;
        return true;
      }

      if (selector_pseudo_id != context.pseudo_id) {
        return false;
      }
      result.dynamic_pseudo = context.pseudo_id;
      if (selector_pseudo_id == kPseudoIdViewTransition) {
        return true;
      }

      CHECK(!selector.IdentList().empty());
      const AtomicString& name_or_wildcard = selector.IdentList()[0];

      // note that the pseudo_ident_list_ is the class list, and
      // pseudo_argument_ is the name, while in the selector the IdentList() is
      // both the name and the classes.
      if (name_or_wildcard != CSSSelector::UniversalSelectorAtom() &&
          name_or_wildcard != pseudo_argument_) {
        return false;
      }

      // https://drafts.csswg.org/css-view-transitions-2/#typedef-pt-class-selector
      // A named view transition pseudo-element selector which has one or more
      // <custom-ident> values in its <pt-class-selector> would only match an
      // element if the class list value in named elements for the
      // pseudo-element’s view-transition-name contains all of those values.

      // selector.IdentList() is equivalent to
      // <pt-name-selector><pt-class-selector>, as in [name, class, class, ...]
      // so we check that all of its items excluding the first one are
      // contained in the pseudo element's classes (pseudo_ident_list_).
      return base::ranges::all_of(
          selector.IdentList().begin() + 1, selector.IdentList().end(),
          [&](const AtomicString& class_from_selector) {
            return base::Contains(pseudo_ident_list_, class_from_selector);
          });
    }
    case CSSSelector::kPseudoScrollbarButton:
    case CSSSelector::kPseudoScrollbarCorner:
    case CSSSelector::kPseudoScrollbarThumb:
    case CSSSelector::kPseudoScrollbarTrack:
    case CSSSelector::kPseudoScrollbarTrackPiece: {
      if (CSSSelector::GetPseudoId(selector.GetPseudoType()) !=
          context.pseudo_id) {
        return false;
      }
      result.dynamic_pseudo = context.pseudo_id;
      return true;
    }
    case CSSSelector::kPseudoScrollMarker: {
      // The style for ::column::scroll-marker is stored on the originating
      // element's style.
      result.dynamic_pseudo =
          context.previously_matched_pseudo_element == kPseudoIdColumn
              ? kPseudoIdColumnScrollMarker
              : kPseudoIdScrollMarker;
      return true;
    }
    case CSSSelector::kPseudoTargetText:
      if (!is_ua_rule_) {
        UseCounter::Count(context.element->GetDocument(),
                          WebFeature::kCSSSelectorTargetText);
      }
      [[fallthrough]];
    default:
      DCHECK_NE(mode_, kQueryingRules);
      result.dynamic_pseudo =
          CSSSelector::GetPseudoId(selector.GetPseudoType());
      DCHECK_NE(result.dynamic_pseudo, kPseudoIdNone);
      return true;
  }
}

bool SelectorChecker::CheckPseudoHost(const SelectorCheckingContext& context,
                                      MatchResult& result) const {
  const CSSSelector& selector = *context.selector;
  Element& element = *context.element;

  // :host only matches a shadow host when :host is in a shadow tree of the
  // shadow host.
  if (!context.scope) {
    return false;
  }
  const ContainerNode* shadow_host = context.scope->OwnerShadowHost();
  if (!shadow_host || shadow_host != element) {
    return false;
  }
  DCHECK(IsShadowHost(element));
  DCHECK(element.GetShadowRoot());

  // For the case with no parameters, i.e. just :host.
  if (!selector.SelectorList()) {
    return true;
  }

  DCHECK(selector.SelectorList()->HasOneSelector());

  SelectorCheckingContext sub_context(context);
  sub_context.is_sub_selector = true;
  sub_context.selector = selector.SelectorList()->First();
  sub_context.treat_shadow_host_as_normal_scope = true;
  sub_context.scope = context.scope;
  // Use FlatTreeTraversal to traverse a composed ancestor list of a given
  // element.
  Element* next_element = &element;
  SelectorCheckingContext host_context(sub_context);
  do {
    SubResult sub_result(result);
    host_context.element = next_element;
    if (MatchSelector(host_context, sub_result) == kSelectorMatches) {
      return true;
    }
    host_context.treat_shadow_host_as_normal_scope = false;
    host_context.scope = nullptr;

    if (selector.GetPseudoType() == CSSSelector::kPseudoHost) {
      break;
    }

    host_context.in_rightmost_compound = false;
    host_context.impact = Impact::kNonSubject;
    next_element = FlatTreeTraversal::ParentElement(*next_element);
  } while (next_element);

  // FIXME: this was a fallthrough condition.
  return false;
}

bool SelectorChecker::CheckPseudoScope(const SelectorCheckingContext& context,
                                       MatchResult& result) const {
  Element& element = *context.element;
  if (!context.scope) {
    return false;
  }
  if (context.scope->IsElementNode()) {
    return context.scope == &element;
  }
  return element == element.GetDocument().documentElement();
}

bool SelectorChecker::CheckScrollbarPseudoClass(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  const CSSSelector& selector = *context.selector;

  if (selector.GetPseudoType() == CSSSelector::kPseudoNot) {
    return CheckPseudoNot(context, result);
  }

  // FIXME: This is a temporary hack for resizers and scrollbar corners.
  // Eventually :window-inactive should become a real
  // pseudo class and just apply to everything.
  if (selector.GetPseudoType() == CSSSelector::kPseudoWindowInactive) {
    return !context.element->GetDocument()
                .GetPage()
                ->GetFocusController()
                .IsActive();
  }

  if (!scrollbar_) {
    return false;
  }

  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoEnabled:
      return scrollbar_->Enabled();
    case CSSSelector::kPseudoDisabled:
      return !scrollbar_->Enabled();
    case CSSSelector::kPseudoHover: {
      ScrollbarPart hovered_part = scrollbar_->HoveredPart();
      if (scrollbar_part_ == kScrollbarBGPart) {
        return hovered_part != kNoPart;
      }
      if (scrollbar_part_ == kTrackBGPart) {
        return hovered_part == kBackTrackPart ||
               hovered_part == kForwardTrackPart || hovered_part == kThumbPart;
      }
      return scrollbar_part_ == hovered_part;
    }
    case CSSSelector::kPseudoActive: {
      ScrollbarPart pressed_part = scrollbar_->PressedPart();
      if (scrollbar_part_ == kScrollbarBGPart) {
        return pressed_part != kNoPart;
      }
      if (scrollbar_part_ == kTrackBGPart) {
        return pressed_part == kBackTrackPart ||
               pressed_part == kForwardTrackPart || pressed_part == kThumbPart;
      }
      return scrollbar_part_ == pressed_part;
    }
    case CSSSelector::kPseudoHorizontal:
      return scrollbar_->Orientation() == kHorizontalScrollbar;
    case CSSSelector::kPseudoVertical:
      return scrollbar_->Orientation() == kVerticalScrollbar;
    case CSSSelector::kPseudoDecrement:
      return scrollbar_part_ == kBackButtonStartPart ||
             scrollbar_part_ == kBackButtonEndPart ||
             scrollbar_part_ == kBackTrackPart;
    case CSSSelector::kPseudoIncrement:
      return scrollbar_part_ == kForwardButtonStartPart ||
             scrollbar_part_ == kForwardButtonEndPart ||
             scrollbar_part_ == kForwardTrackPart;
    case CSSSelector::kPseudoStart:
      return scrollbar_part_ == kBackButtonStartPart ||
             scrollbar_part_ == kForwardButtonStartPart ||
             scrollbar_part_ == kBackTrackPart;
    case CSSSelector::kPseudoEnd:
      return scrollbar_part_ == kBackButtonEndPart ||
             scrollbar_part_ == kForwardButtonEndPart ||
             scrollbar_part_ == kForwardTrackPart;
    case CSSSelector::kPseudoDoubleButton:
      // :double-button matches nothing on all platforms.
      return false;
    case CSSSelector::kPseudoSingleButton:
      if (!scrollbar_->GetTheme().NativeThemeHasButtons()) {
        return false;
      }
      return scrollbar_part_ == kBackButtonStartPart ||
             scrollbar_part_ == kForwardButtonEndPart ||
             scrollbar_part_ == kBackTrackPart ||
             scrollbar_part_ == kForwardTrackPart;
    case CSSSelector::kPseudoNoButton:
      if (scrollbar_->GetTheme().NativeThemeHasButtons()) {
        return false;
      }
      return scrollbar_part_ == kBackTrackPart ||
             scrollbar_part_ == kForwardTrackPart;
    case CSSSelector::kPseudoCornerPresent:
      return scrollbar_->IsScrollCornerVisible();
    default:
      return false;
  }
}

bool SelectorChecker::MatchesSelectorFragmentAnchorPseudoClass(
    const Element& element) {
  return element == element.GetDocument().CssTarget() &&
         element.GetDocument().View()->GetFragmentAnchor() &&
         element.GetDocument()
             .View()
             ->GetFragmentAnchor()
             ->IsSelectorFragmentAnchor();
}

bool SelectorChecker::MatchesFocusPseudoClass(
    const Element& element,
    PseudoId matching_for_pseudo_element) {
  const Element* matching_element = &element;
  if (matching_for_pseudo_element != kPseudoIdNone) {
    matching_element = element.GetPseudoElement(matching_for_pseudo_element);
    if (!matching_element) {
      return false;
    }
  }
  bool force_pseudo_state = false;
  probe::ForcePseudoState(const_cast<Element*>(matching_element),
                          CSSSelector::kPseudoFocus, &force_pseudo_state);
  if (force_pseudo_state) {
    return true;
  }
  return matching_element->IsFocused() && IsFrameFocused(*matching_element);
}

bool SelectorChecker::MatchesFocusVisiblePseudoClass(const Element& element) {
  bool force_pseudo_state = false;
  probe::ForcePseudoState(const_cast<Element*>(&element),
                          CSSSelector::kPseudoFocusVisible,
                          &force_pseudo_state);
  if (force_pseudo_state) {
    return true;
  }

  if (!element.IsFocused() || !IsFrameFocused(element)) {
    return false;
  }

  const Document& document = element.GetDocument();
  // Exclude shadow hosts with non-UA ShadowRoot.
  if (document.FocusedElement() != element && element.GetShadowRoot() &&
      !element.GetShadowRoot()->IsUserAgent()) {
    return false;
  }

  const Settings* settings = document.GetSettings();
  bool always_show_focus = settings->GetAccessibilityAlwaysShowFocus();
  bool is_text_input = element.MayTriggerVirtualKeyboard();
  bool last_focus_from_mouse =
      document.GetFrame() &&
      document.GetFrame()->Selection().FrameIsFocusedAndActive() &&
      document.LastFocusType() == mojom::blink::FocusType::kMouse;
  bool had_keyboard_event = document.HadKeyboardEvent();

  return (always_show_focus || is_text_input || !last_focus_from_mouse ||
          had_keyboard_event);
}

namespace {

// CalculateActivations will not produce any activations unless there is
// an outer activation (i.e. an activation of the outer StyleScope). If there
// is no outer StyleScope, we use this DefaultActivations as the outer
// activation. The scope provided to DefaultActivations is typically
// a ShadowTree.
StyleScopeActivations& DefaultActivations(const ContainerNode* scope) {
  auto* activations = MakeGarbageCollected<StyleScopeActivations>();
  activations->vector = HeapVector<StyleScopeActivation>(
      1, StyleScopeActivation{scope, std::numeric_limits<unsigned>::max()});
  return *activations;
}

// The activation ceiling is the highest ancestor element that can
// match inside some StyleScopeActivation.
//
// You would think that only elements inside the scoping root (activation.root)
// could match, but it is possible for a selector to be matched with respect to
// some scoping root [1] without actually being scoped to that root [2].
//
// This is relevant when matching elements inside a shadow tree, where the root
// of the default activation will be the ShadowRoot, but the host element (which
// sits *above* the ShadowRoot) should still be reached with :host.
//
// [1] https://drafts.csswg.org/selectors-4/#the-scope-pseudo
// [2] https://drafts.csswg.org/selectors-4/#scoped-selector
const Element* ActivationCeiling(const StyleScopeActivation& activation) {
  if (!activation.root) {
    return nullptr;
  }
  if (auto* element = DynamicTo<Element>(activation.root.Get())) {
    return element;
  }
  ShadowRoot* shadow_root = activation.root->GetShadowRoot();
  return shadow_root ? &shadow_root->host() : nullptr;
}

// True if this StyleScope has an implicit root at the specified element.
// This is used to find the roots for prelude-less @scope rules.
bool HasImplicitRoot(const StyleScope& style_scope, Element& element) {
  if (const StyleScopeData* style_scope_data = element.GetStyleScopeData()) {
    return style_scope_data->TriggersScope(style_scope);
  }
  return false;
}

}  // namespace

const StyleScopeActivations& SelectorChecker::EnsureActivations(
    const SelectorCheckingContext& context,
    const StyleScope& style_scope) const {
  DCHECK(context.style_scope_frame);

  // The *outer activations* are the activations of the outer StyleScope.
  // If there is no outer StyleScope, we create a "default" activation to
  // make the code in CalculateActivations more readable.
  //
  // Must not be confused with the *parent activations* (seen in
  // CalculateActivations), which are the activations (for the same StyleScope)
  // of the *parent element*.
  const StyleScopeActivations* outer_activations =
      style_scope.Parent() ? &EnsureActivations(context, *style_scope.Parent())
                           : &DefaultActivations(context.scope);
  // The `match_visited` flag may have been set to false e.g. due to a link
  // having been encountered (see DisallowMatchVisited), but scope activations
  // are calculated lazily when :scope is first seen in a compound selector,
  // and the scoping limit needs to evaluate according to the original setting.
  //
  // Consider the following, which should not match, because the :visited link
  // is a scoping limit:
  //
  //  @scope (#foo) to (:visited) { :scope a:visited { ... } }
  //
  // In the above selector, we first match a:visited, and set match_visited to
  // false since a link was encountered. Then we encounter a compound
  // with :scope, which causes scopes to be activated (kScopeActivation). At
  // this point we try to find the scoping limit (:visited), but it wouldn't
  // match anything because match_visited is set to false, so the selector
  // would incorrectly match. For this reason we need to evaluate the scoping
  // root and limits with the original match_visited setting.
  bool match_visited = context.match_visited || context.had_match_visited;
  // We only use the cache when matching normal/non-visited rules. Otherwise
  // we'd need to double up the cache.
  StyleScopeFrame* style_scope_frame =
      match_visited ? nullptr : context.style_scope_frame;
  const StyleScopeActivations* activations = CalculateActivations(
      context.style_scope_frame->element_, style_scope, *outer_activations,
      style_scope_frame, match_visited);
  DCHECK(activations);
  return *activations;
}

// Calculates all activations (i.e. active scopes) for `element`.
//
// This function will traverse the whole ancestor chain in the worst case,
// however, if a StyleScopeFrame is provided, it will reuse cached results
// found on that StyleScopeFrame.
const StyleScopeActivations* SelectorChecker::CalculateActivations(
    Element& element,
    const StyleScope& style_scope,
    const StyleScopeActivations& outer_activations,
    StyleScopeFrame* style_scope_frame,
    bool match_visited) const {
  Member<const StyleScopeActivations>* cached_activations_entry = nullptr;
  if (style_scope_frame) {
    auto entry = style_scope_frame->data_.insert(&style_scope, nullptr);
    // We must not modify `style_scope_frame->data_` for the remainder
    // of this function, since `cached_activations_entry` now points into
    // the hash table.
    cached_activations_entry = &entry.stored_value->value;
    if (!entry.is_new_entry) {
      DCHECK(cached_activations_entry->Get());
      return cached_activations_entry->Get();
    }
  }

  auto* activations = MakeGarbageCollected<StyleScopeActivations>();

  if (!outer_activations.vector.empty()) {
    const StyleScopeActivations* parent_activations = nullptr;

    // Remain within the outer scope. I.e. don't look at elements above the
    // highest outer activation.
    if (&element != ActivationCeiling(outer_activations.vector.front())) {
      if (Element* parent = element.ParentOrShadowHostElement()) {
        // When calculating the activations on the parent element, we pass
        // the parent StyleScopeFrame (if we have it) to be able to use the
        // cached results, and avoid traversing the ancestor chain.
        StyleScopeFrame* parent_frame =
            style_scope_frame ? style_scope_frame->GetParentFrameOrNull(*parent)
                              : nullptr;
        // Disable :visited matching when encountering the first link.
        // This matches the behavior for regular child/descendant combinators.
        bool parent_match_visited = match_visited && !element.IsLink();
        parent_activations =
            CalculateActivations(*parent, style_scope, outer_activations,
                                 parent_frame, parent_match_visited);
      }
    }

    // The activations of the parent element are still active for this element,
    // unless this element is a scoping limit.
    if (parent_activations) {
      activations->match_flags = parent_activations->match_flags;

      for (const StyleScopeActivation& activation :
           parent_activations->vector) {
        if (!ElementIsScopingLimit(style_scope, activation, element,
                                   match_visited, activations->match_flags)) {
          activations->vector.push_back(
              StyleScopeActivation{activation.root, activation.proximity + 1});
        }
      }
    }

    // Check if we need to add a new activation for this element.
    for (const StyleScopeActivation& outer_activation :
         outer_activations.vector) {
      if (style_scope.From()
              ? MatchesWithScope(element, *style_scope.From(),
                                 outer_activation.root, match_visited,
                                 activations->match_flags)
              : HasImplicitRoot(style_scope, element)) {
        StyleScopeActivation activation{&element, 0};
        // It's possible for a newly created activation to be immediately
        // limited (e.g. @scope (.x) to (.x)).
        if (!ElementIsScopingLimit(style_scope, activation, element,
                                   match_visited, activations->match_flags)) {
          activations->vector.push_back(activation);
        }
        break;
      }
      // TODO(crbug.com/1280240): Break if we don't depend on :scope.
    }
  }

  // Cache the result if possible.
  if (cached_activations_entry) {
    *cached_activations_entry = activations;
  }

  return activations;
}

bool SelectorChecker::MatchesWithScope(Element& element,
                                       const CSSSelector& selector_list,
                                       const ContainerNode* scope,
                                       bool match_visited,
                                       MatchFlags& match_flags) const {
  SelectorCheckingContext context(&element);
  context.scope = scope;
  context.match_visited = match_visited;
  // We are matching this selector list with the intent of storing the result
  // in a cache (StyleScopeFrame). The :scope pseudo-class which triggered
  // this call to MatchesWithScope, is either part of the subject compound
  // or *not* part of the subject compound, but subsequent cache hits which
  // return this result may have the opposite subject/non-subject position.
  // Therefore we're using Impact::kBoth, to ensure sufficient invalidation.
  context.impact = Impact::kBoth;
  for (context.selector = &selector_list; context.selector;
       context.selector = CSSSelectorList::Next(*context.selector)) {
    MatchResult match_result;
    bool match = MatchSelector(context, match_result) ==
                 SelectorChecker::kSelectorMatches;
    match_flags |= match_result.flags;
    if (match) {
      return true;
    }
  }
  return false;
}

bool SelectorChecker::ElementIsScopingLimit(
    const StyleScope& style_scope,
    const StyleScopeActivation& activation,
    Element& element,
    bool match_visited,
    MatchFlags& match_flags) const {
  if (!style_scope.To()) {
    return false;
  }
  return MatchesWithScope(element, *style_scope.To(), activation.root.Get(),
                          match_visited, match_flags);
}

}  // namespace blink
