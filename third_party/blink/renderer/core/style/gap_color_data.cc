// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/gap_color_data.h"

namespace blink {

StyleColorRepeater::StyleColorRepeater(StyleColorVector repeated_colors)
    : repeated_colors_(repeated_colors) {
  CHECK(repeated_colors_.size() > 0);
}

StyleColorRepeater::StyleColorRepeater(StyleColorVector repeated_colors,
                                       wtf_size_t repeat_count)
    : StyleColorRepeater(repeated_colors) {
  repeat_count_ = repeat_count;
}

bool StyleColorRepeater::operator==(const StyleColorRepeater& other) const {
  return repeated_colors_ == other.repeated_colors_ &&
         repeat_count_ == other.repeat_count_;
}

GapColorData::GapColorData(StyleColor gap_color) : gap_color_(gap_color) {}

GapColorData::GapColorData(StyleColorRepeater* color_repeater)
    : color_repeater_(color_repeater) {}

bool GapColorData::operator==(const GapColorData& other) const {
  return gap_color_ == other.gap_color_ &&
         base::ValuesEquivalent(color_repeater_, other.color_repeater_);
}

}  // namespace blink
