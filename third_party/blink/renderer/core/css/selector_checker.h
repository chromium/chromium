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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SELECTOR_CHECKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SELECTOR_CHECKER_H_

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"

namespace blink {

class CSSSelector;
class ContainerNode;
class CustomScrollbar;
class ComputedStyle;
class Element;
class PartNames;

class SelectorChecker {
  STACK_ALLOCATED();

 public:
  enum VisitedMatchType : uint8_t {
    kVisitedMatchDisabled,
    kVisitedMatchEnabled
  };

  enum Mode {
    // Used when matching selectors inside style recalc. This mode will set
    // restyle flags across the tree during matching which impact how style
    // sharing and invalidation work later.
    kResolvingStyle,

    // Used when collecting which rules match into a StyleRuleList, the engine
    // internal represention.
    //
    // TODO(esprehn): This doesn't change the behavior of the SelectorChecker
    // we should merge it with a generic CollectingRules mode.
    kCollectingStyleRules,

    // Used when collecting which rules match into a CSSRuleList, the CSSOM api
    // represention.
    //
    // TODO(esprehn): This doesn't change the behavior of the SelectorChecker
    // we should merge it with a generic CollectingRules mode.
    kCollectingCSSRules,

    // Used when matching rules for querySelector and <content select>. This
    // disables the special handling for positional selectors during parsing
    // and also enables static profile only selectors like >>>.
    kQueryingRules,
  };

  struct Init {
    STACK_ALLOCATED();

   public:
    Mode mode = kResolvingStyle;
    bool is_ua_rule = false;
    ComputedStyle* element_style = nullptr;
    CustomScrollbar* scrollbar = nullptr;
    ScrollbarPart scrollbar_part = kNoPart;
    PartNames* part_names = nullptr;
  };

  explicit SelectorChecker(const Init& init)
      : element_style_(init.element_style),
        scrollbar_(init.scrollbar),
        part_names_(init.part_names),
        scrollbar_part_(init.scrollbar_part),
        mode_(init.mode) {
#if DCHECK_IS_ON()
    is_ua_rule_ = init.is_ua_rule;
#endif
  }
  SelectorChecker(const SelectorChecker&) = delete;
  SelectorChecker& operator=(const SelectorChecker&) = delete;

  // Wraps the current element and a CSSSelector and stores some other state of
  // the selector matching process.
  struct SelectorCheckingContext {
    STACK_ALLOCATED();

   public:
    // Initial selector constructor
    SelectorCheckingContext(Element* element,
                            VisitedMatchType visited_match_type)
        : element(element), visited_match_type(visited_match_type) {}

    const CSSSelector* selector = nullptr;
    Element* element = nullptr;
    Element* previous_element = nullptr;
    const ContainerNode* scope = nullptr;
    VisitedMatchType visited_match_type;
    PseudoId pseudo_id = kPseudoIdNone;
    bool is_sub_selector = false;
    bool in_rightmost_compound = true;
    bool has_scrollbar_pseudo = false;
    bool has_selection_pseudo = false;
    bool treat_shadow_host_as_normal_scope = false;
    bool is_from_vtt = false;
    bool in_nested_complex_selector = false;
  };

  struct MatchResult {
    STACK_ALLOCATED();

   public:
    MatchResult() : dynamic_pseudo(kPseudoIdNone), specificity(0) {}

    PseudoId dynamic_pseudo;
    unsigned specificity;
  };

  bool Match(const SelectorCheckingContext& context, MatchResult& result) const;

  bool Match(const SelectorCheckingContext& context) const {
    MatchResult ignore_result;
    return Match(context, ignore_result);
  }

  static bool MatchesFocusPseudoClass(const Element&);
  static bool MatchesFocusVisiblePseudoClass(const Element&);
  static bool MatchesSpatialNavigationInterestPseudoClass(const Element&);

 private:
  // Does the work of checking whether the simple selector and element pointed
  // to by the context are a match. Delegates most of the work to the Check*
  // methods below.
  bool CheckOne(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckOneForVTT(const SelectorCheckingContext&, MatchResult&) const;

  enum MatchStatus {
    kSelectorMatches,
    kSelectorFailsLocally,
    kSelectorFailsAllSiblings,
    kSelectorFailsCompletely
  };

  // MatchSelector is the core of the recursive selector matching process. It
  // calls through to the Match* methods below and Match above.
  //
  // At each level of the recursion the context (which selector and element we
  // are considering) is represented by a SelectorCheckingContext. A context may
  // also contain a scope, this can limit the matching to occur within a
  // specific shadow tree (and its descendants). As the recursion proceeds, new
  // `SelectorCheckingContext` objects are created by copying a previous one and
  // changing the selector and/or the element being matched
  //
  // MatchSelector uses CheckOne to determine what element matches the current
  // selector. If CheckOne succeeds we recurse with a new context pointing to
  // the next selector (in a selector list, we proceed leftwards through the
  // compound selectors). If CheckOne fails we may try again with a different
  // element or we may fail the match entirely. In both cases, the next element
  // to try (e.g. same element, parent, sibling) depends on the combinators in
  // the selectors.
  MatchStatus MatchSelector(const SelectorCheckingContext&, MatchResult&) const;
  MatchStatus MatchSelectorForVTT(const SelectorCheckingContext&,
                                  MatchResult&) const;
  MatchStatus MatchForSubSelector(const SelectorCheckingContext&,
                                  MatchResult&) const;
  MatchStatus MatchForSubSelectorForVTT(const SelectorCheckingContext&,
                                        MatchResult&) const;
  MatchStatus MatchForRelation(const SelectorCheckingContext&,
                               MatchResult&) const;
  MatchStatus MatchForRelationForVTT(const SelectorCheckingContext&,
                                     MatchResult&) const;
  MatchStatus MatchForPseudoContent(const SelectorCheckingContext&,
                                    const Element&,
                                    MatchResult&) const;
  MatchStatus MatchForPseudoShadow(const SelectorCheckingContext&,
                                   const ContainerNode*,
                                   MatchResult&) const;
  bool MatchVTTBlockSelector(const SelectorCheckingContext& context,
                             MatchResult& result) const;
  bool CheckPseudoClass(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckPseudoClassForVTT(const SelectorCheckingContext&,
                              MatchResult&) const;
  bool CheckPseudoElement(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckPseudoElementForVTT(const SelectorCheckingContext&,
                                MatchResult&) const;
  bool CheckScrollbarPseudoClass(const SelectorCheckingContext&,
                                 MatchResult&) const;
  bool CheckPseudoHost(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckPseudoNot(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckPseudoNotForVTT(const SelectorCheckingContext&, MatchResult&) const;

  ComputedStyle* element_style_;
  CustomScrollbar* scrollbar_;
  PartNames* part_names_;
  ScrollbarPart scrollbar_part_;
  Mode mode_;
#if DCHECK_IS_ON()
  bool is_ua_rule_;
#else
  static constexpr bool is_ua_rule_ = true;
#endif
};

}  // namespace blink

#endif
