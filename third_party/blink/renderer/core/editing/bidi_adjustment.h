// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_BIDI_ADJUSTMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_BIDI_ADJUSTMENT_H_

#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class InlineBox;
struct InlineBoxPosition;
struct NGCaretPosition;
class NGPaintFragment;
enum class TextDirection : uint8_t;

class BidiAdjustment final {
  STATIC_ONLY(BidiAdjustment);

 public:
  // Function to be called at the end of caret position resolution, adjusting
  // the result in bidi text runs.
  static InlineBoxPosition AdjustForCaretPositionResolution(
      const InlineBoxPosition&);
  static NGCaretPosition AdjustForCaretPositionResolution(
      const NGCaretPosition&);

  // Function to be called at the end of hit tests, adjusting the result in bidi
  // text runs.
  static InlineBoxPosition AdjustForHitTest(const InlineBoxPosition&);
  static NGCaretPosition AdjustForHitTest(const NGCaretPosition&);

  // Function to be called at the end of creating a range selection by mouse
  // dragging, ensuring that the created range selection matches the dragging
  // even with bidi adjustment.
  // TODO(editing-dev): Eliminate |VisiblePosition| from this function.
  static SelectionInFlatTree AdjustForRangeSelection(
      const PositionInFlatTreeWithAffinity&,
      const PositionInFlatTreeWithAffinity&);
};

TextDirection ParagraphDirectionOf(const InlineBox&);
TextDirection ParagraphDirectionOf(const NGPaintFragment&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_BIDI_ADJUSTMENT_H_
