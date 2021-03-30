// Copyright 2020 The Chromium Authors. All rights reserved.
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

// Stores tracks related data by compressing repeated tracks into a single node.
struct NGGridTrackRepeater {
  enum RepeatType {
    kNoAutoRepeat,
    kAutoFill,
    kAutoFit,
  };
  NGGridTrackRepeater(wtf_size_t repeat_index,
                      wtf_size_t repeat_size,
                      wtf_size_t repeat_count,
                      RepeatType repeat_type);
  String ToString() const;
  bool operator==(const NGGridTrackRepeater& o) const;

  // |NGGridTrackList| will store the sizes for each track in this repeater
  // consecutively in a single vector for all repeaters; this index specifies
  // the position of the first track size that belongs to this repeater.
  wtf_size_t repeat_index;
  // Amount of tracks to be repeated.
  wtf_size_t repeat_size;
  // Amount of times the group of tracks are repeated.
  wtf_size_t repeat_count;
  // Type of repetition.
  RepeatType repeat_type;
};

class CORE_EXPORT NGGridTrackList {
 public:
  NGGridTrackList() = default;
  NGGridTrackList(const NGGridTrackList& other) = default;

  // Returns the repeat count of the repeater at |index|, or |auto_value|
  // if the repeater is auto.
  wtf_size_t RepeatCount(const wtf_size_t index,
                         const wtf_size_t auto_value) const;
  // Returns the number of tracks in the repeater at |index|.
  wtf_size_t RepeatSize(const wtf_size_t index) const;
  // Returns the repeat type of the repeater at |index|.
  NGGridTrackRepeater::RepeatType RepeatType(const wtf_size_t index) const;
  // Returns the size of the |n|-th specified track of the repeater at |index|.
  const GridTrackSize& RepeatTrackSize(const wtf_size_t index,
                                       const wtf_size_t n) const;

  // Returns the count of repeaters.
  wtf_size_t RepeaterCount() const;
  // Returns the total count of all the tracks in this list.
  wtf_size_t TotalTrackCount() const;
  // Returns the number of tracks in the auto repeater, or 0 if there is none.
  wtf_size_t AutoRepeatSize() const;

  // Adds a non-auto repeater.
  bool AddRepeater(const Vector<GridTrackSize>& repeater_track_sizes,
                   wtf_size_t repeat_count);
  // Adds an auto repeater.
  bool AddAutoRepeater(const Vector<GridTrackSize>& repeater_track_sizes,
                       NGGridTrackRepeater::RepeatType repeat_type);
  // Returns true if this list contains an auto repeater.
  bool HasAutoRepeater() const;

  // Clears all data.
  void Clear();

  String ToString() const;

  void operator=(const NGGridTrackList& o);
  bool operator==(const NGGridTrackList& o) const;

 private:
  bool AddRepeater(const Vector<GridTrackSize>& repeater_track_sizes,
                   NGGridTrackRepeater::RepeatType repeat_type,
                   wtf_size_t repeat_count);
  // Returns the amount of tracks available before overflow.
  wtf_size_t AvailableTrackCount() const;

  Vector<NGGridTrackRepeater> repeaters_;

  // Stores the track sizes of every repeater added to this list; tracks from
  // the same repeater group are stored consecutively.
  Vector<GridTrackSize> repeater_track_sizes_;

  // The index of the automatic repeater, if there is one; |kInvalidRangeIndex|
  // otherwise.
  wtf_size_t auto_repeater_index_ = kNotFound;
  // Total count of tracks.
  wtf_size_t total_track_count_ = 0;
};

// This class wraps both legacy grid track list type, and the GridNG version:
// Vector<GridTrackSize>, and NGGridTrackList respectively. The NGGridTrackList
// is stored in a pointer to keep size down when not running GridNG.
class GridTrackList {
  DISALLOW_NEW();

 public:
  GridTrackList();

  GridTrackList(const GridTrackList& other);
  explicit GridTrackList(const GridTrackSize& default_track_size);
  explicit GridTrackList(Vector<GridTrackSize>& legacy_tracks);

  Vector<GridTrackSize>& LegacyTrackList();
  const Vector<GridTrackSize>& LegacyTrackList() const;

  NGGridTrackList& NGTrackList();
  const NGGridTrackList& NGTrackList() const;

  void operator=(const GridTrackList& other);
  bool operator==(const GridTrackList& other) const;
  bool operator!=(const GridTrackList& other) const;

 private:
  void AssignFrom(const GridTrackList& other);
  Vector<GridTrackSize> legacy_track_list_;
  std::unique_ptr<NGGridTrackList> ng_track_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GRID_TRACK_LIST_H_
