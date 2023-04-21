// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_VIEW_TIMELINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_VIEW_TIMELINE_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/animation/scroll_timeline.h"
#include "third_party/blink/renderer/core/animation/timeline_inset.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ViewTimelineOptions;

// Implements the ViewTimeline from the Scroll-linked Animations spec.
//
// A ViewTimeline is a special form of ScrollTimeline in which the start and
// end scroll offsets are derived based on relative offset of the subject view
// within the source scroll container.
//
// TODO(crbug.com/1329159): Update link once rewrite replaces the draft version.
// https://drafts.csswg.org/scroll-animations-1/rewrite#viewtimeline-interface
class CORE_EXPORT ViewTimeline : public ScrollTimeline {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ViewTimeline* Create(Document&, ViewTimelineOptions*, ExceptionState&);

  ViewTimeline(Document*,
               TimelineAttachment attachment,
               Element* subject,
               ScrollAxis axis,
               TimelineInset);

  bool IsViewTimeline() const override { return true; }

  CSSNumericValue* getCurrentTime(const String& rangeName) override;

  AnimationTimeDelta CalculateIntrinsicIterationDuration(
      const Animation*,
      const Timing&) override;

  AnimationTimeDelta CalculateIntrinsicIterationDuration(
      const absl::optional<TimelineOffset>& rangeStart,
      const absl::optional<TimelineOffset>& rangeEnd,
      const Timing&) override;

  // IDL API implementation.
  Element* subject() const;

  bool Matches(TimelineAttachment,
               Element* subject,
               ScrollAxis,
               const TimelineInset&) const;

  const TimelineInset& GetInset() const;

  // Converts a delay that is expressed as a (phase,percentage) pair to
  // a fractional offset.
  double ToFractionalOffset(const TimelineOffset& timeline_offset) const;

  CSSNumericValue* startOffset() const;
  CSSNumericValue* endOffset() const;

  bool ResolveTimelineOffsets(bool invalidate_effect) const;

  Animation* Play(AnimationEffect*,
                  ExceptionState& = ASSERT_NO_EXCEPTION) override;

  void Trace(Visitor*) const override;

 protected:
  absl::optional<ScrollOffsets> CalculateOffsets(
      PaintLayerScrollableArea* scrollable_area,
      ScrollOrientation physical_orientation) const override;

  // ScrollSnapshotClient:
  void UpdateSnapshot() override;
  bool ValidateSnapshot() override;

  void FlushStyleUpdate() override;

 private:
  // Cache values to make timeline phase conversions more efficient.
  mutable double target_offset_;
  mutable double target_size_;
  mutable double viewport_size_;
  mutable double start_side_inset_;
  mutable double end_side_inset_;
  mutable double start_offset_ = 0;
  mutable double end_offset_ = 0;
  // If either of the following elements are non-null, we need to update
  // |inset_| on a style change.
  Member<const CSSValue> style_dependant_start_inset_;
  Member<const CSSValue> style_dependant_end_inset_;
};

template <>
struct DowncastTraits<ViewTimeline> {
  static bool AllowFrom(const AnimationTimeline& value) {
    return value.IsViewTimeline();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_VIEW_TIMELINE_H_
