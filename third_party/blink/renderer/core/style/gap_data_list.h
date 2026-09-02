// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_

#include <algorithm>

#include "third_party/blink/renderer/core/style/gap_data.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder_stream.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

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

  bool operator==(const GapDataList& o) const {
    return gap_data_list_ == o.gap_data_list_;
  }

 private:
  GapDataVector gap_data_list_;
};

// Provides random access to the value assigned to a given gap slot. The caller
// determines that slot, for example when placement order differs from paint
// order.
//
// It never expands the list: fixed regions containing only single values use
// direct indexing, fixed regions with integer repeaters use binary search over
// cumulative per-entry slot counts, and the auto-repeat region uses direct
// modulo arithmetic.
//
// This accessor provides O(1) access in the common cases, such as single-value
// lists, and lists without integer repeaters. For fixed regions containing
// integer repeaters, lookup is O(log n), where n is the number of unexpanded
// entries in that region.
template <typename T>
class CORE_EXPORT GapDataListValueAccessor {
  DISALLOW_NEW();

 public:
  using GapDataVector = GapDataList<T>::GapDataVector;
  using GapData = GapData<T>;

  GapDataListValueAccessor(const GapDataVector& gap_data_list,
                           wtf_size_t gap_slot_count)
      : gap_data_list_(gap_data_list), gap_slot_count_(gap_slot_count) {
    CHECK(!gap_data_list_.empty());
    const auto counts = ComputeRegionSlotCounts();
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

  // Retargets this accessor to a different number of gap slots, e.g. when a
  // grid-lanes `CrossGap`'s decoration values are assigned independently
  // within its own lane, rather than across the whole container.
  void SetGapSlotCount(wtf_size_t gap_slot_count) {
    if (gap_slot_count == gap_slot_count_) {
      return;
    }
    gap_slot_count_ = gap_slot_count;
    if (HasAutoRepeater()) {
      // Fixed regions do not depend on the gap count, so only the auto-repeat
      // region needs to be recomputed.
      const wtf_size_t combined_slot_count =
          leading_region_.total_slots + trailing_region_.total_slots;
      auto_repeat_slot_count_ = combined_slot_count < gap_slot_count_
                                    ? gap_slot_count_ - combined_slot_count
                                    : 0;
    }
  }

  // Returns the value assigned to gap slot `index`, out of the configured
  // number of gap slots.
  T ValueAt(wtf_size_t index) const {
    CHECK_LT(index, gap_slot_count_);

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
  // The Leading/Auto/Trailing region slot counts and the auto-repeater index
  // for a `GapDataList`, given a known gap count.
  struct RegionSlotCounts {
    wtf_size_t leading = 0;
    wtf_size_t auto_repeat = 0;
    wtf_size_t trailing = 0;
    wtf_size_t auto_idx = kNotFound;
    bool leading_has_integer_repeaters = false;
    bool trailing_has_integer_repeaters = false;
  };

  // A contiguous run of non-auto-repeater entries in `gap_data_list_`. Plain
  // values use direct indexing. Regions with integer repeaters store cumulative
  // slot counts for binary search.
  struct FixedRegion {
    wtf_size_t start_list_index = 0;
    wtf_size_t total_slots = 0;
    // Cumulative boundaries for regions with integer repeaters.
    Vector<wtf_size_t> slot_ends;
  };

  RegionSlotCounts ComputeRegionSlotCounts() const {
    RegionSlotCounts counts;

    for (wtf_size_t i = 0; i < gap_data_list_.size(); ++i) {
      const auto& gap_data = gap_data_list_[i];

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
      if (combined_slot_count < gap_slot_count_) {
        counts.auto_repeat = gap_slot_count_ - combined_slot_count;
      }
    }
    return counts;
  }

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
  // Usually the container's total gap count. For grid-lanes, this may instead
  // be the number of gaps in one lane.
  wtf_size_t gap_slot_count_;

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
