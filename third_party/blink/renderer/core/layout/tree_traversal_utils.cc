// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/tree_traversal_utils.h"

#include "third_party/blink/renderer/core/layout/inline/fragment_items.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"

namespace blink {

namespace {

void HandleBoxFragment(
    const PhysicalBoxFragment& fragment,
    base::FunctionRef<FragmentTraversalNextStep(const PhysicalBoxFragment&)>
        callback) {
  FragmentTraversalNextStep next_step = callback(fragment);
  if (next_step != FragmentTraversalNextStep::kSkipChildren) {
    ForAllBoxFragmentDescendants(fragment, callback);
  }
}

}  // anonymous namespace

void ForAllBoxFragmentDescendants(
    const PhysicalBoxFragment& fragment,
    base::FunctionRef<FragmentTraversalNextStep(const PhysicalBoxFragment&)>
        callback) {
  for (const PhysicalFragmentLink& child : fragment.Children()) {
    if (const auto* child_box_fragment =
            DynamicTo<PhysicalBoxFragment>(child.get())) {
      HandleBoxFragment(*child_box_fragment, callback);
    }
  }

  if (fragment.HasItems()) {
    for (const FragmentItem& item : fragment.Items()->Items()) {
      if (const PhysicalBoxFragment* child_box_fragment = item.BoxFragment()) {
        HandleBoxFragment(*child_box_fragment, callback);
      }
    }
  }
}

}  // namespace blink
