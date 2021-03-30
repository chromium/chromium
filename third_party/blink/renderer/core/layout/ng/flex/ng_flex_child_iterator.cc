// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/flex/ng_flex_child_iterator.h"

namespace blink {

NGFlexChildIterator::NGFlexChildIterator(const NGBlockNode node) {
  bool is_deprecated_webkit_box = node.Style().IsDeprecatedWebkitBox();
  int initial_order = is_deprecated_webkit_box
                          ? ComputedStyleInitialValues::InitialBoxOrdinalGroup()
                          : ComputedStyleInitialValues::InitialOrder();
  bool needs_sort = false;

  // Collect all our children, and order them by either their
  // -webkit-box-ordinal-group/order property.
  for (NGLayoutInputNode child = node.FirstChild(); child;
       child = child.NextSibling()) {
    int order = is_deprecated_webkit_box ? child.Style().BoxOrdinalGroup()
                                         : child.Style().Order();
    needs_sort |= order != initial_order;
    children_.emplace_back(To<NGBlockNode>(child), order);
  }

  // We only need to sort this vector if we encountered a non-initial
  // -webkit-box-ordinal-group/order property.
  if (needs_sort) {
    std::stable_sort(children_.begin(), children_.end(),
                     [](const ChildWithOrder& c1, const ChildWithOrder& c2) {
                       return c1.order < c2.order;
                     });
  }

  iterator_ = children_.begin();
}

}  // namespace blink
