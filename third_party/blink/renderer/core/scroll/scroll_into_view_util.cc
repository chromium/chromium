// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"

#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink::scroll_into_view_util {

gfx::RectF FocusedEditableBoundsFromParams(
    const gfx::RectF& caret_rect,
    const mojom::blink::ScrollIntoViewParamsPtr& params) {
  DCHECK(params->for_focused_editable);
  DCHECK(!params->for_focused_editable->size.IsEmpty());

  gfx::PointF editable_location =
      caret_rect.origin() + params->for_focused_editable->relative_location;
  return gfx::RectF(editable_location, params->for_focused_editable->size);
}

void ConvertParamsToParentFrame(mojom::blink::ScrollIntoViewParamsPtr& params,
                                const gfx::RectF& caret_rect_in_src,
                                LayoutObject& src_frame,
                                LayoutView& dest_frame) {
  if (!params->for_focused_editable)
    return;

  // The source frame will be a LayoutView if the conversion is local or a
  // LayoutEmbeddedContent if we're crossing a remote boundary.
  DCHECK(src_frame.IsLayoutView() || src_frame.IsLayoutEmbeddedContent());

  gfx::RectF editable_bounds_in_src =
      FocusedEditableBoundsFromParams(caret_rect_in_src, params);

  PhysicalRect editable_bounds_in_dest = src_frame.LocalToAncestorRect(
      PhysicalRect::EnclosingRect(editable_bounds_in_src), &dest_frame,
      kTraverseDocumentBoundaries);

  PhysicalRect caret_rect_in_dest = src_frame.LocalToAncestorRect(
      PhysicalRect::EnclosingRect(caret_rect_in_src), &dest_frame,
      kTraverseDocumentBoundaries);

  params->for_focused_editable->relative_location = gfx::Vector2dF(
      editable_bounds_in_dest.offset - caret_rect_in_dest.offset);
  params->for_focused_editable->size = gfx::SizeF(editable_bounds_in_dest.size);
}

}  // namespace blink::scroll_into_view_util
