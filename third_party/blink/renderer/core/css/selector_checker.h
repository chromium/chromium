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

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"

namespace blink {

class CSSSelector;
class ContainerNode;
class CustomScrollbar;
class ComputedStyle;
class Element;
class PartNames;

class CORE_EXPORT SelectorChecker {
  STACK_ALLOCATED();

 public:
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

  explicit inline SelectorChecker(const Mode& mode)
      : element_style_(nullptr),
        scrollbar_(nullptr),
        part_names_(nullptr),
        pseudo_argument_(g_null_atom),
        scrollbar_part_(kNoPart),
        mode_(mode),
        is_ua_rule_(false) {}
  inline SelectorChecker(ComputedStyle* element_style,
                         PartNames* part_names,
                         const StyleRequest& style_request,
                         const Mode& mode,
                         const bool& is_ua_rule)
      : element_style_(element_style),
        scrollbar_(style_request.scrollbar),
        part_names_(part_names),
        pseudo_argument_(style_request.pseudo_argument),
        scrollbar_part_(style_request.scrollbar_part),
        mode_(mode),
        is_ua_rule_(is_ua_rule) {}

  SelectorChecker(const SelectorChecker&) = delete;
  SelectorChecker& operator=(const SelectorChecker&) = delete;

  // Wraps the current element and a CSSSelector and stores some other state of
  // the selector matching process.
  struct SelectorCheckingContext {
    STACK_ALLOCATED();

   public:
    // Initial selector constructor
    explicit SelectorCheckingContext(Element* element) : element(element) {}

    const CSSSelector* selector = nullptr;
    Element* element = nullptr;
    Element* previous_element = nullptr;
    const ContainerNode* scope = nullptr;
    PseudoId pseudo_id = kPseudoIdNone;
    bool is_sub_selector = false;
    bool in_rightmost_compound = true;
    bool has_scrollbar_pseudo = false;
    bool has_selection_pseudo = false;
    bool treat_shadow_host_as_normal_scope = false;
    Element* vtt_originating_element = nullptr;
    bool in_nested_complex_selector = false;
    bool is_inside_visited_link = false;
  };

  struct MatchResult {
    STACK_ALLOCATED();

   public:
    PseudoId dynamic_pseudo{kPseudoIdNone};

    // From the shortest argument selector match, we need to get the element
    // that matches the leftmost compound selector to mark the correct scope
    // elements of :has() pseudo class having the argument selectors starts
    // with descendant combinator.
    //
    // <main id=main>
    //   <div id=d1>
    //     <div id=d2 class="a">
    //       <div id=d3 class="a">
    //         <div id=d4>
    //           <div id=d5 class="b">
    //           </div>
    //         </div>
    //       </div>
    //     </div>
    //   </div>
    // </div>
    // <script>
    //  main.querySelectorAll('div:has(.a .b)'); // Should return #d1, #d2
    // </script>
    //
    // In case of the above example, div#d5 element matches the argument
    // selector '.a .b'. Among the ancestors of the div#d5, the div#d3 and
    // div#d4 is not the correct candidate scope element of ':has(.a .b)'
    // because those elements don't have .a element as it's descendant.
    // So instead of marking ancestors of div#d5, we should mark ancestors
    // of div#d3 to prevent incorrect marking.
    // In case of the shortest match for the argument selector '.a .b' on
    // div#d5 element, the div#d3 is the element that matches the leftmost
    // compound selector '.a'. So the MatchResult will return the div#d3
    // element for the matching operation.
    //
    // In case of matching none desendant relative argument selectors, we
    // can get the candidate leftmost compound matches while matching the
    // argument selector.
    // To process the 'main.querySelectorAll("div:has(:scope > .a .b)")'
    // on the above DOM tree, selector checker will try to match the
    // argument selector ':scope > .a .b' on the descendants of #d1 div
    // element with the :scope element as #d1. When it matches the argument
    // selector on #d5 element, the matching result is true and it can get
    // the element that matches the leftmost(except :scope) compound '.a'
    // as #d2 element. But while matching the argument selector on the #d5
    // element, selector checker can also aware that the #d3 element can
    // be a leftmost compound matching element when the scope element is
    // #d2 element. So the selector checker will return the #d2 and #d3
    // element so that the #d1 and #d2 can be marked as matched with the
    // ':has(:scope > .a .b)'
    //
    // Instead of having vector for the :has argument matching, MatchResult
    // has a pointer field to hold a element vector instance to minimize the
    // MatchResult instance allocation overhead for none-has matching operations
    HeapVector<Member<Element>>* has_argument_leftmost_compound_matches{
        nullptr};
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
  MatchStatus MatchForSubSelector(const SelectorCheckingContext&,
                                  MatchResult&) const;
  MatchStatus MatchForRelation(const SelectorCheckingContext&,
                               MatchResult&) const;
  MatchStatus MatchForPseudoContent(const SelectorCheckingContext&,
                                    const Element&,
                                    MatchResult&) const;
  MatchStatus MatchForPseudoShadow(const SelectorCheckingContext&,
                                   const ContainerNode*,
                                   MatchResult&) const;
  bool CheckPseudoClass(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckPseudoElement(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckScrollbarPseudoClass(const SelectorCheckingContext&,
                                 MatchResult&) const;
  bool CheckPseudoHost(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckPseudoNot(const SelectorCheckingContext&, MatchResult&) const;
  bool CheckPseudoHas(const SelectorCheckingContext&, MatchResult&) const;

  ComputedStyle* element_style_;
  CustomScrollbar* scrollbar_;
  PartNames* part_names_;
  const String pseudo_argument_;
  ScrollbarPart scrollbar_part_;
  Mode mode_;
  bool is_ua_rule_;
#if DCHECK_IS_ON()
  mutable bool inside_match_ = false;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_SELECTOR_CHECKER_H_
