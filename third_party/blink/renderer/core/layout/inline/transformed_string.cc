// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/transformed_string.h"

namespace blink {

TransformedString TransformedString::Substring(unsigned start,
                                               unsigned length) const {
  StringView sub_view = StringView(view_, start, length);
  if (length_map_.empty()) {
    return TransformedString(sub_view);
  }
  CHECK_EQ(view_.length(), length_map_.size());
  CHECK_LE(start, view_.length());
  CHECK_LE(start + length, view_.length());
  return TransformedString(sub_view, length_map_.subspan(start, length));
}

}  // namespace blink
