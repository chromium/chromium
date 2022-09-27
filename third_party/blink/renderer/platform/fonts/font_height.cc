// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/font_height.h"

namespace blink {

void FontHeight::AddLeading(LayoutUnit line_height) {
  DCHECK(!IsEmpty());
  LayoutUnit half_leading = (line_height - (ascent + descent)) / 2;
  // TODO(kojii): floor() is to make text dump compatible with legacy test
  // results. Revisit when we paint.
  ascent += half_leading.Floor();
  descent = line_height - ascent;
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
