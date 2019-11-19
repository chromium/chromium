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

#include "third_party/blink/renderer/core/layout/layout_state.h"

#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"

namespace blink {

LayoutState::LayoutState(LayoutView& view)
    : is_paginated_(view.PageLogicalHeight()),
      containing_block_logical_width_changed_(false),
      pagination_state_changed_(false),
      flow_thread_(nullptr),
      next_(nullptr),
      layout_object_(view) {
  DCHECK(!view.GetLayoutState());
  view.PushLayoutState(*this);
}

LayoutState::LayoutState(LayoutBox& layout_object,
                         bool containing_block_logical_width_changed)
    : containing_block_logical_width_changed_(
          containing_block_logical_width_changed),
      next_(layout_object.View()->GetLayoutState()),
      layout_object_(layout_object) {
  if (layout_object.IsLayoutFlowThread())
    flow_thread_ = ToLayoutFlowThread(&layout_object);
  else
    flow_thread_ = next_->FlowThread();
  pagination_state_changed_ = next_->pagination_state_changed_;
  height_offset_for_table_headers_ = next_->HeightOffsetForTableHeaders();
  height_offset_for_table_footers_ = next_->HeightOffsetForTableFooters();
  layout_object.View()->PushLayoutState(*this);

  if (layout_object.IsLayoutFlowThread()) {
    // Entering a new pagination context.
    pagination_offset_ = LayoutSize();
    is_paginated_ = true;
    return;
  }

  // Disable pagination for objects we don't support. For now this includes
  // overflow:scroll/auto, inline blocks and writing mode roots. Additionally,
  // pagination inside SVG is not allowed.
  if (layout_object.GetPaginationBreakability() == LayoutBox::kForbidBreaks ||
      layout_object_.IsSVGChild()) {
    flow_thread_ = nullptr;
    is_paginated_ = false;
    return;
  }

  is_paginated_ = next_->is_paginated_;
  if (!is_paginated_)
    return;

  // Now adjust the pagination offset, so that we can easily figure out how far
  // away we are from the start of the pagination context.
  pagination_offset_ =
      next_->pagination_offset_ + layout_object.LocationOffset();
  if (!layout_object.IsOutOfFlowPositioned())
    return;
  if (LayoutObject* container = layout_object.Container()) {
    if (container->StyleRef().HasInFlowPosition() &&
        container->IsLayoutInline()) {
      pagination_offset_ += ToLayoutInline(container)
                                ->OffsetForInFlowPositionedInline(layout_object)
                                .ToLayoutSize();
    }
  }

  // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if
  // present.
}

LayoutState::LayoutState(LayoutObject& root)
    : is_paginated_(false),
      containing_block_logical_width_changed_(false),
      pagination_state_changed_(false),
      flow_thread_(nullptr),
      next_(root.View()->GetLayoutState()),
      layout_object_(root) {
  DCHECK(!next_);
  DCHECK(!root.IsLayoutView());
  root.View()->PushLayoutState(*this);
}

LayoutState::~LayoutState() {
  if (layout_object_.View()->GetLayoutState()) {
    DCHECK_EQ(layout_object_.View()->GetLayoutState(), this);
    layout_object_.View()->PopLayoutState();
  }
}

}  // namespace blink
