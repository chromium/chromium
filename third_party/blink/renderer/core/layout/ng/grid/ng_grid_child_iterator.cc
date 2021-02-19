// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/grid/ng_grid_child_iterator.h"

namespace blink {

NGGridChildIterator::NGGridChildIterator(const NGBlockNode node) {
  Setup(node);
}

void NGGridChildIterator::Setup(const NGBlockNode node) {
  const int initial_order = ComputedStyleInitialValues::InitialOrder();
  bool needs_sort = false;

  // Collect all our children, and order them by either their order property.
  for (NGLayoutInputNode child = node.FirstChild(); child;
       child = child.NextSibling()) {
    int order = child.Style().Order();
    needs_sort |= order != initial_order;
    children_.emplace_back(To<NGBlockNode>(child), order);
  }

  // We only need to sort this vector if we encountered a non-initial order
  // property.
  if (needs_sort) {
    std::stable_sort(children_.begin(), children_.end(),
                     [](const ChildWithOrder& c1, const ChildWithOrder& c2) {
                       return c1.order < c2.order;
                     });
  }

  iterator_ = children_.begin();
}

}  // namespace blink
