// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_

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

  explicit GapDataList(Vector<Length>& lengths) {
    for (const auto& length : lengths) {
      gap_data_list_.emplace_back(GapData<int>(length.Pixels()));
    }
  }

  explicit GapDataList(HeapVector<StyleColor, 1>& colors) {
    for (const auto& color : colors) {
      gap_data_list_.emplace_back(GapData<StyleColor>(color));
    }
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

  void Trace(Visitor* visitor) const {
    visitor->Trace(gap_data_list_);
  }

  const GapDataVector& GetGapDataList() const { return gap_data_list_; }

  bool HasSingleValue() const {
    return gap_data_list_.size() == 1 && !gap_data_list_[0].IsRepeaterData();
  }

  const T GetLegacyValue() const {
    return gap_data_list_[0].GetValue();
  }

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
  explicit GapDataListIterator(const GapDataVector& gap_data_list,
                               wtf_size_t gap_count)
      : gap_data_list_(gap_data_list), gap_count_(gap_count) {
    CHECK(!gap_data_list_.empty());
    BuildRegions();

    if (auto_idx_ == 0) {
      // Here, the auto repeater is the first item, so start at kAuto region.
      region_ = kAuto;
      current_region_slots_remaining_ = auto_repeat_slot_count_;
      repeated_value_idx_ = 0;
    } else {
      // Auto-repeater is not the first item, start at kLeading region.
      region_ = kLeading;
      current_region_slots_remaining_ = leading_slot_count_;
      list_idx_ = 0;
      InitNonAutoDataState();
    }
  }

  bool HasNext() const { return current_gap_index_ < gap_count_; }

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
  // Iterates through `gap_data_list_` to determine region boundaries and slot
  // counts.
  void BuildRegions() {
    leading_slot_count_ = 0;
    trailing_slot_count_ = 0;
    auto_repeat_slot_count_ = 0;
    auto_idx_ = kNotFound;

    for (wtf_size_t i = 0; i < gap_data_list_.size(); ++i) {
      const GapData& gap_data = gap_data_list_[i];

      wtf_size_t gap_data_slot_count = 1;
      if (gap_data.IsRepeaterData()) {
        if (gap_data.GetValueRepeater()->IsAutoRepeater()) {
          CHECK_EQ(auto_idx_, kNotFound);
          auto_idx_ = i;
          continue;
        }
        gap_data_slot_count =
            gap_data.GetValueRepeater()->RepeatCount() *
            gap_data.GetValueRepeater()->RepeatedValues().size();
      }

      if (auto_idx_ == kNotFound) {
        leading_slot_count_ += gap_data_slot_count;
      } else {
        trailing_slot_count_ += gap_data_slot_count;
      }
    }
    if (auto_idx_ != kNotFound) {
      // Compute the number of slots allocated to the auto region. If the
      // combined slots from leading and trailing regions is greater than the
      // total gap count, the auto region slot count remains zero.
      wtf_size_t combined_slot_count =
          leading_slot_count_ + trailing_slot_count_;
      if (combined_slot_count < gap_count_) {
        auto_repeat_slot_count_ = gap_count_ - combined_slot_count;
      }
    }
  }

  // Retrieves the current value based on the region and index.
  T GetData() const {
    const GapData& gap_data =
        gap_data_list_[region_ == kAuto ? auto_idx_ : list_idx_];
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
    wtf_size_t repeated_auto_values_size =
        gap_data_list_[auto_idx_].GetValueRepeater()->RepeatedValues().size();
    repeated_value_idx_ = (repeated_value_idx_ + 1) % repeated_auto_values_size;
  }

  void TransitionToNextRegion() {
    switch (region_) {
      case kLeading:
        if (auto_idx_ == kNotFound) {
          // No auto-repeater, so cycle back to the leading.
          current_region_slots_remaining_ = leading_slot_count_;
          list_idx_ = 0;
          InitNonAutoDataState();
        } else {
          if (auto_repeat_slot_count_ > 0) {
            // Move from leading to the auto region.
            region_ = GapDataListRegion::kAuto;
            current_region_slots_remaining_ = auto_repeat_slot_count_;
            repeated_value_idx_ = 0;
          } else {
            // Auto-repeater is present but squashed due
            // `leading_slot_count_` + `trailing_slot_count_`
            // being greater than or equal to the number of gaps, so jump to
            // trailing segment.
            region_ = GapDataListRegion::kTrailing;
            current_region_slots_remaining_ = trailing_slot_count_;
            list_idx_ = auto_idx_ + 1;
            InitNonAutoDataState();
          }
        }
        break;
      case kAuto:
        // Move from the auto region to the trailing region.
        region_ = GapDataListRegion::kTrailing;
        current_region_slots_remaining_ = trailing_slot_count_;
        list_idx_ = auto_idx_ + 1;
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

  const GapDataVector& gap_data_list_;
  wtf_size_t gap_count_;

  // Index of the current gap to which we are assigning a gap data.
  wtf_size_t current_gap_index_ = 0;

  wtf_size_t leading_slot_count_, auto_repeat_slot_count_, trailing_slot_count_;

  // Traversal states.
  GapDataListRegion region_;
  wtf_size_t current_region_slots_remaining_ = 0;

  // Internal iterators states.
  wtf_size_t list_idx_ = 0;
  wtf_size_t auto_idx_ = kNotFound;
  wtf_size_t repeats_left_ = 0;
  wtf_size_t repeated_value_idx_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_
