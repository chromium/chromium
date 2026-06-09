/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/selector_query.h"

#include <memory>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_cache_scope.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/css/selector_filter.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/nth_index_cache.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

// Uncomment to run the SelectorQueryTests for stats in a release build.
// #define RELEASE_QUERY_STATS

namespace blink {

#if DCHECK_IS_ON() || defined(RELEASE_QUERY_STATS)
static SelectorQuery::QueryStats& CurrentQueryStats() {
  DEFINE_STATIC_LOCAL(SelectorQuery::QueryStats, stats, ());
  return stats;
}

SelectorQuery::QueryStats SelectorQuery::LastQueryStats() {
  return CurrentQueryStats();
}

#define QUERY_STATS_INCREMENT(name) (void)(CurrentQueryStats().name++);
#define QUERY_STATS_RESET() (void)(CurrentQueryStats() = {});

#else

#define QUERY_STATS_INCREMENT(name)
#define QUERY_STATS_RESET()

#endif

struct SingleElementSelectorQueryTrait {
  typedef Element* OutputType;
  static const bool kShouldOnlyMatchFirstElement = true;
  ALWAYS_INLINE static bool IsEmpty(const OutputType& output) {
    return !output;
  }
  ALWAYS_INLINE static void AppendElement(OutputType& output,
                                          Element& element) {
    DCHECK(!output);
    output = &element;
  }
};

struct AllElementsSelectorQueryTrait {
  typedef HeapVector<Member<Element>> OutputType;
  static const bool kShouldOnlyMatchFirstElement = false;
  ALWAYS_INLINE static bool IsEmpty(const OutputType& output) {
    return output.empty();
  }
  ALWAYS_INLINE static void AppendElement(OutputType& output,
                                          Element& element) {
    output.push_back(&element);
  }
};

inline bool SelectorMatches(const CSSSelector* selector,
                            Element& element,
                            const ContainerNode& root_node,
                            const SelectorChecker& checker) {
  QUERY_STATS_INCREMENT(recheck_selector);

  SelectorChecker::SelectorCheckingContext context(&element);
  context.selector = selector;
  context.scope = &root_node;
  context.tree_scope = &root_node.GetTreeScope();
  return checker.Match(context);
}

bool SelectorQuery::Matches(Element& target_element) const {
  QUERY_STATS_RESET();
  CheckPseudoHasCacheScope check_pseudo_has_cache_scope(
      &target_element.GetDocument(), /*within_selector_checking=*/false);
  if (compounds_.size() == 1 && !need_full_check_ && !compounds_[0].nth_child) {
    FillMissingData(target_element);
    return MatchCompound(target_element, compounds_[0], kUnknownSiblingIndex,
                         IsA<HTMLDocument>(target_element.GetDocument()));
  } else {
    return SelectorListMatches(target_element, target_element);
  }
}

Element* SelectorQuery::Closest(Element& target_element) const {
  QUERY_STATS_RESET();
  CheckPseudoHasCacheScope check_pseudo_has_cache_scope(
      &target_element.GetDocument(), /*within_selector_checking=*/false);
  if (selector_start_offsets_.empty()) {
    return nullptr;
  }

  for (Element* current_element = &target_element; current_element;
       current_element = current_element->parentElement()) {
    if (SelectorListMatches(target_element, *current_element)) {
      return current_element;
    }
  }
  return nullptr;
}

StaticElementList* SelectorQuery::QueryAll(ContainerNode& root_node) const {
  QUERY_STATS_RESET();
  CheckPseudoHasCacheScope check_pseudo_has_cache_scope(
      &root_node.GetDocument(), /*within_selector_checking=*/false);
  NthIndexCache nth_index_cache(root_node.GetDocument());
  HeapVector<Member<Element>> result;
  Execute<AllElementsSelectorQueryTrait>(root_node, result);
  return StaticElementList::Adopt(result);
}

Element* SelectorQuery::QueryFirst(ContainerNode& root_node) const {
  QUERY_STATS_RESET();
  CheckPseudoHasCacheScope check_pseudo_has_cache_scope(
      &root_node.GetDocument(), /*within_selector_checking=*/false);
  NthIndexCache nth_index_cache(root_node.GetDocument());
  Element* matched_element = nullptr;
  Execute<SingleElementSelectorQueryTrait>(root_node, matched_element);
  return matched_element;
}

inline bool MatchesTagName(const QualifiedName& tag_name,
                           const Element& element) {
  if (tag_name == AnyQName()) {
    return true;
  }
  if (element.HasLocalName(tag_name.LocalName())) {
    return true;
  }
  // Non-html elements in html documents are normalized to their camel-cased
  // version during parsing if applicable. Yet, type selectors are lower-cased
  // for selectors in html documents. Compare the upper case converted names
  // instead to allow matching SVG elements like foreignObject.
  if (!element.IsHTMLElement() && IsA<HTMLDocument>(element.GetDocument())) {
    return element.TagQName().LocalNameUpper() == tag_name.LocalNameUpper();
  }
  return false;
}

// TODO(sesse): Reduce the duplication against SelectorChecker.
static bool AttributeValueMatchesExact(const Attribute& attribute_item,
                                       const AtomicString& selector_value,
                                       bool case_insensitive) {
  const AtomicString& value = attribute_item.Value();
  if (value.IsNull()) {
    return false;
  }
  return selector_value == value ||
         (case_insensitive && EqualIgnoringAsciiCase(selector_value, value));
}

static Element::AttributesToExcludeHashesFor ExclusionPolicy(
    const ContainerNode& root_node) {
  return IsA<HTMLDocument>(root_node.GetDocument())
             ? Element::kExcludeAllLazilySynchronizedAttributes
             : Element::kExcludeLowercaseLazilySynchronizedAttributes;
}

bool SelectorQuery::SelectorListMatches(ContainerNode& root_node,
                                        Element& element) const {
  SelectorChecker checker(SelectorChecker::kQueryingRules);
  for (unsigned offset : selector_start_offsets_) {
    if (SelectorMatches(UNSAFE_TODO(selector_list_->First() + offset), element,
                        root_node, checker)) {
      return true;
    }
  }
  return false;
}

template <typename SelectorQueryTrait>
void SelectorQuery::ExecuteSlow(
    ContainerNode& root_node,
    typename SelectorQueryTrait::OutputType& output) const {
  for (Element& element : ElementTraversal::DescendantsOf(root_node)) {
    QUERY_STATS_INCREMENT(slow_scan);
    if (!SelectorListMatches(root_node, element)) {
      continue;
    }
    SelectorQueryTrait::AppendElement(output, element);
    if (SelectorQueryTrait::kShouldOnlyMatchFirstElement) {
      return;
    }
  }
}

bool SelectorQuery::MatchCompound(const Element& element,
                                  const SelectorQuery::Compound& compound,
                                  unsigned sibling_idx,
                                  bool is_html_doc) {
  if (compound.id_needed) {
    QUERY_STATS_INCREMENT(check_id);
    if (!element.HasID() ||
        element.IdForStyleResolution() != compound.id_needed) {
      return false;
    }
  }
  if (!compound.tag_needed.IsNull()) {
    QUERY_STATS_INCREMENT(check_tag);
    if (!MatchesTagName(compound.tag_needed, element)) {
      return false;
    }
  }
  if (compound.class_needed) {
    QUERY_STATS_INCREMENT(check_class);
    if (!element.HasClassName(compound.class_needed)) {
      return false;
    }
  }
  if (!compound.attr_needed.IsNull()) {
    QUERY_STATS_INCREMENT(check_attr);
    if (compound.needs_synchronize_attribute) {
      // Synchronize the attribute in case it is lazy-computed.
      // Currently all lazy properties have a null namespace, so only pass
      // localName().
      element.SynchronizeAttribute(compound.attr_needed.LocalName());
    }
    AttributeCollection attributes = element.AttributesWithoutUpdate();
    bool match_attr = false;
    for (const auto& attribute_item : attributes) {
      if (!attribute_item.Matches(compound.attr_needed)) {
        if (element.IsHTMLElement() || !is_html_doc) {
          continue;
        }
        // Non-HTML attributes in HTML documents are normalized to their camel-
        // cased version during parsing if applicable. Yet, attribute selectors
        // are lower-cased for selectors in HTML documents. Compare the selector
        // and the attribute local name insensitively to e.g. allow matching SVG
        // attributes like viewBox.
        //
        // NOTE: If changing this behavior, be sure to also update the bucketing
        // in ElementRuleCollector::CollectMatchingRules() accordingly.
        if (!attribute_item.MatchesCaseInsensitive(compound.attr_needed)) {
          continue;
        }
      }
      if (AttributeValueMatchesExact(attribute_item, compound.attr_value,
                                     compound.attr_case_insensitive)) {
        match_attr = true;
        break;
      }
      if (compound.attr_needed.NamespaceURI() != g_star_atom) {
        break;
      }
    }
    if (!match_attr) {
      return false;
    }
  }

  if (compound.nth_child && !(sibling_idx & kUnknownSiblingIndex)) {
    QUERY_STATS_INCREMENT(check_nth_child);
    if (compound.nth_child != sibling_idx) {
      return false;
    }
  }

  return true;
}

unsigned SelectorQuery::FirstCompoundNotSeenBeforeRoot(
    const ContainerNode& root_node,
    unsigned from_idx,
    unsigned to_idx,
    bool is_html_doc,
    bool horizontally) const {
  if (from_idx == to_idx) {
    return from_idx;
  }

  base::span<const Compound> earlier_compounds =
      base::span(compounds_).subspan(from_idx, to_idx - from_idx);

  // Reset the flags.
  unsigned num_unmatched_compounds = 0;
  for (const Compound& compound : earlier_compounds) {
    if (!horizontally && compound.next_compound_is_horizontal) {
      // We don't search to the left, only straight upwards, so we take
      // every sibling as already found.
      compound.seen_before_root = true;
    } else {
      compound.seen_before_root = false;
      ++num_unmatched_compounds;
    }
  }

  for (const Node* node = horizontally ? root_node.previousSibling()
                                       : root_node.parentNode();
       node && num_unmatched_compounds;
       node = horizontally ? node->previousSibling() : node->parentNode()) {
    const Element* element = DynamicTo<Element>(node);
    if (!element) {
      continue;
    }

    for (const Compound& compound : earlier_compounds) {
      if (compound.seen_before_root) {
        // Already matched, don't bother.
        continue;
      }

      if (MatchCompound(*element, compound, kUnknownSiblingIndex,
                        is_html_doc)) {
        compound.seen_before_root = true;
        --num_unmatched_compounds;
      }
    }
  }

  for (unsigned i = from_idx; i < to_idx; ++i) {
    if (!compounds_[i].seen_before_root) {
      return i;
    }
  }
  return to_idx;
}

// When we go downwards, we may need to go back in the list of compounds
// if we have sibling selectors; e.g., for a + b > c, if we've matched a
// but not b (compound_idx=1), going down will reset us back to compound_idx=0.
// Only when matching both a and b (compound_idx=2), and then going down will
// actually allow us to keep compound_idx=2 in the subtree. If we look for
// further siblings of b, we will again have resets to compound_idx=0 for
// children (until we match b on some other element).
unsigned SelectorQuery::FindStartOfLevel(unsigned compound_idx) const {
  unsigned first_compound_idx_this_level = compound_idx;
  while (first_compound_idx_this_level > 0 &&
         compounds_[first_compound_idx_this_level - 1]
             .next_compound_is_horizontal) {
    --first_compound_idx_this_level;
  }
  return first_compound_idx_this_level;
}

template <typename SelectorQueryTrait>
void SelectorQuery::Execute(
    ContainerNode& root_node,
    typename SelectorQueryTrait::OutputType& output) const {
  if (selector_start_offsets_.empty()) {
    return;
  } else if (selector_start_offsets_.size() > 1) {
    ExecuteSlow<SelectorQueryTrait>(root_node, output);
    return;
  }

  CHECK_EQ(selector_start_offsets_.size(), 1u);

  FillMissingData(root_node);

  // Find out which compounds we've already seen above us and
  // must skip (although note that if we see only some siblings
  // of a level above us, we've effectively matched none of that
  // level).
  const bool is_html_doc = IsA<HTMLDocument>(root_node.GetDocument());
  const unsigned subject_compound_idx = compounds_.size() - 1;
  unsigned start_compound_idx = FirstCompoundNotSeenBeforeRoot(
      root_node,
      /*from_idx=*/0, /*to_idx=*/FindStartOfLevel(subject_compound_idx),
      is_html_doc,
      /*horizontally=*/false);
  start_compound_idx = FindStartOfLevel(start_compound_idx);

  // Similarly to the left of us, although note that unlike upwards,
  // this state change can actually be temporary; if we match some siblings
  // and then go down a level without matching the rest, we go back
  // to the start of the level.
  start_compound_idx = FirstCompoundNotSeenBeforeRoot(
      root_node, /*from_idx=*/start_compound_idx,
      /*to_idx=*/subject_compound_idx, is_html_doc,
      /*horizontally=*/true);

  // NOTE: If we only skip one level/compound, and it's a descendant/indirect
  // adjacent selector, then we could probably avoid need_full_check. For now,
  // we don't bother.
  const bool need_full_check = need_full_check_ || start_compound_idx > 0;

  SelectorChecker checker(SelectorChecker::kQueryingRules);

  // Now go and see if any of the remaining compounds contain an ID selector.
  // The DOM maintains special structures to locate IDs quickly, so this is
  // our preferred acceleration if available; if there are multiple ID
  // selectors, we pick the one on the lowest level in the (fairly reasonable)
  // hope that this contains the smallest subtree to look in.
  //
  // Note that we don't do this for compounds we've already matched,
  // since that will make us do at least one search from _above_ the root node,
  // which usually is more of a deoptimization.
  //
  // NOTE: In quirks mode, getElementById("a") is case-sensitive and
  // should only match elements with lowercase id "a", but querySelector
  // is case-insensitive, so querySelector("#a") == querySelector("#A"),
  // which means we can only use the ID fast path when we're in a
  // standards-mode document.
  //
  // TODO(sesse): We could re-check the discarded compounds to reject candidates
  // here more quickly (but it may not be worth it compared to just doing normal
  // selector checking).
  if (last_compound_with_id_selector_ >= static_cast<int>(start_compound_idx) &&
      root_node.IsInTreeScope() && !root_node.GetDocument().InQuirksMode()) {
    const TreeScope& scope = root_node.GetTreeScope();
    const Compound& compound = compounds_[last_compound_with_id_selector_];
    const AtomicString& id =
        compound.id_needed ? compound.id_needed : compound.attr_value;
    // The element with the given ID must be part of the match;
    // if we don't immediately match it (i.e., we stay at the current
    // compound, or even go backwards), we can drop the rest of the tree.
    for (unsigned i = 0; i < compounds_.size(); ++i) {
      compounds_[i].valid_for_progress =
          (static_cast<int>(i) > last_compound_with_id_selector_);
    }

    // Returns true if we are to stop the search.
    auto match = [&](Element* element) -> bool {
      if (!element) {
        return false;
      }
      QUERY_STATS_INCREMENT(fast_id_roots);
      if (element != &root_node && !element->IsDescendantOf(&root_node)) {
        return false;
      }
      return ExecuteSearch<SelectorQueryTrait>(
          *element, root_node, &compounds_[last_compound_with_id_selector_],
          kUnknownSiblingIndex,
          need_full_check || last_compound_with_id_selector_ > 0, is_html_doc,
          checker, output);
    };
    if (scope.ContainsMultipleElementsWithId(id) &&
        last_compound_with_id_selector_ !=
            static_cast<int>(compounds_.size() - 1)) {
      // We need to avoid duplicates; if we have a structure where multiple
      // elements share the same ID, and some of those are children of each
      // other, we cannot blindly traverse the subtrees below each of them. If
      // we're in querySelectorAll(), this matters for correctness, and even if
      // we're in querySelector(), it's a potential performance trap to keep
      // scanning the same subtree over and over again. (If we only have a
      // single compound, it's not an issue, as we're never moving beyond the
      // initial element in that case.)
      //
      // We could have solved this by skipping directly to the subject and
      // cutting off the search when we see a descendant with the right ID, but
      // it's not worth it; we already have enough complexity.
    } else if (scope.ContainsMultipleElementsWithId(id)) {
      for (Element* element : scope.GetAllElementsById(id)) {
        if (match(element)) {
          break;
        }
      }
      return;
    } else {
      match(scope.getElementById(id));
      return;
    }
  }

  // OK, there are no ID selectors anywhere, so do the normal matching.
  for (const Compound& compound : compounds_) {
    compound.valid_for_progress = true;
  }
  const Compound* start_compound = &compounds_[start_compound_idx];
  if (start_compound->simple_traversal_from_here) {
    Element* first_child = ElementTraversal::FirstChild(root_node);
    if (first_child) {
      ExecuteSearchSingleCompound<SelectorQueryTrait>(
          root_node, *first_child, root_node, start_compound, need_full_check,
          is_html_doc, checker, output);
    }
  } else {
    ExecuteSearch<SelectorQueryTrait>(root_node, root_node, start_compound,
                                      kUnknownSiblingIndex, need_full_check,
                                      is_html_doc, checker, output);
  }
}

template <typename SelectorQueryTrait>
bool SelectorQuery::ExecuteSearchSingleCompound(
    ContainerNode& root_node,
    Element& first_interesting_child,
    const ContainerNode& scope,
    const Compound* compound,
    bool need_full_check,
    bool is_html_doc,
    SelectorChecker& checker,
    typename SelectorQueryTrait::OutputType& output) const {
  DCHECK(compound->simple_traversal_from_here);
  const Element::TinyBloomFilter selector_filter = compound->selector_filter;

  if (IsA<Element>(root_node) &&
      !To<Element>(root_node).CouldMatchFilter(selector_filter)) {
    // Neither this nor any of its children could match this query,
    // so we can exit early.
    QUERY_STATS_INCREMENT(skipped_subtree);
    return false;
  }

  for (Element* element = &first_interesting_child; element;) {
    if (!element->CouldMatchFilter(selector_filter)) {
      QUERY_STATS_INCREMENT(skipped_subtree);
    } else {
      QUERY_STATS_INCREMENT(elements_seen);
      if (MatchCompound(*element, *compound, kUnknownSiblingIndex,
                        is_html_doc) &&
          (!need_full_check ||
           SelectorMatches(OnlySelector(), *element, scope, checker))) {
        SelectorQueryTrait::AppendElement(output, *element);
        if (SelectorQueryTrait::kShouldOnlyMatchFirstElement) {
          return true;
        }
      }
      Element* child = ElementTraversal::FirstChild(*element);
      if (child) {
        element = child;
        continue;
      }
    }
    Element* sibling = ElementTraversal::NextSibling(*element);
    if (sibling) {
      element = sibling;
    } else {
      for (Node& parent : NodeTraversal::AncestorsOf(*element)) {
        if (parent == &root_node) {
          return false;
        }
        sibling = ElementTraversal::NextSibling(parent);
        if (sibling) {
          element = sibling;
          break;
        }
      }
    }
  }

  return false;
}

template <typename SelectorQueryTrait, bool is_top_level>
bool SelectorQuery::ExecuteSearch(
    ContainerNode& root_node,
    const ContainerNode& scope,
    const Compound* start_compound,
    unsigned sibling_idx,
    bool need_full_check,
    bool is_html_doc,
    SelectorChecker& checker,
    typename SelectorQueryTrait::OutputType& output) const {
  if (is_top_level && IsA<Element>(root_node) &&
      !To<Element>(root_node).CouldMatchFilter(
          start_compound->selector_filter)) {
    // Neither this nor any of its children could match this query,
    // so we can exit early.
    QUERY_STATS_INCREMENT(skipped_subtree);
    return false;
  }

  ContainerNode* node = &root_node;
  const Compound* compound = start_compound;

  for (;;) {  // Termination condition within loop.
    Element* element = DynamicTo<Element>(node);

    if (element) {
      QUERY_STATS_INCREMENT(elements_seen);
    }

    const Compound* compound_for_children;
    const Compound* compound_for_siblings;
    const bool match =
        !(compound->is_subject && node == &scope) &&
        ((compound->skip_for_shadow_root && IsA<ShadowRoot>(node)) ||
         (element &&
          MatchCompound(*element, *compound, sibling_idx, is_html_doc)));
    if (match) {
      if (compound->nth_child && !(sibling_idx & kUnknownSiblingIndex)) {
        // This compound required :nth-child(), but we don't know
        // our element index, so we need a full recheck.
        // (This can only happen on the root node, so it's fine
        // that this is kept for all nodes in the rest of the tree.
        // There are some edge cases involving sibling selectors
        // where the flag would get wrongly stuck, but that's fine.)
        need_full_check = true;
      }
      if (compound->is_subject) {
        // We've matched all compounds, so this may be the actual matching
        // element. If needed, run the full selector checker to make sure.
        if (element &&
            (!need_full_check ||
             SelectorMatches(OnlySelector(), *element, scope, checker))) {
          // We actually matched.
          SelectorQueryTrait::AppendElement(output, *element);
          if (SelectorQueryTrait::kShouldOnlyMatchFirstElement) {
            return true;
          }
        }
      }
      compound_for_children = compound->next_compound_for_children_on_match;
      compound_for_siblings = compound->next_compound_for_siblings_on_match;
    } else {
      compound_for_children = compound->next_compound_for_children_on_mismatch;
      compound_for_siblings = compound;
    }

    // We're going to descend to the first (interesting) child of this node,
    // but if we have more (interesting) siblings, we need to remember them for
    // later traversal.
    Element* first_child = ElementTraversal::FirstChild(*node);
    Element* next_sibling =
        node == &scope ? nullptr : ElementTraversal::NextSibling(*node);

    if (is_top_level) {
      if (!compound_for_siblings->valid_for_progress) {
        next_sibling = nullptr;
      }
      if (!compound_for_children->valid_for_progress) {
        first_child = nullptr;
      }
    } else {
      DCHECK(compound_for_siblings->valid_for_progress);
      DCHECK(compound_for_children->valid_for_progress);
    }

    // Skip uninteresting children and siblings; we don't have to go through
    // the entire loop for them.
    unsigned first_child_sibling_idx = 1;
    unsigned next_sibling_idx = sibling_idx + 1;
    while (first_child && !first_child->CouldMatchFilter(
                              compound_for_children->selector_filter)) {
      QUERY_STATS_INCREMENT(skipped_subtree);
      first_child = ElementTraversal::NextSibling(*first_child);
      ++first_child_sibling_idx;
    }
    while (next_sibling && !next_sibling->CouldMatchFilter(
                               compound_for_siblings->selector_filter)) {
      QUERY_STATS_INCREMENT(skipped_subtree);
      next_sibling = ElementTraversal::NextSibling(*next_sibling);
      ++next_sibling_idx;
    }

    if (first_child && compound_for_children->simple_traversal_from_here) {
      if (ExecuteSearchSingleCompound<SelectorQueryTrait>(
              *node, *first_child, scope, compound_for_children,
              need_full_check, is_html_doc, checker, output)) {
        return true;
      }
      first_child = nullptr;
    }

    // Navigate to the first child if we can, the next sibling if not,
    // and back to earlier-unvisited sublings if we have neither.
    // (This is similar to ElementTraversal::Next(), except that we skip
    // uninteresting elements and reset our compound data/sibling index
    // on backtracking.)
    if (next_sibling) {
      if (first_child) {
        // We can go both down and right, and we want to follow
        // both paths. We could do this either via explicit recursion
        // or storing the state in a HeapVector, and the former measured
        // better (probably because of Oilpan barriers).
        if (ExecuteSearch<SelectorQueryTrait, /*is_top_level=*/false>(
                *first_child, scope, compound_for_children,
                first_child_sibling_idx, need_full_check, is_html_doc, checker,
                output)) {
          return true;
        }
      }
      node = next_sibling;
      compound = compound_for_siblings;
      sibling_idx = next_sibling_idx;
    } else if (first_child) {
      node = first_child;
      compound = compound_for_children;
      sibling_idx = first_child_sibling_idx;
    } else {
      // We're done.
      return false;
    }
  }
}

SelectorQuery::SelectorQuery(CSSSelectorList* selector_list)
    : selector_list_(selector_list) {
  const CSSSelector* base = selector_list_->First();
  for (const CSSSelector* selector = base; selector;
       selector = CSSSelectorList::Next(*selector)) {
    if (selector->MatchesPseudoElement()) {
      continue;
    }
    selector_start_offsets_.push_back(selector - base);
  }

  if (selector_start_offsets_.size() == 1) {
    BuildCompounds(OnlySelector());
  }
}

void SelectorQuery::BuildCompounds(const CSSSelector* first_selector) {
  Compound current_compound;

  compounds_.clear();
  need_full_check_ = false;
  last_compound_with_id_selector_ = -1;
  Element::TinyBloomFilter selector_filter_this_level = 0;

  for (const CSSSelector* current = first_selector; current;
       current = current->NextSimpleSelector()) {
    // See if we can satisfy this simple selector using one of the quick
    // checks that we do.
    switch (current->Match()) {
      case CSSSelector::kClass:
        if (current_compound.class_needed.IsNull()) {
          current_compound.class_needed = current->Value();
        } else {
          need_full_check_ = true;
        }
        break;

      case CSSSelector::kUniversalTag:
        if (current->TagQName().NamespaceURI() != g_star_atom) {
          // We don't check namespaces.
          need_full_check_ = true;
        }
        break;

      case CSSSelector::kTag:
        if (current_compound.tag_needed.IsNull()) {
          current_compound.tag_needed = current->TagQName();
          if (current->TagQName().NamespaceURI() != g_star_atom) {
            // We don't check namespaces.
            need_full_check_ = true;
          }
        } else {
          need_full_check_ = true;
        }
        break;

      case CSSSelector::kAttributeExact:
        if (current_compound.attr_needed.IsNull()) {
          current_compound.attr_needed = current->Attribute();
          current_compound.attr_value = current->Value();
          current_compound.match_type_case_insensitive =
              current->AttributeMatch() ==
              CSSSelector::AttributeMatchType::kCaseInsensitive;
          current_compound.legacy_case_insensitive =
              current->LegacyCaseInsensitiveMatch();
        } else {
          need_full_check_ = true;
        }
        break;

      case CSSSelector::kId:
        if (current_compound.id_needed.IsNull()) {
          current_compound.id_needed = current->Value();
        } else {
          need_full_check_ = true;
        }
        break;

      case CSSSelector::kPseudoClass:
        if (current->GetPseudoType() == CSSSelector::kPseudoNthChild &&
            !current->SelectorList() && current->NthAValue() == 0 &&
            !current_compound.nth_child) {
          // TODO(sesse): Consider supporting aN + b, not just b.
          current_compound.nth_child = current->NthBValue();
        } else if (current->GetPseudoType() == CSSSelector::kPseudoFirstChild &&
                   !current_compound.nth_child) {
          current_compound.nth_child = 1;
        } else {
          need_full_check_ = true;
        }
        break;

      default:
        need_full_check_ = true;
        break;
    }

    // TODO(sesse): If we have :has(), we could probably get bits from it here
    // (we cannot easily in normal selector checking, since we need to scan the
    // entire subtree to set kAffectedByHas invalidation bits anyway).
    SelectorFilter::CollectSingleSelectorIdentifierHashes(
        current, Element::kExcludeAllLazilySynchronizedAttributes,
        current_compound.selector_filter);
    selector_filter_this_level |= current_compound.selector_filter;

    // See if we are switching compounds.
    if (!current->IsLastInComplexSelector() &&
        current->Relation() != CSSSelector::kSubSelector) {
      compounds_.push_back(std::move(current_compound));
      current_compound = Compound();

      if (current->Relation() == CSSSelector::kDirectAdjacent ||
          current->Relation() == CSSSelector::kIndirectAdjacent) {
        current_compound.next_compound_is_horizontal = true;
      } else {
        current_compound.selector_filter = selector_filter_this_level;
        selector_filter_this_level = 0;
      }

      if (current->Relation() == CSSSelector::kDirectAdjacent ||
          current->Relation() == CSSSelector::kChild) {
        // Since we are relaxing the combinator to a more permissive one,
        // we need to re-run SelectorChecker after finding a match.
        //
        // NOTE: If these combinators become very important to us, it is
        // possible to do an accurate check by making the state machine
        // non-deterministic, i.e., after a match we could choose to either
        // move to the next state or pretend it's a non-match (and then
        // require actual direct adjacency, of course). For now, we're fine
        // with the simpler version.
        need_full_check_ = true;
      }
    }
  }

  // TODO(sesse): If we have a sibling selector in the topmost compound,
  // it could be advantageous to use the final selector_filter_this_level
  // value to check that the _parent_ element has the required subtree bits.

  compounds_.push_back(std::move(current_compound));
  std::reverse(compounds_.begin(), compounds_.end());

  for (Compound& compound : compounds_) {
    if (compound.id_needed || !compound.tag_needed.IsNull() ||
        compound.class_needed || !compound.attr_needed.IsNull()) {
      compound.skip_for_shadow_root = false;
    }
  }

  for (unsigned compound_idx = compounds_.size(); compound_idx-- > 0;) {
    const Compound& compound = compounds_[compound_idx];
    // NOTE: compound.attr_case_insensitive has not been set yet,
    // so we use a more conservative test.
    if (compound.id_needed || (compound.attr_needed == html_names::kIdAttr &&
                               !compound.match_type_case_insensitive &&
                               !compound.legacy_case_insensitive)) {
      last_compound_with_id_selector_ = compound_idx;
      break;
    }
  }

  for (unsigned compound_idx = 0; compound_idx < compounds_.size();
       ++compound_idx) {
    Compound& compound = compounds_[compound_idx];

    // Defaults.
    compound.next_compound_for_siblings_on_match = &compound;
    compound.next_compound_for_children_on_mismatch =
        compound.next_compound_for_children_on_match =
            &compounds_[FindStartOfLevel(compound_idx)];

    if (compound_idx == compounds_.size() - 1) {
      // We cannot progress past the subject.
    } else {
      if (compound.next_compound_is_horizontal) {
        compound.next_compound_for_siblings_on_match =
            &compounds_[compound_idx + 1];
      } else {
        compound.next_compound_for_children_on_match =
            &compounds_[compound_idx + 1];
      }
    }
  }

  Compound& subject_compound = compounds_[compounds_.size() - 1];
  subject_compound.is_subject = true;

  // If we get to the subject and cannot ever go backwards from there
  // (i.e., it is not related to a sibling combinator), and we don't
  // care about the index in the DOM, we can do with a simpler,
  // more direct search without any recursion.
  subject_compound.simple_traversal_from_here =
      (subject_compound.next_compound_for_children_on_mismatch ==
       &subject_compound) &&
      !subject_compound.nth_child;
}

// Fill in attribute data that we can only fill in when we know the document.
void SelectorQuery::FillMissingData(const ContainerNode& root_node) const {
  const bool is_html_doc = IsA<HTMLDocument>(root_node.GetDocument());
  for (const Compound& compound : compounds_) {
    if (!compound.attr_needed.IsNull()) {
      // Legacy dictates that values of some attributes should be compared
      // in a case-insensitive manner regardless of whether the case
      // insensitive flag is set or not (but an explicit case sensitive flag
      // will override that, by causing LegacyCaseInsensitiveMatch() never
      // to be set).
      compound.attr_case_insensitive =
          compound.match_type_case_insensitive ||
          (compound.legacy_case_insensitive && is_html_doc);

      // SynchronizeAttribute() is rather expensive to call. We can
      // determine ahead of time if it's needed.
      compound.needs_synchronize_attribute = Element::IsExcludedAttribute(
          compound.attr_needed, ExclusionPolicy(root_node));
    }
  }
}

SelectorQuery* SelectorQueryCache::Add(const AtomicString& selectors,
                                       const Document& document,
                                       ExceptionState& exception_state) {
  if (selectors.empty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The provided selector is empty.");
    return nullptr;
  }

  auto it = entries_.find(selectors);
  if (it != entries_.end()) {
    return it->value.Get();
  }

  HeapVector<CSSSelector> arena;
  base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          document, document.BaseURL(), true /* origin_clean */, Referrer()),
      CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr, nullptr,
      selectors, arena);

  if (selector_vector.empty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        StrCat({"'", selectors, "' is not a valid selector."}));
    return nullptr;
  }

  CSSSelectorList* selector_list =
      CSSSelectorList::AdoptSelectorVector(selector_vector);

  const unsigned kMaximumSelectorQueryCacheSize = 256;
  if (entries_.size() == kMaximumSelectorQueryCacheSize) {
    entries_.erase(entries_.begin());
  }

  return entries_
      .insert(selectors, MakeGarbageCollected<SelectorQuery>(selector_list))
      .stored_value->value.Get();
}

void SelectorQueryCache::Invalidate() {
  entries_.clear();
}

std::ostream& operator<<(std::ostream& stream,
                         const SelectorQuery::QueryStats& stats) {
  if (stats == SelectorQuery::QueryStats()) {
    return stream << "(empty)";
  } else {
    if (stats.elements_seen) {
      stream << ".elements_seen = " << stats.elements_seen << ", ";
    }
    if (stats.fast_id_roots) {
      stream << ".fast_id_roots = " << stats.fast_id_roots << ", ";
    }
    if (stats.check_id) {
      stream << ".check_id = " << stats.check_id << ", ";
    }
    if (stats.check_tag) {
      stream << ".check_tag = " << stats.check_tag << ", ";
    }
    if (stats.check_class) {
      stream << ".check_class = " << stats.check_class << ", ";
    }
    if (stats.check_attr) {
      stream << ".check_attr = " << stats.check_attr << ", ";
    }
    if (stats.check_nth_child) {
      stream << ".check_nth_child = " << stats.check_nth_child << ", ";
    }
    if (stats.recheck_selector) {
      stream << ".recheck_selector = " << stats.recheck_selector << ", ";
    }
    if (stats.slow_scan) {
      stream << ".slow_scan = " << stats.slow_scan << ", ";
    }
    if (stats.skipped_subtree) {
      stream << ".skipped_subtree = " << stats.skipped_subtree << ", ";
    }
    return stream;
  }
}

}  // namespace blink
