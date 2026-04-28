// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LOGICAL_FRAGMENT_LINK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LOGICAL_FRAGMENT_LINK_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

class PhysicalFragment;
class WritingDirectionMode;

// Similar to |PhysicalFragmentLink| but with |LogicalOffset| instead of
// |PhysicalOffset|.
struct CORE_EXPORT LogicalFragmentLink {
  DISALLOW_NEW();

 public:
  LogicalFragmentLink() = default;
  LogicalFragmentLink(const PhysicalFragment& fragment, LogicalOffset offset)
      : fragment(&fragment), offset(offset) {}

  const LogicalOffset& Offset() const { return offset; }
  const PhysicalFragment* get() const { return fragment.Get(); }

  // Reverses this fragment's offset along the reversal axis in place, so that a
  // fragment laid out at the start of the container ends up positioned at the
  // equivalent distance from the end.
  //
  // - `writing_direction`: writing mode + direction of the container.
  // - `is_block_direction`: when true the block-axis offset is reversed;
  //   otherwise the inline-axis offset is reversed.
  // - `container_size_in_reversal_axis`: the container's available size
  //   (excluding border, scrollbar, and padding) along the reversal axis.
  // - `border_scrollbar_padding_start`: the size of border, scrollbar, and
  //   padding before the container at the start of the reversal axis.
  //
  // Note that baselines of the item don't need to be adjusted during reversal,
  // as baselines should remain the same for each reversal axis regardless of
  // the fill direction.
  void ReverseChildOffset(const WritingDirectionMode& writing_direction,
                          bool is_block_direction,
                          LayoutUnit container_size_in_reversal_axis,
                          LayoutUnit border_scrollbar_padding_start);

  explicit operator bool() const { return fragment != nullptr; }
  const PhysicalFragment& operator*() const { return *fragment; }
  const PhysicalFragment* operator->() const { return fragment.Get(); }

  void Trace(Visitor* visitor) const { visitor->Trace(fragment); }

  Member<const PhysicalFragment> fragment;
  LogicalOffset offset;
};

using LogicalFragmentLinkVector = HeapVector<LogicalFragmentLink, 4>;

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::LogicalFragmentLink)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LOGICAL_FRAGMENT_LINK_H_
