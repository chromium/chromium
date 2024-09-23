/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/dom/user_action_element_set.h"

#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node.h"

namespace blink {

UserActionElementSet::UserActionElementSet() = default;

void UserActionElementSet::DidDetach(Element& element) {
  DCHECK(element.IsUserActionElement());
  ClearFlags(&element, kIsActiveFlag | kInActiveChainFlag | kIsHoveredFlag |
                           kHasFocusWithinFlag);
}

bool UserActionElementSet::HasFlags(const Node* node, unsigned flags) const {
  DCHECK(node->IsUserActionElement() && node->IsElementNode());
  return HasFlags(To<Element>(node), flags);
}

HeapVector<Member<Element>> UserActionElementSet::GetAllWithFlags(
    const unsigned flags) const {
  HeapVector<Member<Element>> found_elements;
  for (const auto& pair : elements_) {
    if (pair.value & flags) {
      found_elements.push_back(pair.key);
    }
  }
  return found_elements;
}

void UserActionElementSet::SetFlags(Node* node, unsigned flags) {
  auto* this_element = DynamicTo<Element>(node);
  if (!this_element)
    return;
  return SetFlags(this_element, flags);
}

void UserActionElementSet::ClearFlags(Node* node, unsigned flags) {
  auto* this_element = DynamicTo<Element>(node);
  if (!this_element)
    return;
  return ClearFlags(this_element, flags);
}

inline bool UserActionElementSet::HasFlags(const Element* element,
                                           unsigned flags) const {
  DCHECK(element->IsUserActionElement());
  ElementFlagMap::const_iterator found =
      elements_.find(const_cast<Element*>(element));
  if (found == elements_.end())
    return false;
  return found->value & flags;
}

inline void UserActionElementSet::ClearFlags(Element* element, unsigned flags) {
  if (!element->IsUserActionElement()) {
    DCHECK(elements_.end() == elements_.find(element));
    return;
  }

  ElementFlagMap::iterator found = elements_.find(element);
  if (found == elements_.end()) {
    element->SetUserActionElement(false);
    return;
  }

  unsigned updated = found->value & ~flags;
  if (!updated) {
    element->SetUserActionElement(false);
    elements_.erase(found);
    return;
  }

  found->value = updated;
}

inline void UserActionElementSet::SetFlags(Element* element, unsigned flags) {
  ElementFlagMap::iterator result = elements_.find(element);
  if (result != elements_.end()) {
    DCHECK(element->IsUserActionElement());
    result->value |= flags;
    return;
  }

  element->SetUserActionElement(true);
  elements_.insert(element, flags);
}

void UserActionElementSet::Trace(Visitor* visitor) const {
  visitor->Trace(elements_);
}

}  // namespace blink
