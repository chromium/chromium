// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_grid_track_list.h"

#include <memory>
#include "third_party/blink/renderer/core/animation/interpolable_grid_track_repeater.h"

namespace blink {

InterpolableGridTrackList::InterpolableGridTrackList(InterpolableList* values,
                                                     double progress)
    : values_(values), progress_(progress) {
  DCHECK(values_);
}

// static
InterpolableGridTrackList* InterpolableGridTrackList::MaybeCreate(
    const NGGridTrackList& track_list,
    const CSSProperty& property,
    float zoom) {
  // Subgrids do not have sizes stored on their track list to interpolate.
  if (track_list.HasAutoRepeater() || track_list.IsSubgriddedAxis()) {
    return nullptr;
  }

  wtf_size_t repeater_count = track_list.RepeaterCount();
  InterpolableList* values =
      MakeGarbageCollected<InterpolableList>(repeater_count);

  for (wtf_size_t i = 0; i < repeater_count; ++i) {
    Vector<GridTrackSize, 1> repeater_track_sizes;
    for (wtf_size_t j = 0; j < track_list.RepeatSize(i); ++j)
      repeater_track_sizes.push_back(track_list.RepeatTrackSize(i, j));

    const NGGridTrackRepeater repeater(
        track_list.RepeatIndex(i), track_list.RepeatSize(i),
        track_list.RepeatCount(i, 0), track_list.LineNameIndicesCount(i),
        track_list.RepeatType(i));
    InterpolableGridTrackRepeater* result =
        InterpolableGridTrackRepeater::Create(repeater, repeater_track_sizes,
                                              property, zoom);
    DCHECK(result);
    values->Set(i, result);
  }
  return MakeGarbageCollected<InterpolableGridTrackList>(values, 0);
}

NGGridTrackList InterpolableGridTrackList::CreateNGGridTrackList(
    const CSSToLengthConversionData& conversion_data) const {
  NGGridTrackList new_track_list;
  for (wtf_size_t i = 0; i < values_->length(); ++i) {
    const InterpolableGridTrackRepeater& repeater =
        To<InterpolableGridTrackRepeater>(*values_->Get(i));
    new_track_list.AddRepeater(repeater.CreateTrackSizes(conversion_data),
                               repeater.RepeatType(), repeater.RepeatCount());
  }
  return new_track_list;
}

InterpolableGridTrackList* InterpolableGridTrackList::RawClone() const {
  InterpolableList* values(values_->Clone());
  return MakeGarbageCollected<InterpolableGridTrackList>(std::move(values),
                                                         progress_);
}

InterpolableGridTrackList* InterpolableGridTrackList::RawCloneAndZero() const {
  InterpolableList* values(values_->CloneAndZero());
  return MakeGarbageCollected<InterpolableGridTrackList>(std::move(values),
                                                         progress_);
}

bool InterpolableGridTrackList::Equals(const InterpolableValue& other) const {
  return IsCompatibleWith(other) &&
         values_->Equals(*(To<InterpolableGridTrackList>(other).values_));
}

void InterpolableGridTrackList::Scale(double scale) {
  values_->Scale(scale);
}

void InterpolableGridTrackList::Add(const InterpolableValue& other) {
  // We can only add interpolable lists that have equal length and have
  // compatible repeaters.
  DCHECK(IsCompatibleWith(other));
  const InterpolableGridTrackList& other_track_list =
      To<InterpolableGridTrackList>(other);
  values_->Add(*other_track_list.values_);
  progress_ = other_track_list.progress_;
}

bool InterpolableGridTrackList::IsCompatibleWith(
    const InterpolableValue& other) const {
  const InterpolableGridTrackList& other_track_list =
      To<InterpolableGridTrackList>(other);
  if (values_->length() != other_track_list.values_->length())
    return false;

  for (wtf_size_t i = 0; i < values_->length(); ++i) {
    const InterpolableGridTrackRepeater& repeater =
        To<InterpolableGridTrackRepeater>(*values_->Get(i));
    if (!repeater.IsCompatibleWith(*other_track_list.values_->Get(i)))
      return false;
  }
  return true;
}

void InterpolableGridTrackList::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  const InterpolableGridTrackList& other_track_list =
      To<InterpolableGridTrackList>(other);

  DCHECK_EQ(values_->length(), other_track_list.values_->length());
  values_->AssertCanInterpolateWith(*other_track_list.values_);
}

void InterpolableGridTrackList::Interpolate(const InterpolableValue& to,
                                            const double progress,
                                            InterpolableValue& result) const {
  const InterpolableGridTrackList& grid_track_list_to =
      To<InterpolableGridTrackList>(to);
  InterpolableGridTrackList& grid_track_list_result =
      To<InterpolableGridTrackList>(result);
  values_->Interpolate(*grid_track_list_to.values_, progress,
                       *grid_track_list_result.values_);
  grid_track_list_result.progress_ = progress;
}

}  // namespace blink
