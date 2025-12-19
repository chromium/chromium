// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_COLUMN_PSEUDO_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_COLUMN_PSEUDO_ELEMENT_H_

#include "third_party/blink/renderer/core/dom/indexed_pseudo_element.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"

namespace blink {

// A ::column pseudo-element. When needed, each column in a multicol container
// will create one of these, during layout.
class CORE_EXPORT ColumnPseudoElement : public IndexedPseudoElement {
 public:
  ColumnPseudoElement(Element* originating_element, wtf_size_t index);

  bool IsColumnPseudoElement() const final { return true; }

  // The column rectangle, relatively to the multicol container.
  const PhysicalRect& ColumnRect() const { return column_rect_; }
  void SetColumnRect(PhysicalRect column_rect) { column_rect_ = column_rect; }

  // Return the first element that starts in the column, in DOM order.
  Element* FirstChildInDOMOrder() const;

  // Sets IsInsideInactiveColumnTab on all LayoutObjects whose fragments
  // are inside this column. This is used to efficiently mark content that
  // should be hidden for accessibility when this column is not the active
  // tab in a scroll-marker-group with tabs mode.
  void SetIsInsideInactiveColumnTabForDescendants(bool is_inactive) const;

  void AttachLayoutTree(AttachContext&) final;
  void DetachLayoutTree(bool performing_reattach) final;

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
