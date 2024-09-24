// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/fragment_painter.h"

#include "third_party/blink/renderer/core/layout/outline_utils.h"
#include "third_party/blink/renderer/core/paint/outline_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"

namespace blink {

void FragmentPainter::PaintOutline(const PaintInfo& paint_info,
                                   const PhysicalOffset& paint_offset,
                                   const ComputedStyle& style_to_use) {
  const PhysicalBoxFragment& fragment = PhysicalFragment();
  DCHECK(HasPaintedOutline(style_to_use, fragment.GetNode()));
  VectorOutlineRectCollector collector;
  LayoutObject::OutlineInfo info;
  fragment.AddSelfOutlineRects(
      paint_offset, style_to_use.OutlineRectsShouldIncludeBlockInkOverflow(),
      collector, &info);

  VectorOf<PhysicalRect> outline_rects = collector.TakeRects();
  if (outline_rects.empty())
    return;

  OutlinePainter::PaintOutlineRects(paint_info, GetDisplayItemClient(),
                                    outline_rects, info, style_to_use);
}

void FragmentPainter::AddURLRectIfNeeded(const PaintInfo& paint_info,
                                         const PhysicalOffset& paint_offset) {
  DCHECK(paint_info.ShouldAddUrlMetadata());

  const PhysicalBoxFragment& fragment = PhysicalFragment();
  if (fragment.Style().UsedVisibility() != EVisibility::kVisible) {
    return;
  }

  Node* node = fragment.GetNode();
  if (!node || !node->IsLink())
    return;

  KURL url = To<Element>(node)->HrefURL();
  if (!url.IsValid())
    return;

  auto outline_rects = fragment.GetLayoutObject()->OutlineRects(
      nullptr, paint_offset, OutlineType::kIncludeBlockInkOverflow);
  gfx::Rect rect = ToPixelSnappedRect(UnionRect(outline_rects));
  if (rect.IsEmpty())
    return;

  const DisplayItemClient& display_item_client = GetDisplayItemClient();
  if (DrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, display_item_client,
          DisplayItem::kPrintedContentPDFURLRect))
    return;

  DrawingRecorder recorder(paint_info.context, display_item_client,
                           DisplayItem::kPrintedContentPDFURLRect);

  Document& document = fragment.GetLayoutObject()->GetDocument();
  if (url.HasFragmentIdentifier() &&
      EqualIgnoringFragmentIdentifier(url, document.BaseURL())) {
    String fragment_name = url.FragmentIdentifier().ToString();
    if (document.FindAnchor(fragment_name))
      paint_info.context.SetURLFragmentForRect(fragment_name, rect);
    return;
  }
  paint_info.context.SetURLForRect(url, rect);
}

}  // namespace blink
