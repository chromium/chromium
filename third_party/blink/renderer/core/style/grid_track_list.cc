// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/grid_track_list.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
namespace blink {

NGGridTrackRepeater::NGGridTrackRepeater(wtf_size_t repeat_index,
                                         wtf_size_t repeat_size,
                                         wtf_size_t repeat_count,
                                         wtf_size_t line_name_indices_count,
                                         RepeatType repeat_type)
    : repeat_index(repeat_index),
      repeat_size(repeat_size),
      repeat_count(repeat_count),
      line_name_indices_count(line_name_indices_count),
      repeat_type(repeat_type) {}

String NGGridTrackRepeater::ToString() const {
  StringBuilder builder;
  builder.Append("Repeater: [Index: ");
  builder.AppendNumber<wtf_size_t>(repeat_index);
  builder.Append("], [RepeatSize: ");
  builder.AppendNumber<wtf_size_t>(repeat_size);
  builder.Append("], [LineNameIndicesCount: ");
  builder.AppendNumber<wtf_size_t>(line_name_indices_count);
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

bool NGGridTrackRepeater::operator==(const NGGridTrackRepeater& other) const {
  return repeat_index == other.repeat_index &&
         repeat_size == other.repeat_size &&
         repeat_count == other.repeat_count && repeat_type == other.repeat_type;
}

wtf_size_t NGGridTrackList::RepeatCount(wtf_size_t index,
                                        wtf_size_t auto_value) const {
  DCHECK_LT(index, RepeaterCount());
  if (index == auto_repeater_index_) {
    return auto_value;
  }
  return repeaters_[index].repeat_count;
}

wtf_size_t NGGridTrackList::RepeatIndex(wtf_size_t index) const {
  // `repeat_index` is used for sizes, which subgrids don't have.
  DCHECK(!IsSubgriddedAxis());
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_index;
}

wtf_size_t NGGridTrackList::RepeatSize(wtf_size_t index) const {
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_size;
}

wtf_size_t NGGridTrackList::LineNameIndicesCount(wtf_size_t index) const {
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].line_name_indices_count;
}

NGGridTrackRepeater::RepeatType NGGridTrackList::RepeatType(
    wtf_size_t index) const {
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_type;
}

const GridTrackSize& NGGridTrackList::RepeatTrackSize(wtf_size_t index,
                                                      wtf_size_t n) const {
  // Subgrids don't have track sizes associated with them.
  DCHECK(!IsSubgriddedAxis());
  DCHECK_LT(index, RepeaterCount());
  DCHECK_LT(n, RepeatSize(index));

  wtf_size_t repeat_index = repeaters_[index].repeat_index;
  DCHECK_LT(repeat_index + n, repeater_track_sizes_.size());
  return repeater_track_sizes_[repeat_index + n];
}

wtf_size_t NGGridTrackList::RepeaterCount() const {
  return repeaters_.size();
}

wtf_size_t NGGridTrackList::TrackCountWithoutAutoRepeat() const {
  return track_count_without_auto_repeat_;
}

wtf_size_t NGGridTrackList::AutoRepeatTrackCount() const {
  return HasAutoRepeater() ? repeaters_[auto_repeater_index_].repeat_size : 0;
}

wtf_size_t NGGridTrackList::NonAutoRepeatLineCount() const {
  DCHECK(IsSubgriddedAxis());
  return non_auto_repeat_line_count_;
}

void NGGridTrackList::IncrementNonAutoRepeatLineCount() {
  DCHECK(IsSubgriddedAxis());
  ++non_auto_repeat_line_count_;
}

bool NGGridTrackList::AddRepeater(
    const Vector<GridTrackSize, 1>& repeater_track_sizes,
    NGGridTrackRepeater::RepeatType repeat_type,
    wtf_size_t repeat_count,
    wtf_size_t repeat_number_of_lines,
    wtf_size_t line_name_indices_count) {
  // Non-subgrid repeaters always have sizes associated with them, while
  // subgrids repeaters never do, as sizes will come from the parent grid.
  DCHECK(!IsSubgriddedAxis() || repeater_track_sizes.empty());
  if (!IsSubgriddedAxis() &&
      (repeat_count == 0u || repeater_track_sizes.empty())) {
    return false;
  }

  // If the repeater is auto or there isn't a repeater, the repeat_count should
  // be 1.
  DCHECK(repeat_type == NGGridTrackRepeater::RepeatType::kInteger ||
         repeat_count == 1u);

  // Ensure adding tracks will not overflow the total in this track list and
  // that there is only one auto repeater per track list. For subgrids,
  // track sizes are not supported, so use the number of lines specified.
  wtf_size_t repeat_size =
      IsSubgriddedAxis() ? repeat_number_of_lines : repeater_track_sizes.size();
  switch (repeat_type) {
    case NGGridTrackRepeater::RepeatType::kNoRepeat:
    case NGGridTrackRepeater::RepeatType::kInteger:
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
    case NGGridTrackRepeater::RepeatType::kAutoFill:
    case NGGridTrackRepeater::RepeatType::kAutoFit:  // Intentional Fallthrough.
      if (HasAutoRepeater() || repeat_size > AvailableTrackCount()) {
        return false;
      }
      // Update auto repeater index and append repeater.
      auto_repeater_index_ = repeaters_.size();
      break;
  }

  repeaters_.emplace_back(repeater_track_sizes_.size(), repeat_size,
                          repeat_count, line_name_indices_count, repeat_type);
  if (!IsSubgriddedAxis()) {
    repeater_track_sizes_.AppendVector(repeater_track_sizes);
  }
  return true;
}

String NGGridTrackList::ToString() const {
  StringBuilder builder;
  builder.Append("TrackList: {");
  for (wtf_size_t i = 0; i < repeaters_.size(); ++i) {
    builder.Append(" ");
    builder.Append(repeaters_[i].ToString());
    if (i + 1 != repeaters_.size()) {
      builder.Append(", ");
    }
  }
  builder.Append(" } ");
  return builder.ToString();
}

bool NGGridTrackList::HasAutoRepeater() const {
  return auto_repeater_index_ != kNotFound;
}

bool NGGridTrackList::IsSubgriddedAxis() const {
  return axis_type_ == GridAxisType::kSubgriddedAxis;
}

void NGGridTrackList::SetAxisType(GridAxisType axis_type) {
  axis_type_ = axis_type;
}

wtf_size_t NGGridTrackList::AvailableTrackCount() const {
  return kNotFound - 1 - track_count_without_auto_repeat_;
}

void NGGridTrackList::operator=(const NGGridTrackList& other) {
  repeaters_ = other.repeaters_;
  repeater_track_sizes_ = other.repeater_track_sizes_;
  auto_repeater_index_ = other.auto_repeater_index_;
  track_count_without_auto_repeat_ = other.track_count_without_auto_repeat_;
  non_auto_repeat_line_count_ = other.non_auto_repeat_line_count_;
  axis_type_ = other.axis_type_;
}

bool NGGridTrackList::operator==(const NGGridTrackList& other) const {
  return TrackCountWithoutAutoRepeat() == other.TrackCountWithoutAutoRepeat() &&
         RepeaterCount() == other.RepeaterCount() &&
         auto_repeater_index_ == other.auto_repeater_index_ &&
         repeaters_ == other.repeaters_ &&
         repeater_track_sizes_ == other.repeater_track_sizes_ &&
         non_auto_repeat_line_count_ == other.non_auto_repeat_line_count_ &&
         axis_type_ == other.axis_type_;
}

}  // namespace blink
