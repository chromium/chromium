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
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"

namespace blink {

static const LayoutBlock* EnclosingScrollableAncestor(
    const LayoutObject* layout_object) {
  DCHECK(!layout_object->IsLayoutView());

  // Trace up the containingBlocks until we reach either the layoutObject view
  // or a scrollable object.
  const LayoutBlock* container = layout_object->ContainingBlock();
  while (!container->HasOverflowClip() && !container->IsLayoutView())
    container = container->ContainingBlock();
  return container;
}

static FloatRect ToNormalizedRect(const FloatRect& absolute_rect,
                                  const LayoutObject* layout_object,
                                  const LayoutBlock* container) {
  DCHECK(layout_object);

  DCHECK(container || layout_object->IsLayoutView());
  if (!container)
    return FloatRect();

  // We want to normalize by the max layout overflow size instead of only the
  // visible bounding box.  Quads and their enclosing bounding boxes need to be
  // used in order to keep results transform-friendly.
  PhysicalRect overflow_rect = container->FlipForWritingMode(
      LayoutRect(LayoutPoint(), container->MaxLayoutOverflow()));

  // For overflow:scroll we need to get where the actual origin is independently
  // of the scroll.
  if (container->HasOverflowClip())
    overflow_rect.Move(-PhysicalOffset(container->ScrolledContentOffset()));

  FloatRect container_rect(container->LocalToAbsoluteRect(overflow_rect));

  if (container_rect.IsEmpty())
    return FloatRect();

  // Make the coordinates relative to the container enclosing bounding box.
  // Since we work with rects enclosing quad unions this is still
  // transform-friendly.
  FloatRect normalized_rect = absolute_rect;
  normalized_rect.MoveBy(-container_rect.Location());

  normalized_rect.Scale(1 / container_rect.Width(),
                        1 / container_rect.Height());
  return normalized_rect;
}

FloatRect FindInPageRectFromAbsoluteRect(
    const FloatRect& input_rect,
    const LayoutObject* base_layout_object) {
  if (!base_layout_object || input_rect.IsEmpty())
    return FloatRect();

  // Normalize the input rect to its container block.
  const LayoutBlock* base_container =
      EnclosingScrollableAncestor(base_layout_object);
  FloatRect normalized_rect =
      ToNormalizedRect(input_rect, base_layout_object, base_container);

  // Go up across frames.
  for (const LayoutBox* layout_object = base_container; layout_object;) {
    // Go up the layout tree until we reach the root of the current frame (the
    // LayoutView).
    while (!layout_object->IsLayoutView()) {
      const LayoutBlock* container = EnclosingScrollableAncestor(layout_object);

      // Compose the normalized rects.
      FloatRect normalized_box_rect =
          ToNormalizedRect(FloatRect(layout_object->AbsoluteBoundingBoxRect()),
                           layout_object, container);
      normalized_rect.Scale(normalized_box_rect.Width(),
                            normalized_box_rect.Height());
      normalized_rect.MoveBy(normalized_box_rect.Location());

      layout_object = container;
    }

    DCHECK(layout_object->IsLayoutView());

    // Jump to the layoutObject owning the frame, if any.
    layout_object = layout_object->GetFrame()
                        ? layout_object->GetFrame()->OwnerLayoutObject()
                        : nullptr;
  }

  return normalized_rect;
}

FloatRect FindInPageRectFromRange(const EphemeralRange& range) {
  if (range.IsNull() || !range.StartPosition().NodeAsRangeFirstNode())
    return FloatRect();

  const LayoutObject* const baseLayoutObject =
      range.StartPosition().NodeAsRangeFirstNode()->GetLayoutObject();
  if (!baseLayoutObject)
    return FloatRect();

  return FindInPageRectFromAbsoluteRect(ComputeTextFloatRect(range),
                                        baseLayoutObject);
}

}  // namespace blink
