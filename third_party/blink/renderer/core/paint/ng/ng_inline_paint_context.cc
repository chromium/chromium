// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

namespace blink {

NGInlinePaintContext::ScopedInlineItem::ScopedInlineItem(
    const NGFragmentItem& item,
    NGInlinePaintContext* inline_context) {
  if (!RuntimeEnabledFeatures::TextDecoratingBoxEnabled())
    return;
  DCHECK(inline_context);
  if (!item.IsTextDecorationBoundary()) {
    DCHECK_EQ(inline_context->decorating_boxes_.size(),
              item.Style().AppliedTextDecorations().size());
    return;
  }

  inline_context_ = inline_context;
  is_pushed_ =
      inline_context->PushDecoratingBox(item, &saved_decorating_boxes_);
  DCHECK_EQ(inline_context->decorating_boxes_.size(),
            item.Style().AppliedTextDecorations().size());
}

bool NGInlinePaintContext::PushDecoratingBox(
    const NGFragmentItem& item,
    DecoratingBoxList* saved_decorating_boxes) {
  DCHECK(RuntimeEnabledFeatures::TextDecoratingBoxEnabled());
  DCHECK(item.IsTextDecorationBoundary());
  DCHECK(saved_decorating_boxes);
  DCHECK(saved_decorating_boxes->IsEmpty());

  // Check if this boundary is to stop the propagation. If so, save the current
  // |decorating_boxes_| and clear it.
  const Vector<AppliedTextDecoration>& applied_text_decorations =
      item.Style().AppliedTextDecorations();
  if (!decorating_boxes_.IsEmpty() && applied_text_decorations.size() <= 1)
    decorating_boxes_.swap(*saved_decorating_boxes);

  const bool should_push = !applied_text_decorations.IsEmpty();
  if (should_push)
    PushDecoratingBox(item);

  DCHECK_EQ(decorating_boxes_.size(),
            item.Style().AppliedTextDecorations().size());
  return should_push;
}

NGInlinePaintContext::ScopedInlineBoxAncestors::ScopedInlineBoxAncestors(
    const NGInlineCursor& inline_box,
    NGInlinePaintContext* inline_context) {
  if (!RuntimeEnabledFeatures::TextDecoratingBoxEnabled())
    return;
  DCHECK(inline_context);
  inline_context_ = inline_context;
  inline_context->PushDecoratingBoxAncestors(inline_box);
}

void NGInlinePaintContext::PushDecoratingBoxAncestors(
    const NGInlineCursor& inline_box) {
  DCHECK(RuntimeEnabledFeatures::TextDecoratingBoxEnabled());
  DCHECK(inline_box.Current());
  DCHECK(inline_box.Current().IsInlineBox());
  DCHECK(decorating_boxes_.IsEmpty());

  DecoratingBoxList ancestor_boxes;
  Vector<const NGFragmentItem*, 1> boundary_items;
  for (NGInlineCursor cursor = inline_box;;) {
    cursor.MoveToParent();
    const NGInlineCursorPosition& current = cursor.Current();
    DCHECK(current);

    if (current.IsLineBox()) {
      SetLineBox(*current);
      for (const NGFragmentItem* item : base::Reversed(boundary_items)) {
        DecoratingBoxList saved_decorating_boxes;
        PushDecoratingBox(*item, &saved_decorating_boxes);
      }
      return;
    }

    if (current->IsTextDecorationBoundary())
      boundary_items.push_back(current.Item());
  }
}

NGInlinePaintContext::ScopedLineBox::ScopedLineBox(
    const NGFragmentItem& line_item,
    NGInlinePaintContext* inline_context) {
  if (!RuntimeEnabledFeatures::TextDecoratingBoxEnabled())
    return;
  DCHECK(inline_context);
  inline_context_ = inline_context;
  inline_context->SetLineBox(line_item);
}

void NGInlinePaintContext::SetLineBox(const NGFragmentItem& line_item) {
  DCHECK(RuntimeEnabledFeatures::TextDecoratingBoxEnabled());
  DCHECK_EQ(line_item.Type(), NGFragmentItem::kLine);
  line_item_ = &line_item;
  decorating_boxes_.Shrink(0);

  const ComputedStyle& style = line_item.Style();
  const Vector<AppliedTextDecoration>& applied_text_decorations =
      style.AppliedTextDecorations();
  if (applied_text_decorations.IsEmpty())
    return;

  // The decorating box of a block container is an anonymous inline box that
  // wraps all children of the block container.
  // https://drafts.csswg.org/css-text-decor-3/#decorating-box
  //
  // Compute the offset of the non-existent anonymous inline box.
  PhysicalOffset offset = line_item.OffsetInContainerFragment();
  const NGPhysicalLineBoxFragment* fragment = line_item.LineBoxFragment();
  DCHECK(fragment);
  offset.top += fragment->Metrics().ascent;
  offset.top -= style.GetFont().PrimaryFont()->GetFontMetrics().FixedAscent();

  for (wtf_size_t i = 0; i < applied_text_decorations.size(); ++i)
    decorating_boxes_.emplace_back(offset, style);
}

}  // namespace blink
