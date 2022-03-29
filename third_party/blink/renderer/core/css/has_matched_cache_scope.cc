// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/has_matched_cache_scope.h"

#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/has_argument_match_context.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

HasMatchedCacheScope::HasMatchedCacheScope(Document* document)
    : document_(document) {
  DCHECK(document_);

  if (document_->GetHasMatchedCacheScope())
    return;

  document_->SetHasMatchedCacheScope(this);
}

HasMatchedCacheScope::~HasMatchedCacheScope() {
  if (document_->GetHasMatchedCacheScope() != this)
    return;

  document_->SetHasMatchedCacheScope(nullptr);
}

// static
ElementHasMatchedMap& HasMatchedCacheScope::GetCacheForSelector(
    const Document* document,
    const CSSSelector* selector) {
  // To increase the cache hit ratio, we need to have a same cache key
  // for multiple selector instances those are actually has a same selector.
  // TODO(blee@igalia.com) Find a way to get hash key without serialization.
  String selector_text = selector->SelectorText();

  DCHECK(document);
  DCHECK(document->GetHasMatchedCacheScope());

  HasMatchedCache& cache =
      document->GetHasMatchedCacheScope()->has_matched_cache_;

  auto element_has_matched_map = cache.find(selector_text);

  if (element_has_matched_map == cache.end()) {
    return *cache
                .Set(selector_text,
                     MakeGarbageCollected<ElementHasMatchedMap>())
                .stored_value->value;
  }

  return *element_has_matched_map->value;
}

HasMatchedCacheScope::Context::Context(
    const Document* document,
    const HasArgumentMatchContext& has_argument_match_context)
    : map_(HasMatchedCacheScope::GetCacheForSelector(
          document,
          has_argument_match_context.HasArgument())),
      argument_match_context_(has_argument_match_context) {}

uint8_t HasMatchedCacheScope::Context::SetMatchedAndGetOldResult(
    Element* element) {
  return SetResultAndGetOld(element, kChecked | kMatched);
}

void HasMatchedCacheScope::Context::SetChecked(Element* element) {
  SetResultAndGetOld(element, kChecked);
}

uint8_t HasMatchedCacheScope::Context::SetResultAndGetOld(
    Element* element,
    uint8_t match_result) {
  uint8_t old_result = kNotCached;
  auto cache_result = map_.insert(element, match_result);
  if (!cache_result.is_new_entry) {
    old_result = cache_result.stored_value->value;
    cache_result.stored_value->value |= match_result;
  }

  // kMatched must set with kChecked
  DCHECK_NE(cache_result.stored_value->value & (kMatched | kChecked), kMatched);

  // kAllDescendantsOrNextSiblingsChecked must set with kChecked
  DCHECK_NE(cache_result.stored_value->value &
                (kAllDescendantsOrNextSiblingsChecked | kChecked),
            kAllDescendantsOrNextSiblingsChecked);

  return old_result;
}

void HasMatchedCacheScope::Context::SetTraversedElementAsChecked(
    Element* traversed_element,
    Element* parent) {
  DCHECK(traversed_element);
  DCHECK(parent);
  DCHECK_EQ(traversed_element->parentElement(), parent);
  SetResultAndGetOld(traversed_element,
                     kChecked | kAllDescendantsOrNextSiblingsChecked);
  SetResultAndGetOld(parent, kSomeChildrenChecked);
}

void HasMatchedCacheScope::Context::SetAllTraversedElementsAsChecked(
    Element* last_traversed_element,
    int last_traversed_depth) {
  DCHECK(last_traversed_element);
  switch (argument_match_context_.TraversalScope()) {
    case HasArgumentMatchTraversalScope::kAllNextSiblingSubtrees:
      if (last_traversed_depth == 1 &&
          !ElementTraversal::PreviousSibling(*last_traversed_element)) {
        // The :has() argument matching traversal stopped at the first child of
        // a depth 0 element. It means that, all the descendants of the depth 0
        // element were checked. In this case, we can set the depth 0 element as
        // '[NotMatched|Matched]AndAllDescendantsOrNextSiblingsChecked' instead
        // of setting it as '[NotCached|Matched]AndSomeChildrenChecked'.
        // We can skip the following :has() matching operation of the depth 0
        // element with the cached matching result ('NotMatched' or 'Matched').
        Element* parent = last_traversed_element->parentElement();
        SetTraversedElementAsChecked(parent, parent->parentElement());
        break;
      }
      [[fallthrough]];
    case HasArgumentMatchTraversalScope::kSubtree:
    case HasArgumentMatchTraversalScope::kOneNextSiblingSubtree: {
      // Mark the traversed elements in the subtree or next sibling subtree
      // of the ':has()' scope element as checked.
      Element* element = last_traversed_element;
      Element* parent = element->parentElement();
      int depth = last_traversed_depth;
      for (; depth > 0; --depth) {
        if (element)
          SetTraversedElementAsChecked(element, parent);
        element = ElementTraversal::NextSibling(*parent);
        parent = parent->parentElement();
      }

      // If the argument matching traverses all the next siblings' subtrees,
      // it guarantees that we can get all the possibly matched next siblings.
      // By marking all the traversed next siblings as checked, we can skip
      // to match ':has()' on the already-checked next siblings.
      if (argument_match_context_.TraversalScope() ==
              HasArgumentMatchTraversalScope::kAllNextSiblingSubtrees &&
          element) {
        SetTraversedElementAsChecked(element, parent);
      }
    } break;
    case HasArgumentMatchTraversalScope::kAllNextSiblings:
      DCHECK_EQ(last_traversed_depth, 0);
      // Mark the last traversed element and all its next siblings as checked.
      SetTraversedElementAsChecked(last_traversed_element,
                                   last_traversed_element->parentElement());
      break;
    default:
      break;
  }
}

uint8_t HasMatchedCacheScope::Context::GetResult(Element* element) const {
  auto iterator = map_.find(element);
  return iterator == map_.end() ? kNotCached : iterator->value;
}

bool HasMatchedCacheScope::Context::
    HasSiblingsWithAllDescendantsOrNextSiblingsChecked(Element* element) const {
  for (Element* sibling = ElementTraversal::PreviousSibling(*element); sibling;
       sibling = ElementTraversal::PreviousSibling(*sibling)) {
    uint8_t sibling_result = GetResult(sibling);
    if (sibling_result == kNotCached)
      continue;
    if (sibling_result & kAllDescendantsOrNextSiblingsChecked)
      return true;
  }
  return false;
}

bool HasMatchedCacheScope::Context::
    HasAncestorsWithAllDescendantsOrNextSiblingsChecked(
        Element* element) const {
  for (Element* parent = element->parentElement(); parent;
       element = parent, parent = element->parentElement()) {
    uint8_t parent_result = GetResult(parent);
    if (parent_result == kNotCached)
      continue;
    if (parent_result & kAllDescendantsOrNextSiblingsChecked)
      return true;
    if (parent_result & kSomeChildrenChecked) {
      if (HasSiblingsWithAllDescendantsOrNextSiblingsChecked(element))
        return true;
    }
  }
  return false;
}

bool HasMatchedCacheScope::Context::AlreadyChecked(Element* element) const {
  switch (argument_match_context_.TraversalScope()) {
    case HasArgumentMatchTraversalScope::kSubtree:
    case HasArgumentMatchTraversalScope::kOneNextSiblingSubtree:
    case HasArgumentMatchTraversalScope::kAllNextSiblingSubtrees:
      return HasAncestorsWithAllDescendantsOrNextSiblingsChecked(element);
    case HasArgumentMatchTraversalScope::kAllNextSiblings:
      if (Element* parent = element->parentElement()) {
        if (!(GetResult(parent) & kSomeChildrenChecked))
          return false;
        return HasSiblingsWithAllDescendantsOrNextSiblingsChecked(element);
      }
      break;
    default:
      break;
  }
  return false;
}

}  // namespace blink
