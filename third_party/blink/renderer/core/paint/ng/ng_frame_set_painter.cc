// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_frame_set_painter.h"

#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"

namespace blink {

void NGFrameSetPainter::PaintObject(const PaintInfo& paint_info,
                                    const PhysicalOffset& paint_offset) {
  if (paint_info.phase != PaintPhase::kForeground)
    return;

  if (box_fragment_.Children().size() == 0)
    return;

  if (box_fragment_.Style().Visibility() != EVisibility::kVisible)
    return;

  PaintInfo paint_info_for_descendants = paint_info.ForDescendants();
  paint_info_for_descendants.SetIsInFragmentTraversal();
  PaintChildren(paint_info_for_descendants);

  PaintBorders(paint_info, paint_offset);
}

void NGFrameSetPainter::PaintChildren(const PaintInfo& paint_info) {
  if (paint_info.DescendantPaintingBlocked())
    return;

  for (const NGLink& link : box_fragment_.Children()) {
    const NGPhysicalFragment& child_fragment = *link;
    if (child_fragment.HasSelfPaintingLayer())
      continue;
    if (To<NGPhysicalBoxFragment>(child_fragment).CanTraverse()) {
      NGBoxFragmentPainter(To<NGPhysicalBoxFragment>(child_fragment))
          .Paint(paint_info);
    } else {
      child_fragment.GetLayoutObject()->Paint(paint_info);
    }
  }
}

void NGFrameSetPainter::PaintBorders(const PaintInfo& paint_info,
                                     const PhysicalOffset& paint_offset) {}

}  // namespace blink
