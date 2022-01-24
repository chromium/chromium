// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/animation/compositor_filter_keyframe.h"

#include <memory>
#include "third_party/blink/renderer/platform/animation/timing_function.h"

namespace blink {

CompositorFilterKeyframe::CompositorFilterKeyframe(
    double time,
    CompositorFilterOperations value,
    const TimingFunction& timing_function)
    : filter_keyframe_(
          cc::FilterKeyframe::Create(base::Seconds(time),
                                     value.ReleaseCcFilterOperations(),
                                     timing_function.CloneToCC())) {}

CompositorFilterKeyframe::~CompositorFilterKeyframe() = default;

base::TimeDelta CompositorFilterKeyframe::Time() const {
  return filter_keyframe_->Time();
}

const gfx::TimingFunction* CompositorFilterKeyframe::CcTimingFunction() const {
  return filter_keyframe_->timing_function();
}

std::unique_ptr<cc::FilterKeyframe> CompositorFilterKeyframe::CloneToCC()
    const {
  return filter_keyframe_->Clone();
}

}  // namespace blink
