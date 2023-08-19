// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_height.h"

namespace blink {

void FontHeight::AddLeading(const FontHeight& start_and_end_leading) {
  DCHECK(!IsEmpty());
  ascent += start_and_end_leading.ascent;
  descent += start_and_end_leading.descent;
}

void FontHeight::Move(LayoutUnit delta) {
  DCHECK(!IsEmpty());
  ascent -= delta;
  descent += delta;
}

void FontHeight::Unite(const FontHeight& other) {
  ascent = std::max(ascent, other.ascent);
  descent = std::max(descent, other.descent);
}

void FontHeight::operator+=(const FontHeight& other) {
  DCHECK(ascent != LayoutUnit::Min() && descent != LayoutUnit::Min());
  DCHECK(other.ascent != LayoutUnit::Min() &&
         other.descent != LayoutUnit::Min());
  ascent += other.ascent;
  descent += other.descent;
}

std::ostream& operator<<(std::ostream& stream, const FontHeight& metrics) {
  return stream << "ascent=" << metrics.ascent
                << ", descent=" << metrics.descent;
}

}  // namespace blink
