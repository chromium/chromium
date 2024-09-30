// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/keyframe.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range_offset.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/timeline_range.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

const double Keyframe::kNullComputedOffset =
    std::numeric_limits<double>::quiet_NaN();

Keyframe::PropertySpecificKeyframe::PropertySpecificKeyframe(
    double offset,
    scoped_refptr<TimingFunction> easing,
    EffectModel::CompositeOperation composite)
    : offset_(offset), easing_(std::move(easing)), composite_(composite) {
  DCHECK(std::isfinite(offset));
  if (!easing_)
    easing_ = LinearTimingFunction::Shared();
}

Interpolation* Keyframe::PropertySpecificKeyframe::CreateInterpolation(
    const PropertyHandle& property_handle,
    const Keyframe::PropertySpecificKeyframe& end) const {
  // const_cast to take refs.
  return MakeGarbageCollected<InvalidatableInterpolation>(
      property_handle, const_cast<PropertySpecificKeyframe*>(this),
      const_cast<PropertySpecificKeyframe*>(&end));
}

void Keyframe::AddKeyframePropertiesToV8Object(V8ObjectBuilder& object_builder,
                                               Element* element) const {
  // If the keyframe has a timeline offset add it instead of offset.
  if (timeline_offset_) {
    TimelineRangeOffset* timeline_range_offset = TimelineRangeOffset::Create();
    timeline_range_offset->setRangeName(timeline_offset_->name);
    DCHECK(timeline_offset_->offset.IsPercent());
    timeline_range_offset->setOffset(
        CSSUnitValue::Create(timeline_offset_->offset.Value(),
                             CSSPrimitiveValue::UnitType::kPercentage));
    object_builder.Add("offset", timeline_range_offset);
  } else if (offset_) {
    object_builder.AddNumber("offset", offset_.value());
  } else {
    object_builder.AddNull("offset");
  }
  object_builder.AddString("easing", easing_->ToString());
  if (composite_) {
    object_builder.AddString(
        "composite", V8CompositeOperation(EffectModel::CompositeOperationToEnum(
                                              composite_.value()))
                         .AsCStr());
  } else {
    object_builder.AddString("composite", "auto");
  }
}

bool Keyframe::ResolveTimelineOffset(const TimelineRange& timeline_range,
                                     double range_start,
                                     double range_end) {
  if (!timeline_offset_) {
    return false;
  }

  double relative_offset =
      timeline_range.ToFractionalOffset(timeline_offset_.value());
  double range = range_end - range_start;
  if (!range) {
    if (offset_) {
      offset_.reset();
      computed_offset_ = kNullComputedOffset;
      return true;
    }
  } else {
    double resolved_offset = (relative_offset - range_start) / range;
    if (!offset_ || offset_.value() != resolved_offset) {
      offset_ = resolved_offset;
      computed_offset_ = resolved_offset;
      return true;
    }
  }

  return false;
}

/* static */
bool Keyframe::LessThan(const Member<Keyframe>& a, const Member<Keyframe>& b) {
  std::optional first =
      a->ComputedOffset().has_value() ? a->ComputedOffset() : a->Offset();
  std::optional second =
      b->ComputedOffset().has_value() ? b->ComputedOffset() : b->Offset();

  if (first < second) {
    return true;
  }

  if (first > second) {
    return false;
  }

  if (a->original_index_ < b->original_index_) {
    return true;
  }

  return false;
}

}  // namespace blink
