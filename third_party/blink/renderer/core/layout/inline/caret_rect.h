// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_CARET_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_CARET_RECT_H_

#include "third_party/blink/renderer/core/editing/forward.h"

namespace blink {

// This file provides utility functions for computing caret rect in LayoutNG.

struct InlineCaretPosition;
struct LocalCaretRect;
struct LogicalRect;
class InlineCursor;
class ComputedStyle;

enum class CaretShape { kBar, kBlock, kUnderscore };

// Get the caret shape from the computed style.
CaretShape GetCaretShapeFromComputedStyle(const ComputedStyle& style);

// Given a position, returns the caret rect.
LocalCaretRect ComputeLocalCaretRect(const InlineCaretPosition&, CaretShape);

// Almost the same as ComputeNGLocalCaretRect, except that the returned rect
// is adjusted to span the containing line box in the block direction.
LocalCaretRect ComputeLocalSelectionRect(const InlineCaretPosition&);

// Given the inlineCursor and caret shape, get the logical rect of the caret at
// the given offset.
LogicalRect GetCaretRectAtTextOffset(const InlineCursor&,
                                     unsigned text_offset,
                                     CaretShape);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_INLINE_CARET_RECT_H_
