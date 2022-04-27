// Copyright 2021 The Chromium Authors. All rights reserved.
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

CheckPseudoHasCacheScope::CheckPseudoHasCacheScope(Document* document)
    : document_(document) {
  DCHECK(document_);

  if (document_->GetCheckPseudoHasCacheScope())
    return;

  document_->SetCheckPseudoHasCacheScope(this);
}

CheckPseudoHasCacheScope::~CheckPseudoHasCacheScope() {
  if (document_->GetCheckPseudoHasCacheScope() != this)
    return;

  document_->SetCheckPseudoHasCacheScope(nullptr);
}

// static
ElementCheckPseudoHasResultMap& CheckPseudoHasCacheScope::GetResultMap(
    const Document* document,
    const CSSSelector* selector) {
  // To increase the cache hit ratio, we need to have a same cache key
  // for multiple selector instances those are actually has a same selector.
  // TODO(blee@igalia.com) Find a way to get hash key without serialization.
  String selector_text = selector->SelectorText();

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
      break;
    default:
      cache_allowed_ = false;
      break;
  }
}

uint8_t CheckPseudoHasCacheScope::Context::SetMatchedAndGetOldResult(
    Element* element) {
  return SetResultAndGetOld(element, kChecked | kMatched);
}

void CheckPseudoHasCacheScope::Context::SetChecked(Element* element) {
  SetResultAndGetOld(element, kChecked);
}

uint8_t CheckPseudoHasCacheScope::Context::SetResultAndGetOld(Element* element,
                                                              uint8_t result) {
  DCHECK(cache_allowed_);
  DCHECK(result_map_);
  uint8_t old_result = kNotCached;
  auto cache_result = result_map_->insert(element, result);
  if (!cache_result.is_new_entry) {
    old_result = cache_result.stored_value->value;
    cache_result.stored_value->value |= result;
  }

  // kMatched must set with kChecked
  DCHECK_NE(cache_result.stored_value->value & (kMatched | kChecked), kMatched);

  // kAllDescendantsOrNextSiblingsChecked must set with kChecked
  DCHECK_NE(cache_result.stored_value->value &
                (kAllDescendantsOrNextSiblingsChecked | kChecked),
            kAllDescendantsOrNextSiblingsChecked);

  return old_result;
}

void CheckPseudoHasCacheScope::Context::SetTraversedElementAsChecked(
    Element* traversed_element,
    Element* parent) {
  DCHECK(traversed_element);
  DCHECK(parent);
  DCHECK_EQ(traversed_element->parentElement(), parent);
  SetResultAndGetOld(traversed_element,
                     kChecked | kAllDescendantsOrNextSiblingsChecked);
  SetResultAndGetOld(parent, kSomeChildrenChecked);
}

void CheckPseudoHasCacheScope::Context::SetAllTraversedElementsAsChecked(
    Element* last_traversed_element,
    int last_traversed_depth) {
  DCHECK(last_traversed_element);
  switch (argument_context_.TraversalScope()) {
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees:
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
    case CheckPseudoHasArgumentTraversalScope::kSubtree:
    case CheckPseudoHasArgumentTraversalScope::kOneNextSiblingSubtree: {
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

uint8_t CheckPseudoHasCacheScope::Context::GetResult(Element* element) const {
  DCHECK(cache_allowed_);
  DCHECK(result_map_);
  auto iterator = result_map_->find(element);
  return iterator == result_map_->end() ? kNotCached : iterator->value;
}

bool CheckPseudoHasCacheScope::Context::
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

bool CheckPseudoHasCacheScope::Context::
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

bool CheckPseudoHasCacheScope::Context::AlreadyChecked(Element* element) const {
  switch (argument_context_.TraversalScope()) {
    case CheckPseudoHasArgumentTraversalScope::kSubtree:
    case CheckPseudoHasArgumentTraversalScope::kOneNextSiblingSubtree:
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblingSubtrees:
      return HasAncestorsWithAllDescendantsOrNextSiblingsChecked(element);
    case CheckPseudoHasArgumentTraversalScope::kAllNextSiblings:
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
