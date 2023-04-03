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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class LayoutBox;
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
// To use LayoutState, the layout() functions have to allocate a new LayoutState
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

  bool ContainingBlockLogicalWidthChanged() const {
    return containing_block_logical_width_changed_;
  }

  LayoutState* Next() const { return next_; }

  LayoutObject& GetLayoutObject() const { return *layout_object_; }

  void Trace(Visitor*) const;

 private:
  bool containing_block_logical_width_changed_ : 1;

  LayoutState* next_;

  const Member<LayoutObject> layout_object_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LAYOUT_STATE_H_
