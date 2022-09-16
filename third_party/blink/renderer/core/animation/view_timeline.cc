// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/view_timeline.h"

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_view_timeline_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/platform/geometry/calculation_value.h"

namespace blink {

namespace {

double ComputeOffset(LayoutBox* subject,
                     LayoutBox* source,
                     ScrollOrientation physical_orientation) {
  Element* source_element = DynamicTo<Element>(source->GetNode());
  MapCoordinatesFlags flags = kIgnoreScrollOffset;
  gfx::PointF point = gfx::PointF(
      subject->LocalToAncestorPoint(PhysicalOffset(), source, flags));

  // We can not call the regular clientLeft/Top functions here, because we
  // may reach this function during style resolution, and clientLeft/Top
  // also attempt to update style/layout.
  if (physical_orientation == kHorizontalScroll)
    return point.x() - source_element->ClientLeftNoLayout();
  else
    return point.y() - source_element->ClientTopNoLayout();
}

double ComputeInset(const Length& inset, double viewport_size) {
  if (inset.IsFixed())
    return inset.Pixels();
  if (inset.IsPercent())
    return (viewport_size * (inset.Percent() / 100.0));
  if (inset.IsCalculated())
    return inset.GetCalculationValue().Evaluate(ClampTo<float>(viewport_size));
  // TODO(crbug.com/1344151): Support 'auto'.
  DCHECK(inset.IsAuto());
  return 0;
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
  if (subject) {
    // This ensures that Client[Left,Top]NoLayout (reached via SnapshotState)
    // returns up-to-date information.
    document.UpdateStyleAndLayoutForNode(subject,
                                         DocumentUpdateReason::kJavaScript);
  }
  ViewTimeline* view_timeline = MakeGarbageCollected<ViewTimeline>(
      &document, subject, orientation, Inset());
  view_timeline->SnapshotState();
  return view_timeline;
}

ViewTimeline::ViewTimeline(Document* document,
                           Element* subject,
                           ScrollDirection orientation,
                           Inset inset)
    : ScrollTimeline(document,
                     ReferenceType::kNearestAncestor,
                     subject,
                     orientation),
      inset_(inset) {
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

  // Note that the end_side_inset is used to adjust the start offset,
  // and the start_side_inset is used to adjust the end offset.
  // This is because "start side" refers to logical start side [1] of the
  // source box, where as "start offset" refers to the start of the timeline,
  // and similarly for end side/offset.
  // [1] https://drafts.csswg.org/css-writing-modes-4/#css-start
  double end_side_inset = ComputeInset(inset_.end_side, viewport_size);
  double start_side_inset = ComputeInset(inset_.start_side, viewport_size);

  double start_offset = target_offset - viewport_size + end_side_inset;
  double end_offset = target_offset + target_size - start_side_inset;
  return absl::make_optional<ScrollOffsets>(start_offset, end_offset);
}

}  // namespace blink
