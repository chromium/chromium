// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/view_timeline.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"

namespace blink {

namespace {

double ComputeOffset(LayoutBox* subject,
                     LayoutBox* source,
                     ScrollOrientation physical_orientation) {
  Element* source_element = DynamicTo<Element>(source->GetNode());
  MapCoordinatesFlags flags = kIgnoreScrollOffset;
  gfx::PointF point = gfx::PointF(
      subject->LocalToAncestorPoint(PhysicalOffset(), source, flags));

  if (physical_orientation == kHorizontalScroll)
    return point.x() - source_element->clientLeft();
  else
    return point.y() - source_element->clientTop();
}

}  // end namespace

ViewTimeline* ViewTimeline::Create(Document& document,
                                   ViewTimelineOptions* options,
                                   ExceptionState& exception_state) {
  Element* subject = options->subject();

  ScrollDirection orientation;
  if (!StringToScrollDirection(options->axis(), orientation)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Invalid axis");
    return nullptr;
  }
  ViewTimeline* view_timeline =
      MakeGarbageCollected<ViewTimeline>(&document, subject, orientation);
  view_timeline->SnapshotState();
  return view_timeline;
}

ViewTimeline::ViewTimeline(Document* document,
                           Element* subject,
                           ScrollDirection orientation)
    : ScrollTimeline(document,
                     ReferenceType::kNearestAncestor,
                     subject,
                     orientation) {
  // Ensure that the timeline stays alive as long as the subject.
  if (subject)
    subject->RegisterScrollTimeline(this);
}

absl::optional<ScrollTimeline::ScrollOffsets> ViewTimeline::CalculateOffsets(
    PaintLayerScrollableArea* scrollable_area,
    ScrollOrientation physical_orientation) const {
  DCHECK(subject());
  LayoutBox* layout_box = subject()->GetLayoutBox();
  DCHECK(layout_box);
  Element* source = SourceInternal();
  DCHECK(source);
  LayoutBox* source_layout = source->GetLayoutBox();
  DCHECK(source_layout);

  double target_offset =
      ComputeOffset(layout_box, source_layout, physical_orientation);
  double target_size = 0;
  double viewport_size = 0;
  if (physical_orientation == kHorizontalScroll) {
    target_size = layout_box->Size().Width().ToDouble();
    viewport_size = scrollable_area->VisibleScrollSnapportRect().Width();
  } else {
    target_size = layout_box->Size().Height().ToDouble();
    viewport_size = scrollable_area->VisibleScrollSnapportRect().Height();
  }

  double start_offset = target_offset - viewport_size;
  double end_offset = target_offset + target_size;
  return absl::make_optional<ScrollOffsets>(start_offset, end_offset);
}

}  // namespace blink
