// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_OFFSET_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

// Represents a range of text, as a |start| and |end| offset pair.
struct CORE_EXPORT NGTextOffset {
  NGTextOffset() = default;
  NGTextOffset(unsigned start, unsigned end) : start(start), end(end) {
    AssertValid();
  }

  unsigned Length() const {
    AssertValid();
    return end - start;
  }

  void AssertValid() const { DCHECK_GE(end, start); }
  void AssertNotEmpty() const { DCHECK_GT(end, start); }

  bool operator==(const NGTextOffset& other) const {
    return start == other.start && end == other.end;
  }
  bool operator!=(const NGTextOffset& other) const {
    return !operator==(other);
  }

  unsigned start;
  unsigned end;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const NGTextOffset&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_OFFSET_H_
