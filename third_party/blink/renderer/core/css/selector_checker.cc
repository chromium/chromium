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

#include "third_party/blink/renderer/core/css/selector_checker.h"

#include "base/auto_reset.h"
#include "style_rule.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_argument_context.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_cache_scope.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/part_names.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/css_toggle.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/nth_index_cache.h"
#include "third_party/blink/renderer/core/dom/popover_data.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/track/vtt/vtt_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
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
  return html_input_element && html_input_element->list();
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
  if (tag_q_name == AnyQName())
    return true;
  const AtomicString& local_name = tag_q_name.LocalName();
  if (local_name != CSSSelector::UniversalSelectorAtom() &&
      local_name != element.localName()) {
    if (element.IsHTMLElement() || !IsA<HTMLDocument>(element.GetDocument()))
      return false;
    // Non-html elements in html documents are normalized to their camel-cased
    // version during parsing if applicable. Yet, type selectors are lower-cased
    // for selectors in html documents. Compare the upper case converted names
    // instead to allow matching SVG elements like foreignObject.
    if (element.TagQName().LocalNameUpper() != tag_q_name.LocalNameUpper())
      return false;
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
       context.scope->GetTreeScope() == context.element->GetTreeScope()))
    return context.element->ParentOrShadowHostElement();
  return context.element->parentElement();
}

// If context has scope, return slot that matches the scope, otherwise return
// the assigned slot for scope-less matching of ::slotted pseudo element.
static const HTMLSlotElement* FindSlotElementInScope(
    const SelectorChecker::SelectorCheckingContext& context) {
  if (!context.scope)
    return context.element->AssignedSlot();

  for (const HTMLSlotElement* slot = context.element->AssignedSlot(); slot;
       slot = slot->AssignedSlot()) {
    if (slot->GetTreeScope() == context.scope->GetTreeScope())
      return slot;
  }
  return nullptr;
}

static inline bool NextSelectorExceedsScope(
    const SelectorChecker::SelectorCheckingContext& context) {
  if (context.scope && context.scope->IsInShadowTree())
    return context.element == context.scope->OwnerShadowHost();

  return false;
}

static bool ShouldMatchHoverOrActive(
    const SelectorChecker::SelectorCheckingContext& context) {
  // If we're in quirks mode, then :hover and :active should never match anchors
  // with no href and *:hover and *:active should not match anything. This is
  // specified in https://quirks.spec.whatwg.org/#the-:active-and-:hover-quirk
  if (!context.element->GetDocument().InQuirksMode())
    return true;
  if (context.is_sub_selector)
    return true;
  if (context.element->IsLink())
    return true;
  const CSSSelector* selector = context.selector;
  while (selector->Relation() == CSSSelector::kSubSelector &&
         selector->TagHistory()) {
    selector = selector->TagHistory();
    if (selector->Match() != CSSSelector::kPseudoClass)
      return true;
    if (selector->GetPseudoType() != CSSSelector::kPseudoHover &&
        selector->GetPseudoType() != CSSSelector::kPseudoActive)
      return true;
  }
  return false;
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

bool SelectorChecker::Match(const SelectorCheckingContext& context,
                            MatchResult& result) const {
  DCHECK(context.selector);
#if DCHECK_IS_ON()
  DCHECK(!inside_match_) << "Do not re-enter Match: use MatchSelector instead";
  base::AutoReset<bool> reset_inside_match(&inside_match_, true);
#endif  // DCHECK_IS_ON()

  if (UNLIKELY(context.vtt_originating_element)) {
    // A kUAShadow combinator is required for VTT matching.
    if (context.selector->IsLastInTagHistory())
      return false;
  }
  if (MatchSelector(context, result) != kSelectorMatches)
    return false;
  if (context.style_scope != nullptr &&
      RuntimeEnabledFeatures::CSSScopeEnabled() &&
      !CheckInStyleScope(context, result)) {
    return false;
  }
  return true;
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
  if (!CheckOne(context, sub_result))
    return kSelectorFailsLocally;

  if (sub_result.dynamic_pseudo != kPseudoIdNone) {
    result.dynamic_pseudo = sub_result.dynamic_pseudo;
    result.custom_highlight_name = std::move(sub_result.custom_highlight_name);
  }

  if (context.selector->IsLastInTagHistory())
    return kSelectorMatches;

  MatchStatus match;
  if (context.selector->Relation() != CSSSelector::kSubSelector) {
    if (NextSelectorExceedsScope(context))
      return kSelectorFailsCompletely;

    if (context.pseudo_id != kPseudoIdNone &&
        context.pseudo_id != result.dynamic_pseudo)
      return kSelectorFailsCompletely;

    base::AutoReset<PseudoId> dynamic_pseudo_scope(&result.dynamic_pseudo,
                                                   kPseudoIdNone);
    match = MatchForRelation(context, result);
  } else {
    match = MatchForSubSelector(context, result);
  }
  return match;
}

static inline SelectorChecker::SelectorCheckingContext
PrepareNextContextForRelation(
    const SelectorChecker::SelectorCheckingContext& context) {
  SelectorChecker::SelectorCheckingContext next_context(context);
  DCHECK(context.selector->TagHistory());
  next_context.selector = context.selector->TagHistory();
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

  // Only match pseudo classes following scrollbar pseudo elements while
  // actually computing style for scrollbar pseudo elements. This is to
  // avoid incorrectly setting affected-by flags on actual elements for
  // cases like: div::-webkit-scrollbar-thumb:hover { }
  // Also allow pseudo-classes after ::view-transition* pseudo-elements if the
  // class is the rightmost compound in the selector.
  if (context.in_rightmost_compound && dynamic_pseudo != kPseudoIdNone &&
      dynamic_pseudo != kPseudoIdSelection &&
      !IsTransitionPseudoElement(dynamic_pseudo) &&
      !next_context.has_scrollbar_pseudo) {
    return kSelectorFailsCompletely;
  }

  next_context.has_selection_pseudo = dynamic_pseudo == kPseudoIdSelection;
  next_context.is_sub_selector = true;
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
                                     relation != CSSSelector::kChild)))
    next_context.is_inside_visited_link = false;

  next_context.in_rightmost_compound = false;
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
        if (next_context.selector->IsLastInTagHistory()) {
          if (context.scope->IsDocumentFragment())
            return kSelectorMatches;
        }
      }
      for (next_context.element = ParentElement(next_context);
           next_context.element;
           next_context.element = ParentElement(next_context)) {
        MatchStatus match = MatchSelector(next_context, result);
        if (match == kSelectorMatches || match == kSelectorFailsCompletely)
          return match;
        if (NextSelectorExceedsScope(next_context))
          return kSelectorFailsCompletely;
        if (next_context.element->IsLink())
          next_context.is_inside_visited_link = false;
      }
      return kSelectorFailsCompletely;
    case CSSSelector::kRelativeChild:
      DCHECK(result.has_argument_leftmost_compound_matches);
      result.has_argument_leftmost_compound_matches->push_back(context.element);
      [[fallthrough]];
    case CSSSelector::kChild: {
      next_context.element = ParentElement(next_context);
      if (!next_context.element)
        return kSelectorFailsCompletely;
      return MatchSelector(next_context, result);
    }
    case CSSSelector::kRelativeDirectAdjacent:
      DCHECK(result.has_argument_leftmost_compound_matches);
      result.has_argument_leftmost_compound_matches->push_back(context.element);
      [[fallthrough]];
    case CSSSelector::kDirectAdjacent:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent =
                context.element->ParentElementOrShadowRoot())
          parent->SetChildrenAffectedByDirectAdjacentRules();
      }
      next_context.element =
          ElementTraversal::PreviousSibling(*context.element);
      if (!next_context.element)
        return kSelectorFailsAllSiblings;
      return MatchSelector(next_context, result);
    case CSSSelector::kRelativeIndirectAdjacent:
      DCHECK(result.has_argument_leftmost_compound_matches);
      result.has_argument_leftmost_compound_matches->push_back(context.element);
      [[fallthrough]];
    case CSSSelector::kIndirectAdjacent:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent =
                context.element->ParentElementOrShadowRoot())
          parent->SetChildrenAffectedByIndirectAdjacentRules();
      }
      next_context.element =
          ElementTraversal::PreviousSibling(*context.element);
      for (; next_context.element;
           next_context.element =
               ElementTraversal::PreviousSibling(*next_context.element)) {
        MatchStatus match = MatchSelector(next_context, result);
        if (match == kSelectorMatches || match == kSelectorFailsAllSiblings ||
            match == kSelectorFailsCompletely)
          return match;
      }
      return kSelectorFailsAllSiblings;

    case CSSSelector::kUAShadow: {
      // If we're in the same tree-scope as the scoping element, then following
      // a kUAShadow combinator would escape that and thus the scope.
      if (context.scope && context.scope->OwnerShadowHost() &&
          context.scope->OwnerShadowHost()->GetTreeScope() ==
              context.element->GetTreeScope())
        return kSelectorFailsCompletely;

      Element* shadow_host = context.element->OwnerShadowHost();
      if (!shadow_host)
        return kSelectorFailsCompletely;
      // Match against featureless-like Element described by spec:
      // https://w3c.github.io/webvtt/#obtaining-css-boxes
      if (context.vtt_originating_element)
        shadow_host = context.vtt_originating_element;
      next_context.element = shadow_host;
      return MatchSelector(next_context, result);
    }

    case CSSSelector::kShadowSlot: {
      if (ToHTMLSlotElementIfSupportsAssignmentOrNull(*context.element))
        return kSelectorFailsCompletely;
      const HTMLSlotElement* slot = FindSlotElementInScope(context);
      if (!slot)
        return kSelectorFailsCompletely;

      next_context.element = const_cast<HTMLSlotElement*>(slot);
      return MatchSelector(next_context, result);
    }

    case CSSSelector::kShadowPart:
      // We ascend through ancestor shadow host elements until we reach the host
      // in the TreeScope associated with the style rule. We then match against
      // that host.
      while (next_context.element) {
        next_context.element = next_context.element->OwnerShadowHost();
        if (!next_context.element)
          return kSelectorFailsCompletely;

        // Generally a ::part() rule needs to be in the host’s tree scope, but
        // if (and only if) we are preceded by :host or :host(), then the rule
        // could also be in the same scope as the subject.
        TreeScope& host_tree_scope =
            next_context.selector->IsHostPseudoClass()
                ? *context.scope->GetTreeScope().ParentTreeScope()
                : context.scope->GetTreeScope();

        if (next_context.element->GetTreeScope() == host_tree_scope)
          return MatchSelector(next_context, result);
      }
      return kSelectorFailsCompletely;
    case CSSSelector::kSubSelector:
      break;
  }
  NOTREACHED();
  return kSelectorFailsCompletely;
}

static bool AttributeValueMatches(const Attribute& attribute_item,
                                  CSSSelector::MatchType match,
                                  const AtomicString& selector_value,
                                  TextCaseSensitivity case_sensitivity) {
  // TODO(esprehn): How do we get here with a null value?
  const AtomicString& value = attribute_item.Value();
  if (value.IsNull())
    return false;

  switch (match) {
    case CSSSelector::kAttributeExact:
      if (case_sensitivity == kTextCaseSensitive)
        return selector_value == value;
      return EqualIgnoringASCIICase(selector_value, value);
    case CSSSelector::kAttributeSet:
      return true;
    case CSSSelector::kAttributeList: {
      // Ignore empty selectors or selectors containing HTML spaces
      if (selector_value.empty() ||
          selector_value.Find(&IsHTMLSpace<UChar>) != kNotFound)
        return false;

      unsigned start_search_at = 0;
      while (true) {
        wtf_size_t found_pos =
            value.Find(selector_value, start_search_at, case_sensitivity);
        if (found_pos == kNotFound)
          return false;
        if (!found_pos || IsHTMLSpace<UChar>(value[found_pos - 1])) {
          unsigned end_str = found_pos + selector_value.length();
          if (end_str == value.length() || IsHTMLSpace<UChar>(value[end_str]))
            break;  // We found a match.
        }

        // No match. Keep looking.
        start_search_at = found_pos + 1;
      }
      return true;
    }
    case CSSSelector::kAttributeContain:
      if (selector_value.empty())
        return false;
      return value.Contains(selector_value, case_sensitivity);
    case CSSSelector::kAttributeBegin:
      if (selector_value.empty())
        return false;
      return value.StartsWith(selector_value, case_sensitivity);
    case CSSSelector::kAttributeEnd:
      if (selector_value.empty())
        return false;
      return value.EndsWith(selector_value, case_sensitivity);
    case CSSSelector::kAttributeHyphen:
      if (value.length() < selector_value.length())
        return false;
      if (!value.StartsWith(selector_value, case_sensitivity))
        return false;
      // It they start the same, check for exact match or following '-':
      if (value.length() != selector_value.length() &&
          value[selector_value.length()] != '-')
        return false;
      return true;
    default:
      NOTREACHED();
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

  const AtomicString& selector_value = selector.Value();
  TextCaseSensitivity case_sensitivity =
      (selector.AttributeMatch() ==
       CSSSelector::AttributeMatchType::kCaseInsensitive)
          ? kTextCaseASCIIInsensitive
          : kTextCaseSensitive;

  AttributeCollection attributes = element.AttributesWithoutUpdate();
  for (const auto& attribute_item : attributes) {
    if (!attribute_item.Matches(selector_attr)) {
      if (element.IsHTMLElement() || !IsA<HTMLDocument>(element.GetDocument()))
        continue;
      // Non-html attributes in html documents are normalized to their camel-
      // cased version during parsing if applicable. Yet, attribute selectors
      // are lower-cased for selectors in html documents. Compare the selector
      // and the attribute local name insensitively to e.g. allow matching SVG
      // attributes like viewBox.
      //
      // NOTE: If changing this behavior, be sure to also update the bucketing
      // in ElementRuleCollector::CollectMatchingRules() accordingly.
      if (!attribute_item.MatchesCaseInsensitive(selector_attr))
        continue;
    }

    if (AttributeValueMatches(attribute_item, match, selector_value,
                              case_sensitivity))
      return true;

    if (case_sensitivity == kTextCaseASCIIInsensitive) {
      if (selector_attr.NamespaceURI() != g_star_atom)
        return false;
      continue;
    }

    // Legacy dictates that values of some attributes should be compared in
    // a case-insensitive manner regardless of whether the case insensitive
    // flag is set or not.
    bool legacy_case_insensitive = IsA<HTMLDocument>(element.GetDocument()) &&
                                   !selector.IsCaseSensitiveAttribute();

    // If case-insensitive, re-check, and count if result differs.
    // See http://code.google.com/p/chromium/issues/detail?id=327060
    if (legacy_case_insensitive &&
        AttributeValueMatches(attribute_item, match, selector_value,
                              kTextCaseASCIIInsensitive)) {
      // If the `s` modifier is in the attribute selector, return false
      // despite of legacy_case_insensitive.
      if (selector.AttributeMatch() ==
          CSSSelector::AttributeMatchType::kCaseSensitiveAlways) {
        DCHECK(RuntimeEnabledFeatures::CSSCaseSensitiveSelectorEnabled());
        return false;
      }

      UseCounter::Count(element.GetDocument(),
                        WebFeature::kCaseInsensitiveAttrSelectorMatch);
      return true;
    }
    if (selector_attr.NamespaceURI() != g_star_atom)
      return false;
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

  // Only :host and :host-context() should match the host:
  // http://drafts.csswg.org/css-scoping/#host-element
  if (context.scope && context.scope->OwnerShadowHost() == element &&
      (!selector.IsHostPseudoClass() &&
       selector.GetPseudoType() != CSSSelector::kPseudoScope &&
       !context.treat_shadow_host_as_normal_scope &&
       selector.Match() != CSSSelector::kPseudoElement))
    return false;

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
      NOTREACHED();
      return false;
  }
}

bool SelectorChecker::CheckPseudoNot(const SelectorCheckingContext& context,
                                     MatchResult& result) const {
  const CSSSelector& selector = *context.selector;
  DCHECK(selector.SelectorList());
  SelectorCheckingContext sub_context(context);
  sub_context.is_sub_selector = true;
  sub_context.in_nested_complex_selector = true;
  sub_context.pseudo_id = kPseudoIdNone;
  for (sub_context.selector = selector.SelectorList()->First();
       sub_context.selector;
       sub_context.selector = CSSSelectorList::Next(*sub_context.selector)) {
    SubResult sub_result(result);
    if (MatchSelector(sub_context, sub_result) == kSelectorMatches)
      return false;
  }
  return true;
}

namespace {

Element* TraverseToParent(Element* element) {
  return element->parentElement();
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
        if (has_matched_element == has_anchor_element)
          selector_matched = true;
        uint8_t old_result =
            cache_scope_context.SetMatchedAndGetOldResult(has_matched_element);
        if (old_result == kCheckPseudoHasResultNotCached)
          continue;
        if (old_result & kCheckPseudoHasResultMatched)
          break;
      }
    }
    return selector_matched;
  }

  for (auto leftmost : has_argument_leftmost_compound_matches) {
    for (Element* has_matched_element = next(leftmost); has_matched_element;
         has_matched_element = next(has_matched_element)) {
      if (has_matched_element == has_anchor_element)
        return true;
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
        if (has_matched_element == has_anchor_element)
          selector_matched = true;
      }
    }
    return selector_matched;
  }

  for (auto leftmost : has_argument_leftmost_compound_matches) {
    if (Element* has_matched_element = next(leftmost)) {
      if (has_matched_element == has_anchor_element)
        return true;
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
          cache_scope_context, TraverseToParent);
    case CSSSelector::kRelativeChild:
      return CacheMatchedElementsAndReturnMatchedResultForDirectRelation(
          has_anchor_element, has_argument_leftmost_compound_matches,
          cache_scope_context, TraverseToParent);
    case CSSSelector::kRelativeDirectAdjacent:
      return CacheMatchedElementsAndReturnMatchedResultForDirectRelation(
          has_anchor_element, has_argument_leftmost_compound_matches,
          cache_scope_context, TraverseToPreviousSibling);
    case CSSSelector::kRelativeIndirectAdjacent:
      return CacheMatchedElementsAndReturnMatchedResultForIndirectRelation(
          has_anchor_element, has_argument_leftmost_compound_matches,
          cache_scope_context, TraverseToPreviousSibling);
    default:
      NOTREACHED();
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
  if (previous_result & kCheckPseudoHasResultChecked)
    return previous_result;

  // If the selector checking context is for the subject :has() in the argument
  // of the JavaScript API 'matches()', skip to check whether the :has() anchor
  // element was already checked or not.
  if (!ContextForSubjectHasInMatchesArgument(has_checking_context) &&
      cache_scope_context.AlreadyChecked(has_anchor_element)) {
    // If the element already have cache item, set the element as checked.
    // Otherwise, skip to set to prevent increasing unnecessary cache item.
    if (previous_result != kCheckPseudoHasResultNotCached)
      cache_scope_context.SetChecked(has_anchor_element);

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
      NOTREACHED();
      break;
  }
}

void SetAffectedByHasFlagsForHasAnchorSiblings(
    CheckPseudoHasArgumentContext& argument_context,
    Element* has_anchor_element) {
  if (argument_context.AdjacentDistanceLimit() == 0)
    return;
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
      current_element_ = current_element_->parentElement();
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
  if (context.DepthLimit() > 0 && iterator.CurrentDepth() == 0)
    return true;

  // The current element of the iterator cannot be a subject of :has() argument
  // if the :has() argument selector only matches on the elements at a fixed
  // depth and the current element of the iterator is not at the certain depth.
  // (e.g. For the style rule '.a:has(> .b > .c) {}', a child of '.a' or a great
  // grand child of '.a' cannot be a subject of the argument '> .b > .c'. Only
  // the grand child of '.a' can be a subject of the argument)
  if (context.DepthFixed() && (iterator.CurrentDepth() != context.DepthLimit()))
    return true;

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
  if (argument_context.AdjacentDistanceLimit() > 0)
    last_element = ElementTraversal::NextSibling(*last_element);
  if (last_element) {
    if (argument_context.DepthLimit() > 0) {
      last_element = ElementTraversal::FirstChild(*last_element);
      last_depth = 1;
    }
  }
  if (!last_element)
    return;
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
  if (!cache_scope_context.CacheAllowed())
    return kNoEarlyBreak;

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
  if (is_new_entry && !has_anchor_element->AffectedByMultipleHas())
    return kNoEarlyBreak;

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

}  // namespace

bool SelectorChecker::CheckPseudoHas(const SelectorCheckingContext& context,
                                     MatchResult& result) const {
  CheckPseudoHasCacheScope check_pseudo_has_cache_scope(
      &context.element->GetDocument());

  Element* has_anchor_element = context.element;
  Document& document = has_anchor_element->GetDocument();
  DCHECK(document.GetCheckPseudoHasCacheScope());
  SelectorCheckingContext sub_context(has_anchor_element);
  sub_context.scope = context.scope;
  // sub_context.is_inside_visited_link is false (by default) to disable
  // :visited matching when it is in the :has argument
  sub_context.is_inside_has_pseudo_class = true;
  sub_context.pseudo_has_in_rightmost_compound = context.in_rightmost_compound;
  bool update_affected_by_has_flags = mode_ == kResolvingStyle;

  DCHECK(context.selector->SelectorList());
  for (const CSSSelector* selector = context.selector->SelectorList()->First();
       selector; selector = CSSSelectorList::Next(*selector)) {
    CheckPseudoHasArgumentContext argument_context(selector);
    CSSSelector::RelationType leftmost_relation =
        argument_context.LeftmostRelation();
    CheckPseudoHasCacheScope::Context cache_scope_context(&document,
                                                          argument_context);

    if (update_affected_by_has_flags) {
      SetAffectedByHasFlagsForHasAnchorElement(argument_context,
                                               has_anchor_element);
    }

    EarlyBreakOnHasArgumentChecking early_break = CheckEarlyBreakForHasArgument(
        context, has_anchor_element, argument_context, cache_scope_context,
        update_affected_by_has_flags);
    if (early_break == kBreakEarlyAndReturnAsMatched)
      return true;
    else if (early_break == kBreakEarlyAndMoveToNextArgument)
      continue;

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

      if (SkipCheckingHasArgument(argument_context, iterator))
        continue;

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

      if (selector_matched)
        break;
    }

    if (cache_scope_context.CacheAllowed() && last_argument_checked_element) {
      cache_scope_context.SetAllTraversedElementsAsChecked(
          last_argument_checked_element, last_argument_checked_depth);
    }

    if (!selector_matched)
      continue;

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
      if (mode_ == kResolvingStyle)
        element.SetStyleAffectedByEmpty();
      return is_empty;
    }
    case CSSSelector::kPseudoFirstChild:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent = element.ParentElementOrDocumentFragment())
          parent->SetChildrenAffectedByFirstChildRules();
        element.SetAffectedByFirstChildRules();
      }
      return IsFirstChild(element);
    case CSSSelector::kPseudoFirstOfType:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent = element.ParentElementOrDocumentFragment())
          parent->SetChildrenAffectedByForwardPositionalRules();
      }
      return IsFirstOfType(element, element.TagQName());
    case CSSSelector::kPseudoLastChild: {
      ContainerNode* parent = element.ParentElementOrDocumentFragment();
      if (mode_ == kResolvingStyle) {
        if (parent)
          parent->SetChildrenAffectedByLastChildRules();
        element.SetAffectedByLastChildRules();
      }
      if (mode_ != kQueryingRules && parent &&
          !parent->IsFinishedParsingChildren())
        return false;
      return IsLastChild(element);
    }
    case CSSSelector::kPseudoLastOfType: {
      ContainerNode* parent = element.ParentElementOrDocumentFragment();
      if (mode_ == kResolvingStyle) {
        if (parent)
          parent->SetChildrenAffectedByBackwardPositionalRules();
      }
      if (mode_ != kQueryingRules && parent &&
          !parent->IsFinishedParsingChildren())
        return false;
      return IsLastOfType(element, element.TagQName());
    }
    case CSSSelector::kPseudoOnlyChild: {
      if (IsTransitionPseudoElement(context.pseudo_id)) {
        DCHECK(element.IsDocumentElement());
        DCHECK_NE(context.pseudo_id, kPseudoIdViewTransition);
        DCHECK(context.pseudo_argument);

        auto* transition =
            ViewTransitionUtils::GetActiveTransition(element.GetDocument());
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
          !parent->IsFinishedParsingChildren())
        return false;
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
          !parent->IsFinishedParsingChildren())
        return false;
      return IsFirstOfType(element, element.TagQName()) &&
             IsLastOfType(element, element.TagQName());
    }
    case CSSSelector::kPseudoPlaceholderShown:
      if (auto* text_control = ToTextControlOrNull(element))
        return text_control->IsPlaceholderVisible();
      break;
    case CSSSelector::kPseudoNthChild:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent = element.ParentElementOrDocumentFragment())
          parent->SetChildrenAffectedByForwardPositionalRules();
      }
      return selector.MatchNth(NthIndexCache::NthChildIndex(element));
    case CSSSelector::kPseudoNthOfType:
      if (mode_ == kResolvingStyle) {
        if (ContainerNode* parent = element.ParentElementOrDocumentFragment())
          parent->SetChildrenAffectedByForwardPositionalRules();
      }
      return selector.MatchNth(NthIndexCache::NthOfTypeIndex(element));
    case CSSSelector::kPseudoNthLastChild: {
      ContainerNode* parent = element.ParentElementOrDocumentFragment();
      if (mode_ == kResolvingStyle && parent)
        parent->SetChildrenAffectedByBackwardPositionalRules();
      if (mode_ != kQueryingRules && parent &&
          !parent->IsFinishedParsingChildren())
        return false;
      return selector.MatchNth(NthIndexCache::NthLastChildIndex(element));
    }
    case CSSSelector::kPseudoNthLastOfType: {
      ContainerNode* parent = element.ParentElementOrDocumentFragment();
      if (mode_ == kResolvingStyle && parent)
        parent->SetChildrenAffectedByBackwardPositionalRules();
      if (mode_ != kQueryingRules && parent &&
          !parent->IsFinishedParsingChildren())
        return false;
      return selector.MatchNth(NthIndexCache::NthLastOfTypeIndex(element));
    }
    case CSSSelector::kPseudoSelectorFragmentAnchor:
      return MatchesSelectorFragmentAnchorPseudoClass(element);
    case CSSSelector::kPseudoTarget:
      probe::ForcePseudoState(&element, CSSSelector::kPseudoTarget,
                              &force_pseudo_state);
      if (force_pseudo_state)
        return true;
      return element == element.GetDocument().CssTarget();
    case CSSSelector::kPseudoIs:
    case CSSSelector::kPseudoWhere:
    case CSSSelector::kPseudoAny:
    case CSSSelector::kPseudoParent: {
      SelectorCheckingContext sub_context(context);
      sub_context.is_sub_selector = true;
      sub_context.in_nested_complex_selector = true;
      sub_context.pseudo_id = kPseudoIdNone;
      for (sub_context.selector = selector.SelectorListOrParent();
           sub_context.selector; sub_context.selector = CSSSelectorList::Next(
                                     *sub_context.selector)) {
        SubResult sub_result(result);
        if (MatchSelector(sub_context, sub_result) == kSelectorMatches)
          return true;
      }
      break;
    }
    case CSSSelector::kPseudoAutofill:
    case CSSSelector::kPseudoWebKitAutofill: {
      auto* html_form_element = DynamicTo<HTMLFormControlElement>(&element);
      return html_form_element && html_form_element->IsAutofilled();
    }
    case CSSSelector::kPseudoAutofillPreviewed: {
      auto* html_form_element = DynamicTo<HTMLFormControlElement>(&element);
      return html_form_element && html_form_element->GetAutofillState() ==
                                      WebAutofillState::kPreviewed;
    }
    case CSSSelector::kPseudoAutofillSelected: {
      auto* html_form_element = DynamicTo<HTMLFormControlElement>(&element);
      return html_form_element && html_form_element->HighlightAutofilled();
    }
    case CSSSelector::kPseudoAnyLink:
    case CSSSelector::kPseudoWebkitAnyLink:
      return element.IsLink();
    case CSSSelector::kPseudoLink:
      return element.IsLink() && !context.is_inside_visited_link;
    case CSSSelector::kPseudoVisited:
      return element.IsLink() && context.is_inside_visited_link;
    case CSSSelector::kPseudoDrag:
      if (mode_ == kResolvingStyle) {
        if (!context.in_rightmost_compound)
          element.SetChildrenOrSiblingsAffectedByDrag();
      }
      if (context.in_rightmost_compound)
        result.SetFlag(MatchFlag::kAffectedByDrag);
      return element.IsDragged();
    case CSSSelector::kPseudoFocus:
      if (mode_ == kResolvingStyle) {
        if (UNLIKELY(context.is_inside_has_pseudo_class)) {
          element.SetAncestorsOrSiblingsAffectedByFocusInHas();
        } else {
          if (!context.in_rightmost_compound)
            element.SetChildrenOrSiblingsAffectedByFocus();
        }
      }
      return MatchesFocusPseudoClass(element);
    case CSSSelector::kPseudoFocusVisible:
      if (mode_ == kResolvingStyle) {
        if (UNLIKELY(context.is_inside_has_pseudo_class)) {
          element.SetAncestorsOrSiblingsAffectedByFocusVisibleInHas();
        } else {
          if (!context.in_rightmost_compound)
            element.SetChildrenOrSiblingsAffectedByFocusVisible();
        }
      }
      return MatchesFocusVisiblePseudoClass(element);
    case CSSSelector::kPseudoFocusWithin:
      if (mode_ == kResolvingStyle) {
        if (UNLIKELY(context.is_inside_has_pseudo_class)) {
          element.SetAncestorsOrSiblingsAffectedByFocusInHas();
        } else if (!context.in_rightmost_compound) {
          element.SetChildrenOrSiblingsAffectedByFocusWithin();
        }
      }
      if (context.in_rightmost_compound)
        result.SetFlag(MatchFlag::kAffectedByFocusWithin);
      probe::ForcePseudoState(&element, CSSSelector::kPseudoFocusWithin,
                              &force_pseudo_state);
      if (force_pseudo_state)
        return true;
      return element.HasFocusWithin();
    case CSSSelector::kPseudoHover:
      if (mode_ == kResolvingStyle) {
        if (UNLIKELY(context.is_inside_has_pseudo_class)) {
          element.SetAncestorsOrSiblingsAffectedByHoverInHas();
        } else if (!context.in_rightmost_compound) {
          element.SetChildrenOrSiblingsAffectedByHover();
        }
      }
      if (context.in_rightmost_compound)
        result.SetFlag(MatchFlag::kAffectedByHover);
      if (!ShouldMatchHoverOrActive(context))
        return false;
      probe::ForcePseudoState(&element, CSSSelector::kPseudoHover,
                              &force_pseudo_state);
      if (force_pseudo_state)
        return true;
      return element.IsHovered();
    case CSSSelector::kPseudoActive:
      if (mode_ == kResolvingStyle) {
        if (UNLIKELY(context.is_inside_has_pseudo_class)) {
          element.SetAncestorsOrSiblingsAffectedByActiveInHas();
        } else if (!context.in_rightmost_compound) {
          element.SetChildrenOrSiblingsAffectedByActive();
        }
      }
      if (context.in_rightmost_compound)
        result.SetFlag(MatchFlag::kAffectedByActive);
      if (!ShouldMatchHoverOrActive(context))
        return false;
      probe::ForcePseudoState(&element, CSSSelector::kPseudoActive,
                              &force_pseudo_state);
      if (force_pseudo_state)
        return true;
      return element.IsActive();
    case CSSSelector::kPseudoEnabled:
      return element.MatchesEnabledPseudoClass();
    case CSSSelector::kPseudoFullPageMedia:
      return element.GetDocument().IsMediaDocument();
    case CSSSelector::kPseudoDefault:
      return element.MatchesDefaultPseudoClass();
    case CSSSelector::kPseudoDisabled:
      return element.IsDisabledFormControl();
    case CSSSelector::kPseudoReadOnly:
      return element.MatchesReadOnlyPseudoClass();
    case CSSSelector::kPseudoReadWrite:
      return element.MatchesReadWritePseudoClass();
    case CSSSelector::kPseudoOptional:
      return element.IsOptionalFormControl();
    case CSSSelector::kPseudoRequired:
      return element.IsRequiredFormControl();
    case CSSSelector::kPseudoValid:
      return element.MatchesValidityPseudoClasses() && element.IsValidElement();
    case CSSSelector::kPseudoInvalid:
      return element.MatchesValidityPseudoClasses() &&
             !element.IsValidElement();
    case CSSSelector::kPseudoChecked: {
      if (auto* input_element = DynamicTo<HTMLInputElement>(element)) {
        // Even though WinIE allows checked and indeterminate to
        // co-exist, the CSS selector spec says that you can't be
        // both checked and indeterminate. We will behave like WinIE
        // behind the scenes and just obey the CSS spec here in the
        // test for matching the pseudo.
        if (input_element->ShouldAppearChecked() &&
            !input_element->ShouldAppearIndeterminate())
          return true;
      } else if (auto* option_element = DynamicTo<HTMLOptionElement>(element)) {
        if (option_element->Selected()) {
          return true;
        }
      }
      break;
    }
    case CSSSelector::kPseudoIndeterminate:
      return element.ShouldAppearIndeterminate();
    case CSSSelector::kPseudoRoot:
      return element == element.GetDocument().documentElement();
    case CSSSelector::kPseudoLang: {
      auto* vtt_element = DynamicTo<VTTElement>(element);
      AtomicString value = vtt_element ? vtt_element->Language()
                                       : element.ComputeInheritedLanguage();
      const AtomicString& argument = selector.Argument();
      if (value.empty() ||
          !value.StartsWith(argument, kTextCaseASCIIInsensitive))
        break;
      if (value.length() != argument.length() &&
          value[argument.length()] != '-')
        break;
      return true;
    }
    case CSSSelector::kPseudoDir: {
      const AtomicString& argument = selector.Argument();
      if (argument.empty())
        break;

      TextDirection direction;
      if (EqualIgnoringASCIICase(argument, "ltr"))
        direction = TextDirection::kLtr;
      else if (EqualIgnoringASCIICase(argument, "rtl"))
        direction = TextDirection::kRtl;
      else
        break;

      if (auto* html_element = DynamicTo<HTMLElement>(element)) {
        return html_element->CachedDirectionality() == direction;
      }
      break;
    }
    case CSSSelector::kPseudoOpen:
      if (auto* html_element = DynamicTo<HTMLElement>(element);
          html_element && html_element->HasPopoverAttribute()) {
        return html_element->popoverOpen();
      }
      return false;
    case CSSSelector::kPseudoClosed:
      if (!RuntimeEnabledFeatures::HTMLPopoverAttributeEnabled(
              element.GetDocument().GetExecutionContext())) {
        // The html.css UA stylesheet contains a rule for <dialog> elements
        // that uses :closed, with `dialog:not(:not(:closed))`, so it's
        // important to *match* when the feature is *disabled*.
        return true;
      }
      if (auto* html_element = DynamicTo<HTMLElement>(element);
          html_element && html_element->HasPopoverAttribute()) {
        return html_element->GetPopoverData()->visibilityState() ==
               PopoverVisibilityState::kHidden;
      }
      return false;
    case CSSSelector::kPseudoFullscreen:
    // fall through
    case CSSSelector::kPseudoFullScreen:
      return Fullscreen::IsFullscreenElement(element);
    case CSSSelector::kPseudoFullScreenAncestor:
      return element.ContainsFullScreenElement();
    case CSSSelector::kPseudoPaused: {
      DCHECK(RuntimeEnabledFeatures::CSSPseudoPlayingPausedEnabled());
      auto* media_element = DynamicTo<HTMLMediaElement>(element);
      return media_element && media_element->paused();
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
    case CSSSelector::kPseudoInRange:
      return element.IsInRange();
    case CSSSelector::kPseudoOutOfRange:
      return element.IsOutOfRange();
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
    case CSSSelector::kPseudoSpatialNavigationInterest:
      DCHECK(is_ua_rule_);
      return MatchesSpatialNavigationInterestPseudoClass(element);
    case CSSSelector::kPseudoHasDatalist:
      DCHECK(is_ua_rule_);
      return MatchesHasDatalistPseudoClass(element);
    case CSSSelector::kPseudoIsHtml:
      DCHECK(is_ua_rule_);
      return IsA<HTMLDocument>(element.GetDocument());
    case CSSSelector::kPseudoListBox:
      DCHECK(is_ua_rule_);
      return MatchesListBoxPseudoClass(element);
    case CSSSelector::kPseudoMultiSelectFocus:
      DCHECK(is_ua_rule_);
      return MatchesMultiSelectFocusPseudoClass(element);
    case CSSSelector::kPseudoHostHasAppearance:
      DCHECK(is_ua_rule_);
      if (ShadowRoot* root = element.ContainingShadowRoot()) {
        if (!root->IsUserAgent())
          return false;
        const ComputedStyle* style = root->host().GetComputedStyle();
        return style && style->HasEffectiveAppearance();
      }
      return false;
    case CSSSelector::kPseudoWindowInactive:
      if (!context.has_selection_pseudo)
        return false;
      return !element.GetDocument().GetPage()->GetFocusController().IsActive();
    case CSSSelector::kPseudoState: {
      return element.DidAttachInternals() &&
             element.EnsureElementInternals().HasState(selector.Value());
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
      if (Fullscreen::IsFullscreenElement(element))
        return true;
      if (const auto* dialog_element = DynamicTo<HTMLDialogElement>(element))
        return dialog_element->IsModal();
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
        if (context.in_rightmost_compound)
          element.SetAffectedBySubjectHas();
        else
          element.SetAffectedByNonSubjectHas();

        if (selector.ContainsPseudoInsideHasPseudoClass())
          element.SetAffectedByPseudoInHas();

        if (selector.ContainsComplexLogicalCombinationsInsideHasPseudoClass())
          element.SetAffectedByLogicalCombinationsInHas();
      }
      return CheckPseudoHas(context, result);
    case CSSSelector::kPseudoRelativeAnchor:
      DCHECK(context.relative_anchor_element);
      return context.relative_anchor_element == &element;
    case CSSSelector::kPseudoToggle: {
      using State = ToggleRoot::State;

      const AtomicString& name = selector.Argument();
      const State* value = selector.ToggleValue();

      CSSToggle* toggle = CSSToggle::FindToggleInScope(element, name);
      // An element matches :toggle() if the element is in scope for a toggle
      // with the name given by <custom-ident>, and ...
      if (!toggle)
        return false;

      if (value) {
        // ... either the toggle’s value matches the provided <toggle-value>,
        // ...
        return toggle->ValueMatches(*value);
      } else {
        // ... or the <toggle-value> is omitted and the toggle is in any
        // active value.
        return !toggle->ValueMatches(State(0));
      }
    }
    case CSSSelector::kPseudoUnknown:
    default:
      NOTREACHED();
      break;
  }
  return false;
}

static bool MatchesUAShadowElement(Element& element, const AtomicString& id) {
  ShadowRoot* root = element.ContainingShadowRoot();
  return root && root->IsUserAgent() && element.ShadowPseudoId() == id;
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
        if (MatchSelector(sub_context, sub_result) == kSelectorMatches)
          return true;
      }
      return false;
    }
    case CSSSelector::kPseudoPart:
      DCHECK(part_names_);
      for (const auto& part_name : *selector.PartNames()) {
        if (!part_names_->Contains(part_name))
          return false;
      }
      return true;
    case CSSSelector::kPseudoFileSelectorButton:
      return MatchesUAShadowElement(
          element, shadow_element_names::kPseudoFileUploadButton);
    case CSSSelector::kPseudoPlaceholder:
      return MatchesUAShadowElement(
          element, shadow_element_names::kPseudoInputPlaceholder);
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
      if (MatchSelector(sub_context, sub_result) != kSelectorMatches)
        return false;
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
    case CSSSelector::kPseudoViewTransitionGroup:
    case CSSSelector::kPseudoViewTransitionImagePair:
    case CSSSelector::kPseudoViewTransitionOld:
    case CSSSelector::kPseudoViewTransitionNew: {
      if (CSSSelector::GetPseudoId(selector.GetPseudoType()) !=
          context.pseudo_id) {
        return false;
      }
      result.dynamic_pseudo = context.pseudo_id;
      return selector.Argument() == CSSSelector::UniversalSelectorAtom() ||
             selector.Argument() == pseudo_argument_;
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
  if (!context.scope)
    return false;
  const ContainerNode* shadow_host = context.scope->OwnerShadowHost();
  if (!shadow_host || shadow_host != element)
    return false;
  DCHECK(IsShadowHost(element));
  DCHECK(element.GetShadowRoot());

  // For the case with no parameters, i.e. just :host.
  if (!selector.SelectorList())
    return true;

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
    if (MatchSelector(host_context, sub_result) == kSelectorMatches)
      return true;
    host_context.treat_shadow_host_as_normal_scope = false;
    host_context.scope = nullptr;

    if (selector.GetPseudoType() == CSSSelector::kPseudoHost)
      break;

    host_context.in_rightmost_compound = false;
    next_element = FlatTreeTraversal::ParentElement(*next_element);
  } while (next_element);

  // FIXME: this was a fallthrough condition.
  return false;
}

bool SelectorChecker::CheckPseudoScope(const SelectorCheckingContext& context,
                                       MatchResult& result) const {
  Element& element = *context.element;
  if (RuntimeEnabledFeatures::CSSScopeEnabled() && context.style_scope) {
    DCHECK(context.style_scope_frame);
    const Activations& activations =
        EnsureActivations(context, *context.style_scope);
    // The same @scope may produce multiple activations, but only (at most)
    // one activation per element in the ancestor chain. Therefore we do not
    // need to check the list of activations in any particular order.
    for (const StyleScopeActivation& activation : activations) {
      if (&element == activation.root) {
        result.proximity = activation.proximity;
        return true;
      }
    }
    return false;
  }
  if (!context.scope)
    return false;
  if (context.scope->IsElementNode())
    return context.scope == &element;
  return element == element.GetDocument().documentElement();
}

bool SelectorChecker::CheckScrollbarPseudoClass(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  const CSSSelector& selector = *context.selector;

  if (selector.GetPseudoType() == CSSSelector::kPseudoNot)
    return CheckPseudoNot(context, result);

  // FIXME: This is a temporary hack for resizers and scrollbar corners.
  // Eventually :window-inactive should become a real
  // pseudo class and just apply to everything.
  if (selector.GetPseudoType() == CSSSelector::kPseudoWindowInactive)
    return !context.element->GetDocument()
                .GetPage()
                ->GetFocusController()
                .IsActive();

  if (!scrollbar_)
    return false;

  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoEnabled:
      return scrollbar_->Enabled();
    case CSSSelector::kPseudoDisabled:
      return !scrollbar_->Enabled();
    case CSSSelector::kPseudoHover: {
      ScrollbarPart hovered_part = scrollbar_->HoveredPart();
      if (scrollbar_part_ == kScrollbarBGPart)
        return hovered_part != kNoPart;
      if (scrollbar_part_ == kTrackBGPart)
        return hovered_part == kBackTrackPart ||
               hovered_part == kForwardTrackPart || hovered_part == kThumbPart;
      return scrollbar_part_ == hovered_part;
    }
    case CSSSelector::kPseudoActive: {
      ScrollbarPart pressed_part = scrollbar_->PressedPart();
      if (scrollbar_part_ == kScrollbarBGPart)
        return pressed_part != kNoPart;
      if (scrollbar_part_ == kTrackBGPart)
        return pressed_part == kBackTrackPart ||
               pressed_part == kForwardTrackPart || pressed_part == kThumbPart;
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
      if (!scrollbar_->GetTheme().NativeThemeHasButtons())
        return false;
      return scrollbar_part_ == kBackButtonStartPart ||
             scrollbar_part_ == kForwardButtonEndPart ||
             scrollbar_part_ == kBackTrackPart ||
             scrollbar_part_ == kForwardTrackPart;
    case CSSSelector::kPseudoNoButton:
      if (scrollbar_->GetTheme().NativeThemeHasButtons())
        return false;
      return scrollbar_part_ == kBackTrackPart ||
             scrollbar_part_ == kForwardTrackPart;
    case CSSSelector::kPseudoCornerPresent:
      return scrollbar_->GetScrollableArea() &&
             scrollbar_->GetScrollableArea()->IsScrollCornerVisible();
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

bool SelectorChecker::MatchesFocusPseudoClass(const Element& element) {
  bool force_pseudo_state = false;
  probe::ForcePseudoState(const_cast<Element*>(&element),
                          CSSSelector::kPseudoFocus, &force_pseudo_state);
  if (force_pseudo_state)
    return true;
  return element.IsFocused() && IsFrameFocused(element);
}

bool SelectorChecker::MatchesFocusVisiblePseudoClass(const Element& element) {
  bool force_pseudo_state = false;
  probe::ForcePseudoState(const_cast<Element*>(&element),
                          CSSSelector::kPseudoFocusVisible,
                          &force_pseudo_state);
  if (force_pseudo_state)
    return true;

  if (!element.IsFocused() || !IsFrameFocused(element))
    return false;

  const Document& document = element.GetDocument();
  // Exclude shadow hosts with non-UA ShadowRoot.
  if (document.FocusedElement() != element && element.GetShadowRoot() &&
      !element.GetShadowRoot()->IsUserAgent())
    return false;

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

// static
bool SelectorChecker::MatchesSpatialNavigationInterestPseudoClass(
    const Element& element) {
  if (!IsSpatialNavigationEnabled(element.GetDocument().GetFrame()))
    return false;

  if (!RuntimeEnabledFeatures::FocuslessSpatialNavigationEnabled())
    return false;

  DCHECK(element.GetDocument().GetPage());
  Element* interested_element = element.GetDocument()
                                    .GetPage()
                                    ->GetSpatialNavigationController()
                                    .GetInterestedElement();
  return interested_element && *interested_element == element;
}

void SelectorChecker::StyleScopeActivation::Trace(
    blink::Visitor* visitor) const {
  visitor->Trace(root);
}

const SelectorChecker::Activations& SelectorChecker::EnsureActivations(
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
  //
  // TODO(crbug.com/1280240): Pass context.scope instead of nullptr for the
  // default activation.
  const Activations* outer_activations =
      style_scope.Parent()
          ? &EnsureActivations(context, *style_scope.Parent())
          : MakeGarbageCollected<Activations>(
                1, StyleScopeActivation{nullptr /* scope */,
                                        std::numeric_limits<unsigned>::max(),
                                        false});

  auto entry = context.style_scope_frame->data_.insert(&style_scope, nullptr);
  Member<const Activations>& activations = entry.stored_value->value;
  if (entry.is_new_entry) {
    activations = CalculateActivations(context.style_scope_frame->element_,
                                       style_scope, *outer_activations);
  }
  DCHECK(activations.Get());
  return *activations;
}

const SelectorChecker::Activations* SelectorChecker::CalculateActivations(
    Element& element,
    const StyleScope& style_scope,
    const Activations& outer_activations) const {
  auto* activations = MakeGarbageCollected<Activations>();

  if (outer_activations.empty())
    return activations;

  const Activations* parent_activations = nullptr;

  // Remain within the outer scope. I.e. don't look at elements above the
  // highest outer activation.
  if (outer_activations.front().root != &element) {
    // TODO(crbug.com/1280240): Consider :host (etc).
    if (Element* parent = element.parentElement()) {
      parent_activations =
          CalculateActivations(*parent, style_scope, outer_activations);
    }
  }

  // The activations of the parent element are still active for this element,
  // unless the activation was limited.
  if (parent_activations) {
    for (const StyleScopeActivation& activation : *parent_activations) {
      if (!activation.limit) {
        activations->push_back(StyleScopeActivation{
            activation.root, activation.proximity + 1, false});
      }
    }
  }

  // Check if we need to add a new activation for this element.
  for (const StyleScopeActivation& activation : outer_activations) {
    if (MatchesWithScope(element, style_scope.From(), activation.root)) {
      activations->push_back(StyleScopeActivation{&element, 0, false});
      break;
    }
    // TODO(crbug.com/1280240): Break if we don't depend on :scope.
  }

  if (style_scope.To()) {
    for (StyleScopeActivation& activation : *activations) {
      DCHECK(!activation.limit);
      if (MatchesWithScope(element, *style_scope.To(), activation.root.Get())) {
        // TODO(crbug.com/1280240): If we don't depend on :scope, just set all
        // to limit=true.
        activation.limit = true;
      }
    }
  }

  return activations;
}

bool SelectorChecker::MatchesWithScope(Element& element,
                                       const CSSSelectorList& selector_list,
                                       Element* scope) const {
  SelectorCheckingContext context(&element);
  context.scope = scope;
  for (context.selector = selector_list.First(); context.selector;
       context.selector = CSSSelectorList::Next(*context.selector)) {
    SelectorChecker::MatchResult ignore_result;
    if (MatchSelector(context, ignore_result) ==
        SelectorChecker::kSelectorMatches) {
      return true;
    }
  }
  return false;
}

bool SelectorChecker::CheckInStyleScope(const SelectorCheckingContext& context,
                                        MatchResult& result) const {
  SelectorCheckingContext local_context(context);

  // TODO(crbug.com/1280240): We can probably skip this if the main selector
  // contained :scope.

  for (; local_context.element;
       local_context.element = ParentElement(local_context)) {
    if (CheckPseudoScope(local_context, result))
      return true;
    // TODO(crbug.com/1280240): Early-out if there are no activations.
  }

  return false;
}

}  // namespace blink
