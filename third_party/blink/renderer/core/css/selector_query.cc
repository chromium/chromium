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
    return output.empty();
  }
  ALWAYS_INLINE static void AppendElement(OutputType& output,
                                          Element& element) {
    output.push_back(&element);
  }
};

inline bool SelectorMatches(const CSSSelector& selector,
                            Element& element,
                            const ContainerNode& root_node,
                            const SelectorChecker& checker) {
  SelectorChecker::SelectorCheckingContext context(&element);
  context.selector = &selector;
  context.scope = &root_node;
  context.tree_scope = &root_node.GetTreeScope();
  return checker.Match(context);
}

bool SelectorQuery::Matches(Element& target_element) const {
  QUERY_STATS_RESET();
  CheckPseudoHasCacheScope check_pseudo_has_cache_scope(
      &target_element.GetDocument(), /*within_selector_checking=*/false);
  return SelectorListMatches(target_element, target_element);
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

template <typename SelectorQueryTrait>
static void CollectElementsByClassName(
    ContainerNode& root_node,
    const AtomicString& class_name,
    const CSSSelector* selector,
    Element::TinyBloomFilter subject_filter,
    typename SelectorQueryTrait::OutputType& output) {
  SelectorChecker checker(SelectorChecker::kQueryingRules);
  for (Element& element :
       ElementTraversal::DescendantsOfWithFilter(root_node, subject_filter)) {
    QUERY_STATS_INCREMENT(fast_class);
    if (!element.SubtreeMayMatchClassOrAttrFilter(subject_filter)) {
#if DCHECK_IS_ON()
      DCHECK(!selector ||
             !SelectorMatches(*selector, element, root_node, checker))
          << element << " should have matched selector "
          << selector->SelectorText() << ", Bloom bits on element are "
          << element.AttributeOrClassBloomFilter();
#endif
      continue;
    }
    if (!element.HasClassName(class_name)) {
      continue;
    }
    if (selector && !SelectorMatches(*selector, element, root_node, checker)) {
      continue;
    }
    SelectorQueryTrait::AppendElement(output, element);
    if (SelectorQueryTrait::kShouldOnlyMatchFirstElement) {
      return;
    }
  }
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
      if (SelectorQueryTrait::kShouldOnlyMatchFirstElement) {
        return;
      }
    }
  }
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
         (case_insensitive && EqualIgnoringASCIICase(selector_value, value));
}

static Element::AttributesToExcludeHashesFor ExclusionPolicy(
    const ContainerNode& root_node) {
  return IsA<HTMLDocument>(root_node.GetDocument())
             ? Element::kExcludeAllLazilySynchronizedAttributes
             : Element::kExcludeLowercaseLazilySynchronizedAttributes;
}

template <typename SelectorQueryTrait>
static void CollectElementsByAttributeExact(
    ContainerNode& root_node,
    const CSSSelector& selector,
    Element::TinyBloomFilter subject_filter,
    typename SelectorQueryTrait::OutputType& output) {
  const QualifiedName& selector_attr = selector.Attribute();
  const AtomicString& selector_value = selector.Value();
  const bool is_html_doc = IsA<HTMLDocument>(root_node.GetDocument());
  // Legacy dictates that values of some attributes should be compared in
  // a case-insensitive manner regardless of whether the case insensitive
  // flag is set or not (but an explicit case sensitive flag will override
  // that, by causing LegacyCaseInsensitiveMatch() never to be set).
  const bool case_insensitive =
      selector.AttributeMatch() ==
          CSSSelector::AttributeMatchType::kCaseInsensitive ||
      (selector.LegacyCaseInsensitiveMatch() && is_html_doc);

  // SynchronizeAttribute() is rather expensive to call. We can determine ahead
  // of time if it's needed.
  const bool needs_synchronize_attribute =
      Element::IsExcludedAttribute(selector_attr, ExclusionPolicy(root_node));

  for (Element& element :
       ElementTraversal::DescendantsOfWithFilter(root_node, subject_filter)) {
    QUERY_STATS_INCREMENT(fast_scan);

    if (needs_synchronize_attribute) {
      // Synchronize the attribute in case it is lazy-computed.
      // Currently all lazy properties have a null namespace, so only pass
      // localName().
      element.SynchronizeAttribute(selector_attr.LocalName());
    }
#if !DCHECK_IS_ON()
    // In non-debug builds, we test the Bloom filter here and exit early
    // if the attribute could not exist on the element. For non-debug builds,
    // we go through the entire normal operation but verify that the Bloom
    // filter would not erroneously reject a match.
    if (!element.SubtreeMayMatchClassOrAttrFilter(subject_filter)) {
      continue;
    }
#endif
    AttributeCollection attributes = element.AttributesWithoutUpdate();
    for (const auto& attribute_item : attributes) {
      if (!attribute_item.Matches(selector_attr)) {
        if (element.IsHTMLElement() || !is_html_doc) {
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

#if DCHECK_IS_ON()
      // NOTE: Even if the value doesn't match, we want to check that the
      // attribute name was properly found.
      DCHECK(element.SubtreeMayMatchClassOrAttrFilter(subject_filter))
          << element << " should have contained attribute " << selector_attr
          << ", Bloom bits on element are "
          << element.AttributeOrClassBloomFilter();
#endif

      if (AttributeValueMatchesExact(attribute_item, selector_value,
                                     case_insensitive)) {
        SelectorQueryTrait::AppendElement(output, element);
        if (SelectorQueryTrait::kShouldOnlyMatchFirstElement) {
          return;
        }
        break;
      }

      if (selector_attr.NamespaceURI() != g_star_atom) {
        break;
      }
    }
  }
}

inline bool AncestorHasClassName(ContainerNode& root_node,
                                 const AtomicString& class_name) {
  auto* root_node_element = DynamicTo<Element>(root_node);
  if (!root_node_element) {
    return false;
  }

  for (auto* element = root_node_element; element;
       element = element->parentElement()) {
    if (element->HasClassName(class_name)) {
      return true;
    }
  }
  return false;
}

template <typename SelectorQueryTrait>
void SelectorQuery::FindTraverseRootsAndExecute(
    ContainerNode& root_node,
    Element::TinyBloomFilter subject_filter,
    typename SelectorQueryTrait::OutputType& output) const {
  // We need to return the matches in document order. To use id lookup while
  // there is possiblity of multiple matches we would need to sort the
  // results. For now, just traverse the document in that case.
  DCHECK_EQ(selector_start_offsets_.size(), 1u);

  bool is_subject = true;
  bool is_affected_by_sibling_combinator = false;
  Element::TinyBloomFilter ancestor_filter = subject_filter;

  for (const CSSSelector* selector = StartOfComplexSelector(0); selector;
       selector = selector->NextSimpleSelector()) {
    if (!is_affected_by_sibling_combinator &&
        selector->Match() == CSSSelector::kClass) {
      if (is_subject) {
        // Fast path for <anything> <anything>.class
        // (including the case with no descendant selectors).
        CollectElementsByClassName<SelectorQueryTrait>(
            root_node, selector->Value(), StartOfComplexSelector(0),
            subject_filter, output);
        return;
      }

      // Fast path for <anything>.class <not-sibling-combinator> <anything>.

      // Since there exists some ancestor element which has the class name, we
      // need to see all children of rootNode.
      if (AncestorHasClassName(root_node, selector->Value())) {
        break;
      }

      const AtomicString& class_name = selector->Value();
      Element* element =
          ElementTraversal::FirstWithin(root_node, ancestor_filter);
      while (element) {
        QUERY_STATS_INCREMENT(fast_class);
        if (element->HasClassName(class_name)) {
          ExecuteForTraverseRoot<SelectorQueryTrait>(*element, root_node,
                                                     subject_filter, output);
          if (SelectorQueryTrait::kShouldOnlyMatchFirstElement &&
              !SelectorQueryTrait::IsEmpty(output)) {
            return;
          }
          element = ElementTraversal::NextSkippingChildren(*element, &root_node,
                                                           ancestor_filter);
        } else {
          element =
              ElementTraversal::Next(*element, &root_node, ancestor_filter);
        }
      }
      return;
    }

    if (selector->Relation() == CSSSelector::kSubSelector) {
      continue;
    }

    // We couldn't find a class selector in the subject.
    // Move instead to looking for a class selector in an ancestor.
    SelectorFilter::CollectSubjectIdentifierHashes(
        selector, ExclusionPolicy(root_node), ancestor_filter);
    is_subject = false;

    is_affected_by_sibling_combinator =
        selector->Relation() == CSSSelector::kDirectAdjacent ||
        selector->Relation() == CSSSelector::kIndirectAdjacent;
  }

  // Regular path.
  ExecuteForTraverseRoot<SelectorQueryTrait>(root_node, root_node,
                                             subject_filter, output);
}

template <typename SelectorQueryTrait>
void SelectorQuery::ExecuteForTraverseRoot(
    ContainerNode& traverse_root,
    ContainerNode& root_node,
    Element::TinyBloomFilter subject_filter,
    typename SelectorQueryTrait::OutputType& output) const {
  DCHECK_EQ(selector_start_offsets_.size(), 1u);

  const CSSSelector& selector = *StartOfComplexSelector(0);
  SelectorChecker checker(SelectorChecker::kQueryingRules);

  for (Element& element : ElementTraversal::DescendantsOfWithFilter(
           traverse_root, subject_filter)) {
    QUERY_STATS_INCREMENT(fast_scan);
    if (SelectorMatches(selector, element, root_node, checker)) {
      SelectorQueryTrait::AppendElement(output, element);
      if (SelectorQueryTrait::kShouldOnlyMatchFirstElement) {
        return;
      }
    }
  }
}

bool SelectorQuery::SelectorListMatches(ContainerNode& root_node,
                                        Element& element) const {
  SelectorChecker checker(SelectorChecker::kQueryingRules);
  for (unsigned offset : selector_start_offsets_) {
    if (SelectorMatches(UNSAFE_TODO(*(selector_list_->First() + offset)),
                        element, root_node, checker)) {
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

template <typename SelectorQueryTrait>
void SelectorQuery::ExecuteWithId(
    ContainerNode& root_node,
    typename SelectorQueryTrait::OutputType& output) const {
  DCHECK_EQ(selector_start_offsets_.size(), 1u);
  DCHECK(!root_node.GetDocument().InQuirksMode());

  const CSSSelector& first_selector = *StartOfComplexSelector(0);
  DCHECK(root_node.IsInTreeScope());
  const TreeScope& scope = root_node.GetTreeScope();
  SelectorChecker checker(SelectorChecker::kQueryingRules);

  Element::TinyBloomFilter subject_filter = 0;
  SelectorFilter::CollectSubjectIdentifierHashes(
      &first_selector, ExclusionPolicy(root_node), subject_filter);

  if (scope.ContainsMultipleElementsWithId(selector_id_)) {
    // We don't currently handle cases where there's multiple elements with the
    // id and it's not in the rightmost selector.
    if (!selector_id_in_subject_) {
      FindTraverseRootsAndExecute<SelectorQueryTrait>(root_node, subject_filter,
                                                      output);
      return;
    }

    // Fast path for <anything> <anything>#id.
    const auto& elements = scope.GetAllElementsById(selector_id_);
    for (const auto& element : elements) {
      if (!element->IsDescendantOf(&root_node) ||
          !element->SubtreeMayMatchClassOrAttrFilter(subject_filter)) {
        continue;
      }
      QUERY_STATS_INCREMENT(fast_id);
      if (SelectorMatches(first_selector, *element, root_node, checker)) {
        SelectorQueryTrait::AppendElement(output, *element);
        if (SelectorQueryTrait::kShouldOnlyMatchFirstElement) {
          return;
        }
      }
    }
    return;
  }

  Element* element = scope.getElementById(selector_id_);
  if (!element) {
    return;
  }
  if (selector_id_in_subject_) {
    // Fast path for <anything> <anything>#id.
    if (!element->IsDescendantOf(&root_node)) {
      return;
    }
    QUERY_STATS_INCREMENT(fast_id);
    if (element->SubtreeMayMatchClassOrAttrFilter(subject_filter) &&
        SelectorMatches(first_selector, *element, root_node, checker)) {
      SelectorQueryTrait::AppendElement(output, *element);
    }
    return;
  }
  ContainerNode* start = &root_node;
  if (element->IsDescendantOf(&root_node)) {
    start = element;
    if (selector_id_affected_by_sibling_combinator_) {
      start = start->parentNode();
    }
  }
  if (!start) {
    return;
  }
  QUERY_STATS_INCREMENT(fast_id);
  ExecuteForTraverseRoot<SelectorQueryTrait>(*start, root_node, subject_filter,
                                             output);
}

template <typename SelectorQueryTrait>
void SelectorQuery::Execute(
    ContainerNode& root_node,
    typename SelectorQueryTrait::OutputType& output) const {
  if (selector_start_offsets_.empty()) {
    return;
  }

  if (use_slow_scan_) {
    ExecuteSlow<SelectorQueryTrait>(root_node, output);
    return;
  }

  DCHECK_EQ(selector_start_offsets_.size(), 1u);

  // In quirks mode getElementById("a") is case sensitive and should only
  // match elements with lowercase id "a", but querySelector is case-insensitive
  // so querySelector("#a") == querySelector("#A"), which means we can only use
  // the id fast path when we're in a standards mode document.
  if (selector_id_ && root_node.IsInTreeScope() &&
      !root_node.GetDocument().InQuirksMode()) {
    ExecuteWithId<SelectorQueryTrait>(root_node, output);
    return;
  }

  const CSSSelector& first_selector = *StartOfComplexSelector(0);
  Element::TinyBloomFilter subject_filter = 0;
  SelectorFilter::CollectSubjectIdentifierHashes(
      &first_selector, ExclusionPolicy(root_node), subject_filter);

  if (!first_selector.NextSimpleSelector()) {
    // Fast path for querySelector*('.foo'), and querySelector*('div').
    switch (first_selector.Match()) {
      case CSSSelector::kClass:
        CollectElementsByClassName<SelectorQueryTrait>(
            root_node, first_selector.Value(), nullptr, subject_filter, output);
        return;
      case CSSSelector::kTag:
      case CSSSelector::kUniversalTag:
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
      case CSSSelector::kAttributeExact:
        CollectElementsByAttributeExact<SelectorQueryTrait>(
            root_node, first_selector, subject_filter, output);
        return;
      default:
        break;  // If we need another fast path, add here.
    }
  }

  FindTraverseRootsAndExecute<SelectorQueryTrait>(root_node, subject_filter,
                                                  output);
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
    use_slow_scan_ = false;
    for (const CSSSelector* current = StartOfComplexSelector(0); current;
         current = current->NextSimpleSelector()) {
      if (current->Match() == CSSSelector::kId) {
        selector_id_ = current->Value();
        break;
      }
      // We only use the fast path when in standards mode where #id selectors
      // are case sensitive, so we need the same behavior for [id=value].
      if (current->Match() == CSSSelector::kAttributeExact &&
          current->Attribute() == html_names::kIdAttr &&
          current->AttributeMatch() ==
              CSSSelector::AttributeMatchType::kCaseSensitive) {
        selector_id_ = current->Value();
        break;
      }
      if (current->Relation() == CSSSelector::kSubSelector) {
        continue;
      }
      selector_id_in_subject_ = false;
      selector_id_affected_by_sibling_combinator_ =
          current->Relation() == CSSSelector::kDirectAdjacent ||
          current->Relation() == CSSSelector::kIndirectAdjacent;
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

}  // namespace blink
