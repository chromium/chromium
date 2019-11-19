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

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/nth_index_cache.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

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

#define QUERY_STATS_INCREMENT(name) \
  (void)(CurrentQueryStats().total_count++, CurrentQueryStats().name++);
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
    return output.IsEmpty();
  }
  ALWAYS_INLINE static void AppendElement(OutputType& output,
                                          Element& element) {
    output.push_back(&element);
  }
};

inline bool SelectorMatches(const CSSSelector& selector,
                            Element& element,
                            const ContainerNode& root_node) {
  SelectorChecker::Init init;
  init.mode = SelectorChecker::kQueryingRules;
  SelectorChecker checker(init);
  SelectorChecker::SelectorCheckingContext context(
      &element, SelectorChecker::kVisitedMatchDisabled);
  context.selector = &selector;
  context.scope = &root_node;
  return checker.Match(context);
}

bool SelectorQuery::Matches(Element& target_element) const {
  QUERY_STATS_RESET();
  if (needs_updated_distribution_)
    target_element.UpdateDistributionForFlatTreeTraversal();
  return SelectorListMatches(target_element, target_element);
}

Element* SelectorQuery::Closest(Element& target_element) const {
  QUERY_STATS_RESET();
  if (selectors_.IsEmpty())
    return nullptr;
  if (needs_updated_distribution_)
    target_element.UpdateDistributionForFlatTreeTraversal();

  for (Element* current_element = &target_element; current_element;
       current_element = current_element->parentElement()) {
    if (SelectorListMatches(target_element, *current_element))
      return current_element;
  }
  return nullptr;
}

StaticElementList* SelectorQuery::QueryAll(ContainerNode& root_node) const {
  QUERY_STATS_RESET();
  NthIndexCache nth_index_cache(root_node.GetDocument());
  HeapVector<Member<Element>> result;
  Execute<AllElementsSelectorQueryTrait>(root_node, result);
  return StaticElementList::Adopt(result);
}

Element* SelectorQuery::QueryFirst(ContainerNode& root_node) const {
  QUERY_STATS_RESET();
  NthIndexCache nth_index_cache(root_node.GetDocument());
  Element* matched_element = nullptr;
  Execute<SingleElementSelectorQueryTrait>(root_node, matched_element);
  return matched_element;
}

template <typename SelectorQueryTrait>
static void CollectElementsByClassName(
    ContainerNode& root_node,
    const AtomicString& class_name,
    const CSSSelector* selector,
    typename SelectorQueryTrait::OutputType& output) {
  for (Element& element : ElementTraversal::DescendantsOf(root_node)) {
    QUERY_STATS_INCREMENT(fast_class);
    if (!element.HasClassName(class_name))
      continue;
    if (selector && !SelectorMatches(*selector, element, root_node))
      continue;
    SelectorQueryTrait::AppendElement(output, element);
    if (SelectorQueryTrait::kShouldOnlyMatchFirstElement)
      return;
  }
}

inline bool MatchesTagName(const QualifiedName& tag_name,
                           const Element& element) {
  if (tag_name == AnyQName())
    return true;
  if (element.HasLocalName(tag_name.LocalName()))
    return true;
  // Non-html elements in html documents are normalized to their camel-cased
  // version during parsing if applicable. Yet, type selectors are lower-cased
  // for selectors in html documents. Compare the upper case converted names
  // instead to allow matching SVG elements like foreignObject.
  if (!element.IsHTMLElement() && element.GetDocument().IsHTMLDocument())
    return element.TagQName().LocalNameUpper() == tag_name.LocalNameUpper();
  return false;
}

template <typename SelectorQueryTrait>
static void CollectElementsByTagName(
    ContainerNode& root_node,
    const QualifiedName& tag_name,
    typename SelectorQueryTrait::OutputType& output) {
  DCHECK_EQ(tag_name.NamespaceURI(), g_star_atom);
  for (Element& element : ElementTraversal::DescendantsOf(root_node)) {
    QUERY_STATS_INCREMENT(fast_tag_name);
    if (MatchesTagName(tag_name, element)) {
      SelectorQueryTrait::AppendElement(output, element);
      if (SelectorQueryTrait::kShouldOnlyMatchFirstElement)
        return;
    }
  }
}

inline bool AncestorHasClassName(ContainerNode& root_node,
                                 const AtomicString& class_name) {
  auto* root_node_element = DynamicTo<Element>(root_node);
  if (!root_node_element)
    return false;

  for (auto* element = root_node_element; element;
       element = element->parentElement()) {
    if (element->HasClassName(class_name))
      return true;
  }
  return false;
}

template <typename SelectorQueryTrait>
void SelectorQuery::FindTraverseRootsAndExecute(
    ContainerNode& root_node,
    typename SelectorQueryTrait::OutputType& output) const {
  // We need to return the matches in document order. To use id lookup while
  // there is possiblity of multiple matches we would need to sort the
  // results. For now, just traverse the document in that case.
  DCHECK_EQ(selectors_.size(), 1u);

  bool is_rightmost_selector = true;
  bool is_affected_by_sibling_combinator = false;

  for (const CSSSelector* selector = selectors_[0]; selector;
       selector = selector->TagHistory()) {
    if (!is_affected_by_sibling_combinator &&
        selector->Match() == CSSSelector::kClass) {
      if (is_rightmost_selector) {
        CollectElementsByClassName<SelectorQueryTrait>(
            root_node, selector->Value(), selectors_[0], output);
        return;
      }
      // Since there exists some ancestor element which has the class name, we
      // need to see all children of rootNode.
      if (AncestorHasClassName(root_node, selector->Value()))
        break;

      const AtomicString& class_name = selector->Value();
      Element* element = ElementTraversal::FirstWithin(root_node);
      while (element) {
        QUERY_STATS_INCREMENT(fast_class);
        if (element->HasClassName(class_name)) {
          ExecuteForTraverseRoot<SelectorQueryTrait>(*element, root_node,
                                                     output);
          if (SelectorQueryTrait::kShouldOnlyMatchFirstElement &&
              !SelectorQueryTrait::IsEmpty(output))
            return;
          element =
              ElementTraversal::NextSkippingChildren(*element, &root_node);
        } else {
          element = ElementTraversal::Next(*element, &root_node);
        }
      }
      return;
    }

    if (selector->Relation() == CSSSelector::kSubSelector)
      continue;
    is_rightmost_selector = false;
    is_affected_by_sibling_combinator =
        selector->Relation() == CSSSelector::kDirectAdjacent ||
        selector->Relation() == CSSSelector::kIndirectAdjacent;
  }

  ExecuteForTraverseRoot<SelectorQueryTrait>(root_node, root_node, output);
}

template <typename SelectorQueryTrait>
void SelectorQuery::ExecuteForTraverseRoot(
    ContainerNode& traverse_root,
    ContainerNode& root_node,
    typename SelectorQueryTrait::OutputType& output) const {
  DCHECK_EQ(selectors_.size(), 1u);

  const CSSSelector& selector = *selectors_[0];

  for (Element& element : ElementTraversal::DescendantsOf(traverse_root)) {
    QUERY_STATS_INCREMENT(fast_scan);
    if (SelectorMatches(selector, element, root_node)) {
      SelectorQueryTrait::AppendElement(output, element);
      if (SelectorQueryTrait::kShouldOnlyMatchFirstElement)
        return;
    }
  }
}

bool SelectorQuery::SelectorListMatches(ContainerNode& root_node,
                                        Element& element) const {
  for (auto* const selector : selectors_) {
    if (SelectorMatches(*selector, element, root_node))
      return true;
  }
  return false;
}

template <typename SelectorQueryTrait>
void SelectorQuery::ExecuteSlow(
    ContainerNode& root_node,
    typename SelectorQueryTrait::OutputType& output) const {
  for (Element& element : ElementTraversal::DescendantsOf(root_node)) {
    QUERY_STATS_INCREMENT(slow_scan);
    if (!SelectorListMatches(root_node, element))
      continue;
    SelectorQueryTrait::AppendElement(output, element);
    if (SelectorQueryTrait::kShouldOnlyMatchFirstElement)
      return;
  }
}

// FIXME: Move the following helper functions, AuthorShadowRootOf,
// NextTraversingShadowTree to the best place, e.g. NodeTraversal.
static ShadowRoot* AuthorShadowRootOf(const ContainerNode& node) {
  if (!node.IsElementNode())
    return nullptr;
  ShadowRoot* root = node.GetShadowRoot();
  if (root && root->IsOpenOrV0())
    return root;
  return nullptr;
}

static ContainerNode* NextTraversingShadowTree(const ContainerNode& node,
                                               const ContainerNode* root_node) {
  if (ShadowRoot* shadow_root = AuthorShadowRootOf(node))
    return shadow_root;

  const ContainerNode* current = &node;
  while (current) {
    if (Element* next = ElementTraversal::Next(*current, root_node))
      return next;

    if (!current->IsInShadowTree())
      return nullptr;

    ShadowRoot* shadow_root = current->ContainingShadowRoot();
    if (shadow_root == root_node)
      return nullptr;

    current = &shadow_root->host();
  }
  return nullptr;
}

template <typename SelectorQueryTrait>
void SelectorQuery::ExecuteSlowTraversingShadowTree(
    ContainerNode& root_node,
    typename SelectorQueryTrait::OutputType& output) const {
  for (ContainerNode* node = NextTraversingShadowTree(root_node, &root_node);
       node; node = NextTraversingShadowTree(*node, &root_node)) {
    auto* element = DynamicTo<Element>(node);
    if (!element)
      continue;
    QUERY_STATS_INCREMENT(slow_traversing_shadow_tree_scan);
    if (!SelectorListMatches(root_node, *element))
      continue;
    SelectorQueryTrait::AppendElement(output, *element);
    if (SelectorQueryTrait::kShouldOnlyMatchFirstElement)
      return;
  }
}

template <typename SelectorQueryTrait>
void SelectorQuery::ExecuteWithId(
    ContainerNode& root_node,
    typename SelectorQueryTrait::OutputType& output) const {
  DCHECK_EQ(selectors_.size(), 1u);
  DCHECK(!root_node.GetDocument().InQuirksMode());

  const CSSSelector& first_selector = *selectors_[0];
  const TreeScope& scope = root_node.ContainingTreeScope();

  if (scope.ContainsMultipleElementsWithId(selector_id_)) {
    // We don't currently handle cases where there's multiple elements with the
    // id and it's not in the rightmost selector.
    if (!selector_id_is_rightmost_) {
      FindTraverseRootsAndExecute<SelectorQueryTrait>(root_node, output);
      return;
    }
    const auto& elements = scope.GetAllElementsById(selector_id_);
    for (const auto& element : elements) {
      if (!element->IsDescendantOf(&root_node))
        continue;
      QUERY_STATS_INCREMENT(fast_id);
      if (SelectorMatches(first_selector, *element, root_node)) {
        SelectorQueryTrait::AppendElement(output, *element);
        if (SelectorQueryTrait::kShouldOnlyMatchFirstElement)
          return;
      }
    }
    return;
  }

  Element* element = scope.getElementById(selector_id_);
  if (!element)
    return;
  if (selector_id_is_rightmost_) {
    if (!element->IsDescendantOf(&root_node))
      return;
    QUERY_STATS_INCREMENT(fast_id);
    if (SelectorMatches(first_selector, *element, root_node))
      SelectorQueryTrait::AppendElement(output, *element);
    return;
  }
  ContainerNode* start = &root_node;
  if (element->IsDescendantOf(&root_node))
    start = element;
  if (selector_id_affected_by_sibling_combinator_)
    start = start->parentNode();
  if (!start)
    return;
  QUERY_STATS_INCREMENT(fast_id);
  ExecuteForTraverseRoot<SelectorQueryTrait>(*start, root_node, output);
}

template <typename SelectorQueryTrait>
void SelectorQuery::Execute(
    ContainerNode& root_node,
    typename SelectorQueryTrait::OutputType& output) const {
  if (selectors_.IsEmpty())
    return;

  if (use_slow_scan_) {
    if (needs_updated_distribution_)
      root_node.UpdateDistributionForFlatTreeTraversal();
    if (uses_deep_combinator_or_shadow_pseudo_) {
      ExecuteSlowTraversingShadowTree<SelectorQueryTrait>(root_node, output);
    } else {
      ExecuteSlow<SelectorQueryTrait>(root_node, output);
    }
    return;
  }

  DCHECK_EQ(selectors_.size(), 1u);
  DCHECK(!needs_updated_distribution_);
  DCHECK(!uses_deep_combinator_or_shadow_pseudo_);

  // In quirks mode getElementById("a") is case sensitive and should only
  // match elements with lowercase id "a", but querySelector is case-insensitive
  // so querySelector("#a") == querySelector("#A"), which means we can only use
  // the id fast path when we're in a standards mode document.
  if (selector_id_ && root_node.IsInTreeScope() &&
      !root_node.GetDocument().InQuirksMode()) {
    ExecuteWithId<SelectorQueryTrait>(root_node, output);
    return;
  }

  const CSSSelector& first_selector = *selectors_[0];
  if (!first_selector.TagHistory()) {
    // Fast path for querySelector*('.foo'), and querySelector*('div').
    switch (first_selector.Match()) {
      case CSSSelector::kClass:
        CollectElementsByClassName<SelectorQueryTrait>(
            root_node, first_selector.Value(), nullptr, output);
        return;
      case CSSSelector::kTag:
        if (first_selector.TagQName().NamespaceURI() == g_star_atom) {
          CollectElementsByTagName<SelectorQueryTrait>(
              root_node, first_selector.TagQName(), output);
          return;
        }
        // querySelector*() doesn't allow namespace prefix resolution and
        // throws before we get here, but we still may have selectors for
        // elements without a namespace.
        DCHECK_EQ(first_selector.TagQName().NamespaceURI(), g_null_atom);
        break;
      default:
        break;  // If we need another fast path, add here.
    }
  }

  FindTraverseRootsAndExecute<SelectorQueryTrait>(root_node, output);
}

std::unique_ptr<SelectorQuery> SelectorQuery::Adopt(
    CSSSelectorList selector_list) {
  return base::WrapUnique(new SelectorQuery(std::move(selector_list)));
}

SelectorQuery::SelectorQuery(CSSSelectorList selector_list)
    : selector_list_(std::move(selector_list)),
      selector_id_is_rightmost_(true),
      selector_id_affected_by_sibling_combinator_(false),
      uses_deep_combinator_or_shadow_pseudo_(false),
      needs_updated_distribution_(false),
      use_slow_scan_(true) {
  selectors_.ReserveInitialCapacity(selector_list_.ComputeLength());
  for (const CSSSelector* selector = selector_list_.First(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    if (selector->MatchesPseudoElement())
      continue;
    selectors_.UncheckedAppend(selector);
    uses_deep_combinator_or_shadow_pseudo_ |=
        selector->HasDeepCombinatorOrShadowPseudo();
    needs_updated_distribution_ |= selector->NeedsUpdatedDistribution();
  }

  if (selectors_.size() == 1 && !uses_deep_combinator_or_shadow_pseudo_ &&
      !needs_updated_distribution_) {
    use_slow_scan_ = false;
    for (const CSSSelector* current = selectors_[0]; current;
         current = current->TagHistory()) {
      if (current->Match() == CSSSelector::kId) {
        selector_id_ = current->Value();
        break;
      }
      // We only use the fast path when in standards mode where #id selectors
      // are case sensitive, so we need the same behavior for [id=value].
      if (current->Match() == CSSSelector::kAttributeExact &&
          current->Attribute() == html_names::kIdAttr &&
          current->AttributeMatch() == CSSSelector::kCaseSensitive) {
        selector_id_ = current->Value();
        break;
      }
      if (current->Relation() == CSSSelector::kSubSelector)
        continue;
      selector_id_is_rightmost_ = false;
      selector_id_affected_by_sibling_combinator_ =
          current->Relation() == CSSSelector::kDirectAdjacent ||
          current->Relation() == CSSSelector::kIndirectAdjacent;
    }
  }
}

SelectorQuery* SelectorQueryCache::Add(const AtomicString& selectors,
                                       const Document& document,
                                       ExceptionState& exception_state) {
  if (selectors.IsEmpty()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The provided selector is empty.");
    return nullptr;
  }

  HashMap<AtomicString, std::unique_ptr<SelectorQuery>>::iterator it =
      entries_.find(selectors);
  if (it != entries_.end())
    return it->value.get();

  CSSSelectorList selector_list = CSSParser::ParseSelector(
      MakeGarbageCollected<CSSParserContext>(
          document, document.BaseURL(), true /* origin_clean */,
          document.GetReferrerPolicy(), WTF::TextEncoding(),
          CSSParserContext::kSnapshotProfile),
      nullptr, selectors);

  if (!selector_list.First()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "'" + selectors + "' is not a valid selector.");
    return nullptr;
  }

  const unsigned kMaximumSelectorQueryCacheSize = 256;
  if (entries_.size() == kMaximumSelectorQueryCacheSize)
    entries_.erase(entries_.begin());

  return entries_
      .insert(selectors, SelectorQuery::Adopt(std::move(selector_list)))
      .stored_value->value.get();
}

void SelectorQueryCache::Invalidate() {
  entries_.clear();
}

}  // namespace blink
