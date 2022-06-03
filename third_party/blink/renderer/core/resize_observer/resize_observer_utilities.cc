// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_box_options.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/geometry/float_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

namespace {
// Compute the logical paint offset for a layout object.
LayoutSize ComputePaintOffset(LayoutObject* layout_object,
                              const ComputedStyle& style) {
  LayoutSize paint_offset =
      layout_object->FirstFragment().PaintOffset().ToLayoutSize();
  LayoutUnit paint_offset_inline = style.IsHorizontalWritingMode()
                                       ? paint_offset.Width()
                                       : paint_offset.Height();
  LayoutUnit paint_offset_block = style.IsHorizontalWritingMode()
                                      ? paint_offset.Height()
                                      : paint_offset.Width();
  return LayoutSize(paint_offset_inline, paint_offset_block);
}
}  // namespace

FloatSize ResizeObserverUtilities::ComputeZoomAdjustedBox(
    ResizeObserverBoxOptions box_option,
    LayoutObject* layout_object,
    const ComputedStyle& style) {
  auto* layout_box = To<LayoutBox>(layout_object);
  switch (box_option) {
    case ResizeObserverBoxOptions::ContentBox:
      return FloatSize(AdjustForAbsoluteZoom::AdjustLayoutUnit(
                           layout_box->ContentLogicalWidth(), style),
                       AdjustForAbsoluteZoom::AdjustLayoutUnit(
                           layout_box->ContentLogicalHeight(), style));

    case ResizeObserverBoxOptions::BorderBox:
      return FloatSize(AdjustForAbsoluteZoom::AdjustLayoutUnit(
                           layout_box->LogicalWidth(), style),
                       AdjustForAbsoluteZoom::AdjustLayoutUnit(
                           layout_box->LogicalHeight(), style));
    case ResizeObserverBoxOptions::DevicePixelContentBox: {
      LayoutSize box_size = LayoutSize(layout_box->ContentLogicalWidth(),
                                       layout_box->ContentLogicalHeight());

      return ComputeSnappedDevicePixelContentBox(box_size, layout_object,
                                                 style);
    }
    default:
      NOTREACHED();
  }
}

FloatSize ResizeObserverUtilities::ComputeSnappedDevicePixelContentBox(
    LayoutSize box_size,
    LayoutObject* layout_object,
    const ComputedStyle& style) {
  // Get Device Scale Factor for cases where use-zoom-for-dsf is
  // disabled. This is 1 if use-zoom-for-dsf is enabled.
  float device_scale_factor =
      layout_object->GetFrame()->GetPage()->DeviceScaleFactorDeprecated();
  LayoutSize paint_offset = ComputePaintOffset(layout_object, style);
  return FloatSize(
      SnapSizeToPixel(LayoutUnit(box_size.Width()), paint_offset.Width()) *
          device_scale_factor,
      SnapSizeToPixel(LayoutUnit(box_size.Height()), paint_offset.Height()) *
          device_scale_factor);
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

  return DOMRectReadOnly::FromFloatRect(FloatRect(
      FloatPoint(content_rect.Location()), FloatSize(content_rect.Size())));
}

}  // namespace blink
