// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_scrollbar_color.h"

#include <cmath>
#include <memory>
#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/css_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_value.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"

namespace blink {

InterpolableScrollbarColor::InterpolableScrollbarColor() = default;

InterpolableScrollbarColor::InterpolableScrollbarColor(
    InterpolableColor* thumb_color,
    InterpolableColor* track_color)
    : thumb_color_(thumb_color), track_color_(track_color) {}

InterpolableScrollbarColor* InterpolableScrollbarColor::Create(
    const StyleScrollbarColor& scrollbar_color) {
  InterpolableScrollbarColor* result =
      MakeGarbageCollected<InterpolableScrollbarColor>();
  result->thumb_color_ =
      InterpolableColor::Create(scrollbar_color.GetThumbColor().GetColor());
  result->track_color_ =
      InterpolableColor::Create(scrollbar_color.GetTrackColor().GetColor());

  return result;
}

InterpolableScrollbarColor* InterpolableScrollbarColor::RawClone() const {
  return MakeGarbageCollected<InterpolableScrollbarColor>(
      thumb_color_->Clone(), track_color_->Clone());
}

InterpolableScrollbarColor* InterpolableScrollbarColor::RawCloneAndZero()
    const {
  return MakeGarbageCollected<InterpolableScrollbarColor>(
      thumb_color_->CloneAndZero(), track_color_->CloneAndZero());
}

StyleScrollbarColor* InterpolableScrollbarColor::GetScrollbarColor(
    const StyleResolverState& state) const {
  return MakeGarbageCollected<StyleScrollbarColor>(
      StyleColor(CSSColorInterpolationType::ResolveInterpolableColor(
          *thumb_color_, state)),
      StyleColor(CSSColorInterpolationType::ResolveInterpolableColor(
          *track_color_, state)));
}

void InterpolableScrollbarColor::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableScrollbarColor& other_scrollbar_color =
      To<InterpolableScrollbarColor>(other);
  thumb_color_->AssertCanInterpolateWith(*other_scrollbar_color.thumb_color_);
  track_color_->AssertCanInterpolateWith(*other_scrollbar_color.track_color_);
}

void InterpolableScrollbarColor::Scale(double scale) {
  thumb_color_->Scale(scale);
  track_color_->Scale(scale);
}

void InterpolableScrollbarColor::Add(const InterpolableValue& other) {
  const InterpolableScrollbarColor& other_scrollbar_color =
      To<InterpolableScrollbarColor>(other);
  thumb_color_->Add(*other_scrollbar_color.thumb_color_);
  track_color_->Add(*other_scrollbar_color.track_color_);
}

void InterpolableScrollbarColor::Interpolate(const InterpolableValue& to,
                                             const double progress,
                                             InterpolableValue& result) const {
  const InterpolableScrollbarColor& to_scrollbar_color =
      To<InterpolableScrollbarColor>(to);
  InterpolableScrollbarColor& result_scrollbar_color =
      To<InterpolableScrollbarColor>(result);

  thumb_color_->Interpolate(*to_scrollbar_color.thumb_color_, progress,
                            *result_scrollbar_color.thumb_color_);
  track_color_->Interpolate(*to_scrollbar_color.track_color_, progress,
                            *result_scrollbar_color.track_color_);
}

void InterpolableScrollbarColor::Composite(
    const InterpolableScrollbarColor& other,
    double fraction) {
  thumb_color_->Composite(*other.thumb_color_, fraction);
  track_color_->Composite(*other.track_color_, fraction);
}

}  // namespace blink
