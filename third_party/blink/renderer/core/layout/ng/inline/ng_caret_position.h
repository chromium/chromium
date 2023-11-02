// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_POSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_POSITION_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/forward.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutBlockFlow;

// An NGCaretPosition indicates a caret position relative to an inline
// NGPaintFragment:
// - When |fragment| is box, |position_type| is either |kBeforeBox| or
// |kAfterBox|, indicating either of the two caret positions by the box sides;
// |text_offset| is |nullopt| in this case.
// - When |fragment| is text, |position_type| is |kAtTextOffset|, and
// |text_offset| is in the text offset range of the fragment.
//
// TODO(xiaochengh): Support "in empty container" caret type

enum class NGCaretPositionType { kBeforeBox, kAfterBox, kAtTextOffset };
struct NGCaretPosition {
  STACK_ALLOCATED();

 public:
  explicit operator bool() const { return IsNotNull(); }

  bool IsNotNull() const { return cursor.IsNotNull(); }
  bool IsNull() const { return cursor.IsNull(); }

  Position ToPositionInDOMTree() const;
  PositionWithAffinity ToPositionInDOMTreeWithAffinity() const;

  NGInlineCursor cursor;
  NGCaretPositionType position_type;
  absl::optional<unsigned> text_offset;
};

// Given an inline formatting context, a text offset in the context and a text
// affinity, returns the corresponding NGCaretPosition, or null if not found.
// Note that in many cases, null result indicates that we have reached an
// unexpected case that is not properly handled.
// When |layout_text| isn't |nullptr|, this functions returns position for
// |layout_text| in multiple candidates.
CORE_EXPORT NGCaretPosition
ComputeNGCaretPosition(const LayoutBlockFlow& context,
                       unsigned offset,
                       TextAffinity affinity,
                       const LayoutText* layout_text = nullptr);

// Shorthand of the above when the input is a position instead of a
// (context, offset) pair.
CORE_EXPORT NGCaretPosition ComputeNGCaretPosition(const PositionWithAffinity&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_POSITION_H_
