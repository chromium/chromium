// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/utf16.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

namespace blink {

bool IsWellFormed(base::span<const UChar> text) {
  for (size_t i = 0; i < text.size(); ++i) {
    const UChar c = text[i];
    if (U16_IS_LEAD(c)) {
      // A leading surrogate must be immediately followed by a trailing one.
      if (i + 1 >= text.size() || !U16_IS_TRAIL(text[i + 1])) {
        return false;
      }
      ++i;  // Skip the trailing surrogate of this valid pair.
    } else if (U16_IS_TRAIL(c)) {
      // A trailing surrogate not preceded by a leading surrogate.
      return false;
    }
  }
  return true;
}

}  // namespace blink
