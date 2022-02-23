// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/layout/adjust_for_absolute_zoom.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_box_options.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_size.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_utilities.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

ResizeObserverEntry::ResizeObserverEntry(Element* target) : target_(target) {
  if (LayoutObject* layout_object = target->GetLayoutObject()) {
    const ComputedStyle& style = layout_object->StyleRef();

    if (auto* svg_graphics_element = DynamicTo<SVGGraphicsElement>(target)) {
      LayoutSize bounding_box_size =
          LayoutSize(svg_graphics_element->GetBBox().size());
      content_rect_ = DOMRectReadOnly::FromRectF(
          gfx::RectF(gfx::PointF(), gfx::SizeF(bounding_box_size)));
      ResizeObserverSize* size = ResizeObserverSize::Create(
          bounding_box_size.Width(), bounding_box_size.Height());
      content_box_size_.push_back(size);
      border_box_size_.push_back(size);
      bounding_box_size.Scale(style.EffectiveZoom());
      gfx::SizeF snapped_device_pixel_content_box =
          ResizeObserverUtilities::ComputeSnappedDevicePixelContentBox(
              bounding_box_size, layout_object, style);
      ResizeObserverSize* device_pixel_content_box_size =
          ResizeObserverSize::Create(snapped_device_pixel_content_box.width(),
                                     snapped_device_pixel_content_box.height());
      device_pixel_content_box_size_.push_back(device_pixel_content_box_size);
    } else if (layout_object->IsBox()) {
      LayoutBox* layout_box = target->GetLayoutBox();
      LayoutRect content_rect(
          LayoutPoint(layout_box->PaddingLeft(), layout_box->PaddingTop()),
          layout_box->ContentSize());
      content_rect_ =
          ResizeObserverUtilities::ZoomAdjustedLayoutRect(content_rect, style);

      gfx::SizeF content_box = ResizeObserverUtilities::ComputeZoomAdjustedBox(
          ResizeObserverBoxOptions::kContentBox, layout_object, style);
      gfx::SizeF border_box = ResizeObserverUtilities::ComputeZoomAdjustedBox(
          ResizeObserverBoxOptions::kBorderBox, layout_object, style);
      gfx::SizeF device_pixel_content_box =
          ResizeObserverUtilities::ComputeZoomAdjustedBox(
              ResizeObserverBoxOptions::kDevicePixelContentBox, layout_object,
              style);

      ResizeObserverSize* device_pixel_content_box_size =
          ResizeObserverSize::Create(device_pixel_content_box.width(),
                                     device_pixel_content_box.height());
      ResizeObserverSize* content_box_size =
          ResizeObserverSize::Create(content_box.width(), content_box.height());
      ResizeObserverSize* border_box_size =
          ResizeObserverSize::Create(border_box.width(), border_box.height());

      content_box_size_.push_back(content_box_size);
      border_box_size_.push_back(border_box_size);
      device_pixel_content_box_size_.push_back(device_pixel_content_box_size);
    }
  }
  if (!content_rect_)
    content_rect_ = DOMRectReadOnly::FromRectF(gfx::RectF());
  if (content_box_size_.size() == 0)
    content_box_size_.push_back(ResizeObserverSize::Create(0, 0));
  if (border_box_size_.size() == 0)
    border_box_size_.push_back(ResizeObserverSize::Create(0, 0));
  if (device_pixel_content_box_size_.size() == 0)
    device_pixel_content_box_size_.push_back(ResizeObserverSize::Create(0, 0));
}

void ResizeObserverEntry::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
  visitor->Trace(content_rect_);
  visitor->Trace(content_box_size_);
  visitor->Trace(border_box_size_);
  visitor->Trace(device_pixel_content_box_size_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
