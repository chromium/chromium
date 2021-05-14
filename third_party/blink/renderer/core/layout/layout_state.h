/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_STATE_H_

#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class LayoutBox;
class LayoutFlowThread;
class LayoutObject;
class LayoutView;

// LayoutState is an optimization used during layout.
//
// LayoutState's purpose is to cache information as we walk down the container
// block chain during layout. In particular, the absolute layout offset for the
// current LayoutObject is O(1) when using LayoutState, when it is
// O(depthOfTree) without it (thus potentially making layout O(N^2)).
// LayoutState incurs some memory overhead and is pretty intrusive (see next
// paragraphs about those downsides).
//
// To use LayoutState, the layout() functions have to allocate a new LayoutSTate
// object on the stack whenever the LayoutObject creates a new coordinate system
// (which is pretty much all objects but LayoutTableRow).
//
// LayoutStates are linked together with a single linked list, acting in
// practice like a stack that we push / pop. LayoutView holds the top-most
// pointer to this stack.
//
// See the layout() functions on how to set it up during layout.
// See e.g LayoutBox::offsetFromLogicalTopOfFirstPage on how to use LayoutState
// for computations.
class LayoutState {
  // LayoutState is always allocated on the stack.
  // The reason is that it is scoped to layout, thus we can avoid expensive
  // mallocs.
  DISALLOW_NEW();

 public:
  // Constructor for root LayoutState created by LayoutView
  explicit LayoutState(LayoutView&);
  // Constructor for sub-tree layout and orthogonal writing-mode roots
  explicit LayoutState(LayoutObject& root);

  LayoutState(LayoutBox&, bool containing_block_logical_width_changed = false);
  LayoutState(const LayoutState&) = delete;
  LayoutState& operator=(const LayoutState&) = delete;

  ~LayoutState();

  bool IsPaginated() const { return is_paginated_; }

  LayoutUnit HeightOffsetForTableHeaders() const {
    return height_offset_for_table_headers_;
  }
  void SetHeightOffsetForTableHeaders(LayoutUnit offset) {
    height_offset_for_table_headers_ = offset;
  }

  LayoutUnit HeightOffsetForTableFooters() const {
    return height_offset_for_table_footers_;
  }
  void SetHeightOffsetForTableFooters(LayoutUnit offset) {
    height_offset_for_table_footers_ = offset;
  }

  // The input page name is the name specified by the element itself, if any. If
  // the element doesn't specify one, but an ancestor does, return that.
  // Otherwise it's an empty string. This is the page name that will be used on
  // all descendants if none of them override it.
  const AtomicString& InputPageName() const { return input_page_name_; }

  const LayoutSize& PaginationOffset() const { return pagination_offset_; }
  bool ContainingBlockLogicalWidthChanged() const {
    return containing_block_logical_width_changed_;
  }

  bool PaginationStateChanged() const { return pagination_state_changed_; }
  void SetPaginationStateChanged() { pagination_state_changed_ = true; }

  LayoutState* Next() const { return next_; }

  LayoutFlowThread* FlowThread() const { return flow_thread_; }

  LayoutObject& GetLayoutObject() const { return *layout_object_; }

 private:
  // Do not add anything apart from bitfields until after m_flowThread. See
  // https://bugs.webkit.org/show_bug.cgi?id=100173
  bool is_paginated_ : 1;

  bool containing_block_logical_width_changed_ : 1;
  bool pagination_state_changed_ : 1;

  LayoutFlowThread* flow_thread_;

  LayoutState* next_;

  // x/y offset from the logical top / start of the first page. Does not include
  // relative positioning or scroll offsets.
  LayoutSize pagination_offset_;

  // The height we need to make available for repeating table headers in
  // paginated layout.
  LayoutUnit height_offset_for_table_headers_;

  // The height we need to make available for repeating table footers in
  // paginated layout.
  LayoutUnit height_offset_for_table_footers_;

  AtomicString input_page_name_;

  LayoutObject* const layout_object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_STATE_H_
