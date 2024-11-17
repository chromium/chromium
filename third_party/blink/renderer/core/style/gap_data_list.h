// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_

#include "third_party/blink/renderer/core/style/gap_data.h"

namespace blink {

// These are used to store gap decorations values in the order they are
// specified. These values can be an auto repeater, an integer repeater, or a
// single value. The value could be a color, style or width. See:
// https://kbabbitt.github.io/css-gap-decorations/#color-style-width
template <typename T>
class CORE_EXPORT GapDataList {
  DISALLOW_NEW();

 public:
  using GapDataVector = HeapVector<GapData<T>, 1>;
  GapDataList() = default;

  static GapDataList DefaultGapColorDataList() {
    StyleColor color = StyleColor::CurrentColor();
    auto default_gap_color_list = GapDataList(color);
    return default_gap_color_list;
  }

  explicit GapDataList(GapDataVector&& gap_data_list)
      : gap_data_list_(gap_data_list) {
    CHECK(!gap_data_list_.empty());
  }

  explicit GapDataList(const T& value) {
    gap_data_list_.emplace_back(GapData<T>(value));
  }

  void Trace(Visitor* visitor) const { visitor->Trace(gap_data_list_); }

  const GapDataVector& GetGapDataList() const { return gap_data_list_; }

  const T GetLegacyValue() const {
    CHECK_EQ(gap_data_list_.size(), 1U);
    return gap_data_list_[0].GetValue();
  }

  bool operator==(const GapDataList& o) const {
    return gap_data_list_ == o.gap_data_list_;
  }

  bool operator!=(const GapDataList& o) const { return !(*this == o); }

 private:
  GapDataVector gap_data_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_DATA_LIST_H_
