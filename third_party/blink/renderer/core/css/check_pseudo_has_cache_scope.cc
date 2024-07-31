// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/check_pseudo_has_cache_scope.h"

#include "third_party/blink/renderer/core/css/check_pseudo_has_argument_context.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CheckPseudoHasCacheScope::CheckPseudoHasCacheScope(
    Document* document,
    bool within_selector_checking)
    : document_(document), within_selector_checking_(within_selector_checking) {
  DCHECK(document_);

  if (within_selector_checking) {
    document_->EnterPseudoHasChecking();
  }

  if (document_->GetCheckPseudoHasCacheScope()) {
    return;
  }

  document_->SetCheckPseudoHasCacheScope(this);
}

CheckPseudoHasCacheScope::~CheckPseudoHasCacheScope() {
  if (within_selector_checking_) {
    document_->LeavePseudoHasChecking();
  }
  if (document_->GetCheckPseudoHasCacheScope() != this) {
    return;
  }

  document_->SetCheckPseudoHasCacheScope(nullptr);
}

// static
ElementCheckPseudoHasResultMap& CheckPseudoHasCacheScope::GetResultMap(
    const Document* document,
    const CSSSelector* selector) {
  // To increase the cache hit ratio, we need to have a same cache key
  // for multiple selector instances those are actually has a same selector.
  // TODO(blee@igalia.com) Find a way to get hash key without serialization.
  String selector_text = selector->SelectorTextExpandingPseudoParent();

  DCHECK(document);
  DCHECK(document->GetCheckPseudoHasCacheScope());

  auto entry = document->GetCheckPseudoHasCacheScope()->GetResultCache().insert(
      selector_text, nullptr);
  if (entry.is_new_entry) {
    entry.stored_value->value =
        MakeGarbageCollected<ElementCheckPseudoHasResultMap>();
  }
  DCHECK(entry.stored_value->value);
  return *entry.stored_value->value;
}

// static
ElementCheckPseudoHasFastRejectFilterMap&
CheckPseudoHasCacheScope::GetFastRejectFilterMap(
    const Document* document,
    CheckPseudoHasArgumentTraversalType traversal_type) {
  DCHECK(document);
  DCHECK(document->GetCheckPseudoHasCacheScope());

  auto entry = document->GetCheckPseudoHasCacheScope()
                   ->GetFastRejectFilterCache()
                   .insert(traversal_type, nullptr);
  if (entry.is_new_entry) {
    entry.stored_value->value =
        MakeGarbageCollected<ElementCheckPseudoHasFastRejectFilterMap>();
  }
  DCHECK(entry.stored_value->value);
  return *entry.stored_value->value;
}

CheckPseudoHasCacheScope::Context::Context(
    const Document* document,
    const CheckPseudoHasArgumentContext& argument_context)
    : argument_context_(argument_context) {
  switch (argument_context_.TraversalScope()) {
    case CheckPseudoHasArgumentTraversalScope::kSubtree:
    case CheckPseudoHasArgumentTraversalScope::kOneNextSiblingSubtree:
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees:
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblings:
      cache_allowed_ = true;
      result_map_ = &CheckPseudoHasCacheScope::GetResultMap(
          document, argument_context.HasArgument());
      fast_reject_filter_map_ =
          &CheckPseudoHasCacheScope::GetFastRejectFilterMap(
              document, argument_context.TraversalType());
      break;
    default:
      cache_allowed_ = false;
      break;
  }
}

CheckPseudoHasResult
CheckPseudoHasCacheScope::Context::SetMatchedAndGetOldResult(Element* element) {
  return SetResultAndGetOld(
      element, kCheckPseudoHasResultChecked | kCheckPseudoHasResultMatched);
}

void CheckPseudoHasCacheScope::Context::SetChecked(Element* element) {
  SetResultAndGetOld(element, kCheckPseudoHasResultChecked);
}

CheckPseudoHasResult CheckPseudoHasCacheScope::Context::SetResultAndGetOld(
    Element* element,
    CheckPseudoHasResult result) {
  DCHECK(cache_allowed_);
  DCHECK(result_map_);
  CheckPseudoHasResult old_result = kCheckPseudoHasResultNotCached;
  auto cache_result = result_map_->insert(element, result);
  if (!cache_result.is_new_entry) {
    old_result = cache_result.stored_value->value;
    cache_result.stored_value->value |= result;
  }

  // kCheckPseudoHasResultMatched must set with kCheckPseudoHasResultChecked
  DCHECK_NE(cache_result.stored_value->value &
                (kCheckPseudoHasResultMatched | kCheckPseudoHasResultChecked),
            kCheckPseudoHasResultMatched);

  // kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked must set with
  // kCheckPseudoHasResultChecked
  DCHECK_NE(cache_result.stored_value->value &
                (kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked |
                 kCheckPseudoHasResultChecked),
            kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked);

  return old_result;
}

void CheckPseudoHasCacheScope::Context::SetTraversedElementAsChecked(
    Element* traversed_element,
    Element* parent) {
  DCHECK(traversed_element);
  DCHECK(parent);
  DCHECK_EQ(traversed_element->parentElement(), parent);
  SetResultAndGetOld(
      traversed_element,
      kCheckPseudoHasResultChecked |
          kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked);
  SetResultAndGetOld(parent, kCheckPseudoHasResultSomeChildrenChecked);
}

void CheckPseudoHasCacheScope::Context::SetAllTraversedElementsAsChecked(
    Element* last_traversed_element,
    int last_traversed_depth) {
  DCHECK(last_traversed_element);
  switch (argument_context_.TraversalScope()) {
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees:
      if (last_traversed_depth == 1 &&
          !ElementTraversal::PreviousSibling(*last_traversed_element)) {
        // The :has() argument checking traversal stopped at the first child of
        // a depth 0 element. It means that, all the descendants of the depth 0
        // element were checked. In this case, we can set the depth 0 element as
        // '[NotMatched|Matched]AndAllDescendantsOrNextSiblingsChecked' instead
        // of setting it as '[NotCached|Matched]AndSomeChildrenChecked'.
        // We can skip the following :has() checking operation of the depth 0
        // element with the cached checking result ('NotMatched' or 'Matched').
        Element* parent = last_traversed_element->parentElement();
        SetTraversedElementAsChecked(parent, parent->parentElement());
        break;
      }
      [[fallthrough]];
    case CheckPseudoHasArgumentTraversalScope::kSubtree:
    case CheckPseudoHasArgumentTraversalScope::kOneNextSiblingSubtree: {
      // Mark the traversed elements in the subtree or next sibling subtree
      // of the :has() anchor element as checked.
      Element* element = last_traversed_element;
      Element* parent = element->parentElement();
      int depth = last_traversed_depth;
      for (; depth > 0; --depth) {
        if (element) {
          SetTraversedElementAsChecked(element, parent);
        }
        element = ElementTraversal::NextSibling(*parent);
        parent = parent->parentElement();
      }

      // If the argument checking traverses all the next siblings' subtrees,
      // it guarantees that we can get all the possibly matched next siblings.
      // By marking all the traversed next siblings as checked, we can skip
      // to check :has() on the already-checked next siblings.
      if (argument_context_.TraversalScope() ==
              CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees &&
          element) {
        SetTraversedElementAsChecked(element, parent);
      }
    } break;
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblings:
      DCHECK_EQ(last_traversed_depth, 0);
      // Mark the last traversed element and all its next siblings as checked.
      SetTraversedElementAsChecked(last_traversed_element,
                                   last_traversed_element->parentElement());
      break;
    default:
      break;
  }
}

CheckPseudoHasResult CheckPseudoHasCacheScope::Context::GetResult(
    Element* element) const {
  DCHECK(cache_allowed_);
  DCHECK(result_map_);
  auto iterator = result_map_->find(element);
  return iterator == result_map_->end() ? kCheckPseudoHasResultNotCached
                                        : iterator->value;
}

bool CheckPseudoHasCacheScope::Context::
    HasSiblingsWithAllDescendantsOrNextSiblingsChecked(Element* element) const {
  for (Element* sibling = ElementTraversal::PreviousSibling(*element); sibling;
       sibling = ElementTraversal::PreviousSibling(*sibling)) {
    CheckPseudoHasResult sibling_result = GetResult(sibling);
    if (sibling_result == kCheckPseudoHasResultNotCached) {
      continue;
    }
    if (sibling_result &
        kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked) {
      return true;
    }
  }
  return false;
}

bool CheckPseudoHasCacheScope::Context::
    HasAncestorsWithAllDescendantsOrNextSiblingsChecked(
        Element* element) const {
  for (Element* parent = element->parentElement(); parent;
       element = parent, parent = element->parentElement()) {
    CheckPseudoHasResult parent_result = GetResult(parent);
    if (parent_result == kCheckPseudoHasResultNotCached) {
      continue;
    }
    if (parent_result &
        kCheckPseudoHasResultAllDescendantsOrNextSiblingsChecked) {
      return true;
    }
    if (parent_result & kCheckPseudoHasResultSomeChildrenChecked) {
      if (HasSiblingsWithAllDescendantsOrNextSiblingsChecked(element)) {
        return true;
      }
    }
  }
  return false;
}

bool CheckPseudoHasCacheScope::Context::AlreadyChecked(Element* element) const {
  switch (argument_context_.TraversalScope()) {
    case CheckPseudoHasArgumentTraversalScope::kSubtree:
    case CheckPseudoHasArgumentTraversalScope::kOneNextSiblingSubtree:
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees:
      return HasAncestorsWithAllDescendantsOrNextSiblingsChecked(element);
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblings:
      if (Element* parent = element->parentElement()) {
        if (!(GetResult(parent) & kCheckPseudoHasResultSomeChildrenChecked)) {
          return false;
        }
        return HasSiblingsWithAllDescendantsOrNextSiblingsChecked(element);
      }
      break;
    default:
      break;
  }
  return false;
}

CheckPseudoHasFastRejectFilter&
CheckPseudoHasCacheScope::Context::EnsureFastRejectFilter(Element* element,
                                                          bool& is_new_entry) {
  DCHECK(element);
  DCHECK(cache_allowed_);
  DCHECK(fast_reject_filter_map_);

  is_new_entry = false;

  // In order to minimize memory consumption, if the traversal scope of an
  // other element is a superset of the traversal scope of the target element,
  // use the less accurate fast reject filter of the other element.
  switch (argument_context_.TraversalScope()) {
    case CheckPseudoHasArgumentTraversalScope::kSubtree:
      for (Element* parent = element->parentElement(); parent;
           parent = parent->parentElement()) {
        auto iterator = fast_reject_filter_map_->find(parent);
        if (iterator == fast_reject_filter_map_->end()) {
          continue;
        }
        if (!iterator->value->BloomFilterAllocated()) {
          continue;
        }
        return *iterator->value.get();
      }
      break;
    case CheckPseudoHasArgumentTraversalScope::kOneNextSiblingSubtree:
      for (Element* parent = element->parentElement(); parent;
           parent = parent->parentElement()) {
        Element* sibling = ElementTraversal::PreviousSibling(*parent);
        for (int i = argument_context_.AdjacentDistanceLimit() - 1;
             sibling && i >= 0;
             sibling = ElementTraversal::PreviousSibling(*sibling), --i) {
        }
        if (!sibling) {
          continue;
        }
        auto iterator = fast_reject_filter_map_->find(sibling);
        if (iterator == fast_reject_filter_map_->end()) {
          continue;
        }
        if (!iterator->value->BloomFilterAllocated()) {
          continue;
        }
        return *iterator->value.get();
      }
      break;
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees:
      for (Element* parent = element->parentElement(); parent;
           parent = parent->parentElement()) {
        for (Element* sibling = ElementTraversal::PreviousSibling(*parent);
             sibling; sibling = ElementTraversal::PreviousSibling(*sibling)) {
          auto iterator = fast_reject_filter_map_->find(sibling);
          if (iterator == fast_reject_filter_map_->end()) {
            continue;
          }
          if (!iterator->value->BloomFilterAllocated()) {
            continue;
          }
          return *iterator->value.get();
        }
      }
      break;
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblings:
      for (Element* sibling = ElementTraversal::PreviousSibling(*element);
           sibling; sibling = ElementTraversal::PreviousSibling(*sibling)) {
        auto iterator = fast_reject_filter_map_->find(sibling);
        if (iterator == fast_reject_filter_map_->end()) {
          continue;
        }
        if (!iterator->value->BloomFilterAllocated()) {
          continue;
        }
        return *iterator->value.get();
      }
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  auto entry = fast_reject_filter_map_->insert(element, nullptr);
  if (entry.is_new_entry) {
    entry.stored_value->value =
        std::make_unique<CheckPseudoHasFastRejectFilter>();
    is_new_entry = true;
  }
  DCHECK(entry.stored_value->value);
  return *entry.stored_value->value.get();
}

size_t
CheckPseudoHasCacheScope::Context::GetBloomFilterAllocationCountForTesting()
    const {
  if (!cache_allowed_) {
    return 0;
  }
  size_t bloom_filter_allocation_count = 0;
  for (const auto& iterator : *fast_reject_filter_map_) {
    if (iterator.value->BloomFilterAllocated()) {
      bloom_filter_allocation_count++;
    }
  }
  return bloom_filter_allocation_count;
}

}  // namespace blink
