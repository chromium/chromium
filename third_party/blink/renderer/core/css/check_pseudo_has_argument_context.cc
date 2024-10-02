// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/check_pseudo_has_argument_context.h"

#include "third_party/blink/renderer/core/css/check_pseudo_has_fast_reject_filter.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"

namespace blink {

namespace {

// Iterator class for the compound selectors in the :has() argument selector.
// During iteration, this class collects :has() pseudo class argument
// hashes for fast rejection and provides current compound information.
class CheckPseudoHasArgumentCompoundIterator {
  STACK_ALLOCATED();

 public:
  CheckPseudoHasArgumentCompoundIterator(
      const CSSSelector* compound,
      Vector<unsigned>& pseudo_has_argument_hashes);

  void operator++();
  bool AtEnd() const { return !next_compound_; }

  inline CSSSelector::RelationType RelationToNextCompound() const {
    return relation_to_next_compound_;
  }

  bool CurrentCompoundAffectedBySiblingsOfMatchingElement() const {
    return current_compound_affected_by_ & kSiblingsOfMatchingElement;
  }

  bool CurrentCompoundAffectedByAncestorSiblingsOfMatchingElement() const {
    return current_compound_affected_by_ & kAncestorSiblingsOfMatchingElement;
  }

 private:
  // Flags for extracting sibling relationship information from a :has()
  // argument selector.
  //
  // CheckPseudoHasArgumentContext extracts the relationship information
  // (sibling_combinator_between_child_or_descendant_combinator_ and
  // sibling_combinator_at_rightmost_) and provides them to the SelectorChecker
  // so that the SelectorChecker marks elements that affect a :has() state when
  // there is an element that matches the :has() argument selector. (Please
  // refer the SetAffectedByHasForAgumentMatchedElement() in
  // selector_checker.cc)
  //
  // To extract the information, CheckPseudoHasArgumentContext need to check
  // sibling relationships in a :has() argument selector.
  //
  // By default, CheckPseudoHasArgumentContext can get the sibling relationship
  // information from the direct and indirect adjacent combinators ('~', '+')
  // between two compound selectors of the :has() argument selector.
  // (e.g. set sibling_combinator_at_rightmost_ flag for ':has(.a .b ~ .c)')
  //
  // In most cases, a compound selector doesn't have any sibling relationships
  // in it. (.e.g. 'div.item:hover')
  // But it can have implicit sibling relationships when it has a child indexed
  // pseudo class or a logical combination pseudo class containing a complex
  // selector.
  // - .a:nth-child(3) : An element that matches this compound selector has
  //                     relationships with its siblings since 'nth-child(3)'
  //                     state can be affected by sibling existence.
  // - .a:is(.b ~ .c) : An element that matches this compound selector has
  //                    relationships with its siblings since ':is(.b ~ .c)'
  //                    state can be affected by siblings' class values.
  //
  // A compound selector matching result on an element can be affected by
  // following sibling relationships:
  // - affected by the siblings of the matching element
  // - affected by the ancestors' siblings of the matching element.
  //
  // To extract the sibling relationships within a compound selector of a :has()
  // argument, CheckPseudoHasArgumentContext collects these flags from the
  // simple selectors in the compound selector:
  // - kAffectedBySiblingsOfMatchingElement:
  //     Indicates that the siblings of the matching element can affect the
  //     selector match result.
  // - kAffectedByAncestorSiblingsOfMatchingElement:
  //     Indicates that the matching element's ancestors' siblings can affect
  //     the selector match result.
  //
  // 'MatchingElement' in the flag name indicates the selector's subject
  // element, i.e. the element on which the ':has()' argument selector is being
  // tested.
  using AffectedByFlags = uint32_t;
  enum AffectedByFlag : uint32_t {
    kMatchingElementOnly = 0,
    kSiblingsOfMatchingElement = 1 << 0,
    kAncestorSiblingsOfMatchingElement = 1 << 1,
  };

  inline static bool NeedToCollectAffectedByFlagsFromSubSelector(
      const CSSSelector* simple_selector) {
    switch (simple_selector->GetPseudoType()) {
      case CSSSelector::kPseudoIs:
      case CSSSelector::kPseudoWhere:
      case CSSSelector::kPseudoNot:
      case CSSSelector::kPseudoParent:
        return true;
      default:
        return false;
    }
  }

  static void CollectAffectedByFlagsFromSimpleSelector(
      const CSSSelector* simple_selector,
      AffectedByFlags&);

  const CSSSelector* next_compound_;
  Vector<unsigned>& pseudo_has_argument_hashes_;
  CSSSelector::RelationType relation_to_next_compound_ =
      CSSSelector::kSubSelector;
  AffectedByFlags current_compound_affected_by_ = kMatchingElementOnly;
};

CheckPseudoHasArgumentCompoundIterator::CheckPseudoHasArgumentCompoundIterator(
    const CSSSelector* compound,
    Vector<unsigned>& pseudo_has_argument_hashes)
    : next_compound_(compound),
      pseudo_has_argument_hashes_(pseudo_has_argument_hashes) {
  ++(*this);
}

// Collect sibling relationship within a simple selector in ':has()' argument.
//
// In most cases, a simple selector doesn't have any sibling relationships
// in it. (.e.g. 'div', '.item', ':hover')
// But it can have implicit sibling relationships if it is a child indexed
// pseudo class or a logical combination pseudo class containing a complex
// selector.
// - :nth-child(3) : An element that matches this selector has relationships
//                   with its siblings since the match result can be affected
//                   by sibling existence.
// - :is(.a ~ .b) : An element that matches this selector has relationships
//                  with its siblings since the match result can be affected
//                  by siblings' class values.
// - :is(.a ~ .b .c) : An element that matches this selector has relationships
//                     with its ancestors' siblings since the match result can
//                     be affected by ancestors' siblings' class values.
//
// static
void CheckPseudoHasArgumentCompoundIterator::
    CollectAffectedByFlagsFromSimpleSelector(const CSSSelector* simple_selector,
                                             AffectedByFlags& affected_by) {
  if (simple_selector->IsChildIndexedSelector()) {
    affected_by |= kSiblingsOfMatchingElement;
    return;
  }

  if (!NeedToCollectAffectedByFlagsFromSubSelector(simple_selector)) {
    return;
  }

  // In case of a logical combination pseudo class (e.g. :is(), :where()), the
  // relationship within the logical combination can be collected by checking
  // the simple selectors or the combinators in its sub selectors.
  //
  // While checking the simple selectors and combinators in selector matching
  // order (from rightmost to left), if the sibling relationship is collected,
  // we need to differentiate the sibling relationship by checking whether the
  // child or descendant combinator has already been found or not since the
  // collected sibling relationship make the logical combination pseudo class
  // containing sibling relationship or ancestor sibling relationship.
  //
  // We can see this with the following nested ':is()' case:
  // - ':is(:is(.ancestor_sibling ~ .ancestor) .target)'
  //
  // The inner ':is()' pseudo class contains the 'sibling relationship'
  // because there is one adjacent combinator in the sub selector of the
  // pseudo class and there is no child or descendant combinator to the
  // right of the adjacent combinator:
  // - ':is(.ancestor_sibling ~ .ancestor)'
  //
  // The 'sibling relationship' within the inner 'is()' pseudo class makes
  // the outer ':is()' pseudo class containing the 'ancestor sibling
  // relationship' because there is a descendant combinator to the right of
  // the inner ':is()' pseudo class:
  // - ':is(:is(...) .target)'
  const CSSSelector* sub_selector = simple_selector->SelectorListOrParent();
  for (; sub_selector; sub_selector = CSSSelectorList::Next(*sub_selector)) {
    bool found_child_or_descendant_combinator_in_sub_selector = false;

    for (const CSSSelector* selector = sub_selector; selector;
         selector = selector->NextSimpleSelector()) {
      AffectedByFlags simple_in_sub_affected_by = kMatchingElementOnly;

      CollectAffectedByFlagsFromSimpleSelector(selector,
                                               simple_in_sub_affected_by);

      if (simple_in_sub_affected_by & kSiblingsOfMatchingElement) {
        found_child_or_descendant_combinator_in_sub_selector
            ? affected_by |= kAncestorSiblingsOfMatchingElement
            : affected_by |= kSiblingsOfMatchingElement;
      }
      if (simple_in_sub_affected_by & kAncestorSiblingsOfMatchingElement) {
        affected_by |= kAncestorSiblingsOfMatchingElement;
      }

      switch (selector->Relation()) {
        case CSSSelector::kDescendant:
        case CSSSelector::kChild:
          found_child_or_descendant_combinator_in_sub_selector = true;
          break;
        case CSSSelector::kDirectAdjacent:
        case CSSSelector::kIndirectAdjacent:
          found_child_or_descendant_combinator_in_sub_selector
              ? affected_by |= kAncestorSiblingsOfMatchingElement
              : affected_by |= kSiblingsOfMatchingElement;
          break;
        default:
          break;
      }
    }
  }
}

void CheckPseudoHasArgumentCompoundIterator::operator++() {
  DCHECK(next_compound_);
  current_compound_affected_by_ = kMatchingElementOnly;
  for (const CSSSelector* simple_selector = next_compound_; simple_selector;
       simple_selector = simple_selector->NextSimpleSelector()) {
    CheckPseudoHasFastRejectFilter::CollectPseudoHasArgumentHashes(
        pseudo_has_argument_hashes_, simple_selector);

    CollectAffectedByFlagsFromSimpleSelector(simple_selector,
                                             current_compound_affected_by_);

    relation_to_next_compound_ = simple_selector->Relation();
    if (relation_to_next_compound_ != CSSSelector::kSubSelector) {
      next_compound_ = simple_selector->NextSimpleSelector();
      return;
    }
  }
  next_compound_ = nullptr;
}

}  // namespace

CheckPseudoHasArgumentContext::CheckPseudoHasArgumentContext(
    const CSSSelector* selector,
    bool match_in_shadow_tree)
    : has_argument_(selector), match_in_shadow_tree_(match_in_shadow_tree) {
  depth_limit_ = 0;
  adjacent_distance_limit_ = 0;
  bool contains_child_or_descendant_combinator = false;
  bool sibling_combinator_at_leftmost = false;
  CheckPseudoHasArgumentCompoundIterator iterator(selector,
                                                  pseudo_has_argument_hashes_);
  for (; !iterator.AtEnd(); ++iterator) {
    // If the compound contains an :nth-child() or another child-indexed
    // selector, or the compound contains a logical combination pseudo class
    // containing a sibling relationship in its sub-selector, we need to do the
    // same invalidation as for an indirect adjacent combinator since inserting
    // or removing a sibling at any place may change matching of a :has()
    // selector on any of its siblings or sibling descendant.
    if (iterator.CurrentCompoundAffectedBySiblingsOfMatchingElement()) {
      if (contains_child_or_descendant_combinator) {
        sibling_combinator_at_leftmost = true;
      } else {
        sibling_combinator_at_rightmost_ = true;
      }
    }
    if (iterator.CurrentCompoundAffectedByAncestorSiblingsOfMatchingElement()) {
      sibling_combinator_between_child_or_descendant_combinator_ = true;
    }

    switch (iterator.RelationToNextCompound()) {
      case CSSSelector::kRelativeDescendant:
        leftmost_relation_ = iterator.RelationToNextCompound();
        [[fallthrough]];
      case CSSSelector::kDescendant:
        if (sibling_combinator_at_leftmost) {
          sibling_combinator_at_leftmost = false;
          sibling_combinator_between_child_or_descendant_combinator_ = true;
        }
        contains_child_or_descendant_combinator = true;
        depth_limit_ = kInfiniteDepth;
        adjacent_distance_limit_ = 0;
        break;

      case CSSSelector::kRelativeChild:
        leftmost_relation_ = iterator.RelationToNextCompound();
        [[fallthrough]];
      case CSSSelector::kChild:
        if (sibling_combinator_at_leftmost) {
          sibling_combinator_at_leftmost = false;
          sibling_combinator_between_child_or_descendant_combinator_ = true;
        }
        contains_child_or_descendant_combinator = true;
        if (DepthFixed()) {
          depth_limit_++;
        }
        adjacent_distance_limit_ = 0;
        break;

      case CSSSelector::kRelativeDirectAdjacent:
        leftmost_relation_ = iterator.RelationToNextCompound();
        [[fallthrough]];
      case CSSSelector::kDirectAdjacent:
        if (contains_child_or_descendant_combinator) {
          sibling_combinator_at_leftmost = true;
        } else {
          sibling_combinator_at_rightmost_ = true;
        }
        if (AdjacentDistanceFixed()) {
          adjacent_distance_limit_++;
        }
        break;

      case CSSSelector::kRelativeIndirectAdjacent:
        leftmost_relation_ = iterator.RelationToNextCompound();
        [[fallthrough]];
      case CSSSelector::kIndirectAdjacent:
        if (contains_child_or_descendant_combinator) {
          sibling_combinator_at_leftmost = true;
        } else {
          sibling_combinator_at_rightmost_ = true;
        }
        adjacent_distance_limit_ = kInfiniteAdjacentDistance;
        break;

      default:
        NOTREACHED_IN_MIGRATION();
        return;
    }
  }
  DCHECK_NE(leftmost_relation_, CSSSelector::kSubSelector);
  DCHECK_LE(adjacent_distance_limit_, kInfiniteAdjacentDistance);
  DCHECK_LE(depth_limit_, kInfiniteDepth);

  switch (leftmost_relation_) {
    case CSSSelector::kRelativeDescendant:
    case CSSSelector::kRelativeChild:
      if (DepthFixed()) {
        traversal_scope_ = kFixedDepthDescendants;
      } else {
        traversal_scope_ = kSubtree;
      }
      siblings_affected_by_has_flags_ =
          SiblingsAffectedByHasFlags::kNoSiblingsAffectedByHasFlags;
      break;
    case CSSSelector::kRelativeIndirectAdjacent:
    case CSSSelector::kRelativeDirectAdjacent:
      if (DepthLimit() == 0) {
        if (AdjacentDistanceFixed()) {
          traversal_scope_ = kOneNextSibling;
        } else {
          traversal_scope_ = kAllNextSiblings;
        }
        siblings_affected_by_has_flags_ =
            SiblingsAffectedByHasFlags::kFlagForSiblingRelationship;
      } else {
        if (AdjacentDistanceFixed()) {
          if (DepthFixed()) {
            traversal_scope_ = kOneNextSiblingFixedDepthDescendants;
          } else {
            traversal_scope_ = kOneNextSiblingSubtree;
          }
        } else {
          if (DepthFixed()) {
            traversal_scope_ = kAllNextSiblingsFixedDepthDescendants;
          } else {
            traversal_scope_ = kAllNextSiblingSubtrees;
          }
        }
        siblings_affected_by_has_flags_ =
            SiblingsAffectedByHasFlags::kFlagForSiblingDescendantRelationship;
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (match_in_shadow_tree_) {
    switch (traversal_scope_) {
      case kSubtree:
        traversal_scope_ = kShadowRootSubtree;
        break;
      case kFixedDepthDescendants:
        traversal_scope_ = kShadowRootFixedDepthDescendants;
        break;
      default:
        traversal_scope_ = kInvalidShadowRootTraversalScope;
        break;
    }
  }
}

CheckPseudoHasArgumentTraversalIterator::
    CheckPseudoHasArgumentTraversalIterator(
        Element& has_anchor_element,
        CheckPseudoHasArgumentContext& context)
    : has_anchor_element_(&has_anchor_element),
      match_in_shadow_tree_(context.MatchInShadowTree()),
      depth_limit_(context.DepthLimit()) {
  if (match_in_shadow_tree_) {
    if (!has_anchor_element.GetShadowRoot() ||
        context.TraversalScope() == kInvalidShadowRootTraversalScope) {
      DCHECK_EQ(current_element_, nullptr);
      return;
    }
  }

  if (!context.AdjacentDistanceFixed()) {
    // Set the last_element_ as the next sibling of the :has() anchor element,
    // and move to the last sibling of the :has() anchor element, and move again
    // to the last descendant of the last sibling.
    last_element_ = ElementTraversal::NextSibling(*has_anchor_element_);
    if (!last_element_) {
      DCHECK_EQ(current_element_, nullptr);
      return;
    }
    Element* last_sibling =
        ElementTraversal::LastChild(*has_anchor_element_->parentNode());
    current_element_ = LastWithin(last_sibling);
    if (!current_element_) {
      current_element_ = last_sibling;
    }
  } else if (context.AdjacentDistanceLimit() == 0) {
    DCHECK_GT(context.DepthLimit(), 0);
    // Set the last_element_ as the first child of the :has() anchor element,
    // and move to the last descendant of the :has() anchor element without
    // exceeding the depth limit.
    ContainerNode* has_anchor_node = has_anchor_element_;
    if (match_in_shadow_tree_) {
      has_anchor_node = has_anchor_element_->GetShadowRoot();
    }
    last_element_ = ElementTraversal::FirstChild(*has_anchor_node);
    if (!last_element_) {
      DCHECK_EQ(current_element_, nullptr);
      return;
    }
    current_element_ = LastWithin(has_anchor_node);
    DCHECK(current_element_);
  } else {
    // Set last_element_ as the next sibling of the :has() anchor element, set
    // the sibling_at_fixed_distance_ as the element at the adjacent distance
    // of the :has() anchor element, and move to the last descendant of the
    // sibling at fixed distance without exceeding the depth limit.
    int distance = 1;
    Element* old_sibling = nullptr;
    Element* sibling = ElementTraversal::NextSibling(*has_anchor_element_);
    for (; distance < context.AdjacentDistanceLimit() && sibling;
         distance++, sibling = ElementTraversal::NextSibling(*sibling)) {
      old_sibling = sibling;
    }
    if (sibling) {
      sibling_at_fixed_distance_ = sibling;
      current_element_ = LastWithin(sibling_at_fixed_distance_);
      if (!current_element_) {
        current_element_ = sibling_at_fixed_distance_;
      }
    } else {
      current_element_ = old_sibling;
      if (!current_element_) {
        return;
      }
      // set the depth_limit_ to 0 so that the iterator only traverse to the
      // siblings of the :has() anchor element.
      depth_limit_ = 0;
    }
    last_element_ = ElementTraversal::NextSibling(*has_anchor_element_);
  }
}

Element* CheckPseudoHasArgumentTraversalIterator::LastWithin(
    ContainerNode* container_node) {
  // If the current depth is at the depth limit, return null.
  if (current_depth_ == depth_limit_) {
    return nullptr;
  }

  // Return the last element of the pre-order traversal starting from the passed
  // in container node without exceeding the depth limit.
  Element* last_descendant = nullptr;
  for (Element* descendant = ElementTraversal::LastChild(*container_node);
       descendant; descendant = ElementTraversal::LastChild(*descendant)) {
    last_descendant = descendant;
    if (++current_depth_ == depth_limit_) {
      break;
    }
  }
  return last_descendant;
}

void CheckPseudoHasArgumentTraversalIterator::operator++() {
  DCHECK(current_element_);
  DCHECK_NE(current_element_, has_anchor_element_);
  if (current_element_ == last_element_) {
    current_element_ = nullptr;
    return;
  }

  // If current element is the sibling at fixed distance, set the depth_limit_
  // to 0 so that the iterator only traverse to the siblings of the :has()
  // anchor element.
  if (current_depth_ == 0 && sibling_at_fixed_distance_ == current_element_) {
    sibling_at_fixed_distance_ = nullptr;
    depth_limit_ = 0;
  }

  // Move to the previous element in DOM tree order within the depth limit.
  if (Element* next = ElementTraversal::PreviousSibling(*current_element_)) {
    Element* last_descendant = LastWithin(next);
    current_element_ = last_descendant ? last_descendant : next;
  } else {
    DCHECK_GT(current_depth_, 0);
    current_depth_--;
    current_element_ = current_element_->parentElement();
  }
  DCHECK(current_element_);
}

}  // namespace blink
