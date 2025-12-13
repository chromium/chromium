// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_GRID_TRACK_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_GRID_TRACK_LIST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/grid_track_list.h"
#include "third_party/blink/renderer/core/style/grid_track_size.h"
#include "third_party/blink/renderer/core/style/named_grid_lines_map.h"
#include "third_party/blink/renderer/core/style/ordered_named_grid_lines.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT ComputedGridTrackList
    : public GarbageCollected<ComputedGridTrackList> {
 public:
  ComputedGridTrackList() = default;

  explicit ComputedGridTrackList(const GridTrackList& list,
                                 const AutoRepeatType type)
      : track_list_(list), auto_repeat_type_(type) {}

  bool operator==(const ComputedGridTrackList& other) const {
    return track_list_ == other.track_list_ &&
           named_grid_lines_ == other.named_grid_lines_ &&
           auto_repeat_named_grid_lines_ ==
               other.auto_repeat_named_grid_lines_ &&
           ordered_named_grid_lines_ == other.ordered_named_grid_lines_ &&
           auto_repeat_ordered_named_grid_lines_ ==
               other.auto_repeat_ordered_named_grid_lines_ &&
           auto_repeat_insertion_point_ == other.auto_repeat_insertion_point_ &&
           auto_repeat_type_ == other.auto_repeat_type_ &&
           axis_type_ == other.axis_type_;
  }

  bool IsSubgriddedAxis() const {
    return axis_type_ == GridAxisType::kSubgriddedAxis;
  }

  const NamedGridLinesMap& GetNamedGridLines() const {
    return named_grid_lines_;
  }
  NamedGridLinesMap& GetMutableNamedGridLines() { return named_grid_lines_; }
  const NamedGridLinesMap& SetNamedGridLines() const {
    return named_grid_lines_;
  }
  void SetNamedGridLines(const NamedGridLinesMap& lines) {
    named_grid_lines_ = lines;
  }

  const NamedGridLinesMap& GetAutoRepeatNamedGridLines() const {
    return auto_repeat_named_grid_lines_;
  }
  NamedGridLinesMap& GetMutableAutoRepeatNamedGridLines() {
    return auto_repeat_named_grid_lines_;
  }

  const OrderedNamedGridLines& GetOrderedNamedGridLines() const {
    return ordered_named_grid_lines_;
  }
  OrderedNamedGridLines& GetMutableOrderedNamedGridLines() {
    return ordered_named_grid_lines_;
  }
  void SetOrderedNamedGridLines(const OrderedNamedGridLines& lines) {
    ordered_named_grid_lines_ = lines;
  }

  const OrderedNamedGridLines& GetOrderedAutoRepeatNamedGridLines() const {
    return auto_repeat_ordered_named_grid_lines_;
  }
  OrderedNamedGridLines& GetMutableOrderedAutoRepeatNamedGridLines() {
    return auto_repeat_ordered_named_grid_lines_;
  }

  const GridTrackList& GetTrackList() const { return track_list_; }
  GridTrackList& GetMutableTrackList() { return track_list_; }
  void SetTrackList(const GridTrackList& list) { track_list_ = list; }

  wtf_size_t GetAutoRepeatInsertionPoint() const {
    return auto_repeat_insertion_point_;
  }
  void SetAutoRepeatInsertionPoint(wtf_size_t point) {
    auto_repeat_insertion_point_ = point;
  }

  AutoRepeatType GetAutoRepeatType() const { return auto_repeat_type_; }
  void SetAutoRepeatType(AutoRepeatType type) { auto_repeat_type_ = type; }

  GridAxisType GetGridAxisType() const { return axis_type_; }
  void SetGridAxisType(GridAxisType type) { axis_type_ = type; }

  void Trace(Visitor* visitor) const {}

 private:
  GridTrackList track_list_;

  NamedGridLinesMap named_grid_lines_;
  NamedGridLinesMap auto_repeat_named_grid_lines_;
  OrderedNamedGridLines ordered_named_grid_lines_;
  OrderedNamedGridLines auto_repeat_ordered_named_grid_lines_;

  wtf_size_t auto_repeat_insertion_point_{0};
  AutoRepeatType auto_repeat_type_{AutoRepeatType::kNoAutoRepeat};
  GridAxisType axis_type_{GridAxisType::kStandaloneAxis};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_COMPUTED_GRID_TRACK_LIST_H_
