/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/finder/find_in_page_coordinates.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

static const LayoutBlock* EnclosingScrollableAncestor(
    const LayoutObject* layout_object) {
  DCHECK(!IsA<LayoutView>(layout_object));

  // Trace up the containingBlocks until we reach either the layoutObject view
  // or a scrollable object.
  const LayoutBlock* container = layout_object->ContainingBlock();
  while (!container->IsScrollContainer() && !IsA<LayoutView>(container))
    container = container->ContainingBlock();
  return container;
}

static gfx::RectF ToNormalizedRect(const gfx::RectF& absolute_rect,
                                   const LayoutObject* layout_object,
                                   const LayoutBlock* container) {
  DCHECK(layout_object);

  DCHECK(container || IsA<LayoutView>(layout_object));
  if (!container)
    return gfx::RectF();

  // We want to normalize by the max scrollable overflow size instead of only
  // the visible bounding box.  Quads and their enclosing bounding boxes need to
  // be used in order to keep results transform-friendly.
  auto converter = container->CreateWritingModeConverter();
  LogicalRect logical_overflow_rect =
      converter.ToLogical(container->ScrollableOverflowRect());
  logical_overflow_rect.ShiftBlockStartEdgeTo(LayoutUnit());
  logical_overflow_rect.ShiftInlineStartEdgeTo(LayoutUnit());
  PhysicalRect overflow_rect = converter.ToPhysical(logical_overflow_rect);

  // For scrolling we need to get where the actual origin is independently of
  // the scroll.
  if (container->IsScrollContainer())
    overflow_rect.Move(-container->ScrolledContentOffset());

  gfx::RectF container_rect(container->LocalToAbsoluteRect(overflow_rect));

  if (container_rect.IsEmpty())
    return gfx::RectF();

  // Make the coordinates relative to the container enclosing bounding box.
  // Since we work with rects enclosing quad unions this is still
  // transform-friendly.
  gfx::RectF normalized_rect = absolute_rect;
  normalized_rect.Offset(-container_rect.OffsetFromOrigin());

  normalized_rect.Scale(1 / container_rect.width(),
                        1 / container_rect.height());
  return normalized_rect;
}

gfx::RectF FindInPageRectFromAbsoluteRect(
    const gfx::RectF& input_rect,
    const LayoutObject* base_layout_object) {
  if (!base_layout_object || input_rect.IsEmpty())
    return gfx::RectF();

  // Normalize the input rect to its container block.
  const LayoutBlock* base_container =
      EnclosingScrollableAncestor(base_layout_object);
  gfx::RectF normalized_rect =
      ToNormalizedRect(input_rect, base_layout_object, base_container);

  // Go up across frames.
  for (const LayoutBox* layout_object = base_container; layout_object;) {
    // Go up the layout tree until we reach the root of the current frame (the
    // LayoutView).
    while (!IsA<LayoutView>(layout_object)) {
      const LayoutBlock* container = EnclosingScrollableAncestor(layout_object);

      // Compose the normalized rects.
      gfx::RectF normalized_box_rect =
          ToNormalizedRect(gfx::RectF(layout_object->AbsoluteBoundingBoxRect()),
                           layout_object, container);
      normalized_rect.Scale(normalized_box_rect.width(),
                            normalized_box_rect.height());
      normalized_rect.Offset(normalized_box_rect.OffsetFromOrigin());

      layout_object = container;
    }

    DCHECK(IsA<LayoutView>(layout_object));

    // Jump to the layoutObject owning the frame, if any.
    layout_object = layout_object->GetFrame()
                        ? layout_object->GetFrame()->OwnerLayoutObject()
                        : nullptr;
  }

  return normalized_rect;
}

gfx::RectF FindInPageRectFromRange(const EphemeralRange& range) {
  if (range.IsNull() || !range.StartPosition().NodeAsRangeFirstNode())
    return gfx::RectF();

  const LayoutObject* const baseLayoutObject =
      range.StartPosition().NodeAsRangeFirstNode()->GetLayoutObject();
  if (!baseLayoutObject)
    return gfx::RectF();

  return FindInPageRectFromAbsoluteRect(ComputeTextRectF(range),
                                        baseLayoutObject);
}

}  // namespace blink
