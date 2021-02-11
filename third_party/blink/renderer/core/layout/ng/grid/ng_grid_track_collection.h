// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_TRACK_COLLECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_TRACK_COLLECTION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// NGGridTrackCollectionBase provides an implementation for some shared
// functionality on track range collections, specifically binary search on
// the collection to get a range index given a track number.
class CORE_EXPORT NGGridTrackCollectionBase {
 public:
  static constexpr wtf_size_t kInvalidRangeIndex = kNotFound;

  class CORE_EXPORT RangeRepeatIterator {
   public:
    RangeRepeatIterator(const NGGridTrackCollectionBase* collection,
                        wtf_size_t range_index);

    bool IsAtEnd() const;
    // Moves iterator to next range, skipping over repeats in a range. Return
    // true if the move was successful.
    bool MoveToNextRange();

    wtf_size_t RepeatCount() const;
    // Returns the index of this range in the collection.
    wtf_size_t RangeIndex() const;
    // Returns the track number for the start of the range.
    wtf_size_t RangeTrackStart() const;
    // Returns the track number at the end of the range.
    wtf_size_t RangeTrackEnd() const;
    bool IsRangeCollapsed() const;

   private:
    bool SetRangeIndex(wtf_size_t range_index);
    const NGGridTrackCollectionBase* collection_;
    wtf_size_t range_index_;
    wtf_size_t range_count_;

    // First track number of a range.
    wtf_size_t range_track_start_;
    // Count of repeated tracks in a range.
    wtf_size_t range_track_count_;
  };

  // Gets the range index for the range that contains the given track number.
  wtf_size_t RangeIndexFromTrackNumber(wtf_size_t track_number) const;

  RangeRepeatIterator RangeIterator() const;
  String ToString() const;

 protected:
  // Returns the first track number of a range.
  virtual wtf_size_t RangeTrackNumber(wtf_size_t range_index) const = 0;
  // Returns the number of tracks in a range.
  virtual wtf_size_t RangeTrackCount(wtf_size_t range_index) const = 0;
  // Returns true if the range at the given index is collapsed.
  virtual bool IsRangeCollapsed(wtf_size_t range_index) const = 0;
  // Returns the number of track ranges in the collection.
  virtual wtf_size_t RangeCount() const = 0;
};

struct CORE_EXPORT TrackSpanProperties {
 public:
  enum PropertyId : unsigned {
    kNone = 0,
    kHasIntrinsicTrack = 1 << 0,
    kHasFlexibleTrack = 1 << 1,
    kHasAutoMinimumTrack = 1 << 2,
    kIsCollapsed = 1 << 3,
    kIsImplicit = 1 << 4
  };

  inline bool HasProperty(PropertyId id) const { return bitmask_ & id; }
  inline void SetProperty(PropertyId id) { bitmask_ |= id; }

 private:
  uint8_t bitmask_{kNone};
};

class CORE_EXPORT NGGridBlockTrackCollection
    : public NGGridTrackCollectionBase {
 public:
  struct Range {
    bool IsImplicit() const;
    bool IsCollapsed() const;

    void SetIsImplicit();
    void SetIsCollapsed();

    wtf_size_t starting_track_number;
    wtf_size_t track_count;
    wtf_size_t repeater_index;
    wtf_size_t repeater_offset;
    TrackSpanProperties properties;
  };

  explicit NGGridBlockTrackCollection(
      GridTrackSizingDirection track_direction = kForColumns);

  // Sets the specified, implicit tracks, along with a given auto repeat value.
  void SetSpecifiedTracks(const NGGridTrackList* explicit_tracks,
                          const NGGridTrackList* implicit_tracks,
                          wtf_size_t auto_repeat_count);
  // Ensures that after FinalizeRanges is called, a range will start at the
  // |track_number|, and a range will end at |track_number| + |span_length|
  void EnsureTrackCoverage(wtf_size_t track_number, wtf_size_t span_length);
  // Build the collection of ranges based on information provided by
  // SetSpecifiedTracks and EnsureTrackCoverage.
  void FinalizeRanges();

  bool IsRangeImplicit(wtf_size_t range_index) const;
  const Range& RangeAtRangeIndex(wtf_size_t range_index) const;

  GridTrackSizingDirection Direction() const { return direction_; }
  bool IsForColumns() const { return direction_ == kForColumns; }

  const NGGridTrackList& ExplicitTracks() const;
  const NGGridTrackList& ImplicitTracks() const;
  String ToString() const;

 protected:
  // NGGridTrackCollectionBase overrides.
  wtf_size_t RangeTrackNumber(wtf_size_t range_index) const override;
  wtf_size_t RangeTrackCount(wtf_size_t range_index) const override;
  bool IsRangeCollapsed(wtf_size_t range_index) const override;
  wtf_size_t RangeCount() const override;

 private:
  // Returns true if this collection had implicit tracks provided.
  bool HasImplicitTracks() const;
  // Returns the repeat size of the implicit tracks.
  wtf_size_t ImplicitRepeatSize() const;

  bool track_indices_need_sort_ = false;
  GridTrackSizingDirection direction_;
  wtf_size_t auto_repeat_count_ = 0;

  // Stores the specified and implicit tracks specified by SetSpecifiedTracks.
  const NGGridTrackList* explicit_tracks_;
  const NGGridTrackList* implicit_tracks_;

  // Starting and ending tracks mark where ranges will start and end.
  // Once the ranges have been built in FinalizeRanges, these are cleared.
  Vector<wtf_size_t> starting_tracks_;
  Vector<wtf_size_t> ending_tracks_;
  Vector<Range> ranges_;
};

// |NGGridBlockTrackCollection::EnsureTrackCoverage| may introduce a range start
// and/or end at the middle of any repeater from the block collection. This will
// affect how some repeated tracks within the same repeater group resolve their
// track sizes; e.g. consider the track list 'repeat(10, auto)' with a grid item
// spanning from the 3rd to the 7th track in the repeater, every track within
// the item's range will grow to fit the content of that item first.
//
// For the track sizing algorithm we want to have separate data (e.g. base size,
// growth limit, etc.) between tracks in different ranges; instead of trivially
// expanding the repeaters, which will limit our implementation to support
// relatively small track counts, we introduce the concept of a "set".
//
// A "set" is a collection of distinct track definitions that compose a range in
// |NGGridLayoutAlgorithmTrackCollection|; each set element stores the number of
// tracks within the range that share its definition. The |NGGridSet| class
// represents a single element from a set.
//
// As an example, consider the following grid definition:
//   - 'grid-template-columns: repeat(4, 5px 1fr)'
//   - Grid item 1 with 'grid-column: 1 / span 5'
//   - Grid item 2 with 'grid-column: 2 / span 1'
//   - Grid item 3 with 'grid-column: 6 / span 8'
//
// Expanding the track definitions above we would look at the explicit grid:
//   | 5px | 1fr | 5px | 1fr | 5px | 1fr | 5px | 1fr |
//
// This example would produce the following ranges and their respective sets:
//   Range 1:  [1-1], Set 1: {  5px (1) }
//   Range 2:  [2-2], Set 2: {  1fr (1) }
//   Range 3:  [3-5], Set 3: {  5px (2) , 1fr (1) }
//   Range 4:  [6-8], Set 4: {  1fr (2) , 5px (1) }
//   Range 5: [9-13], Set 5: { auto (5) }
//
// Note that, since |NGGridBlockTrackCollection|'s ranges are assured to span a
// single repeater and to not cross any grid item's boundary in the respective
// dimension, tracks within a set are "commutative" and can be sized evenly.
class CORE_EXPORT NGGridSet {
 public:
  NGGridSet(wtf_size_t track_count, bool is_collapsed);
  // |is_content_box_size_indefinite| is used to normalize percentage track
  // sizing functions; from https://drafts.csswg.org/css-grid-2/#track-sizes:
  //   "If the size of the grid container depends on the size of its tracks,
  //   then the <percentage> must be treated as 'auto'".
  NGGridSet(wtf_size_t track_count,
            const GridTrackSize& track_size,
            bool is_content_box_size_indefinite);

  wtf_size_t TrackCount() const { return track_count_; }
  const GridTrackSize& TrackSize() const { return track_size_; }

  LayoutUnit BaseSize() const;
  LayoutUnit GrowthLimit() const;
  LayoutUnit PlannedIncrease() const { return planned_increase_; }
  LayoutUnit FitContentLimit() const { return fit_content_limit_; }
  LayoutUnit ItemIncurredIncrease() const { return item_incurred_increase_; }
  bool IsInfinitelyGrowable() const { return is_infinitely_growable_; }

  void SetBaseSize(LayoutUnit base_size);
  void SetGrowthLimit(LayoutUnit growth_limit);
  void SetPlannedIncrease(LayoutUnit planned_increase) {
    planned_increase_ = planned_increase;
  }
  void SetFitContentLimit(LayoutUnit fit_content_limit) {
    fit_content_limit_ = fit_content_limit;
  }
  void SetItemIncurredIncrease(LayoutUnit item_incurred_increase) {
    item_incurred_increase_ = item_incurred_increase;
  }
  void SetInfinitelyGrowable(bool infinitely_growable) {
    is_infinitely_growable_ = infinitely_growable;
  }

 private:
  bool IsGrowthLimitLessThanBaseSize() const;
  void EnsureGrowthLimitIsNotLessThanBaseSize();

  wtf_size_t track_count_;
  GridTrackSize track_size_;

  // Fields used by the track sizing algorithm.
  LayoutUnit base_size_;
  LayoutUnit growth_limit_;
  LayoutUnit planned_increase_;
  LayoutUnit fit_content_limit_;
  LayoutUnit item_incurred_increase_;
  bool is_infinitely_growable_ : 1;
};

class CORE_EXPORT NGGridLayoutAlgorithmTrackCollection
    : public NGGridTrackCollectionBase {
 public:
  struct Range {
    // Copies fields that are the same as in |GridBlockTrackCollection::Range|.
    Range(const NGGridBlockTrackCollection::Range& block_track_range,
          wtf_size_t starting_set_index);

    bool IsCollapsed() const;

    wtf_size_t starting_track_number;
    wtf_size_t track_count;
    wtf_size_t starting_set_index;
    wtf_size_t set_count;
    TrackSpanProperties properties;
  };

  template <bool is_const>
  class CORE_EXPORT SetIteratorBase {
   public:
    using TrackCollectionPtr =
        typename std::conditional<is_const,
                                  const NGGridLayoutAlgorithmTrackCollection*,
                                  NGGridLayoutAlgorithmTrackCollection*>::type;
    using NGGridSetRef =
        typename std::conditional<is_const, const NGGridSet&, NGGridSet&>::type;

    SetIteratorBase(TrackCollectionPtr track_collection,
                    wtf_size_t begin_set_index,
                    wtf_size_t end_set_index)
        : track_collection_(track_collection),
          current_set_index_(begin_set_index),
          end_set_index_(end_set_index) {
      DCHECK(track_collection_);
      DCHECK_LE(current_set_index_, end_set_index_);
      DCHECK_LE(end_set_index_, track_collection_->SetCount());
    }

    bool IsAtEnd() const {
      DCHECK_LE(current_set_index_, end_set_index_);
      return current_set_index_ == end_set_index_;
    }

    bool MoveToNextSet() {
      current_set_index_ = std::min(current_set_index_ + 1, end_set_index_);
      return current_set_index_ < end_set_index_;
    }

    NGGridSetRef CurrentSet() const {
      DCHECK_LT(current_set_index_, end_set_index_);
      return track_collection_->SetAt(current_set_index_);
    }

   private:
    TrackCollectionPtr track_collection_;
    wtf_size_t current_set_index_;
    wtf_size_t end_set_index_;
  };

  typedef SetIteratorBase<false> SetIterator;
  typedef SetIteratorBase<true> ConstSetIterator;

  NGGridLayoutAlgorithmTrackCollection() = default;
  // |is_content_box_size_indefinite| is used to normalize percentage track
  // sizing functions (see the constructor for |NGGridSet|).
  NGGridLayoutAlgorithmTrackCollection(
      const NGGridBlockTrackCollection& block_track_collection,
      bool is_content_box_size_indefinite);

  wtf_size_t EndLineOfImplicitGrid() const;
  bool IsGridLineWithinImplicitGrid(wtf_size_t grid_line) const;

  // Returns the number of sets in the collection.
  wtf_size_t SetCount() const;
  // Returns a reference to the set located at position |set_index|.
  NGGridSet& SetAt(wtf_size_t set_index);
  const NGGridSet& SetAt(wtf_size_t set_index) const;
  // Returns an iterator for all the sets contained in this collection.
  SetIterator GetSetIterator();
  ConstSetIterator GetSetIterator() const;
  // Returns an iterator for every set in this collection's |sets_| located at
  // an index in the interval [begin_set_index, end_set_index).
  SetIterator GetSetIterator(wtf_size_t begin_set_index,
                             wtf_size_t end_set_index);
  ConstSetIterator GetSetIterator(wtf_size_t begin_set_index,
                                  wtf_size_t end_set_index) const;

  wtf_size_t RangeSetCount(wtf_size_t range_index) const;
  wtf_size_t RangeStartingSetIndex(wtf_size_t range_index) const;

  // Returns true if the specified property has been set in the track span
  // properties bitmask of the range at position |range_index|.
  bool RangeHasTrackSpanProperty(
      wtf_size_t range_index,
      TrackSpanProperties::PropertyId property_id) const;

  GridTrackSizingDirection Direction() const { return direction_; }
  bool IsForColumns() const { return direction_ == kForColumns; }

 protected:
  // NGGridTrackCollectionBase overrides.
  wtf_size_t RangeTrackNumber(wtf_size_t range_index) const override;
  wtf_size_t RangeTrackCount(wtf_size_t range_index) const override;
  bool IsRangeCollapsed(wtf_size_t range_index) const override;
  wtf_size_t RangeCount() const override;

 private:
  void AppendTrackRange(
      const NGGridBlockTrackCollection::Range& block_track_range,
      const NGGridTrackList& specified_track_list,
      bool is_content_box_size_indefinite);

  GridTrackSizingDirection direction_;

  Vector<Range> ranges_;
  // A vector of every set element that compose the entire collection's ranges;
  // track definitions from the same set are stored in consecutive positions,
  // preserving the order in which the definitions appear in their range.
  Vector<NGGridSet> sets_;
};

}  // namespace blink
WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(blink::NGGridTrackRepeater)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_GRID_NG_GRID_TRACK_COLLECTION_H_
