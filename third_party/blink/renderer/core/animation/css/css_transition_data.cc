// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/css/css_transition_data.h"

#include "third_party/blink/renderer/core/animation/timing.h"

namespace blink {

CSSTransitionData::CSSTransitionData() : CSSTimingData(InitialDuration()) {
  property_list_.push_back(InitialProperty());
  behavior_list_.push_back(InitialBehavior());
}

CSSTransitionData::CSSTransitionData(const CSSTransitionData& other) = default;

bool CSSTransitionData::TransitionsMatchForStyleRecalc(
    const CSSTransitionData& other) const {
  return property_list_ == other.property_list_ &&
         TimingMatchForStyleRecalc(other);
}

Timing CSSTransitionData::ConvertToTiming(size_t index) const {
  DCHECK_LT(index, property_list_.size());
  // Note that the backwards fill part is required for delay to work.
  Timing timing = CSSTimingData::ConvertToTiming(index);
  timing.fill_mode = Timing::FillMode::BACKWARDS;
  return timing;
}

}  // namespace blink
