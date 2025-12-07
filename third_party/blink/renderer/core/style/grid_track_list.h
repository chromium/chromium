// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_TRACK_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_TRACK_LIST_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/grid_track_size.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

enum class AutoRepeatType : uint8_t { kNoAutoRepeat, kAutoFill, kAutoFit };
enum class GridAxisType : uint8_t { kStandaloneAxis, kSubgriddedAxis };

// Stores tracks related data by compressing repeated tracks into a single node.
struct GridTrackRepeater {
  enum RepeatType {
    kNoRepeat,
    kAutoFill,
    kAutoFit,
    kInteger,
  };
  GridTrackRepeater(wtf_size_t repeat_index,
                    wtf_size_t repeat_size,
                    wtf_size_t repeat_count,
                    RepeatType repeat_type);
  String ToString() const;
  bool operator==(const GridTrackRepeater& o) const;

  // `GridTrackList` will store the sizes for each track in this repeater
  // consecutively in a single vector for all repeaters; this index specifies
  // the position of the first track size that belongs to this repeater.
  wtf_size_t repeat_index;
  // Amount of tracks to be repeated. For standalone axis, this is the number of
  // track definitions. For subgrids, this is the indices containing named line
  // definitions (e.g. `repeat(auto-fit, [a b], [c d])` would have a size of 2).
  wtf_size_t repeat_size;
  // Amount of times the group of tracks are repeated.
  wtf_size_t repeat_count;
  // Type of repetition.
  RepeatType repeat_type;
};

class CORE_EXPORT GridTrackList {
 public:
  GridTrackList() = default;
  GridTrackList(const GridTrackList& other) = default;
  explicit GridTrackList(const GridTrackSize& default_track_size) {
    AddRepeater({default_track_size});
  }
  explicit GridTrackList(const GridTrackSize& default_track_size,
                         const GridTrackRepeater::RepeatType repeat_type) {
    AddRepeater({default_track_size}, repeat_type);
  }

  // Returns the repeat count of the repeater at `index`, or `auto_value`
  // if the repeater is auto.
  wtf_size_t RepeatCount(wtf_size_t index, wtf_size_t auto_value) const;
  // Returns the position of the first track size in the repeater at `index`.
  wtf_size_t RepeatIndex(wtf_size_t index) const;
  // Returns the number of tracks in the repeater at `index`.
  wtf_size_t RepeatSize(wtf_size_t index) const;
  // Returns the repeat type of the repeater at `index`.
  GridTrackRepeater::RepeatType RepeatType(wtf_size_t index) const;
  // Returns the size of the `n`-th specified track of the repeater at `index`.
  const GridTrackSize& RepeatTrackSize(wtf_size_t index, wtf_size_t n) const;

  // Returns the count of repeaters.
  wtf_size_t RepeaterCount() const;
  // Returns the count of all tracks, ignoring those within an auto repeater.
  wtf_size_t TrackCountWithoutAutoRepeat() const;
  // Returns the count of tracks up to an auto repeater. If there is no auto
  // repeater, returns 0.
  wtf_size_t TrackCountBeforeAutoRepeat() const {
    return track_count_before_auto_repeat_;
  }
  // Returns the number of tracks in the auto repeater, or 0 if there is none.
  wtf_size_t AutoRepeatTrackCount() const;
  // Returns the start index of the auto repeater, or kNotFound if there is
  // none.
  wtf_size_t AutoRepeatTrackIndex() const { return auto_repeater_index_; }
  // Returns the count of line names not including auto repeaters. Note that
  // this is subtly different than `TrackCountWithoutAutoRepeat`, as it is
  // specifically line names (not sizes), and includes empty line names.
  wtf_size_t NonAutoRepeatLineCount() const;
  // Increments the count of line names not including auto repeaters.
  void IncrementNonAutoRepeatLineCount();
  // Adds a repeater.
  bool AddRepeater(const Vector<GridTrackSize, 1>& repeater_track_sizes,
                   GridTrackRepeater::RepeatType repeat_type =
                       GridTrackRepeater::RepeatType::kNoRepeat,
                   wtf_size_t repeat_count = 1u,
                   wtf_size_t repeat_number_of_lines = 1u);
  // Returns true if this list contains an auto repeater.
  bool HasAutoRepeater() const;
  // Returns true if this is a subgridded track list.
  bool IsSubgriddedAxis() const;
  // Sets the axis type (standalone or subgrid).
  void SetAxisType(GridAxisType axis_type);

  // If true, this track list has a repeat definition with an intrinsic sized
  // track with an automatic number of repetitions.
  bool HasIntrinsicSizedRepeater() const {
    return has_intrinsic_sized_repeater_;
  }

  // Clears all data.
  void Clear();

  String ToString() const;

  void operator=(const GridTrackList& o);
  bool operator==(const GridTrackList& o) const;

 private:
  // Returns the amount of tracks available before overflow.
  wtf_size_t AvailableTrackCount() const;

  Vector<GridTrackRepeater, 1> repeaters_;

  // Stores the track sizes of every repeater added to this list; tracks from
  // the same repeater group are stored consecutively.
  Vector<GridTrackSize, 1> repeater_track_sizes_;

  // The index of the automatic repeater, if there is one; `kInvalidRangeIndex`
  // otherwise.
  wtf_size_t auto_repeater_index_{kNotFound};

  // Count of tracks ignoring those within an auto repeater.
  wtf_size_t track_count_without_auto_repeat_{0};

  // Count of tracks up to an auto repeater. If there is no auto repeater, it is
  // 0.
  wtf_size_t track_count_before_auto_repeat_{0};

  // Count of line names outside of auto-repeaters. This is subtly different
  // than `track_count_without_auto_repeat_`, as that is track definitions,
  // while this tracks line names (including empty lines).
  wtf_size_t non_auto_repeat_line_count_{0};

  // If true, this track list has a repeat definition with an intrinsic sized
  // track with an automatic number of repetitions.
  bool has_intrinsic_sized_repeater_{false};

  // The grid axis type (standalone or subgridded).
  GridAxisType axis_type_{GridAxisType::kStandaloneAxis};
};

}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::GridTrackRepeater)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_TRACK_LIST_H_
