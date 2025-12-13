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

PhysicalFragmentTraversalListener::NextStep HandleBoxFragment(
    const PhysicalBoxFragment& fragment,
    PhysicalOffset offset,
    bool is_first_for_node,
    PhysicalFragmentTraversalOptions options,
    PhysicalFragmentTraversalListener& listener) {
  PhysicalFragmentTraversalListener::NextStep next_step =
      listener.HandleEntry(fragment, offset, is_first_for_node);
  if (next_step != PhysicalFragmentTraversalListener::kSkipChildren) {
    ForAllBoxFragmentDescendants(fragment, options, listener);
    listener.HandleExit(fragment, offset);
  }
  return next_step;
}

}  // anonymous namespace

void ForAllBoxFragmentDescendants(const PhysicalBoxFragment& fragment,
                                  PhysicalFragmentTraversalOptions options,
                                  PhysicalFragmentTraversalListener& listener) {
  for (const PhysicalFragmentLink& child : fragment.Children()) {
    if (const auto* child_box_fragment =
            DynamicTo<PhysicalBoxFragment>(child.get())) {
      HandleBoxFragment(*child_box_fragment, child.offset,
                        child_box_fragment->IsFirstForNode(), options,
                        listener);
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
      const FragmentItem* item = cursor.Current().Item();
      PhysicalFragmentTraversalListener::NextStep next_step = HandleBoxFragment(
          *child_box_fragment, item->OffsetInContainerFragment(),
          item->IsFirstForNode(), options, listener);

      // Normal LayoutBox-derived fragments process the subtree on their own.
      // This is not the case for non-atomic inlines (LayoutInline), though,
      // whose actual descendants are to be found in the flat fragment items
      // list that we're walking through here. If kContinue, make sure to visit
      // its children.
      if (next_step == PhysicalFragmentTraversalListener::kContinue &&
          child_box_fragment->IsInlineBox()) {
        cursor.MoveToNext();
      } else {
        cursor.MoveToNextSkippingChildren();
      }
      continue;
    }

    if (options & kFragmentTraversalOptionCulledInlines) {
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
            // Found a culled inline that we haven't seen before in this
            // fragment.
            InlineCursor culled_cursor(*container);
            culled_cursor.MoveToIncludingCulledInline(*layout_inline);
            bool is_first_for_node =
                BoxFragmentIndex(culled_cursor.ContainerFragment()) ==
                BoxFragmentIndex(fragment);
            listener.HandleCulledInline(*layout_inline, is_first_for_node);
          }
        }
      }
    }
    cursor.MoveToNextSkippingChildren();
  }
}

}  // namespace blink
