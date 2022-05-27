// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"

namespace blink {

void NGInlinePaintContext::SetLineBox(const NGFragmentItem& line_item) {
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
