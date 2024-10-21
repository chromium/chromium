// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_COLOR_DATA_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_COLOR_DATA_LIST_H_

#include "third_party/blink/renderer/core/style/gap_color_data.h"

namespace blink {

// These are used to store gap decoration color values in the order they are
// specified. These values can be an auto repeater, an integer repeater, or a
// single color. See:
// https://kbabbitt.github.io/css-gap-decorations/#column-row-rule-color
class CORE_EXPORT GapColorDataList {
  DISALLOW_NEW();

 public:
  GapColorDataList() = default;

  static GapColorDataList DefaultGapColorDataList() {
    StyleColor color = StyleColor::CurrentColor();
    auto default_gap_color_list = GapColorDataList(color);
    return default_gap_color_list;
  }

  explicit GapColorDataList(GapDataVector&& gap_color_data_list)
      : gap_color_data_list_(gap_color_data_list) {
    DCHECK(!gap_color_data_list_.empty());
  }

  explicit GapColorDataList(const StyleColor& color) {
    gap_color_data_list_.emplace_back(GapColorData(color));
  }

  void Trace(Visitor* visitor) const { visitor->Trace(gap_color_data_list_); }

  const GapDataVector& GetGapColorList() const { return gap_color_data_list_; }

  const StyleColor GetLegacyGapColor() const {
    CHECK_EQ(gap_color_data_list_.size(), 1U);
    return gap_color_data_list_[0].GetGapColor();
  }

  bool operator==(const GapColorDataList& o) const {
    return gap_color_data_list_ == o.gap_color_data_list_;
  }

  bool operator!=(const GapColorDataList& o) const { return !(*this == o); }

 private:
  GapDataVector gap_color_data_list_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_GAP_COLOR_DATA_LIST_H_
