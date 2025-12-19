// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INDEXED_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INDEXED_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/pseudo_element.h"

namespace blink {

// An indexed pseudo-element tracks its index amongst its siblings of
// the same type.
class IndexedPseudoElement : public PseudoElement {
 public:
  IndexedPseudoElement(Element* parent,
                       PseudoId pseudo_id,
                       wtf_size_t index,
                       const AtomicString& pseudo_argument = g_null_atom);

  bool IsIndexedPseudoElement() const final { return true; }
  wtf_size_t Index() const { return index_; }

 private:
  wtf_size_t index_;
};

template <>
struct DowncastTraits<IndexedPseudoElement> {
  static bool AllowFrom(const Node& node) {
    return node.IsIndexedPseudoElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_INDEXED_PSEUDO_ELEMENT_H_
