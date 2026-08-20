// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_

#include <algorithm>

#include "base/memory/raw_ptr_exclusion.h"
#include "third_party/blink/renderer/core/style/gap_data.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace gap_data_list_internal {

// The Leading/Auto/Trailing region slot counts and the auto-repeater index
// for a `GapDataList`, given a known gap count. See `GapDataListIterator` for
// the region model.
struct RegionSlotCounts {
  wtf_size_t leading = 0;
  wtf_size_t auto_repeat = 0;
  wtf_size_t trailing = 0;
  wtf_size_t auto_idx = kNotFound;
  bool leading_has_integer_repeaters = false;
  bool trailing_has_integer_repeaters = false;
};

// Computes `RegionSlotCounts` for `gap_data_list`. Shared by
// `GapDataListIterator` (sequential access) and `GapDataListValueAccessor`
// (random access) so the two traversal strategies agree on region boundaries.
template <typename GapDataVector>
RegionSlotCounts ComputeRegionSlotCounts(const GapDataVector& gap_data_list,
                                         wtf_size_t gap_count) {
  RegionSlotCounts counts;

  for (wtf_size_t i = 0; i < gap_data_list.size(); ++i) {
    const auto& gap_data = gap_data_list[i];

    bool is_integer_repeater = false;
    if (gap_data.IsRepeaterData()) {
      if (gap_data.GetValueRepeater()->IsAutoRepeater()) {
        CHECK_EQ(counts.auto_idx, kNotFound);
        counts.auto_idx = i;
        continue;
      }
      is_integer_repeater = true;
    }
    const wtf_size_t gap_data_slot_count = gap_data.GetFixedSlotCount();

    if (counts.auto_idx == kNotFound) {
      counts.leading += gap_data_slot_count;
      counts.leading_has_integer_repeaters |= is_integer_repeater;
    } else {
      counts.trailing += gap_data_slot_count;
      counts.trailing_has_integer_repeaters |= is_integer_repeater;
    }
  }
  if (counts.auto_idx != kNotFound) {
    // Compute the number of slots allocated to the auto region. If the
    // combined slots from leading and trailing regions is greater than the
    // total gap count, the auto region slot count remains zero.
    wtf_size_t combined_slot_count = counts.leading + counts.trailing;
    if (combined_slot_count < gap_count) {
      counts.auto_repeat = gap_count - combined_slot_count;
    }
  }
  return counts;
}

}  // namespace gap_data_list_internal

// These are used to store gap decorations values in the order they are
// specified. These values can be an auto repeater, an integer repeater, or a
// single value. The value could be a color, style or width. See:
// https://drafts.csswg.org/css-gaps-1/#color-style-width
// TODO(crbug.com/357648037): Consider removing the template and instead having
// concrete subclasses
// for StyleColor, EBorderStyle, and int.
template <typename T>
class CORE_EXPORT GapDataList {
  DISALLOW_NEW();

  using VectorType = ValueRepeater<T>::VectorType;

 public:
  using GapDataVector = HeapVector<GapData<T>, 1>;
  GapDataList() = default;

  static GapDataList DefaultGapColorDataList() {
    return GapDataList(StyleColor::CurrentColor());
  }

  static GapDataList DefaultGapWidthDataList() {
    constexpr int kDefaultWidth = 3;
    return GapDataList(kDefaultWidth);
  }

  static GapDataList DefaultGapStyleDataList() {
    return GapDataList(EBorderStyle::kNone);
  }

  explicit GapDataList(GapDataVector&& gap_data_list)
      : gap_data_list_(gap_data_list) {
    CHECK(!gap_data_list_.empty());
  }

  explicit GapDataList(const T& value) {
    gap_data_list_.emplace_back(GapData<T>(value));
  }

  explicit GapDataList(wtf_size_t size) { gap_data_list_.reserve(size); }

  void AddGapData(const GapData<T>& gap_data) {
    gap_data_list_.push_back(gap_data);
  }

  void AddGapData(const Length& length) {
    gap_data_list_.emplace_back(GapData<int>(length.Pixels()));
  }

  void AddGapData(const StyleColor& color) {
    gap_data_list_.emplace_back(GapData<StyleColor>(color));
  }

  // TODO(javiercon): Specialize this for StyleColor, EBorderStyle, and int.
  String ToString() const {
    StringBuilder result;
    for (const auto& gap_data : gap_data_list_) {
      if (gap_data.IsRepeaterData()) {
        result << "Repeater: " << gap_data.GetValueRepeater()->RepeatCount()
               << ", ";
        for (const auto& value :
             gap_data.GetValueRepeater()->RepeatedValues()) {
          result << value << " ";
        }
      } else {
        result << "Value: " << gap_data.GetValue();
      }
      result << "; ";
    }
    return result.ReleaseString();
  }

  void Trace(Visitor* visitor) const { visitor->Trace(gap_data_list_); }

  const GapDataVector& GetGapDataList() const { return gap_data_list_; }

  bool HasSingleValue() const {
    return gap_data_list_.size() == 1 && !gap_data_list_[0].IsRepeaterData();
  }

  const T GetSingleValue() const {
    DCHECK(HasSingleValue());
    return gap_data_list_[0].GetValue();
  }

  const T GetLegacyValue() const { return gap_data_list_[0].GetValue(); }

  bool operator==(const GapDataList& o) const {
    return gap_data_list_ == o.gap_data_list_;
  }

 private:
  GapDataVector gap_data_list_;
};

// GapDataListIterator traverses a GapDataList without fully expanding repeater
// gap data. At paint time, the number of gaps is fixed. Using that information,
// the iterator segments the GapDataList into three logical regions based on the
// position of the auto-repeater: Leading, Auto and Trailing.
//
// Each region is assigned a slot count indicating how many gaps it contributes.
// The iterator uses internal state to walk through the list item-by-item,
// respecting repeat counts and repeated value sequences without constructing
// the expanded form.
template <typename T>
class CORE_EXPORT GapDataListIterator {
  DISALLOW_NEW();
  // Enum to represent three possible regions in the gap data list:
  // - kLeading: Fixed data before an auto-repeater.
  // - kAuto: Auto-repeating segment.
  // - kTrailing: Fixed data after an auto-repeater.
  enum GapDataListRegion { kLeading, kAuto, kTrailing };

 public:
  using GapDataVector = GapDataList<T>::GapDataVector;
  using GapData = GapData<T>;

  GapDataListIterator(const GapDataVector& gap_data_list, wtf_size_t gap_count)
      : gap_data_list_(gap_data_list),
        gap_count_(gap_count),
        counts_(gap_data_list_internal::ComputeRegionSlotCounts(gap_data_list,
                                                                gap_count)) {
    CHECK(!gap_data_list_.empty());

    if (counts_.auto_idx == 0) {
      // Here, the auto repeater is the first item, so start at kAuto region.
      region_ = kAuto;
      current_region_slots_remaining_ = counts_.auto_repeat;
      repeated_value_idx_ = 0;
      if (current_region_slots_remaining_ == 0) {
        TransitionToNextRegion();
      }
    } else {
      // Auto-repeater is not the first item, start at kLeading region.
      region_ = kLeading;
      current_region_slots_remaining_ = counts_.leading;
      list_idx_ = 0;
      InitNonAutoDataState();
    }
  }

  bool HasNext() const { return current_gap_index_ < gap_count_; }

  // Advances the cursor to just before `target_index`, hence subsequent
  // `Next()` call returns the data at `target_index`.
  void AdvanceUpTo(wtf_size_t target_index) {
    CHECK_LE(current_gap_index_, target_index);
    CHECK_LE(target_index, gap_count_);
    while (current_gap_index_ < target_index) {
      Next();
    }
  }

  T Next() {
    CHECK(HasNext());
    T value = GetData();

    --current_region_slots_remaining_;
    current_gap_index_++;

    // Either advance in the current region or move to the next region.
    if (current_region_slots_remaining_ > 0) {
      AdvanceWithinCurrentRegion();
    } else if (current_gap_index_ < gap_count_) {
      TransitionToNextRegion();
    }

    return value;
  }

 private:
  // Retrieves the current value based on the region and index.
  T GetData() const {
    const GapData& gap_data =
        gap_data_list_[region_ == kAuto ? counts_.auto_idx : list_idx_];
    return gap_data.IsRepeaterData()
               ? gap_data.GetValueRepeater()
                     ->RepeatedValues()[repeated_value_idx_]
               : gap_data.GetValue();
  }

  void AdvanceWithinCurrentRegion() {
    if (region_ == GapDataListRegion::kAuto) {
      AdvanceWithinAutoRegion();
    } else {
      AdvanceWithinNonAutoRegion();
    }
  }

  void AdvanceWithinNonAutoRegion() {
    repeated_value_idx_ += 1;

    const GapData& gap_data = gap_data_list_[list_idx_];

    // Determine how many repeated values are associated with this gap_data. If
    // it's not a repeater (i.e. regular gap data item), we treat it as having
    // one repeated value with a single repeat.
    wtf_size_t repeated_values_count =
        gap_data.IsRepeaterData()
            ? gap_data.GetValueRepeater()->RepeatedValues().size()
            : 1;

    // If we've processed all values for this gap_data:
    // - Reset `repeated_value_idx_` for the next repeat cycle.
    // - Decrement remaining repeat count.
    // - If no repeats remain, advance to the next item.
    if (repeated_value_idx_ == repeated_values_count) {
      repeated_value_idx_ = 0;
      repeats_left_ -= 1;
      if (repeats_left_ == 0) {
        list_idx_++;
        InitNonAutoDataState();
      }
    }
  }

  void AdvanceWithinAutoRegion() {
    CHECK_EQ(region_, kAuto);
    wtf_size_t repeated_auto_values_size = gap_data_list_[counts_.auto_idx]
                                               .GetValueRepeater()
                                               ->RepeatedValues()
                                               .size();
    repeated_value_idx_ = (repeated_value_idx_ + 1) % repeated_auto_values_size;
  }

  void TransitionToNextRegion() {
    switch (region_) {
      case kLeading:
        if (counts_.auto_idx == kNotFound) {
          // No auto-repeater, so cycle back to the leading.
          current_region_slots_remaining_ = counts_.leading;
          list_idx_ = 0;
          InitNonAutoDataState();
        } else {
          if (counts_.auto_repeat > 0) {
            // Move from leading to the auto region.
            region_ = GapDataListRegion::kAuto;
            current_region_slots_remaining_ = counts_.auto_repeat;
            repeated_value_idx_ = 0;
          } else {
            // Auto-repeater is present but squashed due
            // `counts_.leading` + `counts_.trailing`
            // being greater than or equal to the number of gaps, so jump to
            // trailing segment.
            region_ = GapDataListRegion::kTrailing;
            current_region_slots_remaining_ = counts_.trailing;
            list_idx_ = counts_.auto_idx + 1;
            InitNonAutoDataState();
          }
        }
        break;
      case kAuto:
        // Move from the auto region to the trailing region.
        region_ = GapDataListRegion::kTrailing;
        current_region_slots_remaining_ = counts_.trailing;
        list_idx_ = counts_.auto_idx + 1;
        InitNonAutoDataState();
        break;
      case kTrailing:
        // Should mark end of iteration.
        CHECK_EQ(current_gap_index_, gap_count_);
        break;
    }
  }

  void InitNonAutoDataState() {
    const GapData& gap_data = gap_data_list_[list_idx_];
    if (gap_data.IsRepeaterData()) {
      CHECK(!gap_data.GetValueRepeater()->IsAutoRepeater());
      repeats_left_ = gap_data.GetValueRepeater()->RepeatCount();
    } else {
      repeats_left_ = 1;
    }
    repeated_value_idx_ = 0;
  }

  // Excluded for performance reasons: this iterator is short-lived, so BRP
  // ref-count churn would cost more than the protection is worth.
  RAW_PTR_EXCLUSION const GapDataVector& gap_data_list_;
  wtf_size_t gap_count_;
  const gap_data_list_internal::RegionSlotCounts counts_;

  // Index of the current gap to which we are assigning a gap data.
  wtf_size_t current_gap_index_ = 0;

  // Traversal states.
  GapDataListRegion region_;
  wtf_size_t current_region_slots_remaining_ = 0;

  // Internal iterators states.
  wtf_size_t list_idx_ = 0;
  wtf_size_t repeats_left_ = 0;
  wtf_size_t repeated_value_idx_ = 0;
};

// Provides random access to the value assigned to a given gap slot. The caller
// determines that slot, for example when placement order differs from paint
// order.
//
// It shares region boundaries with `GapDataListIterator` (see
// `gap_data_list_internal::ComputeRegionSlotCounts`), but never expands the
// list: fixed regions containing only single values use direct indexing, fixed
// regions with integer repeaters use binary search over cumulative per-entry
// slot counts, and the auto-repeat region uses direct modulo arithmetic.
//
// This accessor provides O(1) access in the common cases, such as single-value
// lists, and lists without integer repeaters. For fixed regions containing
// integer repeaters, lookup is O(log n), where n is the number of unexpanded
// entries in that region.
// TODO(javiercon): After profiling reversed and fragmented cases, consider
// using `GapDataListValueAccessor` for all paths and removing the Iterator
// path.
template <typename T>
class CORE_EXPORT GapDataListValueAccessor {
  DISALLOW_NEW();

 public:
  using GapDataVector = GapDataList<T>::GapDataVector;
  using GapData = GapData<T>;

  GapDataListValueAccessor(const GapDataVector& gap_data_list,
                           wtf_size_t gap_count)
      : gap_data_list_(gap_data_list), gap_count_(gap_count) {
    CHECK(!gap_data_list_.empty());
    const auto counts = gap_data_list_internal::ComputeRegionSlotCounts(
        gap_data_list_, gap_count_);
    auto_repeat_slot_count_ = counts.auto_repeat;
    auto_idx_ = counts.auto_idx;

    if (!HasAutoRepeater()) {
      leading_region_ =
          BuildFixedRegion(0, gap_data_list_.size(), counts.leading,
                           counts.leading_has_integer_repeaters);
    } else {
      leading_region_ = BuildFixedRegion(0, auto_idx_, counts.leading,
                                         counts.leading_has_integer_repeaters);
      trailing_region_ = BuildFixedRegion(
          auto_idx_ + 1, gap_data_list_.size(), counts.trailing,
          counts.trailing_has_integer_repeaters);
    }
  }

  // Returns the value assigned to gap slot `index`, out of `gap_count` total
  // slots.
  T ValueAt(wtf_size_t index) const {
    CHECK_LT(index, gap_count_);

    if (!HasAutoRepeater()) {
      // No auto-repeater: the whole list is one region that cycles to fill
      // any slots beyond its own total.
      return ValueInFixedRegion(leading_region_,
                                index % leading_region_.total_slots);
    }
    const wtf_size_t leading_slot_count = leading_region_.total_slots;
    if (index < leading_slot_count) {
      return ValueInFixedRegion(leading_region_, index);
    }
    if (index < leading_slot_count + auto_repeat_slot_count_) {
      const auto& repeated_values =
          gap_data_list_[auto_idx_].GetValueRepeater()->RepeatedValues();
      CHECK(!repeated_values.empty());
      return repeated_values[(index - leading_slot_count) %
                             repeated_values.size()];
    }
    const wtf_size_t trailing_start =
        leading_slot_count + auto_repeat_slot_count_;
    CHECK_GE(index, trailing_start);
    return ValueInFixedRegion(trailing_region_, index - trailing_start);
  }

 private:
  // A contiguous run of non-auto-repeater entries in `gap_data_list_`. Plain
  // values use direct indexing. Regions with integer repeaters store cumulative
  // slot counts for binary search.
  struct FixedRegion {
    wtf_size_t start_list_index = 0;
    wtf_size_t total_slots = 0;
    // Cumulative boundaries for regions with integer repeaters.
    Vector<wtf_size_t> slot_ends;
  };

  bool HasAutoRepeater() const { return auto_idx_ != kNotFound; }

  FixedRegion BuildFixedRegion(wtf_size_t begin,
                               wtf_size_t end,
                               wtf_size_t total_slots,
                               bool has_integer_repeaters) const {
    CHECK_LE(begin, end);
    CHECK_LE(end, gap_data_list_.size());
    FixedRegion region;
    region.start_list_index = begin;
    region.total_slots = total_slots;
    if (!has_integer_repeaters) {
      CHECK_EQ(total_slots, end - begin);
      return region;
    }

    region.slot_ends.ReserveInitialCapacity(end - begin);
    wtf_size_t running_total = 0;
    for (wtf_size_t i = begin; i < end; ++i) {
      running_total += gap_data_list_[i].GetFixedSlotCount();
      region.slot_ends.push_back(running_total);
    }
    CHECK_EQ(running_total, total_slots);
    return region;
  }

  T ValueInFixedRegion(const FixedRegion& region,
                       wtf_size_t local_index) const {
    CHECK_LT(local_index, region.total_slots);
    if (region.slot_ends.empty()) {
      return gap_data_list_[region.start_list_index + local_index].GetValue();
    }

    DCHECK(std::is_sorted(region.slot_ends.begin(), region.slot_ends.end()));
    const auto it = std::upper_bound(region.slot_ends.begin(),
                                     region.slot_ends.end(), local_index);
    CHECK(it != region.slot_ends.end());
    const wtf_size_t entry_offset =
        static_cast<wtf_size_t>(it - region.slot_ends.begin());
    const wtf_size_t entry_start =
        entry_offset == 0 ? 0 : region.slot_ends[entry_offset - 1];
    const GapData& gap_data =
        gap_data_list_[region.start_list_index + entry_offset];
    if (!gap_data.IsRepeaterData()) {
      return gap_data.GetValue();
    }
    const auto& repeated_values = gap_data.GetValueRepeater()->RepeatedValues();
    return repeated_values[(local_index - entry_start) %
                           repeated_values.size()];
  }

  const GapDataVector& gap_data_list_;
  const wtf_size_t gap_count_;

  // The auto-repeat region's slot count, and its index in `gap_data_list_`
  // (`kNotFound` if there is no auto-repeater). The leading/trailing fixed
  // regions' slot counts are `leading_region_.total_slots` and
  // `trailing_region_.total_slots`.
  wtf_size_t auto_repeat_slot_count_ = 0;
  wtf_size_t auto_idx_ = kNotFound;

  FixedRegion leading_region_;
  FixedRegion trailing_region_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_
