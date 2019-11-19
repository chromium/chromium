// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/nth_index_cache.h"

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

namespace {

// Generating the cached nth-index counts when the number of children
// exceeds this count. This number is picked based on testing
// querySelectorAll for :nth-child(3n+2) and :nth-of-type(3n+2) on an
// increasing number of children.

const unsigned kCachedSiblingCountLimit = 32;

unsigned UncachedNthChildIndex(Element& element) {
  int index = 1;
  for (const Element* sibling = ElementTraversal::PreviousSibling(element);
       sibling; sibling = ElementTraversal::PreviousSibling(*sibling))
    index++;

  return index;
}

unsigned UncachedNthLastChildIndex(Element& element) {
  int index = 1;
  for (const Element* sibling = ElementTraversal::NextSibling(element); sibling;
       sibling = ElementTraversal::NextSibling(*sibling))
    ++index;
  return index;
}

unsigned UncachedNthOfTypeIndex(Element& element, unsigned& sibling_count) {
  int index = 1;
  const QualifiedName& tag = element.TagQName();
  for (const Element* sibling = ElementTraversal::PreviousSibling(element);
       sibling; sibling = ElementTraversal::PreviousSibling(*sibling)) {
    if (sibling->TagQName().Matches(tag))
      ++index;
    ++sibling_count;
  }
  return index;
}

unsigned UncachedNthLastOfTypeIndex(Element& element, unsigned& sibling_count) {
  int index = 1;
  const QualifiedName& tag = element.TagQName();
  for (const Element* sibling = ElementTraversal::NextSibling(element); sibling;
       sibling = ElementTraversal::NextSibling(*sibling)) {
    if (sibling->TagQName().Matches(tag))
      ++index;
    ++sibling_count;
  }
  return index;
}

}  // namespace

unsigned NthIndexCache::NthChildIndex(Element& element) {
  if (element.IsPseudoElement() || !element.parentNode())
    return 1;
  NthIndexCache* nth_index_cache = element.GetDocument().GetNthIndexCache();
  NthIndexData* nth_index_data = nullptr;
  if (nth_index_cache && nth_index_cache->parent_map_)
    nth_index_data = nth_index_cache->parent_map_->at(element.parentNode());
  if (nth_index_data)
    return nth_index_data->NthIndex(element);
  unsigned index = UncachedNthChildIndex(element);
  if (nth_index_cache && index > kCachedSiblingCountLimit)
    nth_index_cache->CacheNthIndexDataForParent(element);
  return index;
}

unsigned NthIndexCache::NthLastChildIndex(Element& element) {
  if (element.IsPseudoElement() && !element.parentNode())
    return 1;
  NthIndexCache* nth_index_cache = element.GetDocument().GetNthIndexCache();
  NthIndexData* nth_index_data = nullptr;
  if (nth_index_cache && nth_index_cache->parent_map_)
    nth_index_data = nth_index_cache->parent_map_->at(element.parentNode());
  if (nth_index_data)
    return nth_index_data->NthLastIndex(element);
  unsigned index = UncachedNthLastChildIndex(element);
  if (nth_index_cache && index > kCachedSiblingCountLimit)
    nth_index_cache->CacheNthIndexDataForParent(element);
  return index;
}

NthIndexData* NthIndexCache::NthTypeIndexDataForParent(Element& element) const {
  DCHECK(element.parentNode());
  if (!parent_map_for_type_)
    return nullptr;
  if (const IndexByType* map = parent_map_for_type_->at(element.parentNode()))
    return map->at(element.tagName());
  return nullptr;
}

unsigned NthIndexCache::NthOfTypeIndex(Element& element) {
  if (element.IsPseudoElement() || !element.parentNode())
    return 1;
  NthIndexCache* nth_index_cache = element.GetDocument().GetNthIndexCache();
  if (nth_index_cache) {
    if (NthIndexData* nth_index_data =
            nth_index_cache->NthTypeIndexDataForParent(element))
      return nth_index_data->NthOfTypeIndex(element);
  }
  unsigned sibling_count = 0;
  unsigned index = UncachedNthOfTypeIndex(element, sibling_count);
  if (nth_index_cache && sibling_count > kCachedSiblingCountLimit)
    nth_index_cache->CacheNthOfTypeIndexDataForParent(element);
  return index;
}

unsigned NthIndexCache::NthLastOfTypeIndex(Element& element) {
  if (element.IsPseudoElement() || !element.parentNode())
    return 1;
  NthIndexCache* nth_index_cache = element.GetDocument().GetNthIndexCache();
  if (nth_index_cache) {
    if (NthIndexData* nth_index_data =
            nth_index_cache->NthTypeIndexDataForParent(element))
      return nth_index_data->NthLastOfTypeIndex(element);
  }
  unsigned sibling_count = 0;
  unsigned index = UncachedNthLastOfTypeIndex(element, sibling_count);
  if (nth_index_cache && sibling_count > kCachedSiblingCountLimit)
    nth_index_cache->CacheNthOfTypeIndexDataForParent(element);
  return index;
}

void NthIndexCache::CacheNthIndexDataForParent(Element& element) {
  DCHECK(element.parentNode());
  if (!parent_map_)
    parent_map_ = MakeGarbageCollected<ParentMap>();

  ParentMap::AddResult add_result =
      parent_map_->insert(element.parentNode(), nullptr);
  DCHECK(add_result.is_new_entry);
  add_result.stored_value->value =
      MakeGarbageCollected<NthIndexData>(*element.parentNode());
}

NthIndexCache::IndexByType& NthIndexCache::EnsureTypeIndexMap(
    ContainerNode& parent) {
  if (!parent_map_for_type_)
    parent_map_for_type_ = MakeGarbageCollected<ParentMapForType>();

  ParentMapForType::AddResult add_result =
      parent_map_for_type_->insert(&parent, nullptr);
  if (add_result.is_new_entry)
    add_result.stored_value->value = MakeGarbageCollected<IndexByType>();

  DCHECK(add_result.stored_value->value);
  return *add_result.stored_value->value;
}

void NthIndexCache::CacheNthOfTypeIndexDataForParent(Element& element) {
  DCHECK(element.parentNode());
  IndexByType::AddResult add_result = EnsureTypeIndexMap(*element.parentNode())
                                          .insert(element.tagName(), nullptr);
  DCHECK(add_result.is_new_entry);
  add_result.stored_value->value = MakeGarbageCollected<NthIndexData>(
      *element.parentNode(), element.TagQName());
}

unsigned NthIndexData::NthIndex(Element& element) const {
  DCHECK(!element.IsPseudoElement());

  unsigned index = 0;
  for (Element *sibling = &element; sibling;
       sibling = ElementTraversal::PreviousSibling(*sibling), index++) {
    auto it = element_index_map_.find(sibling);
    if (it != element_index_map_.end())
      return it->value + index;
  }
  return index;
}

unsigned NthIndexData::NthOfTypeIndex(Element& element) const {
  DCHECK(!element.IsPseudoElement());

  unsigned index = 0;
  for (Element *sibling = &element; sibling;
       sibling = ElementTraversal::PreviousSibling(
           *sibling, HasTagName(element.TagQName())),
               index++) {
    auto it = element_index_map_.find(sibling);
    if (it != element_index_map_.end())
      return it->value + index;
  }
  return index;
}

unsigned NthIndexData::NthLastIndex(Element& element) const {
  return count_ - NthIndex(element) + 1;
}

unsigned NthIndexData::NthLastOfTypeIndex(Element& element) const {
  return count_ - NthOfTypeIndex(element) + 1;
}

NthIndexData::NthIndexData(ContainerNode& parent) {
  // The frequency at which we cache the nth-index for a set of siblings.  A
  // spread value of 3 means every third Element will have its nth-index cached.
  // Using a spread value > 1 is done to save memory. Looking up the nth-index
  // will still be done in constant time in terms of sibling count, at most
  // 'spread' elements will be traversed.
  const unsigned kSpread = 3;
  unsigned count = 0;
  for (Element* sibling = ElementTraversal::FirstChild(parent); sibling;
       sibling = ElementTraversal::NextSibling(*sibling)) {
    if (!(++count % kSpread))
      element_index_map_.insert(sibling, count);
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
    if (!(++count % kSpread))
      element_index_map_.insert(sibling, count);
  }
  DCHECK(count);
  count_ = count;
}

void NthIndexData::Trace(Visitor* visitor) {
  visitor->Trace(element_index_map_);
}

}  // namespace blink
