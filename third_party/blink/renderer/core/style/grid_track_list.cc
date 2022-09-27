// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/grid_track_list.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
namespace blink {

NGGridTrackRepeater::NGGridTrackRepeater(wtf_size_t repeat_index,
                                         wtf_size_t repeat_size,
                                         wtf_size_t repeat_count,
                                         RepeatType repeat_type)
    : repeat_index(repeat_index),
      repeat_size(repeat_size),
      repeat_count(repeat_count),
      repeat_type(repeat_type) {}

String NGGridTrackRepeater::ToString() const {
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

bool NGGridTrackRepeater::operator==(const NGGridTrackRepeater& other) const {
  return repeat_index == other.repeat_index &&
         repeat_size == other.repeat_size &&
         repeat_count == other.repeat_count && repeat_type == other.repeat_type;
}

wtf_size_t NGGridTrackList::RepeatCount(const wtf_size_t index,
                                        const wtf_size_t auto_value) const {
  DCHECK_LT(index, RepeaterCount());
  if (index == auto_repeater_index_)
    return auto_value;
  return repeaters_[index].repeat_count;
}

wtf_size_t NGGridTrackList::RepeatIndex(const wtf_size_t index) const {
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_index;
}

wtf_size_t NGGridTrackList::RepeatSize(const wtf_size_t index) const {
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_size;
}

NGGridTrackRepeater::RepeatType NGGridTrackList::RepeatType(
    const wtf_size_t index) const {
  DCHECK_LT(index, RepeaterCount());
  return repeaters_[index].repeat_type;
}

const GridTrackSize& NGGridTrackList::RepeatTrackSize(
    const wtf_size_t index,
    const wtf_size_t n) const {
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

bool NGGridTrackList::AddRepeater(
    const Vector<GridTrackSize, 1>& repeater_track_sizes,
    NGGridTrackRepeater::RepeatType repeat_type,
    wtf_size_t repeat_count) {
  if (repeat_count == 0u || repeater_track_sizes.empty())
    return false;

  // If the repeater is auto or there isn't a repeater, the repeat_count should
  // be 1.
  DCHECK(repeat_type == NGGridTrackRepeater::RepeatType::kInteger ||
         repeat_count == 1u);

  // Ensure adding tracks will not overflow the total in this track list and
  // that there is only one auto repeater per track list.
  wtf_size_t repeat_size = repeater_track_sizes.size();
  switch (repeat_type) {
    case NGGridTrackRepeater::RepeatType::kNoRepeat:
    case NGGridTrackRepeater::RepeatType::kInteger:
      if (repeat_size > AvailableTrackCount() / repeat_count)
        return false;
      track_count_without_auto_repeat_ += repeat_size * repeat_count;
      break;
    case NGGridTrackRepeater::RepeatType::kAutoFill:
    case NGGridTrackRepeater::RepeatType::kAutoFit:  // Intentional Fallthrough.
      if (HasAutoRepeater() || repeat_size > AvailableTrackCount())
        return false;
      // Update auto repeater index and append repeater.
      auto_repeater_index_ = repeaters_.size();
      break;
  }

  repeaters_.emplace_back(repeater_track_sizes_.size(), repeat_size,
                          repeat_count, repeat_type);
  repeater_track_sizes_.AppendVector(repeater_track_sizes);
  return true;
}

String NGGridTrackList::ToString() const {
  StringBuilder builder;
  builder.Append("TrackList: {");
  for (wtf_size_t i = 0; i < repeaters_.size(); ++i) {
    builder.Append(" ");
    builder.Append(repeaters_[i].ToString());
    if (i + 1 != repeaters_.size())
      builder.Append(", ");
  }
  builder.Append(" } ");
  return builder.ToString();
}

bool NGGridTrackList::HasAutoRepeater() const {
  return auto_repeater_index_ != kNotFound;
}

wtf_size_t NGGridTrackList::AvailableTrackCount() const {
  return kNotFound - 1 - track_count_without_auto_repeat_;
}

void NGGridTrackList::operator=(const NGGridTrackList& other) {
  repeaters_ = other.repeaters_;
  repeater_track_sizes_ = other.repeater_track_sizes_;
  auto_repeater_index_ = other.auto_repeater_index_;
  track_count_without_auto_repeat_ = other.track_count_without_auto_repeat_;
}

bool NGGridTrackList::operator==(const NGGridTrackList& other) const {
  return TrackCountWithoutAutoRepeat() == other.TrackCountWithoutAutoRepeat() &&
         RepeaterCount() == other.RepeaterCount() &&
         auto_repeater_index_ == other.auto_repeater_index_ &&
         repeaters_ == other.repeaters_ &&
         repeater_track_sizes_ == other.repeater_track_sizes_;
}

GridTrackList::GridTrackList(const GridTrackList& other) {
  AssignFrom(other);
}

GridTrackList::GridTrackList(const GridTrackSize& default_track_size) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    ng_track_list_.AddRepeater({default_track_size});

  legacy_track_list_.push_back(default_track_size);
}

GridTrackList::GridTrackList(Vector<GridTrackSize, 1>& legacy_tracks)
    : legacy_track_list_(std::move(legacy_tracks)) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    ng_track_list_.AddRepeater(legacy_track_list_);
}

Vector<GridTrackSize, 1>& GridTrackList::LegacyTrackList() {
  return legacy_track_list_;
}

const Vector<GridTrackSize, 1>& GridTrackList::LegacyTrackList() const {
  return legacy_track_list_;
}

NGGridTrackList& GridTrackList::NGTrackList() {
  DCHECK(RuntimeEnabledFeatures::LayoutNGEnabled());
  return ng_track_list_;
}
const NGGridTrackList& GridTrackList::NGTrackList() const {
  DCHECK(RuntimeEnabledFeatures::LayoutNGEnabled());
  return ng_track_list_;
}

void GridTrackList::SetNGGridTrackList(const NGGridTrackList& other) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGEnabled());
  ng_track_list_ = other;
}

void GridTrackList::operator=(const GridTrackList& other) {
  AssignFrom(other);
}

bool GridTrackList::operator==(const GridTrackList& other) const {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    return ng_track_list_ == other.ng_track_list_;

  return LegacyTrackList() == other.LegacyTrackList();
}

bool GridTrackList::operator!=(const GridTrackList& other) const {
  return !(*this == other);
}

void GridTrackList::AssignFrom(const GridTrackList& other) {
  if (RuntimeEnabledFeatures::LayoutNGEnabled())
    ng_track_list_ = other.ng_track_list_;

  legacy_track_list_ = other.legacy_track_list_;
}

}  // namespace blink
