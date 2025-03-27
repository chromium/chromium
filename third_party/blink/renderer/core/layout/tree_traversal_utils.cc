// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/tree_traversal_utils.h"

#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_items.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"

namespace blink {

namespace {

void HandleBoxFragment(const PhysicalBoxFragment& fragment,
                       bool is_first_for_node,
                       BoxFragmentDescendantsCallback callback) {
  FragmentTraversalNextStep next_step =
      callback(&fragment, nullptr, is_first_for_node);
  if (next_step != FragmentTraversalNextStep::kSkipChildren) {
    ForAllBoxFragmentDescendants(fragment, callback);
  }
}

}  // anonymous namespace

void ForAllBoxFragmentDescendants(const PhysicalBoxFragment& fragment,
                                  BoxFragmentDescendantsCallback callback) {
  for (const PhysicalFragmentLink& child : fragment.Children()) {
    if (const auto* child_box_fragment =
            DynamicTo<PhysicalBoxFragment>(child.get())) {
      HandleBoxFragment(*child_box_fragment,
                        child_box_fragment->IsFirstForNode(), callback);
    }
  }

  if (!fragment.HasItems()) {
    return;
  }
  HeapHashSet<Member<const LayoutInline>> culled_inlines;
  const auto* container = To<LayoutBlockFlow>(fragment.GetLayoutObject());
  DCHECK(container);
  for (InlineCursor cursor(fragment, *fragment.Items()); cursor;) {
    if (cursor.Current().Item()->Type() == FragmentItem::kLine) {
      cursor.MoveToNext();
      continue;
    }
    if (const PhysicalBoxFragment* child_box_fragment =
            cursor.Current().BoxFragment()) {
      HandleBoxFragment(*child_box_fragment,
                        cursor.Current().Item()->IsFirstForNode(), callback);
    }
    if (const LayoutObject* descendant = cursor.Current().GetLayoutObject()) {
      // Look for culled inline ancestors. Due to crbug.com/406288653 we
      // unfortunately need to do this.
      DCHECK(descendant != container);
      for (const LayoutObject* walker = descendant->Parent();
           walker != container; walker = walker->Parent()) {
        const auto* layout_inline = DynamicTo<LayoutInline>(walker);
        if (!layout_inline || layout_inline->HasInlineFragments()) {
          continue;
        }
        if (culled_inlines.insert(layout_inline).is_new_entry) {
          // Found a culled inline that we haven't seen before in this fragment.
          InlineCursor culled_cursor(*container);
          culled_cursor.MoveToIncludingCulledInline(*layout_inline);
          bool is_first_for_node =
              BoxFragmentIndex(culled_cursor.ContainerFragment()) ==
              BoxFragmentIndex(fragment);
          // Ignore the return value from the callback. We found this culled
          // inline by walking upwards in the tree (while traversing the
          // subtree).
          callback(nullptr, layout_inline, is_first_for_node);
        }
      }
    }
    cursor.MoveToNextSkippingChildren();
  }
}

}  // namespace blink
