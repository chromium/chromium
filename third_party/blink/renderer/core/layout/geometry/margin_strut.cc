// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/margin_strut.h"

#include <algorithm>

namespace blink {

void MarginStrut::Append(const LayoutUnit& value, bool is_quirky) {
  if (is_quirky_container_start && is_quirky)
    return;

  if (value < 0) {
    negative_margin = std::min(value, negative_margin);
  } else {
    if (is_quirky) {
      DCHECK(value >= 0);

      quirky_positive_margin = std::max(value, quirky_positive_margin);
    } else {
      positive_margin = std::max(value, positive_margin);
    }
  }
}

bool MarginStrut::IsEmpty() const {
  if (discard_margins)
    return true;
  return positive_margin == LayoutUnit() && negative_margin == LayoutUnit() &&
         quirky_positive_margin == LayoutUnit();
}

bool MarginStrut::operator==(const MarginStrut& other) const {
  return positive_margin == other.positive_margin &&
         negative_margin == other.negative_margin &&
         quirky_positive_margin == other.quirky_positive_margin &&
         discard_margins == other.discard_margins &&
         is_quirky_container_start == other.is_quirky_container_start;
}

}  // namespace blink
