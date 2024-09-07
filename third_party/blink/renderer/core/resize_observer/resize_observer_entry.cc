// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"

#include "third_party/blink/renderer/bindings/core/v8/frozen_array.h"
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
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

ResizeObserverEntry::ResizeObserverEntry(Element* target) : target_(target) {
  HeapVector<Member<ResizeObserverSize>> content_box_size;
  HeapVector<Member<ResizeObserverSize>> border_box_size;
  HeapVector<Member<ResizeObserverSize>> device_pixel_content_box_size;

  if (const LayoutObject* layout_object = target->GetLayoutObject()) {
    if (layout_object->IsSVGChild()) {
      PopulateFromSVGChild(*layout_object, content_box_size, border_box_size,
                           device_pixel_content_box_size);
    } else if (const auto* layout_box = DynamicTo<LayoutBox>(*layout_object)) {
      PopulateFromLayoutBox(*layout_box, content_box_size, border_box_size,
                            device_pixel_content_box_size);
    }
  }

  if (!content_rect_) {
    content_rect_ = DOMRectReadOnly::FromRectF(gfx::RectF());
  }
  if (content_box_size.empty()) {
    content_box_size.push_back(ResizeObserverSize::Create(0, 0));
  }
  if (border_box_size.empty()) {
    border_box_size.push_back(ResizeObserverSize::Create(0, 0));
  }
  if (device_pixel_content_box_size.empty()) {
    device_pixel_content_box_size.push_back(ResizeObserverSize::Create(0, 0));
  }
  content_box_size_ =
      MakeGarbageCollected<FrozenArray<ResizeObserverSize>>(content_box_size);
  border_box_size_ =
      MakeGarbageCollected<FrozenArray<ResizeObserverSize>>(border_box_size);
  device_pixel_content_box_size_ =
      MakeGarbageCollected<FrozenArray<ResizeObserverSize>>(
          device_pixel_content_box_size);
}

void ResizeObserverEntry::PopulateFromLayoutBox(
    const LayoutBox& layout_box,
    HeapVector<Member<ResizeObserverSize>>& content_box_size,
    HeapVector<Member<ResizeObserverSize>>& border_box_size,
    HeapVector<Member<ResizeObserverSize>>& device_pixel_content_box_size) {
  const ComputedStyle& style = layout_box.StyleRef();
  PhysicalRect content_rect(
      PhysicalOffset(layout_box.PaddingLeft(), layout_box.PaddingTop()),
      layout_box.ContentSize());
  content_rect_ =
      ResizeObserverUtilities::ZoomAdjustedPhysicalRect(content_rect, style);

  gfx::SizeF content_box = ResizeObserverUtilities::ComputeZoomAdjustedBox(
      ResizeObserverBoxOptions::kContentBox, layout_box, style);
  gfx::SizeF border_box = ResizeObserverUtilities::ComputeZoomAdjustedBox(
      ResizeObserverBoxOptions::kBorderBox, layout_box, style);
  gfx::SizeF device_pixel_content_box =
      ResizeObserverUtilities::ComputeZoomAdjustedBox(
          ResizeObserverBoxOptions::kDevicePixelContentBox, layout_box, style);

  content_box_size.push_back(
      ResizeObserverSize::Create(content_box.width(), content_box.height()));
  border_box_size.push_back(
      ResizeObserverSize::Create(border_box.width(), border_box.height()));
  device_pixel_content_box_size.push_back(ResizeObserverSize::Create(
      device_pixel_content_box.width(), device_pixel_content_box.height()));
}

void ResizeObserverEntry::PopulateFromSVGChild(
    const LayoutObject& layout_object,
    HeapVector<Member<ResizeObserverSize>>& content_box_size,
    HeapVector<Member<ResizeObserverSize>>& border_box_size,
    HeapVector<Member<ResizeObserverSize>>& device_pixel_content_box_size) {
  DCHECK(layout_object.IsSVGChild());
  auto* svg_graphics_element =
      DynamicTo<SVGGraphicsElement>(layout_object.GetNode());
  if (!svg_graphics_element)
    return;
  const gfx::SizeF bounding_box_size = svg_graphics_element->GetBBox().size();
  content_rect_ =
      DOMRectReadOnly::FromRectF(gfx::RectF(gfx::PointF(), bounding_box_size));
  ResizeObserverSize* size = ResizeObserverSize::Create(
      bounding_box_size.width(), bounding_box_size.height());
  content_box_size.push_back(size);
  border_box_size.push_back(size);
  const ComputedStyle& style = layout_object.StyleRef();
  const gfx::SizeF scaled_bounding_box_size(
      gfx::ScaleSize(bounding_box_size, style.EffectiveZoom()));
  gfx::SizeF snapped_device_pixel_content_box =
      ResizeObserverUtilities::ComputeSnappedDevicePixelContentBox(
          scaled_bounding_box_size, layout_object, style);
  device_pixel_content_box_size.push_back(
      ResizeObserverSize::Create(snapped_device_pixel_content_box.width(),
                                 snapped_device_pixel_content_box.height()));
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
