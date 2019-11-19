// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_OFFSET_H_

#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

// Represents a range of text, as a |start| and |end| offset pair.
struct CORE_EXPORT NGTextOffset {
  NGTextOffset() = default;
  NGTextOffset(unsigned start, unsigned end) : start(start), end(end) {
    DCHECK_GE(end, start);
  }

  unsigned Length() const { return end - start; }

  void AssertValid() const { DCHECK_GE(end, start); }

  unsigned start;
  unsigned end;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_TEXT_OFFSET_H_
