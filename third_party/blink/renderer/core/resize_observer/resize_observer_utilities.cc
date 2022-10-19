// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer_utilities.h"

#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_box_options.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {
// Compute the logical paint offset for a layout object.
LayoutSize ComputePaintOffset(const LayoutObject& layout_object,
                              const ComputedStyle& style) {
  LayoutSize paint_offset =
      layout_object.FirstFragment().PaintOffset().ToLayoutSize();
  LayoutUnit paint_offset_inline = style.IsHorizontalWritingMode()
                                       ? paint_offset.Width()
                                       : paint_offset.Height();
  LayoutUnit paint_offset_block = style.IsHorizontalWritingMode()
                                      ? paint_offset.Height()
                                      : paint_offset.Width();
  return LayoutSize(paint_offset_inline, paint_offset_block);
}
}  // namespace

gfx::SizeF ResizeObserverUtilities::ComputeZoomAdjustedBox(
    ResizeObserverBoxOptions box_option,
    const LayoutBox& layout_box,
    const ComputedStyle& style) {
  switch (box_option) {
    case ResizeObserverBoxOptions::kContentBox:
      return gfx::SizeF(AdjustForAbsoluteZoom::AdjustLayoutUnit(
                            layout_box.ContentLogicalWidth(), style),
                        AdjustForAbsoluteZoom::AdjustLayoutUnit(
                            layout_box.ContentLogicalHeight(), style));

    case ResizeObserverBoxOptions::kBorderBox:
      return gfx::SizeF(AdjustForAbsoluteZoom::AdjustLayoutUnit(
                            layout_box.LogicalWidth(), style),
                        AdjustForAbsoluteZoom::AdjustLayoutUnit(
                            layout_box.LogicalHeight(), style));
    case ResizeObserverBoxOptions::kDevicePixelContentBox: {
      LayoutSize box_size = LayoutSize(layout_box.ContentLogicalWidth(),
                                       layout_box.ContentLogicalHeight());
      return ComputeSnappedDevicePixelContentBox(box_size, layout_box, style);
    }
    default:
      NOTREACHED();
  }
}

gfx::SizeF ResizeObserverUtilities::ComputeSnappedDevicePixelContentBox(
    LayoutSize box_size,
    const LayoutObject& layout_object,
    const ComputedStyle& style) {
  LayoutSize paint_offset = ComputePaintOffset(layout_object, style);
  return gfx::SizeF(SnapSizeToPixel(box_size.Width(), paint_offset.Width()),
                    SnapSizeToPixel(box_size.Height(), paint_offset.Height()));
}

DOMRectReadOnly* ResizeObserverUtilities::ZoomAdjustedLayoutRect(
    LayoutRect content_rect,
    const ComputedStyle& style) {
  content_rect.SetX(
      AdjustForAbsoluteZoom::AdjustLayoutUnit(content_rect.X(), style));
  content_rect.SetY(
      AdjustForAbsoluteZoom::AdjustLayoutUnit(content_rect.Y(), style));
  content_rect.SetWidth(
      AdjustForAbsoluteZoom::AdjustLayoutUnit(content_rect.Width(), style));
  content_rect.SetHeight(
      AdjustForAbsoluteZoom::AdjustLayoutUnit(content_rect.Height(), style));

  return DOMRectReadOnly::FromRectF(gfx::RectF(content_rect));
}

}  // namespace blink
