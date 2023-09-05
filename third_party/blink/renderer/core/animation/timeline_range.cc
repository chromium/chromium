// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/timeline_range.h"

#include "third_party/blink/renderer/core/animation/timeline_offset.h"
#include "third_party/blink/renderer/core/animation/timing_calculations.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

bool TimelineRange::IsEmpty() const {
  return TimingCalculations::LessThanOrEqualToWithinEpsilon(
      offsets_.end - offsets_.start, 0.0);
}

double TimelineRange::ToFractionalOffset(
    const TimelineOffset& timeline_offset) const {
  if (IsEmpty()) {
    // This is either a monotonic timeline or an inactive ScrollTimeline.
    return 0.0;
  }
  double full_range_size = offsets_.end - offsets_.start;

  ScrollOffsets range(0, 0);

  if (view_offsets_ == ViewOffsets()) {
    // This is a non-view ScrollTimeline, or it can also be a ViewTimeline
    // that happens have subject with size=0.
    range = {offsets_.start, offsets_.end};
  } else {
    range = ConvertNamedRange(timeline_offset.name);
  }

  DCHECK_GT(full_range_size, 0);

  double offset =
      range.start + MinimumValueForLength(timeline_offset.offset,
                                          LayoutUnit(range.end - range.start));
  return (offset - offsets_.start) / full_range_size;
}

TimelineRange::ScrollOffsets TimelineRange::ConvertNamedRange(
    NamedRange named_range) const {
  // https://drafts.csswg.org/scroll-animations-1/#view-timelines-ranges
  double align_subject_start_view_end = offsets_.start;
  double align_subject_end_view_start = offsets_.end;
  double align_subject_start_view_start =
      align_subject_end_view_start - view_offsets_.exit_crossing_distance;
  double align_subject_end_view_end =
      align_subject_start_view_end + view_offsets_.entry_crossing_distance;

  // TODO(crbug.com/1448294): This needs to account for when the subject (or an
  // ancestor) is position: sticky and stuck to the viewport during entry/exit
  // or before entry/cover. Currently, we only handle stickiness during the
  // "contain" range (see ViewTimeline::CalculateOffsets).

  switch (named_range) {
    case TimelineOffset::NamedRange::kNone:
    case TimelineOffset::NamedRange::kCover:
      // Represents the full range of the view progress timeline:
      //   0% progress represents the position at which the start border edge of
      //   the element’s principal box coincides with the end edge of its view
      //   progress visibility range.
      //   100% progress represents the position at which the end border edge of
      //   the element’s principal box coincides with the start edge of its view
      //   progress visibility range.
      return {align_subject_start_view_end, align_subject_end_view_start};

    case TimelineOffset::NamedRange::kContain:
      // Represents the range during which the principal box is either fully
      // contained by, or fully covers, its view progress visibility range
      // within the scrollport.
      // 0% progress represents the earlier position at which:
      //   1. the start border edge of the element’s principal box coincides
      //      with the start edge of its view progress visibility range.
      //   2. the end border edge of the element’s principal box coincides with
      //      the end edge of its view progress visibility range.
      // 100% progress represents the later position at which:
      //   1. the start border edge of the element’s principal box coincides
      //      with the start edge of its view progress visibility range.
      //   2. the end border edge of the element’s principal box coincides with
      //      the end edge of its view progress visibility range.
      return {
          std::min(align_subject_start_view_start, align_subject_end_view_end),
          std::max(align_subject_start_view_start, align_subject_end_view_end)};

    case TimelineOffset::NamedRange::kEntry:
      // Represents the range during which the principal box is entering the
      // view progress visibility range.
      //   0% is equivalent to 0% of the cover range.
      //   100% is equivalent to 0% of the contain range.
      return {
          align_subject_start_view_end,
          std::min(align_subject_start_view_start, align_subject_end_view_end)};

    case TimelineOffset::NamedRange::kEntryCrossing:
      // Represents the range during which the principal box is crossing the
      // entry edge of the viewport.
      //   0% is equivalent to 0% of the cover range.
      return {align_subject_start_view_end, align_subject_end_view_end};

    case TimelineOffset::NamedRange::kExit:
      // Represents the range during which the principal box is exiting the view
      // progress visibility range.
      //   0% is equivalent to 100% of the contain range.
      //   100% is equivalent to 100% of the cover range.
      return {
          std::max(align_subject_start_view_start, align_subject_end_view_end),
          align_subject_end_view_start};

    case TimelineOffset::NamedRange::kExitCrossing:
      // Represents the range during which the principal box is exiting the view
      // progress visibility range.
      //   100% is equivalent to 100% of the cover range.
      return {align_subject_start_view_start, align_subject_end_view_start};
  }
}

}  // namespace blink
