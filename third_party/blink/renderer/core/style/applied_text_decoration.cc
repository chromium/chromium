// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/applied_text_decoration.h"

namespace blink {

AppliedTextDecoration::AppliedTextDecoration(TextDecorationLine line,
                                             ETextDecorationStyle style,
                                             Color color,
                                             TextDecorationThickness thickness,
                                             Length underline_offset)

    : lines_(static_cast<unsigned>(line)),
      style_(static_cast<unsigned>(style)),
      color_(color),
      thickness_(thickness),
      underline_offset_(underline_offset) {}

bool AppliedTextDecoration::operator==(const AppliedTextDecoration& o) const {
  return color_ == o.color_ && lines_ == o.lines_ && style_ == o.style_ &&
         thickness_ == o.thickness_ && underline_offset_ == o.underline_offset_;
}

}  // namespace blink
