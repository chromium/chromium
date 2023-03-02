// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/keyframe.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/core/animation/effect_model.h"
#include "third_party/blink/renderer/core/animation/invalidatable_interpolation.h"
#include "third_party/blink/renderer/core/animation/view_timeline.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

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
  if (offset_) {
    object_builder.Add("offset", offset_.value());
  } else {
    object_builder.AddNull("offset");
  }
  object_builder.Add("easing", easing_->ToString());
  object_builder.AddString("composite",
                           EffectModel::CompositeOperationToString(composite_));
}

bool Keyframe::ResolveTimelineOffset(const ViewTimeline* view_timeline,
                                     double range_start,
                                     double range_end) {
  if (!timeline_offset_) {
    return false;
  }

  double relative_offset =
      view_timeline->ToFractionalOffset(timeline_offset_.value());
  double range = range_end - range_start;
  if (!range) {
    if (offset_) {
      offset_.reset();
      return true;
    }
  } else {
    double resolved_offset = (relative_offset - range_start) / range;
    if (!offset_ || offset_.value() != resolved_offset) {
      offset_ = resolved_offset;
      return true;
    }
  }

  return false;
}

/* static */
bool Keyframe::LessThan(const Member<Keyframe>& a, const Member<Keyframe>& b) {
  if (a->Offset() < b->Offset()) {
    return true;
  }

  if (a->Offset() > b->Offset()) {
    return false;
  }

  if (a->original_index_ < b->original_index_) {
    return true;
  }

  return false;
}

bool Keyframe::ResetOffsetResolvedFromTimeline() {
  if (!timeline_offset_.has_value()) {
    return false;
  }

  offset_.reset();
  return true;
}

}  // namespace blink
