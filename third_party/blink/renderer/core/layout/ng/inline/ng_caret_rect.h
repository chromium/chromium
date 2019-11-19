// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_RECT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"

namespace blink {

// This file provides utility functions for computing caret rect in LayoutNG.

struct LocalCaretRect;

// Given a position with affinity, returns the caret rect if the position is
// laid out with LayoutNG, and a caret can be placed at the position with the
// given affinity. The caret rect location is local to the containing inline
// formatting context.
CORE_EXPORT LocalCaretRect ComputeNGLocalCaretRect(const PositionWithAffinity&);

// Almost the same as ComputeNGLocalCaretRect, except that the returned rect
// is adjusted to span the containing line box in the block direction.
CORE_EXPORT LocalCaretRect
ComputeNGLocalSelectionRect(const PositionWithAffinity&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_RECT_H_
