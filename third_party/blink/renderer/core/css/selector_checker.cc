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
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/part_names.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/nth_index_cache.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/dom/v0_insertion_point.h"
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
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/core/scroll/scrollbar_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
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

static bool MatchesTagNameForVTT(
    const Element& element,
    const QualifiedName& tag_q_name,
    const SelectorChecker::SelectorCheckingContext& context) {
  if (tag_q_name == AnyQName())
    return true;

  const AtomicString& local_name = tag_q_name.LocalName();

  // The originating element for the cues has no explicit name.
  if (!context.in_rightmost_compound && !local_name.IsEmpty() &&
      local_name != CSSSelector::UniversalSelectorAtom()) {
    return false;
  }

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
  return namespace_uri == g_star_atom || namespace_uri.IsEmpty();
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

static bool ScopeContainsLastMatchedElement(
    const SelectorChecker::SelectorCheckingContext& context) {
  // If this context isn't scoped, skip checking.
  if (!context.scope)
    return true;

  if (context.scope->GetTreeScope() == context.element->GetTreeScope())
    return true;

  // Because Blink treats a shadow host's TreeScope as a separate one from its
  // descendent shadow roots, if the last matched element is a shadow host, the
  // condition above isn't met, even though it should be.
  return context.element == context.scope->OwnerShadowHost() &&
         (!context.previous_element ||
          context.previous_element->IsInDescendantTreeOf(context.element));
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

static bool IsFirstChild(Element& element) {
  return !ElementTraversal::PreviousSibling(element);
}

static bool IsLastChild(Element& element) {
  return !ElementTraversal::NextSibling(element);
}

static bool IsFirstOfType(Element& element, const QualifiedName& type) {
  return !ElementTraversal::PreviousSibling(element, HasTagName(type));
}

static bool IsLastOfType(Element& element, const QualifiedName& type) {
  return !ElementTraversal::NextSibling(element, HasTagName(type));
}

bool SelectorChecker::Match(const SelectorCheckingContext& context,
                            MatchResult& result) const {
  DCHECK(context.selector);
  if (context.is_from_vtt)
    return MatchVTTBlockSelector(context, result);
  return MatchSelector(context, result) == kSelectorMatches;
}

bool SelectorChecker::MatchVTTBlockSelector(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  DCHECK(context.selector);
  if (context.selector->IsLastInTagHistory())
    return false;

  return MatchSelectorForVTT(context, result) == kSelectorMatches;
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
  MatchResult sub_result;
  if (!CheckOne(context, sub_result))
    return kSelectorFailsLocally;

  if (sub_result.dynamic_pseudo != kPseudoIdNone)
    result.dynamic_pseudo = sub_result.dynamic_pseudo;

  if (context.selector->IsLastInTagHistory()) {
    if (ScopeContainsLastMatchedElement(context)) {
      result.specificity += sub_result.specificity;
      return kSelectorMatches;
    }
    return kSelectorFailsLocally;
  }

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
  if (match == kSelectorMatches)
    result.specificity += sub_result.specificity;
  return match;
}

SelectorChecker::MatchStatus SelectorChecker::MatchSelectorForVTT(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  MatchResult sub_result;
  if (!CheckOneForVTT(context, sub_result))
    return kSelectorFailsLocally;

  if (sub_result.dynamic_pseudo != kPseudoIdNone)
    result.dynamic_pseudo = sub_result.dynamic_pseudo;

  if (context.selector->IsLastInTagHistory()) {
    if (ScopeContainsLastMatchedElement(context)) {
      result.specificity += sub_result.specificity;
      return kSelectorMatches;
    }
    return kSelectorFailsLocally;
  }

  MatchStatus match;
  if (context.selector->Relation() != CSSSelector::kSubSelector) {
    if (NextSelectorExceedsScope(context))
      return kSelectorFailsCompletely;

    if (context.pseudo_id != kPseudoIdNone &&
        context.pseudo_id != result.dynamic_pseudo)
      return kSelectorFailsCompletely;

    base::AutoReset<PseudoId> dynamic_pseudo_scope(&result.dynamic_pseudo,
                                                   kPseudoIdNone);
    match = MatchForRelationForVTT(context, result);
  } else {
    match = MatchForSubSelectorForVTT(context, result);
  }
  if (match == kSelectorMatches)
    result.specificity += sub_result.specificity;
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
  if (context.in_rightmost_compound && dynamic_pseudo != kPseudoIdNone &&
      dynamic_pseudo != kPseudoIdSelection &&
      !next_context.has_scrollbar_pseudo) {
    return kSelectorFailsCompletely;
  }

  next_context.has_selection_pseudo = dynamic_pseudo == kPseudoIdSelection;
  next_context.is_sub_selector = true;
  return MatchSelector(next_context, result);
}

SelectorChecker::MatchStatus SelectorChecker::MatchForSubSelectorForVTT(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  SelectorCheckingContext next_context = PrepareNextContextForRelation(context);
  next_context.is_sub_selector = true;
  return MatchSelectorForVTT(next_context, result);
}

static inline bool IsV0ShadowRoot(const Node* node) {
  auto* shadow_root = DynamicTo<ShadowRoot>(node);
  return shadow_root && shadow_root->GetType() == ShadowRootType::V0;
}

SelectorChecker::MatchStatus SelectorChecker::MatchForPseudoShadow(
    const SelectorCheckingContext& context,
    const ContainerNode* node,
    MatchResult& result) const {
  if (!IsV0ShadowRoot(node))
    return kSelectorFailsCompletely;
  if (!context.previous_element)
    return kSelectorFailsCompletely;
  return MatchSelector(context, result);
}

static inline Element* ParentOrV0ShadowHostElement(const Element& element) {
  if (auto* shadow_root = DynamicTo<ShadowRoot>(element.parentNode())) {
    if (shadow_root->GetType() != ShadowRootType::V0)
      return nullptr;
  }
  return element.ParentOrShadowHostElement();
}

SelectorChecker::MatchStatus SelectorChecker::MatchForRelation(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  SelectorCheckingContext next_context = PrepareNextContextForRelation(context);

  CSSSelector::RelationType relation = context.selector->Relation();

  // Disable :visited matching when we see the first link or try to match
  // anything else than an ancestors.
  //
  // FIXME(emilio): This is_sub_selector check is wrong if we allow sub
  // selectors with combinators somewhere.
  if (!context.is_sub_selector &&
      (context.element->IsLink() || (relation != CSSSelector::kDescendant &&
                                     relation != CSSSelector::kChild)))
    next_context.visited_match_type = kVisitedMatchDisabled;

  next_context.in_rightmost_compound = false;
  next_context.is_sub_selector = false;
  next_context.previous_element = context.element;
  next_context.pseudo_id = kPseudoIdNone;

  switch (relation) {
    case CSSSelector::kShadowDeepAsDescendant:
      Deprecation::CountDeprecation(context.element->GetExecutionContext(),
                                    WebFeature::kCSSDeepCombinator);
      FALLTHROUGH;
    case CSSSelector::kDescendant:
      if (next_context.selector->GetPseudoType() == CSSSelector::kPseudoScope) {
        if (next_context.selector->IsLastInTagHistory()) {
          if (context.scope->IsDocumentFragment())
            return kSelectorMatches;
        }
      }

      if (context.selector->RelationIsAffectedByPseudoContent()) {
        for (Element* element = context.element; element;
             element = element->parentElement()) {
          if (MatchForPseudoContent(next_context, *element, result) ==
              kSelectorMatches)
            return kSelectorMatches;
        }
        return kSelectorFailsCompletely;
      }

      if (next_context.selector->GetPseudoType() == CSSSelector::kPseudoShadow)
        return MatchForPseudoShadow(
            next_context, context.element->ContainingShadowRoot(), result);

      for (next_context.element = ParentElement(next_context);
           next_context.element;
           next_context.element = ParentElement(next_context)) {
        MatchStatus match = MatchSelector(next_context, result);
        if (match == kSelectorMatches || match == kSelectorFailsCompletely)
          return match;
        if (NextSelectorExceedsScope(next_context))
          return kSelectorFailsCompletely;
        if (next_context.element->IsLink())
          next_context.visited_match_type = kVisitedMatchDisabled;
      }
      return kSelectorFailsCompletely;
    case CSSSelector::kChild: {
      if (next_context.selector->GetPseudoType() == CSSSelector::kPseudoScope) {
        if (next_context.selector->IsLastInTagHistory()) {
          if (context.element->parentNode() == context.scope &&
              context.scope->IsDocumentFragment())
            return kSelectorMatches;
        }
      }

      if (context.selector->RelationIsAffectedByPseudoContent())
        return MatchForPseudoContent(next_context, *context.element, result);

      if (next_context.selector->GetPseudoType() == CSSSelector::kPseudoShadow)
        return MatchForPseudoShadow(next_context, context.element->parentNode(),
                                    result);

      next_context.element = ParentElement(next_context);
      if (!next_context.element)
        return kSelectorFailsCompletely;
      return MatchSelector(next_context, result);
    }
    case CSSSelector::kDirectAdjacent:
      // Shadow roots can't have sibling elements
      if (next_context.selector->GetPseudoType() == CSSSelector::kPseudoShadow)
        return kSelectorFailsCompletely;

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

    case CSSSelector::kIndirectAdjacent:
      // Shadow roots can't have sibling elements
      if (next_context.selector->GetPseudoType() == CSSSelector::kPseudoShadow)
        return kSelectorFailsCompletely;

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

    case CSSSelector::kShadowPseudo: {
      DCHECK(mode_ == kQueryingRules ||
             context.selector->GetPseudoType() != CSSSelector::kPseudoShadow);
      if (context.selector->GetPseudoType() == CSSSelector::kPseudoShadow) {
        UseCounter::Count(context.element->GetDocument(),
                          WebFeature::kPseudoShadowInStaticProfile);
      }
      // If we're in the same tree-scope as the scoping element, then following
      // a shadow descendant combinator would escape that and thus the scope.
      if (context.scope && context.scope->OwnerShadowHost() &&
          context.scope->OwnerShadowHost()->GetTreeScope() ==
              context.element->GetTreeScope())
        return kSelectorFailsCompletely;

      Element* shadow_host = context.element->OwnerShadowHost();
      if (!shadow_host)
        return kSelectorFailsCompletely;
      next_context.element = shadow_host;
      return MatchSelector(next_context, result);
    }

    case CSSSelector::kShadowDeep: {
      DCHECK(mode_ == kQueryingRules);
      UseCounter::Count(context.element->GetDocument(),
                        WebFeature::kDeepCombinatorInStaticProfile);
      if (ShadowRoot* root = context.element->ContainingShadowRoot()) {
        if (root->IsUserAgent())
          return kSelectorFailsCompletely;
      }

      if (context.selector->RelationIsAffectedByPseudoContent()) {
        // TODO(kochi): closed mode tree should be handled as well for
        // ::content.
        for (Element* element = context.element; element;
             element = element->ParentOrShadowHostElement()) {
          if (MatchForPseudoContent(next_context, *element, result) ==
              kSelectorMatches) {
            if (context.element->IsInShadowTree()) {
              UseCounter::Count(context.element->GetDocument(),
                                WebFeature::kCSSDeepCombinatorAndShadow);
            }
            return kSelectorMatches;
          }
        }
        return kSelectorFailsCompletely;
      }

      for (next_context.element = ParentOrV0ShadowHostElement(*context.element);
           next_context.element;
           next_context.element =
               ParentOrV0ShadowHostElement(*next_context.element)) {
        MatchStatus match = MatchSelector(next_context, result);
        if (match == kSelectorMatches && context.element->IsInShadowTree()) {
          UseCounter::Count(context.element->GetDocument(),
                            WebFeature::kCSSDeepCombinatorAndShadow);
        }
        if (match == kSelectorMatches || match == kSelectorFailsCompletely)
          return match;
        if (NextSelectorExceedsScope(next_context))
          return kSelectorFailsCompletely;
      }
      return kSelectorFailsCompletely;
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

        if (next_context.element->GetTreeScope() ==
            context.scope->GetTreeScope())
          return MatchSelector(next_context, result);
      }
      return kSelectorFailsCompletely;
    case CSSSelector::kSubSelector:
      break;
  }
  NOTREACHED();
  return kSelectorFailsCompletely;
}

SelectorChecker::MatchStatus SelectorChecker::MatchForRelationForVTT(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  SelectorCheckingContext next_context = PrepareNextContextForRelation(context);

  CSSSelector::RelationType relation = context.selector->Relation();

  // Rules that come from a WebVTT STYLE block apply to a hypothetical
  // document with a single empty element with no explicit name, no namespace,
  // no attribute, no classes, no IDs, and unknown primary language that acts
  // as the originating element for the cue pseudo-elements. This element
  // must not be generally selectable.
  if (relation != CSSSelector::kShadowPseudo)
    return kSelectorFailsCompletely;

  if (!context.is_sub_selector)
    next_context.visited_match_type = kVisitedMatchDisabled;

  next_context.in_rightmost_compound = false;
  next_context.is_sub_selector = false;
  next_context.previous_element = context.element;
  next_context.pseudo_id = kPseudoIdNone;

  DCHECK(mode_ == kQueryingRules ||
         context.selector->GetPseudoType() != CSSSelector::kPseudoShadow);
  if (context.selector->GetPseudoType() == CSSSelector::kPseudoShadow) {
    UseCounter::Count(context.element->GetDocument(),
                      WebFeature::kPseudoShadowInStaticProfile);
  }
  // If we're in the same tree-scope as the scoping element, then following
  // a shadow descendant combinator would escape that and thus the scope.
  if (context.scope && context.scope->OwnerShadowHost() &&
      context.scope->OwnerShadowHost()->GetTreeScope() ==
          context.element->GetTreeScope())
    return kSelectorFailsCompletely;

  Element* shadow_host = context.element->OwnerShadowHost();
  if (!shadow_host)
    return kSelectorFailsCompletely;
  next_context.element = shadow_host;
  return MatchSelectorForVTT(next_context, result);
}

SelectorChecker::MatchStatus SelectorChecker::MatchForPseudoContent(
    const SelectorCheckingContext& context,
    const Element& element,
    MatchResult& result) const {
  HeapVector<Member<V0InsertionPoint>, 8> insertion_points;
  CollectDestinationInsertionPoints(element, insertion_points);
  SelectorCheckingContext next_context(context);
  for (const auto& insertion_point : insertion_points) {
    next_context.element = insertion_point;
    if (Match(next_context, result))
      return kSelectorMatches;
  }
  return kSelectorFailsLocally;
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
      if (selector_value.IsEmpty() ||
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
      if (selector_value.IsEmpty())
        return false;
      return value.Contains(selector_value, case_sensitivity);
    case CSSSelector::kAttributeBegin:
      if (selector_value.IsEmpty())
        return false;
      return value.StartsWith(selector_value, case_sensitivity);
    case CSSSelector::kAttributeEnd:
      if (selector_value.IsEmpty())
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
    bool legacy_case_insensitive =
        IsA<HTMLDocument>(element.GetDocument()) &&
        !HTMLDocument::IsCaseSensitiveAttribute(selector_attr);

    // If case-insensitive, re-check, and count if result differs.
    // See http://code.google.com/p/chromium/issues/detail?id=327060
    if (legacy_case_insensitive &&
        AttributeValueMatches(attribute_item, match, selector_value,
                              kTextCaseASCIIInsensitive)) {
      UseCounter::Count(element.GetDocument(),
                        WebFeature::kCaseInsensitiveAttrSelectorMatch);
      return true;
    }
    if (selector_attr.NamespaceURI() != g_star_atom)
      return false;
  }

  return false;
}

bool SelectorChecker::CheckOne(const SelectorCheckingContext& context,
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

bool SelectorChecker::CheckOneForVTT(const SelectorCheckingContext& context,
                                     MatchResult& result) const {
  DCHECK(context.element);
  Element& element = *context.element;
  DCHECK(context.selector);
  const CSSSelector& selector = *context.selector;

  switch (selector.Match()) {
    case CSSSelector::kTag:
      return MatchesTagNameForVTT(element, selector.TagQName(), context);
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
      return CheckPseudoClassForVTT(context, result);
    case CSSSelector::kPseudoElement:
      return CheckPseudoElementForVTT(context, result);

    default:
      return false;
  }
}

bool SelectorChecker::CheckPseudoNot(const SelectorCheckingContext& context,
                                     MatchResult& result) const {
  const CSSSelector& selector = *context.selector;

  SelectorCheckingContext sub_context(context);
  sub_context.is_sub_selector = true;
  DCHECK(selector.SelectorList());
  for (sub_context.selector = selector.SelectorList()->First();
       sub_context.selector;
       sub_context.selector = sub_context.selector->TagHistory()) {
    // :not cannot nest. I don't really know why this is a
    // restriction in CSS3, but it is, so let's honor it.
    // the parser enforces that this never occurs
    DCHECK_NE(sub_context.selector->GetPseudoType(), CSSSelector::kPseudoNot);
    // We select between :visited and :link when applying. We don't know which
    // one applied (or not) yet.
    if (sub_context.selector->GetPseudoType() == CSSSelector::kPseudoVisited ||
        (sub_context.selector->GetPseudoType() == CSSSelector::kPseudoLink &&
         sub_context.visited_match_type == kVisitedMatchEnabled))
      return true;
    if (!CheckOne(sub_context, result))
      return true;
  }
  return false;
}

bool SelectorChecker::CheckPseudoNotForVTT(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  const CSSSelector& selector = *context.selector;
  SelectorCheckingContext sub_context(context);
  sub_context.is_sub_selector = true;
  DCHECK(selector.SelectorList());
  for (sub_context.selector = selector.SelectorList()->First();
       sub_context.selector;
       sub_context.selector = sub_context.selector->TagHistory()) {
    // :not cannot nest. I don't really know why this is a
    // restriction in CSS3, but it is, so let's honor it.
    // the parser enforces that this never occurs
    DCHECK_NE(sub_context.selector->GetPseudoType(), CSSSelector::kPseudoNot);
    // We select between :visited and :link when applying. We don't know which
    // one applied (or not) yet.
    if (sub_context.selector->GetPseudoType() == CSSSelector::kPseudoVisited ||
        (sub_context.selector->GetPseudoType() == CSSSelector::kPseudoLink &&
         sub_context.visited_match_type == kVisitedMatchEnabled))
      return true;
    if (!CheckOneForVTT(sub_context, result))
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
          if (!text_node->data().IsEmpty()) {
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
    case CSSSelector::kPseudoTarget:
      return element == element.GetDocument().CssTarget();
    case CSSSelector::kPseudoIs:
    case CSSSelector::kPseudoWhere:
    case CSSSelector::kPseudoAny: {
      SelectorCheckingContext sub_context(context);
      sub_context.is_sub_selector = true;
      if (!selector.SelectorList())
        break;
      for (sub_context.selector = selector.SelectorList()->First();
           sub_context.selector; sub_context.selector = CSSSelectorList::Next(
                                     *sub_context.selector)) {
        MatchResult sub_result;
        if (MatchSelector(sub_context, sub_result) == kSelectorMatches)
          return true;
      }
    } break;
    case CSSSelector::kPseudoAutofill: {
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
      return html_form_element && html_form_element->GetAutofillState() ==
                                      WebAutofillState::kAutofilled;
    }
    case CSSSelector::kPseudoAnyLink:
    case CSSSelector::kPseudoWebkitAnyLink:
    case CSSSelector::kPseudoLink:
      return element.IsLink();
    case CSSSelector::kPseudoVisited:
      return element.IsLink() &&
             context.visited_match_type == kVisitedMatchEnabled;
    case CSSSelector::kPseudoDrag:
      if (mode_ == kResolvingStyle) {
        if (context.in_rightmost_compound)
          element_style_->SetAffectedByDrag();
        else
          element.SetChildrenOrSiblingsAffectedByDrag();
      }
      return element.IsDragged();
    case CSSSelector::kPseudoFocus:
      if (mode_ == kResolvingStyle && !context.in_rightmost_compound)
        element.SetChildrenOrSiblingsAffectedByFocus();
      return MatchesFocusPseudoClass(element);
    case CSSSelector::kPseudoFocusVisible:
      if (mode_ == kResolvingStyle && !context.in_rightmost_compound)
        element.SetChildrenOrSiblingsAffectedByFocusVisible();
      return MatchesFocusVisiblePseudoClass(element);
    case CSSSelector::kPseudoFocusWithin:
      if (mode_ == kResolvingStyle) {
        if (context.in_rightmost_compound)
          element_style_->SetAffectedByFocusWithin();
        else
          element.SetChildrenOrSiblingsAffectedByFocusWithin();
      }
      probe::ForcePseudoState(&element, CSSSelector::kPseudoFocusWithin,
                              &force_pseudo_state);
      if (force_pseudo_state)
        return true;
      return element.HasFocusWithin();
    case CSSSelector::kPseudoHover:
      if (mode_ == kResolvingStyle) {
        if (context.in_rightmost_compound)
          element_style_->SetAffectedByHover();
        else
          element.SetChildrenOrSiblingsAffectedByHover();
      }
      if (!ShouldMatchHoverOrActive(context))
        return false;
      probe::ForcePseudoState(&element, CSSSelector::kPseudoHover,
                              &force_pseudo_state);
      if (force_pseudo_state)
        return true;
      return element.IsHovered();
    case CSSSelector::kPseudoActive:
      if (mode_ == kResolvingStyle) {
        if (context.in_rightmost_compound)
          element_style_->SetAffectedByActive();
        else
          element.SetChildrenOrSiblingsAffectedByActive();
      }
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
      if (value.IsEmpty() ||
          !value.StartsWith(argument, kTextCaseASCIIInsensitive))
        break;
      if (value.length() != argument.length() &&
          value[argument.length()] != '-')
        break;
      return true;
    }
    case CSSSelector::kPseudoFullscreen:
    // fall through
    case CSSSelector::kPseudoFullScreen:
      return Fullscreen::IsFullscreenElement(element);
    case CSSSelector::kPseudoFullScreenAncestor:
      return element.ContainsFullScreenElement();
    case CSSSelector::kPseudoPictureInPicture:
      return PictureInPictureController::IsElementInPictureInPicture(&element);
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
      if (!context.scope)
        return false;
      if (context.scope == &element.GetDocument())
        return element == element.GetDocument().documentElement();
      if (auto* shadow_root = DynamicTo<ShadowRoot>(context.scope))
        return element == shadow_root->host();
      return context.scope == &element;
    case CSSSelector::kPseudoUnresolved:
      return !element.IsDefined() && element.IsUnresolvedV0CustomElement();
    case CSSSelector::kPseudoDefined:
      return element.IsDefined() || element.IsUpgradedV0CustomElement();
    case CSSSelector::kPseudoHostContext:
      UseCounter::Count(
          context.element->GetDocument(),
          mode_ == kQueryingRules
              ? WebFeature::kCSSSelectorHostContextInSnapshotProfile
              : WebFeature::kCSSSelectorHostContextInLiveProfile);
      FALLTHROUGH;
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
    case CSSSelector::kPseudoUnknown:
    default:
      NOTREACHED();
      break;
  }
  return false;
}

bool SelectorChecker::CheckPseudoClassForVTT(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  const CSSSelector& selector = *context.selector;

  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoNot:
      return CheckPseudoNotForVTT(context, result);
    case CSSSelector::kPseudoAny: {
      SelectorCheckingContext sub_context(context);
      sub_context.is_sub_selector = true;
      DCHECK(selector.SelectorList());
      for (sub_context.selector = selector.SelectorList()->First();
           sub_context.selector; sub_context.selector = CSSSelectorList::Next(
                                     *sub_context.selector)) {
        MatchResult sub_result;
        if (MatchSelectorForVTT(sub_context, sub_result) == kSelectorMatches)
          return true;
      }
    } break;
    case CSSSelector::kPseudoHostContext:
      FALLTHROUGH;
    case CSSSelector::kPseudoHost:
      return false;
    default:
      return CheckPseudoClass(context, result);
  }
  return false;
}

bool SelectorChecker::CheckPseudoElement(const SelectorCheckingContext& context,
                                         MatchResult& result) const {
  const CSSSelector& selector = *context.selector;
  Element& element = *context.element;

  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoCue: {
      SelectorCheckingContext sub_context(context);
      sub_context.is_sub_selector = true;
      sub_context.scope = nullptr;
      sub_context.treat_shadow_host_as_normal_scope = false;

      for (sub_context.selector = selector.SelectorList()->First();
           sub_context.selector; sub_context.selector = CSSSelectorList::Next(
                                     *sub_context.selector)) {
        MatchResult sub_result;
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
    case CSSSelector::kPseudoPlaceholder:
      if (ShadowRoot* root = element.ContainingShadowRoot()) {
        return root->IsUserAgent() &&
               element.ShadowPseudoId() ==
                   shadow_element_names::kPseudoInputPlaceholder;
      }
      return false;
    case CSSSelector::kPseudoWebKitCustomElement: {
      if (ShadowRoot* root = element.ContainingShadowRoot()) {
        if (!root->IsUserAgent())
          return false;
        if (element.ShadowPseudoId() != selector.Value())
          return false;
        if (!is_ua_rule_ &&
            selector.Value() ==
                shadow_element_names::kPseudoWebKitDetailsMarker) {
          UseCounter::Count(element.GetDocument(),
                            WebFeature::kCSSSelectorPseudoWebKitDetailsMarker);
        }
        return true;
      }
      return false;
    }
    case CSSSelector::kPseudoBlinkInternalElement:
      DCHECK(is_ua_rule_);
      if (ShadowRoot* root = element.ContainingShadowRoot())
        return root->IsUserAgent() &&
               element.ShadowPseudoId() == selector.Value();
      return false;
    case CSSSelector::kPseudoSlotted: {
      SelectorCheckingContext sub_context(context);
      sub_context.is_sub_selector = true;
      sub_context.scope = nullptr;
      sub_context.treat_shadow_host_as_normal_scope = false;

      // ::slotted() only allows one compound selector.
      DCHECK(selector.SelectorList()->First());
      DCHECK(!CSSSelectorList::Next(*selector.SelectorList()->First()));
      sub_context.selector = selector.SelectorList()->First();
      MatchResult sub_result;
      if (!Match(sub_context, sub_result))
        return false;
      result.specificity += sub_context.selector->Specificity() +
                            sub_result.specificity +
                            CSSSelector::kTagSpecificity;
      return true;
    }
    case CSSSelector::kPseudoContent:
      return element.IsInShadowTree() && element.IsV0InsertionPoint();
    case CSSSelector::kPseudoShadow:
      return element.IsInShadowTree() && context.previous_element;
    default:
      DCHECK_NE(mode_, kQueryingRules);
      result.dynamic_pseudo =
          CSSSelector::GetPseudoId(selector.GetPseudoType());
      DCHECK_NE(result.dynamic_pseudo, kPseudoIdNone);
      return true;
  }
}

bool SelectorChecker::CheckPseudoElementForVTT(
    const SelectorCheckingContext& context,
    MatchResult& result) const {
  const CSSSelector& selector = *context.selector;
  Element& element = *context.element;
  switch (selector.GetPseudoType()) {
    case CSSSelector::kPseudoCue: {
      SelectorCheckingContext sub_context(context);
      sub_context.is_sub_selector = true;
      sub_context.scope = nullptr;
      sub_context.treat_shadow_host_as_normal_scope = false;

      for (sub_context.selector = selector.SelectorList()->First();
           sub_context.selector; sub_context.selector = CSSSelectorList::Next(
                                     *sub_context.selector)) {
        MatchResult sub_result;
        if (MatchSelectorForVTT(sub_context, sub_result) == kSelectorMatches)
          return true;
      }
      return false;
    }
    case CSSSelector::kPseudoWebKitCustomElement: {
      if (ShadowRoot* root = element.ContainingShadowRoot())
        return root->IsUserAgent() &&
               element.ShadowPseudoId() == selector.Value();
      return false;
    }
    default:
      return false;
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
  bool is_v1_shadow = element.GetShadowRoot()->IsV1();

  // For the case with no parameters, i.e. just :host.
  if (!selector.SelectorList()) {
    if (is_v1_shadow)
      result.specificity += CSSSelector::kClassLikeSpecificity;
    return true;
  }

  SelectorCheckingContext sub_context(context);
  sub_context.is_sub_selector = true;

  bool matched = false;
  unsigned max_specificity = 0;

  // If one of simple selectors matches an element, returns SelectorMatches.
  // Just "OR".
  for (sub_context.selector = selector.SelectorList()->First();
       sub_context.selector;
       sub_context.selector = CSSSelectorList::Next(*sub_context.selector)) {
    sub_context.treat_shadow_host_as_normal_scope = true;
    sub_context.scope = context.scope;
    // Use FlatTreeTraversal to traverse a composed ancestor list of a given
    // element.
    Element* next_element = &element;
    SelectorCheckingContext host_context(sub_context);
    do {
      MatchResult sub_result;
      host_context.element = next_element;
      if (Match(host_context, sub_result)) {
        matched = true;
        // Consider div:host(div:host(div:host(div:host...))).
        max_specificity =
            std::max(max_specificity, host_context.selector->Specificity() +
                                          sub_result.specificity);
        break;
      }
      host_context.treat_shadow_host_as_normal_scope = false;
      host_context.scope = nullptr;

      if (selector.GetPseudoType() == CSSSelector::kPseudoHost)
        break;

      host_context.in_rightmost_compound = false;
      next_element = FlatTreeTraversal::ParentElement(*next_element);
    } while (next_element);
  }
  if (matched) {
    result.specificity += max_specificity;
    if (is_v1_shadow)
      result.specificity += CSSSelector::kClassLikeSpecificity;
    return true;
  }

  // FIXME: this was a fallthrough condition.
  return false;
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

}  // namespace blink
