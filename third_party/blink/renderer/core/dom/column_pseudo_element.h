// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_COLUMN_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_COLUMN_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"

namespace blink {

// A ::column pseudo element. When needed, each column in a multicol container
// will create one of these, during layout.
class ColumnPseudoElement : public PseudoElement {
 public:
  ColumnPseudoElement(Element* originating_element, PhysicalRect column_rect)
      : PseudoElement(originating_element, kPseudoIdColumn),
        column_rect_(column_rect) {}

  bool IsColumnPseudoElement() const final { return true; }

  // The column rectangle, relatively to the multicol container.
  const PhysicalRect& ColumnRect() const { return column_rect_; }

 private:
  PhysicalRect column_rect_;
};

template <>
struct DowncastTraits<ColumnPseudoElement> {
  static bool AllowFrom(const Node& node) {
    return node.IsColumnPseudoElement();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_COLUMN_PSEUDO_ELEMENT_H_
