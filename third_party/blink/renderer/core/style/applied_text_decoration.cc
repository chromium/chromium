// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/applied_text_decoration.h"

namespace blink {

AppliedTextDecoration::AppliedTextDecoration(
    TextDecorationLine line,
    ETextDecorationStyle style,
    Color color,
    TextDecorationThickness thickness,
    Length underline_offset,
    TextDecorationInset decoration_inset,
    EBoxDecorationBreak box_decoration_break)

    : lines_(static_cast<unsigned>(line)),
      style_(static_cast<unsigned>(style)),
      box_decoration_break_(static_cast<unsigned>(box_decoration_break)),
      color_(color),
      thickness_(thickness),
      underline_offset_(underline_offset),
      decoration_inset_(decoration_inset) {}

bool AppliedTextDecoration::operator==(const AppliedTextDecoration&) const =
    default;

}  // namespace blink
