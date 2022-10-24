// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TIMING_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TIMING_DATA_H_

#include "third_party/blink/renderer/core/animation/timing.h"
#include "third_party/blink/renderer/platform/animation/timing_function.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

struct Timing;

class CSSTimingData {
  USING_FAST_MALLOC(CSSTimingData);

 public:
  ~CSSTimingData() = default;

  const Vector<double>& DelayList() const { return delay_list_; }
  const Vector<Timing::Delay>& DelayStartList() const {
    return delay_start_list_;
  }
  const Vector<Timing::Delay>& DelayEndList() const { return delay_end_list_; }
  const Vector<double>& DurationList() const { return duration_list_; }
  const Vector<scoped_refptr<TimingFunction>>& TimingFunctionList() const {
    return timing_function_list_;
  }

  Vector<double>& DelayList() { return delay_list_; }
  Vector<Timing::Delay>& DelayStartList() { return delay_start_list_; }
  Vector<Timing::Delay>& DelayEndList() { return delay_end_list_; }
  Vector<double>& DurationList() { return duration_list_; }
  Vector<scoped_refptr<TimingFunction>>& TimingFunctionList() {
    return timing_function_list_;
  }

  static Timing::Delay InitialDelayStart() { return Timing::Delay(); }
  static Timing::Delay InitialDelayEnd() { return Timing::Delay(); }
  static double InitialDelay() { return 0; }
  static double InitialDuration() { return 0; }
  static scoped_refptr<TimingFunction> InitialTimingFunction() {
    return CubicBezierTimingFunction::Preset(
        CubicBezierTimingFunction::EaseType::EASE);
  }

  template <class T>
  static const T& GetRepeated(const Vector<T>& v, size_t index) {
    return v[index % v.size()];
  }

 protected:
  CSSTimingData();
  explicit CSSTimingData(const CSSTimingData&);

  Timing ConvertToTiming(size_t index) const;
  bool TimingMatchForStyleRecalc(const CSSTimingData&) const;

 private:
  // Values from the animation-delay, animation-delay-start, and
  // animation-delay-end properties are stored separately.
  //
  // See CSSAnimationData::ConvertToTiming.
  //
  // TODO(crbug.com/1375994): Once animation-delay is a shorthand, only
  // start/end should remain.
  Vector<double> delay_list_;
  Vector<Timing::Delay> delay_start_list_;
  Vector<Timing::Delay> delay_end_list_;
  Vector<double> duration_list_;
  Vector<scoped_refptr<TimingFunction>> timing_function_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_CSS_CSS_TIMING_DATA_H_
