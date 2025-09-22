// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/grid_track_list.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
namespace blink {

GridTrackRepeater::GridTrackRepeater(wtf_size_t repeat_index,
                                     wtf_size_t repeat_size,
                                     wtf_size_t repeat_count,
                                     RepeatType repeat_type)
    : repeat_index(repeat_index),
      repeat_size(repeat_size),
      repeat_count(repeat_count),
      repeat_type(repeat_type) {}

String GridTrackRepeater::ToString() const {
  StringBuilder builder;
  builder.Append("Repeater: [Index: ");
  builder.AppendNumber<wtf_size_t>(repeat_index);
  builder.Append("], [RepeatSize: ");
  builder.AppendNumber<wtf_size_t>(repeat_size);
  builder.Append("], [RepeatCount: ");
  switch (repeat_type) {
    case RepeatType::kNoRepeat:
    case RepeatType::kInteger:
      builder.AppendNumber<wtf_size_t>(repeat_count);
      builder.Append("]");
      break;
    case RepeatType::kAutoFill:
      builder.Append("auto-fill]");
      break;
    case RepeatType::kAutoFit:
      builder.Append("auto-fit]");
      break;
  }
  return builder.ToString();
}

bool GridTrackRepeater::operator==(const GridTrackRepeater& other) const {
  return repeat_index == other.repeat_index &&
         repeat_size == other.repeat_size &&
         repeat_count == other.repeat_count && repeat_type == other.repeat_type;
}

wtf_size_t GridTrackList::RepeatCount(wtf_size_t index,
                                      wtf_size_t auto_value) const {
  DCHECK_LT(index, RepeaterCount());
  if (index == auto_repeater_index_) {
    return auto_value;
  }
  return repeaters_[index].repeat_count;
}

wtf_size_t GridTrackList::RepeatIndex(wtf_size_t index) const {
  // `repeat_index` is used for sizes, which subgrids don't have.
  DCHECK(!IsSubgriddedAxis());
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_index;
}

wtf_size_t GridTrackList::RepeatSize(wtf_size_t index) const {
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_size;
}

GridTrackRepeater::RepeatType GridTrackList::RepeatType(
    wtf_size_t index) const {
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_type;
}

const GridTrackSize& GridTrackList::RepeatTrackSize(wtf_size_t index,
                                                    wtf_size_t n) const {
  // Subgrids don't have track sizes associated with them.
  DCHECK(!IsSubgriddedAxis());
  DCHECK_LT(index, RepeaterCount());
  DCHECK_LT(n, RepeatSize(index));

  wtf_size_t repeat_index = repeaters_[index].repeat_index;
  DCHECK_LT(repeat_index + n, repeater_track_sizes_.size());
  return repeater_track_sizes_[repeat_index + n];
}

wtf_size_t GridTrackList::RepeaterCount() const {
  return repeaters_.size();
}

wtf_size_t GridTrackList::TrackCountWithoutAutoRepeat() const {
  return track_count_without_auto_repeat_;
}

wtf_size_t GridTrackList::AutoRepeatTrackCount() const {
  return HasAutoRepeater() ? repeaters_[auto_repeater_index_].repeat_size : 0;
}

wtf_size_t GridTrackList::NonAutoRepeatLineCount() const {
  DCHECK(IsSubgriddedAxis());
  return non_auto_repeat_line_count_;
}

void GridTrackList::IncrementNonAutoRepeatLineCount() {
  DCHECK(IsSubgriddedAxis());
  ++non_auto_repeat_line_count_;
}

bool GridTrackList::AddRepeater(
    const Vector<GridTrackSize, 1>& repeater_track_sizes,
    GridTrackRepeater::RepeatType repeat_type,
    wtf_size_t repeat_count,
    wtf_size_t repeat_number_of_lines) {
  // Non-subgrid repeaters always have sizes associated with them, while
  // subgrids repeaters never do, as sizes will come from the parent grid.
  DCHECK(!IsSubgriddedAxis() || repeater_track_sizes.empty());
  if (!IsSubgriddedAxis() &&
      (repeat_count == 0u || repeater_track_sizes.empty())) {
    return false;
  }

  // If the repeater is auto or there isn't a repeater, the repeat_count should
  // be 1.
  DCHECK(repeat_type == GridTrackRepeater::RepeatType::kInteger ||
         repeat_count == 1u);

  // Ensure adding tracks will not overflow the total in this track list and
  // that there is only one auto repeater per track list. For subgrids,
  // track sizes are not supported, so use the number of lines specified.
  wtf_size_t repeat_size =
      IsSubgriddedAxis() ? repeat_number_of_lines : repeater_track_sizes.size();
  switch (repeat_type) {
    case GridTrackRepeater::RepeatType::kNoRepeat:
    case GridTrackRepeater::RepeatType::kInteger:
      if (repeat_size > AvailableTrackCount() / repeat_count) {
        return false;
      }
      // Don't increment `track_count_without_auto_repeat_` for subgridded
      // axis. This is used to determine how many tracks are defined for
      // placement, but this doesn't apply for subgrid, as it is based entirely
      // on the subgrid span size, which should be used instead.
      if (!IsSubgriddedAxis()) {
        track_count_without_auto_repeat_ += repeat_size * repeat_count;
      }
      break;
    case GridTrackRepeater::RepeatType::kAutoFill:
    case GridTrackRepeater::RepeatType::kAutoFit:  // Intentional Fallthrough.
      track_count_before_auto_repeat_ = track_count_without_auto_repeat_;
      has_intrinsic_sized_repeater_ =
          std::find_if(repeater_track_sizes.begin(), repeater_track_sizes.end(),
                       [](const GridTrackSize& track_size) {
                         return track_size.IsTrackDefinitionIntrinsic();
                       }) != repeater_track_sizes.end();
      if (HasAutoRepeater() || repeat_size > AvailableTrackCount()) {
        return false;
      }
      // Update auto repeater index and append repeater.
      auto_repeater_index_ = repeaters_.size();
      break;
  }

  repeaters_.emplace_back(repeater_track_sizes_.size(), repeat_size,
                          repeat_count, repeat_type);
  if (!IsSubgriddedAxis()) {
    repeater_track_sizes_.AppendVector(repeater_track_sizes);
  }
  return true;
}

String GridTrackList::ToString() const {
  StringBuilder builder;
  builder.Append("TrackList: { ");
  builder.AppendRange(repeaters_, ",  ",
                      [](const auto& repeater) { return repeater.ToString(); });
  builder.Append(" } ");
  return builder.ToString();
}

bool GridTrackList::HasAutoRepeater() const {
  return auto_repeater_index_ != kNotFound;
}

bool GridTrackList::IsSubgriddedAxis() const {
  return axis_type_ == GridAxisType::kSubgriddedAxis;
}

void GridTrackList::SetAxisType(GridAxisType axis_type) {
  axis_type_ = axis_type;
}

wtf_size_t GridTrackList::AvailableTrackCount() const {
  return kNotFound - 1 - track_count_without_auto_repeat_;
}

void GridTrackList::operator=(const GridTrackList& other) {
  repeaters_ = other.repeaters_;
  repeater_track_sizes_ = other.repeater_track_sizes_;
  auto_repeater_index_ = other.auto_repeater_index_;
  track_count_without_auto_repeat_ = other.track_count_without_auto_repeat_;
  track_count_before_auto_repeat_ = other.track_count_before_auto_repeat_;
  non_auto_repeat_line_count_ = other.non_auto_repeat_line_count_;
  axis_type_ = other.axis_type_;
  has_intrinsic_sized_repeater_ = other.has_intrinsic_sized_repeater_;
}

bool GridTrackList::operator==(const GridTrackList& other) const {
  return TrackCountWithoutAutoRepeat() == other.TrackCountWithoutAutoRepeat() &&
         RepeaterCount() == other.RepeaterCount() &&
         auto_repeater_index_ == other.auto_repeater_index_ &&
         repeaters_ == other.repeaters_ &&
         repeater_track_sizes_ == other.repeater_track_sizes_ &&
         non_auto_repeat_line_count_ == other.non_auto_repeat_line_count_ &&
         axis_type_ == other.axis_type_ &&
         has_intrinsic_sized_repeater_ == other.has_intrinsic_sized_repeater_;
}

}  // namespace blink
