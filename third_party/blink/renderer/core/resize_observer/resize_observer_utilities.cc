// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer_utilities.h"

#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_offset.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_box_options.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

// Compute the logical paint offset for a layout object.
LogicalOffset ComputePaintOffset(const LayoutObject& layout_object,
                                 const ComputedStyle& style) {
  PhysicalOffset paint_offset = layout_object.FirstFragment().PaintOffset();
  LayoutUnit paint_offset_inline =
      style.IsHorizontalWritingMode() ? paint_offset.left : paint_offset.top;
  LayoutUnit paint_offset_block =
      style.IsHorizontalWritingMode() ? paint_offset.top : paint_offset.left;
  return LogicalOffset(paint_offset_inline, paint_offset_block);
}

// Convert a layout value to CSS pixels. The result is kept as a float, since
// storing it in a LayoutUnit would truncate it to 1/64px whenever the zoom
// (e.g. a fractional device scale factor) does not divide the value evenly.
float AdjustForZoom(LayoutUnit value, const ComputedStyle& style) {
  if (!RuntimeEnabledFeatures::
          ResizeObserverUnroundedZoomAdjustedSizesEnabled()) {
    return AdjustForAbsoluteZoom::AdjustLayoutUnit(value, style);
  }
  return AdjustForAbsoluteZoom::AdjustFloat(value, style);
}

}  // namespace

gfx::SizeF ResizeObserverUtilities::ComputeZoomAdjustedBox(
    ResizeObserverBoxOptions box_option,
    const LayoutBox& layout_box,
    const ComputedStyle& style) {
  switch (box_option) {
    case ResizeObserverBoxOptions::kContentBox: {
      const LogicalSize size = ToLogicalSize(
          layout_box.PhysicalContentBoxRect().size, style.GetWritingMode());
      return gfx::SizeF(AdjustForZoom(size.inline_size, style),
                        AdjustForZoom(size.block_size, style));
    }
    case ResizeObserverBoxOptions::kBorderBox:
      return gfx::SizeF(AdjustForZoom(layout_box.LogicalWidth(), style),
                        AdjustForZoom(layout_box.LogicalHeight(), style));
    case ResizeObserverBoxOptions::kDevicePixelContentBox: {
      const LogicalSize size = ToLogicalSize(
          layout_box.PhysicalContentBoxRect().size, style.GetWritingMode());
      return gfx::SizeF(
          ComputeSnappedDevicePixelContentBox(size, layout_box, style));
    }
    default:
      NOTREACHED();
  }
}

gfx::Size ResizeObserverUtilities::ComputeSnappedDevicePixelContentBox(
    LogicalSize box_size,
    const LayoutObject& layout_object,
    const ComputedStyle& style) {
  LogicalOffset paint_offset = ComputePaintOffset(layout_object, style);
  return gfx::Size(
      SnapSizeToPixel(box_size.inline_size, paint_offset.inline_offset),
      SnapSizeToPixel(box_size.block_size, paint_offset.block_offset));
}

// static
gfx::Size ResizeObserverUtilities::ComputeSnappedDevicePixelContentBox(
    const gfx::SizeF& box_size,
    const LayoutObject& layout_object,
    const ComputedStyle& style) {
  return ComputeSnappedDevicePixelContentBox(
      LogicalSize(LayoutUnit(box_size.width()), LayoutUnit(box_size.height())),
      layout_object, style);
}

DOMRectReadOnly* ResizeObserverUtilities::ZoomAdjustedPhysicalRect(
    PhysicalRect content_rect,
    const ComputedStyle& style) {
  return DOMRectReadOnly::FromRectF(
      gfx::RectF(AdjustForZoom(content_rect.X(), style),
                 AdjustForZoom(content_rect.Y(), style),
                 AdjustForZoom(content_rect.Width(), style),
                 AdjustForZoom(content_rect.Height(), style)));
}

}  // namespace blink
