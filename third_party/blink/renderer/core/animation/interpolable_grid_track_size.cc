// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_grid_track_size.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolable_grid_length.h"

namespace blink {

InterpolableGridTrackSize::InterpolableGridTrackSize(
    std::unique_ptr<InterpolableValue> min_value,
    std::unique_ptr<InterpolableValue> max_value,
    const GridTrackSizeType type)
    : min_value_(std::move(min_value)),
      max_value_(std::move(max_value)),
      type_(type) {
  DCHECK(min_value_);
  DCHECK(max_value_);
}

// static
std::unique_ptr<InterpolableGridTrackSize> InterpolableGridTrackSize::Create(
    const GridTrackSize& grid_track_size,
    float zoom) {
  std::unique_ptr<InterpolableValue> min_value;
  std::unique_ptr<InterpolableValue> max_value;

  min_value = InterpolableGridLength::Create(
      grid_track_size.MinOrFitContentTrackBreadth(), zoom);
  max_value = InterpolableGridLength::Create(
      grid_track_size.MaxOrFitContentTrackBreadth(), zoom);
  DCHECK(min_value);
  DCHECK(max_value);

  return std::make_unique<InterpolableGridTrackSize>(
      std::move(min_value), std::move(max_value), grid_track_size.GetType());
}

GridTrackSize InterpolableGridTrackSize::CreateTrackSize(
    const CSSToLengthConversionData& conversion_data) const {
  const InterpolableGridLength& interpolable_grid_length_min =
      To<InterpolableGridLength>(*min_value_);
  const InterpolableGridLength& interpolable_grid_length_max =
      To<InterpolableGridLength>(*max_value_);
  GridTrackSize track_size =
      (type_ == kMinMaxTrackSizing)
          ? GridTrackSize(
                interpolable_grid_length_min.CreateGridLength(conversion_data),
                interpolable_grid_length_max.CreateGridLength(conversion_data))
          : GridTrackSize(
                interpolable_grid_length_min.CreateGridLength(conversion_data),
                type_);
  return track_size;
}

InterpolableGridTrackSize* InterpolableGridTrackSize::RawClone() const {
  return new InterpolableGridTrackSize(min_value_->Clone(), max_value_->Clone(),
                                       type_);
}

InterpolableGridTrackSize* InterpolableGridTrackSize::RawCloneAndZero() const {
  return new InterpolableGridTrackSize(min_value_->CloneAndZero(),
                                       max_value_->CloneAndZero(), type_);
}

bool InterpolableGridTrackSize::Equals(const InterpolableValue& other) const {
  const InterpolableGridTrackSize& other_grid_track_size =
      To<InterpolableGridTrackSize>(other);
  return type_ == other_grid_track_size.type_ &&
         min_value_->Equals(*other_grid_track_size.min_value_) &&
         max_value_->Equals(*other_grid_track_size.max_value_);
}

void InterpolableGridTrackSize::Scale(double scale) {
  min_value_->Scale(scale);
  max_value_->Scale(scale);
}

void InterpolableGridTrackSize::Add(const InterpolableValue& other) {
  const InterpolableGridTrackSize& other_interpolable_grid_track_size =
      To<InterpolableGridTrackSize>(other);
  // Similarly to Interpolate(), we add two track sizes only when their types
  // are equal. Otherwise, the values and type are replaced.
  if (type_ == other_interpolable_grid_track_size.type_) {
    min_value_->Add(*other_interpolable_grid_track_size.min_value_);
    max_value_->Add(*other_interpolable_grid_track_size.max_value_);
  } else {
    type_ = other_interpolable_grid_track_size.type_;
    min_value_ = other_interpolable_grid_track_size.min_value_->Clone();
    max_value_ = other_interpolable_grid_track_size.max_value_->Clone();
  }
}

void InterpolableGridTrackSize::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableGridTrackSize& other_interpolable_grid_track_size =
      To<InterpolableGridTrackSize>(other);
  min_value_->AssertCanInterpolateWith(
      *other_interpolable_grid_track_size.min_value_);
  max_value_->AssertCanInterpolateWith(
      *other_interpolable_grid_track_size.max_value_);
}

void InterpolableGridTrackSize::Interpolate(const InterpolableValue& to,
                                            const double progress,
                                            InterpolableValue& result) const {
  const InterpolableGridTrackSize& grid_track_size_to =
      To<InterpolableGridTrackSize>(to);
  InterpolableGridTrackSize& grid_track_size_result =
      To<InterpolableGridTrackSize>(result);
  // If the type is different (e.g. going from fit-content to minmax, minmax to
  // length, etc.), we just flip at 50%.
  if (type_ != grid_track_size_to.type_) {
    if (progress < 0.5) {
      grid_track_size_result.type_ = type_;
      grid_track_size_result.min_value_ = min_value_->Clone();
      grid_track_size_result.max_value_ = max_value_->Clone();
    } else {
      grid_track_size_result.type_ = grid_track_size_to.type_;
      grid_track_size_result.min_value_ =
          grid_track_size_to.min_value_->Clone();
      grid_track_size_result.max_value_ =
          grid_track_size_to.max_value_->Clone();
    }
    return;
  }
  min_value_->Interpolate(*grid_track_size_to.min_value_, progress,
                          *grid_track_size_result.min_value_);
  max_value_->Interpolate(*grid_track_size_to.max_value_, progress,
                          *grid_track_size_result.max_value_);
}

}  // namespace blink
