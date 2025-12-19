// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/indexed_pseudo_element.h"

namespace blink {

IndexedPseudoElement::IndexedPseudoElement(Element* parent,
                                           PseudoId pseudo_id,
                                           wtf_size_t index,
                                           const AtomicString& pseudo_argument)
    : PseudoElement(parent, pseudo_id, pseudo_argument), index_(index) {}
}  // namespace blink
