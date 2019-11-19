// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_UTILS_H_

#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

namespace blink {

// Returns the NG line box fragment containing the caret position of the given
// position. Returns false if the position is not in Layout NG, or does not
// have any caret position.
NGInlineCursor NGContainingLineBoxOf(const PositionWithAffinity&);

// Returns true if the caret positions of the two positions are in the same NG
// line box. Returns false in all other cases.
bool InSameNGLineBox(const PositionWithAffinity&, const PositionWithAffinity&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_LINE_UTILS_H_
