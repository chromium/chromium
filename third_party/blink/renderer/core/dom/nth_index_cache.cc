// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/nth_index_cache.h"

#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"

namespace blink {

NthIndexCache::NthIndexCache(Document& document)
    : document_(&document)
#if DCHECK_IS_ON()
      ,
      dom_tree_version_(document.DomTreeVersion())
#endif
{
  document.SetNthIndexCache(this);
}

NthIndexCache::~NthIndexCache() {
#if DCHECK_IS_ON()
  DCHECK_EQ(dom_tree_version_, document_->DomTreeVersion());
#endif
  document_->SetNthIndexCache(nullptr);
}

void NthIndexCache::Key::Trace(Visitor* visitor) const {
  visitor->Trace(parent);
  visitor->Trace(filter);
}

unsigned NthIndexCache::Key::GetHash() const {
  unsigned hash = WTF::GetHash(parent);
  if (filter != nullptr) {
    WTF::AddIntToHash(hash, WTF::GetHash(filter));
  }
  if (!child_tag_name.empty()) {
    WTF::AddIntToHash(hash, WTF::GetHash(child_tag_name));
  }
  return hash;
}

namespace {

// Generating the cached nth-index counts when the number of children
// exceeds this count. This number is picked based on testing
// querySelectorAll for :nth-child(3n+2) and :nth-of-type(3n+2) on an
// increasing number of children.

const unsigned kCachedSiblingCountLimit = 32;

unsigned UncachedNthOfTypeIndex(Element& element, unsigned& sibling_count) {
  int index = 1;
  const QualifiedName& tag = element.TagQName();
  for (const Element* sibling = ElementTraversal::PreviousSibling(element);
       sibling; sibling = ElementTraversal::PreviousSibling(*sibling)) {
    if (sibling->TagQName().Matches(tag)) {
      ++index;
    }
    ++sibling_count;
  }
  return index;
}

unsigned UncachedNthLastOfTypeIndex(Element& element, unsigned& sibling_count) {
  int index = 1;
  const QualifiedName& tag = element.TagQName();
  for (const Element* sibling = ElementTraversal::NextSibling(element); sibling;
       sibling = ElementTraversal::NextSibling(*sibling)) {
    if (sibling->TagQName().Matches(tag)) {
      ++index;
    }
    ++sibling_count;
  }
  return index;
}

}  // namespace

bool NthIndexCache::MatchesFilter(
    Element* element,
    const CSSSelectorList* filter,
    const SelectorChecker* selector_checker,
    const SelectorChecker::SelectorCheckingContext* context) {
  if (filter == nullptr) {
    // With no selector list, consider all elements.
    return true;
  }

  SelectorChecker::SelectorCheckingContext sub_context(*context);
  sub_context.element = element;
  sub_context.is_sub_selector = true;
  sub_context.in_nested_complex_selector = true;
  sub_context.pseudo_id = kPseudoIdNone;
  for (sub_context.selector = filter->First(); sub_context.selector;
       sub_context.selector = CSSSelectorList::Next(*sub_context.selector)) {
    // NOTE: We don't want to propagate match_result up to the parent;
    // the correct flags were already set when the caller tested that
    // the element matched the selector list itself.
    SelectorChecker::MatchResult dummy_match_result;
    if (selector_checker->MatchSelector(sub_context, dummy_match_result) ==
        SelectorChecker::kSelectorMatches) {
      return true;
    }
  }
  return false;
}

unsigned NthIndexCache::UncachedNthChildIndex(
    Element& element,
    const CSSSelectorList* filter,
    const SelectorChecker* selector_checker,
    const SelectorChecker::SelectorCheckingContext* context,
    unsigned& sibling_count) {
  int index = 1;
  for (Element* sibling = ElementTraversal::PreviousSibling(element); sibling;
       sibling = ElementTraversal::PreviousSibling(*sibling)) {
    if (MatchesFilter(sibling, filter, selector_checker, context)) {
      ++index;
    }
    ++sibling_count;
  }

  return index;
}

unsigned NthIndexCache::UncachedNthLastChildIndex(
    Element& element,
    const CSSSelectorList* filter,
    const SelectorChecker* selector_checker,
    const SelectorChecker::SelectorCheckingContext* context,
    unsigned& sibling_count) {
  int index = 1;
  for (Element* sibling = ElementTraversal::NextSibling(element); sibling;
       sibling = ElementTraversal::NextSibling(*sibling)) {
    if (MatchesFilter(sibling, filter, selector_checker, context)) {
      index++;
    }
    ++sibling_count;
  }
  return index;
}

unsigned NthIndexCache::NthChildIndex(
    Element& element,
    const CSSSelectorList* filter,
    const SelectorChecker* selector_checker,
    const SelectorChecker::SelectorCheckingContext* context) {
  if (element.IsPseudoElement() || !element.parentNode()) {
    return 1;
  }
  NthIndexCache* nth_index_cache = element.GetDocument().GetNthIndexCache();
  if (nth_index_cache && nth_index_cache->cache_) {
    auto it = nth_index_cache->cache_->Find<KeyHashTranslator>(
        Key(element.parentNode(), filter));
    if (it != nth_index_cache->cache_->end()) {
      unsigned result =
          it->value->NthIndex(element, filter, selector_checker, context);
      [[maybe_unused]] unsigned sibling_count = 0;
      DCHECK_EQ(result, UncachedNthChildIndex(element, filter, selector_checker,
                                              context, sibling_count));
      return result;
    }
  }
  unsigned sibling_count = 0;
  unsigned index = UncachedNthChildIndex(element, filter, selector_checker,
                                         context, sibling_count);
  if (nth_index_cache && sibling_count > kCachedSiblingCountLimit) {
    nth_index_cache->CacheNthIndexDataForParent(element, filter,
                                                selector_checker, context);
  }
  return index;
}

unsigned NthIndexCache::NthLastChildIndex(
    Element& element,
    const CSSSelectorList* filter,
    const SelectorChecker* selector_checker,
    const SelectorChecker::SelectorCheckingContext* context) {
  if (element.IsPseudoElement() && !element.parentNode()) {
    return 1;
  }
  NthIndexCache* nth_index_cache = element.GetDocument().GetNthIndexCache();
  if (nth_index_cache && nth_index_cache->cache_) {
    auto it = nth_index_cache->cache_->Find<KeyHashTranslator>(
        Key(element.parentNode(), filter));
    if (it != nth_index_cache->cache_->end()) {
      unsigned result =
          it->value->NthLastIndex(element, filter, selector_checker, context);
      [[maybe_unused]] unsigned sibling_count = 0;
      DCHECK_EQ(result,
                UncachedNthLastChildIndex(element, filter, selector_checker,
                                          context, sibling_count));
      return result;
    }
  }
  unsigned sibling_count = 0;
  unsigned index = UncachedNthLastChildIndex(element, filter, selector_checker,
                                             context, sibling_count);
  if (nth_index_cache && sibling_count > kCachedSiblingCountLimit) {
    nth_index_cache->CacheNthIndexDataForParent(element, filter,
                                                selector_checker, context);
  }
  return index;
}

unsigned NthIndexCache::NthOfTypeIndex(Element& element) {
  if (element.IsPseudoElement() || !element.parentNode()) {
    return 1;
  }
  NthIndexCache* nth_index_cache = element.GetDocument().GetNthIndexCache();
  if (nth_index_cache && nth_index_cache->cache_) {
    auto it = nth_index_cache->cache_->Find<KeyHashTranslator>(
        Key(element.parentNode(), element.tagName()));
    if (it != nth_index_cache->cache_->end()) {
      return it->value->NthOfTypeIndex(element);
    }
  }
  unsigned sibling_count = 0;
  unsigned index = UncachedNthOfTypeIndex(element, sibling_count);
  if (nth_index_cache && sibling_count > kCachedSiblingCountLimit) {
    nth_index_cache->CacheNthOfTypeIndexDataForParent(element);
  }
  return index;
}

unsigned NthIndexCache::NthLastOfTypeIndex(Element& element) {
  if (element.IsPseudoElement() || !element.parentNode()) {
    return 1;
  }
  NthIndexCache* nth_index_cache = element.GetDocument().GetNthIndexCache();
  if (nth_index_cache && nth_index_cache->cache_) {
    auto it = nth_index_cache->cache_->Find<KeyHashTranslator>(
        Key(element.parentNode(), element.tagName()));
    if (it != nth_index_cache->cache_->end()) {
      return it->value->NthLastOfTypeIndex(element);
    }
  }
  unsigned sibling_count = 0;
  unsigned index = UncachedNthLastOfTypeIndex(element, sibling_count);
  if (nth_index_cache && sibling_count > kCachedSiblingCountLimit) {
    nth_index_cache->CacheNthOfTypeIndexDataForParent(element);
  }
  return index;
}

void NthIndexCache::EnsureCache() {
  if (!cache_) {
    cache_ = MakeGarbageCollected<
        HeapHashMap<Member<Key>, Member<NthIndexData>, KeyHashTraits>>();
  }
}

void NthIndexCache::CacheNthIndexDataForParent(
    Element& element,
    const CSSSelectorList* filter,
    const SelectorChecker* selector_checker,
    const SelectorChecker::SelectorCheckingContext* context) {
  DCHECK(element.parentNode());
  EnsureCache();
  auto add_result = cache_->insert(
      MakeGarbageCollected<Key>(element.parentNode(), filter),
      MakeGarbageCollected<NthIndexData>(*element.parentNode(), filter,
                                         selector_checker, context));
  DCHECK(add_result.is_new_entry);
}

void NthIndexCache::CacheNthOfTypeIndexDataForParent(Element& element) {
  DCHECK(element.parentNode());
  EnsureCache();
  auto add_result = cache_->insert(
      MakeGarbageCollected<Key>(element.parentNode(), element.tagName()),
      MakeGarbageCollected<NthIndexData>(*element.parentNode(),
                                         element.TagQName()));
  DCHECK(add_result.is_new_entry);
}

unsigned NthIndexData::NthIndex(
    Element& element,
    const CSSSelectorList* filter,
    const SelectorChecker* selector_checker,
    const SelectorChecker::SelectorCheckingContext* context) const {
  DCHECK(!element.IsPseudoElement());
  auto matches = [&](Element& element) {
    return NthIndexCache::MatchesFilter(&element, filter, selector_checker,
                                        context);
  };

  unsigned index = 0;
  for (Element* sibling = &element; sibling;
       sibling = ElementTraversal::PreviousSibling(*sibling)) {
    if (!matches(*sibling)) {
      continue;
    }
    auto it = element_index_map_.find(sibling);
    if (it != element_index_map_.end()) {
      return it->value + index;
    }
    ++index;
  }
  return index;
}

unsigned NthIndexData::NthOfTypeIndex(Element& element) const {
  DCHECK(!element.IsPseudoElement());

  unsigned index = 0;
  for (Element* sibling = &element; sibling;
       sibling = ElementTraversal::PreviousSibling(
           *sibling, HasTagName(element.TagQName())),
                index++) {
    auto it = element_index_map_.find(sibling);
    if (it != element_index_map_.end()) {
      return it->value + index;
    }
  }
  return index;
}

unsigned NthIndexData::NthLastIndex(
    Element& element,
    const CSSSelectorList* filter,
    const SelectorChecker* selector_checker,
    const SelectorChecker::SelectorCheckingContext* context) const {
  return count_ - NthIndex(element, filter, selector_checker, context) + 1;
}

unsigned NthIndexData::NthLastOfTypeIndex(Element& element) const {
  return count_ - NthOfTypeIndex(element) + 1;
}

NthIndexData::NthIndexData(
    ContainerNode& parent,
    const CSSSelectorList* filter,
    const SelectorChecker* selector_checker,
    const SelectorChecker::SelectorCheckingContext* context) {
  auto matches = [&](Element& element) {
    return NthIndexCache::MatchesFilter(&element, filter, selector_checker,
                                        context);
  };

  // The frequency at which we cache the nth-index for a set of siblings.  A
  // spread value of 3 means every third Element will have its nth-index cached.
  // Using a spread value > 1 is done to save memory. Looking up the nth-index
  // will still be done in constant time in terms of sibling count, at most
  // 'spread' elements will be traversed.
  const unsigned kSpread = 3;
  unsigned count = 0;
  for (Element* sibling = ElementTraversal::FirstChild(parent, matches);
       sibling; sibling = ElementTraversal::NextSibling(*sibling, matches)) {
    if (!(++count % kSpread)) {
      element_index_map_.insert(sibling, count);
    }
  }
  DCHECK(count);
  count_ = count;
}

NthIndexData::NthIndexData(ContainerNode& parent, const QualifiedName& type) {
  // The frequency at which we cache the nth-index of type for a set of
  // siblings.  A spread value of 3 means every third Element of its type will
  // have its nth-index cached.  Using a spread value > 1 is done to save
  // memory. Looking up the nth-index of its type will still be done in less
  // time, as most number of elements traversed will be equal to find 'spread'
  // elements in the sibling set.
  const unsigned kSpread = 3;
  unsigned count = 0;
  for (Element* sibling =
           ElementTraversal::FirstChild(parent, HasTagName(type));
       sibling;
       sibling = ElementTraversal::NextSibling(*sibling, HasTagName(type))) {
    if (!(++count % kSpread)) {
      element_index_map_.insert(sibling, count);
    }
  }
  DCHECK(count);
  count_ = count;
}

void NthIndexData::Trace(Visitor* visitor) const {
  visitor->Trace(element_index_map_);
}

}  // namespace blink
